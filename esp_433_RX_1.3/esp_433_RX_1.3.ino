// ================================================================
//  Arduino + 433 MHz ASK Receiver
//  Compatible with ESP8266 TX TankMessage (2 Float Switch Version)
// ================================================================

#include <RH_ASK.h>
#include <SPI.h>   // Required by RH_ASK

// ────────────────────────────────────────────────
// CONFIGURATION
// ────────────────────────────────────────────────

#define RF_RX_PIN       11      // DATA pin of 433 MHz receiver
#define MOTOR_PIN       12      // Motor/pump control relay
#define STATUS_LED_PIN  LED_BUILTIN  // Optional status LED

// Thresholds for motor control
#define MOTOR_ON_LEVEL  50      // Turn motor ON when level drops to or below this
#define MOTOR_OFF_LEVEL 100     // Turn motor OFF when level reaches this

// Timeout settings
#define RX_TIMEOUT_MS   300000  // 5 minutes - if no signal, assume problem
unsigned long lastReceiveTime = 0;

// Message must EXACTLY match transmitter
struct __attribute__((packed)) TankMessage {
    uint8_t  deviceId;
    uint8_t  levelPercent;
    uint16_t battery_mV;
    uint8_t  reserved;
};

// RH_ASK(speed, rxPin, txPin)
RH_ASK driver(2000, RF_RX_PIN, -1);

// Motor state tracking
bool motorRunning = false;
uint8_t lastKnownLevel = 0;

// ────────────────────────────────────────────────
// SETUP
// ────────────────────────────────────────────────

void setup()
{
    Serial.begin(9600);
    Serial.println(F("=============================================="));
    Serial.println(F("  433 MHz Tank Receiver - 2 Float Version"));
    Serial.println(F("=============================================="));
    Serial.println();

    pinMode(MOTOR_PIN, OUTPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    
    // Start with motor OFF (relay HIGH = OFF for most relay modules)
    motorOff();
    
    Serial.print(F("Initializing RF receiver on pin "));
    Serial.print(RF_RX_PIN);
    Serial.print(F("... "));
    
    if (!driver.init()) {
        Serial.println(F("FAILED!"));
        Serial.println(F("Check wiring and restart"));
        while (1) {
            // Blink LED to indicate error
            digitalWrite(STATUS_LED_PIN, HIGH);
            delay(100);
            digitalWrite(STATUS_LED_PIN, LOW);
            delay(100);
        }
    }
    
    Serial.println(F("OK"));
    Serial.println();
    Serial.println(F("Motor Control Logic:"));
    Serial.print(F("  - Turn ON when level <= "));
    Serial.print(MOTOR_ON_LEVEL);
    Serial.println(F("%"));
    Serial.print(F("  - Turn OFF when level = "));
    Serial.print(MOTOR_OFF_LEVEL);
    Serial.println(F("%"));
    Serial.println();
    Serial.println(F("Waiting for data..."));
    Serial.println();
    
    lastReceiveTime = millis();
}

// ────────────────────────────────────────────────
// MOTOR CONTROL FUNCTIONS
// ────────────────────────────────────────────────

void motorOn()
{
    if (!motorRunning) {
        digitalWrite(MOTOR_PIN, LOW);  // LOW = ON for most relays
        motorRunning = true;
        
        Serial.println(F(">>> MOTOR ON <<<"));
        
        // Blink LED fast to indicate motor ON
        for (int i = 0; i < 5; i++) {
            digitalWrite(STATUS_LED_PIN, HIGH);
            delay(50);
            digitalWrite(STATUS_LED_PIN, LOW);
            delay(50);
        }
    }
}

void motorOff()
{
    if (motorRunning || motorRunning == false) {  // Always set on first call
        digitalWrite(MOTOR_PIN, HIGH);  // HIGH = OFF for most relays
        motorRunning = false;
        
        Serial.println(F(">>> MOTOR OFF <<<"));
        
        // Blink LED slow to indicate motor OFF
        for (int i = 0; i < 3; i++) {
            digitalWrite(STATUS_LED_PIN, HIGH);
            delay(150);
            digitalWrite(STATUS_LED_PIN, LOW);
            delay(150);
        }
    }
}

// ────────────────────────────────────────────────
// MOTOR LOGIC
// ────────────────────────────────────────────────

void updateMotorState(uint8_t level)
{
    // Store last known level
    lastKnownLevel = level;
    
    // Motor control logic with hysteresis
    if (motorRunning) {
        // Motor is ON - turn OFF only when tank is full
        if (level >= MOTOR_OFF_LEVEL) {
            motorOff();
            Serial.println(F("Tank full - stopping motor"));
        }
        else {
            Serial.print(F("Motor running... Tank at "));
            Serial.print(level);
            Serial.println(F("%"));
        }
    }
    else {
        // Motor is OFF - turn ON when level drops
        if (level <= MOTOR_ON_LEVEL) {
            motorOn();
            Serial.println(F("Low water detected - starting motor"));
        }
        else {
            Serial.print(F("Motor off... Tank at "));
            Serial.print(level);
            Serial.println(F("%"));
        }
    }
}

// ────────────────────────────────────────────────
// BATTERY WARNING
// ────────────────────────────────────────────────

void checkBattery(uint16_t voltage_mV)
{
    // Typical Li-ion voltages:
    // 4200 mV = fully charged
    // 3700 mV = ~50%
    // 3400 mV = low
    // 3000 mV = critical
    
    if (voltage_mV < 3000) {
        Serial.println(F("!!! CRITICAL: Battery voltage very low !!!"));
    }
    else if (voltage_mV < 3400) {
        Serial.println(F("!!! WARNING: Battery voltage low !!!"));
    }
}

// ────────────────────────────────────────────────
// TIMEOUT CHECK
// ────────────────────────────────────────────────

void checkTimeout()
{
    if (millis() - lastReceiveTime > RX_TIMEOUT_MS) {
        Serial.println();
        Serial.println(F("!!! WARNING: No signal for 5 minutes !!!"));
        Serial.println(F("!!! Check transmitter battery/connection !!!"));
        
        // Safety: Turn off motor if no signal
        if (motorRunning) {
            Serial.println(F("Safety: Turning motor OFF due to lost signal"));
            motorOff();
        }
        
        lastReceiveTime = millis();  // Reset timeout
    }
}

// ────────────────────────────────────────────────
// LOOP
// ────────────────────────────────────────────────

void loop()
{
    uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
    uint8_t buflen = sizeof(buf);

    if (driver.recv(buf, &buflen)) {
        
        lastReceiveTime = millis();  // Update last receive time
        
        // Blink LED to show reception
        digitalWrite(STATUS_LED_PIN, HIGH);
        
        if (buflen == sizeof(TankMessage)) {
            
            TankMessage msg;
            memcpy(&msg, buf, sizeof(msg));

            // Display received data
            Serial.println(F("╔════════════════════════════════╗"));
            Serial.println(F("║     PACKET RECEIVED            ║"));
            Serial.println(F("╠════════════════════════════════╣"));
            
            Serial.print(F("║ Device ID:    "));
            Serial.print(msg.deviceId);
            Serial.println(F("                   ║"));
            
            Serial.print(F("║ Tank Level:   "));
            Serial.print(msg.levelPercent);
            Serial.print(F("%"));
            if (msg.levelPercent < 100) Serial.print(F(" "));
            if (msg.levelPercent < 10) Serial.print(F(" "));
            Serial.println(F("                ║"));
            
            Serial.print(F("║ Battery:      "));
            Serial.print(msg.battery_mV);
            Serial.println(F(" mV            ║"));
            
            Serial.print(F("║ Motor Status: "));
            Serial.print(motorRunning ? F("RUNNING") : F("OFF    "));
            Serial.println(F("           ║"));
            
            Serial.println(F("╚════════════════════════════════╝"));
            Serial.println();

            // Check battery
            checkBattery(msg.battery_mV);
            
            // Update motor based on level
            updateMotorState(msg.levelPercent);
            
            Serial.println();
        }
        else {
            Serial.print(F("Invalid packet size: "));
            Serial.print(buflen);
            Serial.print(F(" (expected "));
            Serial.print(sizeof(TankMessage));
            Serial.println(F(")"));
        }
        
        digitalWrite(STATUS_LED_PIN, LOW);
    }
    
    // Check for timeout (no signal received)
    checkTimeout();
}


/*
```

## Key Improvements:

### 1. **Better Motor Logic**
- Uses defined thresholds (`MOTOR_ON_LEVEL` and `MOTOR_OFF_LEVEL`)
- Prevents rapid on/off cycling (hysteresis)
- Clear logic: Turn ON at ≤50%, Turn OFF at 100%

### 2. **Safety Features**
- **Timeout detection**: If no signal for 5 minutes, turns motor OFF
- **Battery warnings**: Alerts when TX battery is low
- **Status LED**: Blinks to show activity

### 3. **Better Display**
- Formatted output with boxes
- Shows motor status in each packet
- Clear motor state changes

### 4. **Compatibility**
- Matches your 2-float TX code (0%, 50%, 100% levels)
- Same message structure
- Same baud rate and RF settings

## Wiring:
```
Arduino
┌─────────────────┐
│                 │
│  Pin 11 ────────┼──► 433MHz RX DATA
│                 │
│  Pin 13 ────────┼──► Relay IN (Motor Control)
│                 │
│  LED_BUILTIN ───┼──► Status LED
│                 │
│  5V ────────────┼──► 433MHz RX VCC & Relay VCC
│                 │
│  GND ───────────┼──► 433MHz RX GND & Relay GND
│                 │
└─────────────────┘
```

## Relay Connection:
```
Relay Module        Motor/Pump
┌──────────┐       ┌──────────┐
│ IN ◄─────┼───────┤ Arduino  │
│ VCC ◄────┼───────┤   5V     │
│ GND ◄────┼───────┤  GND     │
│          │       └──────────┘
│ COM ─────┼───────► AC/DC Supply +
│ NO ──────┼───────► Motor +
└──────────┘
                    Motor -
                      │
                   Supply -
```

## Testing Output Example:
```
╔════════════════════════════════╗
║     PACKET RECEIVED            ║
╠════════════════════════════════╣
║ Device ID:    1                ║
║ Tank Level:   50%              ║
║ Battery:      3750 mV          ║
║ Motor Status: RUNNING          ║
╚════════════════════════════════╝

Motor running... Tank at 50%



*/