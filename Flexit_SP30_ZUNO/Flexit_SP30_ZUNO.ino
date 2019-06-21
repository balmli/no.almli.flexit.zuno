
#include "ZUNO_DS18B20.h"

#define DEBUG_2

#define RELAY_1_PIN         9
#define RELAY_2_PIN         10
#define DS18B20_BUS_PIN     11
#define FAN_LEVEL_1_PIN     A1
#define FAN_LEVEL_2_PIN     A2
#define HEATING_PIN         A3
#define LED_PIN             LED_BUILTIN

#define RELAY_IN_1_PIN          18
#define RELAY_IN_2_PIN          17
#define FAN_LEVEL_OUT_1_PIN     PWM2
#define FAN_LEVEL_OUT_2_PIN     PWM3
#define HEATING_OUT_PIN         PWM4

#define MIN_UPDATE_DURATION     30000
#define LED_BLINK_DURATION      25

OneWire ow(DS18B20_BUS_PIN);
DS18B20Sensor ds18b20(&ow);

#define TEMP_SENSORS 4
#define ADDR_SIZE 8
byte addresses[ADDR_SIZE * TEMP_SENSORS];
#define ADDR(i) (&addresses[i * ADDR_SIZE])
byte number_of_sensors;
word temperature[TEMP_SENSORS];
word temperatureCalibration[TEMP_SENSORS];

byte switchValue = 1;
byte lastFanLevel = 1;
byte lastFanLevelReported = 1;
byte lastHeating = 0;
byte lastHeatingReported = 0;

byte sp30FanLevel = 1;
byte sp30Heating = 0;
byte prevSP30r1 = LOW;
byte prevSP30r2 = LOW;

unsigned long fanRelayOnTimer = 0;
unsigned long heatingRelayOnTimer = 0;
unsigned long relayTimer1 = 0;
unsigned long relayTimer2 = 0;
unsigned long fanLevelTimer = 0;
unsigned long fanLevelTimer2 = 0;
unsigned long heatingTimer = 0;
unsigned long heatingTimer2 = 0;
unsigned long tempTimer = 0;
unsigned long ledTimer = 0;
unsigned long listConfigTimer = 0;

word status_report_interval_config;
word temperature_report_interval_config;
word temperature_report_threshold_config;
word relay_duration_config;
word enabled_config;
word default_config_set_config;

ZUNO_SETUP_SLEEPING_MODE(ZUNO_SLEEPING_MODE_ALWAYS_AWAKE);
ZUNO_SETUP_PRODUCT_ID(0xC0, 0xC0);
ZUNO_SETUP_CFGPARAMETER_HANDLER(config_parameter_changed);

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

    pinMode(RELAY_IN_1_PIN, INPUT);
    pinMode(RELAY_IN_2_PIN, INPUT);
    pinMode(FAN_LEVEL_OUT_1_PIN, OUTPUT);
    pinMode(FAN_LEVEL_OUT_2_PIN, OUTPUT);
    pinMode(HEATING_OUT_PIN, OUTPUT);

    fetchConfig();
    number_of_sensors = ds18b20.findAllSensors(addresses);
}

void fetchConfig() {
    status_report_interval_config = zunoLoadCFGParam(64);
    temperature_report_interval_config = zunoLoadCFGParam(65);
    temperature_report_threshold_config = zunoLoadCFGParam(66);
    relay_duration_config = zunoLoadCFGParam(67);
    enabled_config = zunoLoadCFGParam(68);
    for (byte i = 0; i < TEMP_SENSORS; i++) {
        temperatureCalibration[i] = zunoLoadCFGParam(70 + i);
    }

    default_config_set_config = zunoLoadCFGParam(96);
    if (default_config_set_config != 1) {
        status_report_interval_config = 30;
        zunoSaveCFGParam(64, status_report_interval_config);

        temperature_report_interval_config = 60;
        zunoSaveCFGParam(65, temperature_report_interval_config);

        temperature_report_threshold_config = 2;
        zunoSaveCFGParam(66, temperature_report_threshold_config);

        relay_duration_config = 500;
        zunoSaveCFGParam(67, relay_duration_config);

        enabled_config = 0;
        zunoSaveCFGParam(68, enabled_config);

        for (byte i = 0; i < TEMP_SENSORS; i++) {
            temperatureCalibration[i] = 0;
            zunoSaveCFGParam(70 + i, temperatureCalibration[i]);
        }

        default_config_set_config = 1;
        zunoSaveCFGParam(96, default_config_set_config);
    }
}

void loop() {
    if (enabled_config == 1) {
        unsigned long timerNow = millis();
        handleSwitch(timerNow);
        checkFanLevel(timerNow);
        checkHeating(timerNow);
        checkTemperatures(timerNow);
        ledBlinkOff(timerNow);
        listConfig(timerNow);
    }
}

void config_parameter_changed(byte param, word value) {
#ifdef DEBUG
    Serial.print("config ");
    Serial.print(param);
    Serial.print(" = ");
    Serial.println(value);
#endif

    if (param == 64) {
        status_report_interval_config = value;
    }
    if (param == 65) {
        temperature_report_interval_config = value;
    }
    if (param == 66) {
        temperature_report_threshold_config = value;
    }
    if (param == 67) {
        relay_duration_config = value;
    }
    if (param == 68) {
        enabled_config = value;
        if (enabled_config < 0 || enabled_config > 1) {
            enabled_config = 0;
        }
    }
    if (param >= 70 && param >= 73) {
        temperatureCalibration[param - 70] = value;
    }
}

void listConfig(unsigned long timerNow) {
#ifdef DEBUG
    if (timerNow - listConfigTimer > MIN_UPDATE_DURATION) {
        listConfigTimer = timerNow;
        Serial.print("status_report_interval_config: ");
        Serial.println(status_report_interval_config);
        Serial.print("temperature_report_interval_config: ");
        Serial.println(temperature_report_interval_config);
        Serial.print("temperature_report_threshold_config: ");
        Serial.println(temperature_report_threshold_config);
        Serial.print("relay_duration_config: ");
        Serial.println(relay_duration_config);
        Serial.print("enabled_config: ");
        Serial.println(enabled_config);
        for (byte i = 0; i < TEMP_SENSORS; i++) {
            Serial.print("temperatureCalibration[");
            Serial.print(i);
            Serial.print("]: ");
            Serial.println(temperatureCalibration[i]);
        }
    }
#endif
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

void simulateSP30() {
    delay(5);

    // Fan level
    byte r1 = digitalRead(RELAY_IN_1_PIN);
    if (prevSP30r1 == LOW && r1 == HIGH) {
        sp30FanLevel = sp30FanLevel + 1;
        if (sp30FanLevel > 3) {
            sp30FanLevel = 1;
        }
#ifdef DEBUG
        Serial.print("sp30FanLevel: ");
        Serial.println(sp30FanLevel);
#endif
    }
    if (r1 != prevSP30r1) {
        prevSP30r1 = r1;
        digitalWrite(FAN_LEVEL_OUT_1_PIN, sp30FanLevel == 2 ? HIGH : LOW);
        digitalWrite(FAN_LEVEL_OUT_2_PIN, sp30FanLevel == 3 ? HIGH : LOW);
    }

    // Heating
    byte r2 = digitalRead(RELAY_IN_2_PIN);
    if (prevSP30r2 == LOW && r2 == HIGH) {
        sp30Heating = 1 - sp30Heating;
#ifdef DEBUG
        Serial.print("sp30Heating: ");
        Serial.println(sp30Heating);
#endif
    }
    if (r2 != prevSP30r2) {
        prevSP30r2 = r2;
        digitalWrite(HEATING_OUT_PIN, sp30Heating == 1 ? HIGH : LOW);
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
    if (timerNow - fanRelayOnTimer > (2 * relay_duration_config)) {
        fanRelayOnTimer = timerNow;
        if (relayTimer1 == 0) {
            relayTimer1 = timerNow;
            ledBlink(timerNow);
            digitalWrite(RELAY_1_PIN, HIGH);
            simulateSP30();
#ifdef DEBUG
            Serial.print("r1 on ");
            Serial.println(timerNow);
#endif
        }
    }
}

void fanRelayOff(unsigned long timerNow) {
    if (relayTimer1 > 0 && (timerNow - relayTimer1 > relay_duration_config)) {
        digitalWrite(RELAY_1_PIN, LOW);
        simulateSP30();
        ledBlink(timerNow);
#ifdef DEBUG
        Serial.print("r1 off ");
        Serial.println(timerNow);
#endif
        relayTimer1 = 0;
    }
}

void heatingRelayOn(unsigned long timerNow) {
    if (timerNow - heatingRelayOnTimer > (2 * relay_duration_config)) {
        heatingRelayOnTimer = timerNow;
        if (relayTimer2 == 0) {
            relayTimer2 = timerNow;
            ledBlink(timerNow);
            digitalWrite(RELAY_2_PIN, HIGH);
            simulateSP30();
#ifdef DEBUG
            Serial.print("r2 on ");
            Serial.println(timerNow);
#endif
        }
    }
}

void heatingRelayOff(unsigned long timerNow) {
    if (relayTimer2 > 0 && (timerNow - relayTimer2 > relay_duration_config)) {
        digitalWrite(RELAY_2_PIN, LOW);
        simulateSP30();
        ledBlink(timerNow);
#ifdef DEBUG
        Serial.print("r2 off ");
        Serial.println(timerNow);
#endif
        relayTimer2 = 0;
    }
}

void checkFanLevel(unsigned long timerNow) {
    int fl1 = analogRead(FAN_LEVEL_1_PIN);
    int fl2 = analogRead(FAN_LEVEL_2_PIN);
    lastFanLevel = fl1 < 400 && fl2 > 400 ? 3 : (fl1 > 400 && fl2 < 400 ? 2 : 1);
    if ((lastFanLevel != lastFanLevelReported && ((timerNow - fanLevelTimer) > MIN_UPDATE_DURATION)) ||
        ((timerNow - fanLevelTimer) > status_report_interval_config * 1000)) {
        lastFanLevelReported = lastFanLevel;
        fanLevelTimer = timerNow;
        ledBlink(timerNow);
        zunoSendReport(2);
    }
#ifdef DEBUG_2
    if (timerNow - fanLevelTimer2 > 2000) {
        fanLevelTimer2 = timerNow;
        Serial.print("fl1: ");
        Serial.print(fl1);
        Serial.print(", fl2: ");
        Serial.print(fl2);
        Serial.print(", fan level: ");
        Serial.print(lastFanLevel);
        Serial.print(", switchValue: ");
        Serial.println(switchValue);
    }
#endif
}

void checkHeating(unsigned long timerNow) {
    int heat = analogRead(HEATING_PIN);
    lastHeating = heat > 400 ? 10 : 0;
    if ((lastHeating != lastHeatingReported && ((timerNow - heatingTimer) > MIN_UPDATE_DURATION)) ||
        ((timerNow - heatingTimer) > status_report_interval_config * 1000)) {
        lastHeatingReported = lastHeating;
        heatingTimer = timerNow;
        ledBlink(timerNow);
        zunoSendReport(3);
    }
#ifdef DEBUG_2
    if (timerNow - heatingTimer2 > 2000) {
        heatingTimer2 = timerNow;
        Serial.print("heat: ");
        Serial.print(heat);
        Serial.print(", heating: ");
        Serial.print(lastHeating);
        Serial.print(", switchValue: ");
        Serial.println(switchValue);
    }
#endif
}

void checkTemperatures(unsigned long timerNow) {
    if (timerNow - tempTimer > MIN_UPDATE_DURATION) {
        tempTimer = timerNow;
        for (byte i = 0; i < TEMP_SENSORS; i++) {
            temperature[i] = 1600 + i * 200 + (timerNow % 100) + temperatureCalibration[i];
            ledBlink(timerNow);
            zunoSendReport(i + 4);
#ifdef DEBUG
            Serial.print("temp[");
            Serial.print(i);
            Serial.print("]: ");
            Serial.println(temperature[i]);
#endif
        }
    }
}

void setterSwitch(byte value) {
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

