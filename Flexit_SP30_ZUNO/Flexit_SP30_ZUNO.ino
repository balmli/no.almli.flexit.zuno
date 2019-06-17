
#include "ZUNO_DS18B20.h"

#define RELAY_1_PIN         9
#define RELAY_2_PIN         10
#define DS18B20_BUS_PIN     11
#define FAN_LEVEL_1_PIN     A1
#define FAN_LEVEL_2_PIN     A2
#define HEATING_PIN         A3
#define LED_PIN             LED_BUILTIN

#define MIN_SWITCH_DURATION 1000
#define MIN_UPDATE_DURATION 30000
#define RELAY_DURATION      500
#define LED_BLINK_DURATION  25

OneWire ow(DS18B20_BUS_PIN);
DS18B20Sensor ds18b20(&ow);

#define TEMP_SENSORS 4
#define ADDR_SIZE 8
byte addresses[ADDR_SIZE * TEMP_SENSORS];
#define ADDR(i) (&addresses[i * ADDR_SIZE])
byte number_of_sensors;
word temperature[TEMP_SENSORS];

byte switchValue = 0;
byte lastFanLevel = 0;
byte lastHeating = 0;

unsigned long lastChangedSwitch = 0;
unsigned long fanRelayOnTimer = 0;
unsigned long heatingRelayOnTimer = 0;
unsigned long relayTimer1 = 0;
unsigned long relayTimer2 = 0;
unsigned long fanLevelTimer = 0;
unsigned long heatingTimer = 10000;
unsigned long tempTimer = 20000;
unsigned long ledTimer = 0;

ZUNO_SETUP_SLEEPING_MODE(ZUNO_SLEEPING_MODE_ALWAYS_AWAKE);

ZUNO_SETUP_CHANNELS(
    ZUNO_SWITCH_MULTILEVEL(getterSwitch, setterSwitch),

    ZUNO_SENSOR_MULTILEVEL(ZUNO_SENSOR_MULTILEVEL_TYPE_GENERAL_PURPOSE_VALUE,
        SENSOR_MULTILEVEL_SCALE_PERCENTAGE_VALUE,
        SENSOR_MULTILEVEL_SIZE_ONE_BYTE,
        SENSOR_MULTILEVEL_PRECISION_ZERO_DECIMALS,
        getterFanLevel),

    ZUNO_SENSOR_MULTILEVEL(ZUNO_SENSOR_MULTILEVEL_TYPE_GENERAL_PURPOSE_VALUE,
        SENSOR_MULTILEVEL_SCALE_PERCENTAGE_VALUE,
        SENSOR_MULTILEVEL_SIZE_ONE_BYTE,
        SENSOR_MULTILEVEL_PRECISION_ZERO_DECIMALS,
        getterHeating),

    ZUNO_SENSOR_MULTILEVEL(ZUNO_SENSOR_MULTILEVEL_TYPE_TEMPERATURE,
        SENSOR_MULTILEVEL_SCALE_CELSIUS,
        SENSOR_MULTILEVEL_SIZE_TWO_BYTES,
        SENSOR_MULTILEVEL_PRECISION_TWO_DECIMALS,
        getterTemp1),

    ZUNO_SENSOR_MULTILEVEL(ZUNO_SENSOR_MULTILEVEL_TYPE_TEMPERATURE,
        SENSOR_MULTILEVEL_SCALE_CELSIUS,
        SENSOR_MULTILEVEL_SIZE_TWO_BYTES,
        SENSOR_MULTILEVEL_PRECISION_TWO_DECIMALS,
        getterTemp2),

    ZUNO_SENSOR_MULTILEVEL(ZUNO_SENSOR_MULTILEVEL_TYPE_TEMPERATURE,
        SENSOR_MULTILEVEL_SCALE_CELSIUS,
        SENSOR_MULTILEVEL_SIZE_TWO_BYTES,
        SENSOR_MULTILEVEL_PRECISION_TWO_DECIMALS,
        getterTemp3),

    ZUNO_SENSOR_MULTILEVEL(ZUNO_SENSOR_MULTILEVEL_TYPE_TEMPERATURE,
        SENSOR_MULTILEVEL_SCALE_CELSIUS,
        SENSOR_MULTILEVEL_SIZE_TWO_BYTES,
        SENSOR_MULTILEVEL_PRECISION_TWO_DECIMALS,
        getterTemp4)
);

void setup() {
    Serial.begin();
    pinMode(RELAY_1_PIN, OUTPUT);
    pinMode(RELAY_2_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    number_of_sensors = ds18b20.findAllSensors(addresses);
}

void loop() {
    unsigned long timerNow = millis();
    handleSwitch(timerNow);
    checkFanLevel(timerNow);
    checkHeating(timerNow);
    checkTemperatures(timerNow);
    ledBlinkOff(timerNow);
}

void ledBlink(unsigned long timerNow) {
    ledTimer = timerNow;
    digitalWrite(LED_PIN, HIGH);
}

void ledBlinkOff(unsigned long timerNow) {
    if (ledTimer > 0 && (timerNow - ledTimer > LED_BLINK_DURATION)) {
        ledTimer = 0;
        digitalWrite(LED_PIN, LOW);
    }
}

void handleSwitch(unsigned long timerNow) {
    if ((switchValue % 10) != lastFanLevel) {
        fanRelayOn(timerNow);
    }
    fanRelayOff(timerNow);

    if ((switchValue - switchValue % 10) != lastHeating) {
        heatingRelayOn(timerNow);
    }
    heatingRelayOff(timerNow);
}

void fanRelayOn(unsigned long timerNow) {
    if (timerNow - fanRelayOnTimer > MIN_SWITCH_DURATION) {
        fanRelayOnTimer = timerNow;
        if (relayTimer1 == 0) {
            relayTimer1 = timerNow;
            ledBlink(timerNow);
            digitalWrite(RELAY_1_PIN, HIGH);
            Serial.print("r1 on ");
            Serial.println(timerNow);

            // Temp fix
            lastFanLevel = switchValue % 10;
        }
    }
}

void fanRelayOff(unsigned long timerNow) {
    if (relayTimer1 > 0 && (timerNow - relayTimer1 > RELAY_DURATION)) {
        digitalWrite(RELAY_1_PIN, LOW);
        ledBlink(timerNow);
        Serial.print("r1 off ");
        Serial.println(timerNow);
        relayTimer1 = 0;
    }
}

void heatingRelayOn(unsigned long timerNow) {
    if (timerNow - heatingRelayOnTimer > MIN_SWITCH_DURATION) {
        heatingRelayOnTimer = timerNow;
        if (relayTimer2 == 0) {
            relayTimer2 = timerNow;
            ledBlink(timerNow);
            digitalWrite(RELAY_2_PIN, HIGH);
            Serial.print("r2 on ");
            Serial.println(timerNow);

            // Temp fix
            lastHeating = switchValue - switchValue % 10;
        }
    }
}

void heatingRelayOff(unsigned long timerNow) {
    if (relayTimer2 > 0 && (timerNow - relayTimer2 > RELAY_DURATION)) {
        digitalWrite(RELAY_2_PIN, LOW);
        ledBlink(timerNow);
        Serial.print("r2 off ");
        Serial.println(timerNow);
        relayTimer2 = 0;
    }
}

void checkFanLevel(unsigned long timerNow) {
    byte currentFanLevel = 0;
    /*
    byte fl1 = (byte)(analogRead(FAN_LEVEL_1_PIN)*25/256);
    byte fl2 = (byte)(analogRead(FAN_LEVEL_2_PIN)*25/256);
    if (fl1 > 20 && fl2 < 20) {
        currentFanLevel = 10;
    } else if (fl1 < 20 && fl2 > 20) {
        currentFanLevel = 20;
    }
    */
    //currentFanLevel = switchValue % 10;
    //if (currentFanLevel != lastFanLevel) {
        //lastFanLevel = currentFanLevel;
        if (timerNow - fanLevelTimer > MIN_UPDATE_DURATION) {
            fanLevelTimer = timerNow;
            ledBlink(timerNow);
            zunoSendReport(2);
            Serial.print("fan level ");
            Serial.println(lastFanLevel);
        }
    //}
}

void checkHeating(unsigned long timerNow) {
    byte currentHeating = 0;
    /*
    byte heat = (byte)(analogRead(HEATING_PIN)*25/256);
    if (heat > 20) {
        currentHeating = 10;
    }
    */
    //currentHeating = switchValue - switchValue % 10;
    //if (currentHeating != lastHeating) {
        //lastHeating = currentHeating;
        if (timerNow - heatingTimer > MIN_UPDATE_DURATION) {
            heatingTimer = timerNow;
            ledBlink(timerNow);
            zunoSendReport(3);
            Serial.print("heating ");
            Serial.println(lastHeating);
        }
    //}
}

void checkTemperatures(unsigned long timerNow) {
    if (timerNow - tempTimer > MIN_UPDATE_DURATION) {
        tempTimer = timerNow;
        for (byte i = 0; i < TEMP_SENSORS; i++) {
            temperature[i] = 1600 + i * 200 + (timerNow % 100);
            ledBlink(timerNow);
            zunoSendReport(i + 4);
            Serial.print("temp ");
            Serial.print(i);
            Serial.print(" ");
            Serial.println(temperature[i]);
        }
    }
}

void setterSwitch(byte value) {
    lastChangedSwitch = millis();
    switchValue = value;
}

byte getterSwitch(void) {
    return switchValue;
}

byte getterFanLevel(void) {
  return lastFanLevel;
}

byte getterHeating(void) {
  return lastHeating;
}

word getterTemp1() {
    return temperature[0];
}

word getterTemp2() {
    return temperature[1];
}

word getterTemp3() {
    return temperature[2];
}

word getterTemp4() {
    return temperature[3];
}
