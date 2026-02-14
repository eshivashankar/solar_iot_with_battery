// ================================================================
//  Arduino + 433 MHz ASK Receiver
//  Compatible with ESP8266 TX TankMessage
// ================================================================

#include <RH_ASK.h>
#include <SPI.h>   // Required by RH_ASK

// ────────────────────────────────────────────────
// CONFIGURATION
// ────────────────────────────────────────────────

#define RF_RX_PIN 11      // DATA pin of 433 MHz receiver
#define motor_pin 13
char flag=0;

// Message must EXACTLY match transmitter
struct __attribute__((packed)) TankMessage {
    uint8_t  deviceId;
    uint8_t  levelPercent;
    uint16_t battery_mV;
    uint8_t  reserved;
};

// RH_ASK(speed, rxPin, txPin)
RH_ASK driver(2000, RF_RX_PIN, -1);

// ────────────────────────────────────────────────
// SETUP
// ────────────────────────────────────────────────

void setup()
{
    Serial.begin(9600);
    Serial.println(F("433 MHz Tank Receiver starting..."));

    if (!driver.init()) {
        Serial.println(F("RH_ASK init failed"));
        while (1);
    }
    pinMode(motor_pin,OUTPUT);

    Serial.println(F("Receiver ready"));
}
void motor_on()
{
    digitalWrite(motor_pin, HIGH);
    flag=1;
}
void motor_off()
{
    digitalWrite(motor_pin, LOW);
    flag=0;
}
// ────────────────────────────────────────────────
// LOOP
// ────────────────────────────────────────────────

void loop()
{
    uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
    uint8_t buflen = sizeof(buf);

    if (driver.recv(buf, &buflen)) {
        if (buflen == sizeof(TankMessage)) {

            TankMessage msg;
            memcpy(&msg, buf, sizeof(msg));

            Serial.println(F("---- Packet Received ----"));
            Serial.print(F("Device ID: "));
            Serial.println(msg.deviceId);

            Serial.print(F("Tank Level: "));
            Serial.print(msg.levelPercent);
            Serial.println(F("%"));

            Serial.print(F("Battery: "));
            Serial.print(msg.battery_mV);
            Serial.println(F(" mV"));

            Serial.println();

                if(flag ==0)
                {
                    if (msg.levelPercent == 25)
                    {
                        motor_on();
                    }
                }
                if(flag ==1)
                {
                    if (msg.levelPercent == 100)
                    {
                        motor_off();
                    }
                }


        }
        else {
            Serial.print(F("Invalid packet size: "));
            Serial.println(buflen);
        }
    }

}


