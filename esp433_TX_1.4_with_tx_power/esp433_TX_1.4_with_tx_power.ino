// ================================================================
//  ESP8266 + 433 MHz ASK Transmitter - VERY LOW POWER VERSION
//  with Power-Controlled Float Switches AND RF Module
// ================================================================

#include <RH_ASK.h>
#include <SPI.h>
#include <ESP8266WiFi.h>

// ────────────────────────────────────────────────
// CONFIGURATION
// ────────────────────────────────────────────────

#define RF_TX_PIN         D2       // GPIO4 - Data to 433MHz TX
#define STATUS_LED_PIN    LED_BUILTIN

#define POWER_PIN         D1       // GPIO5 - Powers BOTH float switches AND 433MHz TX module
#define FLOAT_TOP_PIN     D5       // GPIO14
#define FLOAT_BOTTOM_PIN  D6       // GPIO12

const uint32_t REPORT_INTERVAL_SEC = 180;
const uint32_t SLEEP_SECONDS = REPORT_INTERVAL_SEC + 8;

const uint8_t DEVICE_ID = 1;

// ────────────────────────────────────────────────
// MESSAGE FORMAT
// ────────────────────────────────────────────────

struct __attribute__((packed)) TankMessage {
    uint8_t  deviceId;
    uint8_t  levelPercent;
    uint16_t battery_mV;
    uint8_t  reserved;
};

RH_ASK driver(2000, -1, RF_TX_PIN);

// ────────────────────────────────────────────────
// SETUP
// ────────────────────────────────────────────────

void setup()
{
    Serial.begin(9600);
    delay(50);
    Serial.println();
    Serial.println(F("=== Tank TX boot ==="));

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);

    // Power control for float switches AND RF module
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(POWER_PIN, LOW);  // OFF initially

    pinMode(FLOAT_TOP_PIN, INPUT_PULLUP);
    pinMode(FLOAT_BOTTOM_PIN, INPUT_PULLUP);

    blink(3, 80);

    // Note: We do NOT init the RF driver here anymore
    // It will be initialized in loop() when power is applied
    
    Serial.println(F("Disabling WiFi"));
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(1);
    
    Serial.println(F("Setup complete"));
}

// ────────────────────────────────────────────────
// LOOP (runs once per wake)
// ────────────────────────────────────────────────

void loop()
{
    TankMessage msg;

    msg.deviceId     = DEVICE_ID;
    
    // Step 1: Read tank level (powers sensors briefly)
    msg.levelPercent = readTankLevel();
    
    // Step 2: Read battery
    msg.battery_mV   = readBatteryVoltage_mV();
    msg.reserved     = 0;

    Serial.print(F("Device ID: "));
    Serial.println(msg.deviceId);

    Serial.print(F("Tank level: "));
    Serial.print(msg.levelPercent);
    Serial.println(F("%"));

    Serial.print(F("Battery: "));
    Serial.print(msg.battery_mV);
    Serial.println(F(" mV"));

    // Step 3: Power ON RF module and send
    Serial.println(F("Powering RF module ON"));
    digitalWrite(POWER_PIN, HIGH);
    delay(100);  // Give RF module time to stabilize
    
    // Initialize RF driver (now that power is applied)
    Serial.print(F("Init RF driver... "));
    if (!driver.init()) {
        Serial.println(F("FAILED"));
        digitalWrite(POWER_PIN, LOW);  // Turn off power on failure
        blink(5, 60);
    }
    else {
        Serial.println(F("OK"));
        
        // Send the message
        digitalWrite(STATUS_LED_PIN, LOW);
        bool ok = driver.send((uint8_t*)&msg, sizeof(msg));
        driver.waitPacketSent();
        digitalWrite(STATUS_LED_PIN, HIGH);

        Serial.print(F("RF send: "));
        Serial.println(ok ? F("OK") : F("FAIL"));

        if (ok) blink(2, 40);
        else    blink(5, 60);
    }
    
    // Power OFF RF module
    digitalWrite(POWER_PIN, LOW);
    Serial.println(F("Powering RF module OFF"));

    Serial.print(F("Sleeping for "));
    Serial.print(SLEEP_SECONDS);
    Serial.println(F(" seconds"));
    Serial.flush();
    delay(100);

    ESP.deepSleep(SLEEP_SECONDS * 1000000ULL, WAKE_RF_DISABLED);
}

// ────────────────────────────────────────────────
// SENSOR FUNCTIONS
// ────────────────────────────────────────────────

uint8_t readTankLevel()
{
    // Power ON the float switches
    Serial.println(F("Powering float switches ON"));
    digitalWrite(POWER_PIN, HIGH);
    delay(50);  // Wait for switches to stabilize
    
    // Read the switches
    bool top = (digitalRead(FLOAT_TOP_PIN) == LOW);
    bool bottom = (digitalRead(FLOAT_BOTTOM_PIN) == LOW);
    
    Serial.print(F("Float sensors - Top: "));
    Serial.print(top ? "WATER" : "AIR");
    Serial.print(F(", Bottom: "));
    Serial.println(bottom ? "WATER" : "AIR");
    
    // Power OFF the float switches
    digitalWrite(POWER_PIN, LOW);
    Serial.println(F("Powering float switches OFF"));
    
    // Calculate level
    uint8_t level;
    if (top && bottom)       level = 0;  // Neither = empty
    // else if (!top && bottom) level = 50;   // Top only (shouldn't happen)
    else if (!top && !bottom) level = 100;   // Both in water = full
    else                     level = 25;   // Only bottom = half
    
    return level;
}

uint16_t readBatteryVoltage_mV()
{
    const float R1 = 220000.0;
    const float R2 = 100000.0;
    const float factor = (R1 + R2) / R2;

    uint16_t raw = analogRead(A0);
    float v_adc = raw * (1.0 / 1023.0);
    return (uint16_t)(v_adc * factor * 1000.0 + 0.5);
}

// ────────────────────────────────────────────────
// LED HELPER
// ────────────────────────────────────────────────

void blink(uint8_t times, uint16_t ms)
{
    for (uint8_t i = 0; i < times; i++) {
        digitalWrite(STATUS_LED_PIN, LOW);
        delay(ms);
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(ms);
    }
}

/*
```

## Key Changes:

1. **Renamed to `POWER_PIN`** - Now controls both float switches AND 433MHz module
2. **RF driver init moved to loop()** - Initialized only when power is applied
3. **Sequential power control**:
   - Power ON → Read sensors → Power OFF
   - Power ON → Init RF → Send → Power OFF
4. **100ms stabilization** for RF module (they need time to power up)

## Hardware Wiring:
```
ESP8266 D1 (GPIO5)
      │
      ├──────────► Float Switch VCC
      │
      └──────────► 433MHz TX Module VCC
      
All GND connected together
```

## Important: Current Considerations

**Check your D1 pin current capacity:**

- ESP8266 GPIO can source ~12mA max
- Float switches: ~1-5mA each = 2-10mA total
- 433MHz TX module: ~20-40mA (during transmission)
- **TOTAL: 22-50mA** ⚠️

### Solution: Use a Transistor

Since the total current likely exceeds 12mA, you **MUST** use a transistor:
```
                       3.3V/Vin
                          │
                          ├───┐
                          │   │
                     Float│   │433MHz TX
                  Switches│   │Module VCC
                      VCC │   │
                          │   │
                       Collector
                          │
                     (NPN Transistor)
                      2N2222 / BC547
                          │
                   Base   │
              1kΩ ────────┤
              │           │
    ESP D1 ───┘        Emitter
                          │
                         GND
```

### Component Values:
- **Transistor**: 2N2222, BC547, or 2N3904
- **Base Resistor**: 1kΩ
- **Power**: Can handle 100mA+ easily

### Wiring with Transistor:
```
ESP8266
┌─────────┐
│         │
│  D1 ────┼──── 1kΩ ───► Base (Transistor)
│  (GPIO5)│
│         │              Collector ──► 3.3V
│  GND ───┼─────────────► Emitter ──► GND
│         │
│         │              Float VCC ◄── Collector
│         │              433MHz VCC ◄─ Collector
└─────────┘


*/