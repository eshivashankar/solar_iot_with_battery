// ================================================================
//  ESP8266 + 433 MHz ASK Transmitter - VERY LOW POWER VERSION
//  with Power-Controlled Float Switches
// ================================================================

#include <RH_ASK.h>
#include <SPI.h>
#include <ESP8266WiFi.h>

// ────────────────────────────────────────────────
// CONFIGURATION
// ────────────────────────────────────────────────

#define RF_TX_PIN         D2       // GPIO4
#define STATUS_LED_PIN    LED_BUILTIN

#define FLOAT_POWER_PIN   D1       // GPIO5 - Powers the float switches
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
    Serial.begin(74880);
    delay(50);
    Serial.println();
    Serial.println(F("=== Tank TX boot ==="));

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);

    // Power control for float switches
    pinMode(FLOAT_POWER_PIN, OUTPUT);
    digitalWrite(FLOAT_POWER_PIN, LOW);  // OFF initially

    pinMode(FLOAT_TOP_PIN, INPUT_PULLUP);
    pinMode(FLOAT_BOTTOM_PIN, INPUT_PULLUP);

    blink(3, 80);

    Serial.print(F("Init RF driver... "));
    if (!driver.init()) {
        Serial.println(F("FAILED"));
        while (true) {
            blink(1, 100);
            delay(400);
        }
    }
    Serial.println(F("OK"));

    Serial.println(F("Disabling WiFi"));
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(1);
}

// ────────────────────────────────────────────────
// LOOP (runs once per wake)
// ────────────────────────────────────────────────

void loop()
{
    TankMessage msg;

    msg.deviceId     = DEVICE_ID;
    msg.levelPercent = readTankLevel();
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

    digitalWrite(STATUS_LED_PIN, LOW);
    bool ok = driver.send((uint8_t*)&msg, sizeof(msg));
    driver.waitPacketSent();
    digitalWrite(STATUS_LED_PIN, HIGH);

    Serial.print(F("RF send: "));
    Serial.println(ok ? F("OK") : F("FAIL"));

    if (ok) blink(2, 40);
    else    blink(5, 60);

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
    digitalWrite(FLOAT_POWER_PIN, HIGH);
    delay(50);  // Wait for switches to stabilize
    
    // Read the switches
    bool top = (digitalRead(FLOAT_TOP_PIN) == LOW);
    bool bottom = (digitalRead(FLOAT_BOTTOM_PIN) == LOW);
    
    Serial.print(F("Float sensors - Top: "));
    Serial.print(top ? "WATER" : "AIR");
    Serial.print(F(", Bottom: "));
    Serial.println(bottom ? "WATER" : "AIR");
    
    // Power OFF the float switches immediately after reading
    digitalWrite(FLOAT_POWER_PIN, LOW);
    Serial.println(F("Powering float switches OFF"));
    
    // Calculate level
    uint8_t level;
    if (top && bottom)       level = 100;  // Both in water = full
    else if (!top && bottom) level = 50;   // Only bottom = half
    else if (!top && !bottom) level = 0;   // Neither = empty
    else                     level = 25;   // Top only (shouldn't happen)
    
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

## Hardware Wiring:
```
ESP8266                Float Switches
┌─────────┐           ┌──────────────┐
│         │           │              │
│  D1 ────┼──────────►│ VCC (3.3V)   │
│  (GPIO5)│           │              │
│         │           │ Top Switch───┼──► D5
│  D5 ────┼───────────┤              │
│         │           │ Bottom───────┼──► D6
│  D6 ────┼───────────┤              │
│         │           │              │
│  GND ───┼──────────►│ GND          │
│         │           │              │
└─────────┘           └──────────────┘
```

## Important Notes:

1. **GPIO Current Limit**: ESP8266 GPIO pins can source ~12mA max
   - If your float switches draw more current, use a **transistor** (2N2222 or 2N7000 MOSFET)

2. **If you need more current**, use this circuit:
```
           ESP8266 D1
               │
               ├──── 1kΩ ────┤ Base
               │              │
              GND         (NPN Transistor)
                              │ Collector
                              ├──────► Float Switch VCC
                              │
                         Emitter
                              │
                           3.3V/Vin
```

3. **Stabilization delay**: The `delay(50)` gives time for the switches to stabilize after power-on. You can reduce this to 10-20ms if needed.

4. **Power Savings**: By powering switches only during reading (~50-100ms every 180 seconds), you save huge amounts of battery compared to continuous power.

## Alternative: Using a P-Channel MOSFET (Best for Low Power)

If you want the absolute best power efficiency:
```
                    3.3V
                     │
                     ├──── 10kΩ ────┐
                     │               │
                     │            Gate│
ESP8266 D1 ─────────┼──────────────►│ P-MOSFET
      (LOW=ON)      │               │ (IRF9540)
                    GND           Source
                                     │
                              Float Switch VCC
                                     
                                  Drain to GND

                                  

                                  */