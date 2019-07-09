
#include "ZUNO_DS18B20.h"

#define DEBUG_3

#define RELAY_1_PIN         9
#define RELAY_2_PIN         10
#define DS18B20_BUS_PIN     11
#define FAN_LEVEL_1_PIN     A1
#define FAN_LEVEL_2_PIN     A2
#define HEATING_PIN         A3
#define LED_PIN             LED_BUILTIN

#define MIN_UPDATE_DURATION         30000
#define MIN_STATE_UPDATE_DURATION   1000
#define MIN_TEMP_UPDATE_DURATION    30000
#define LED_BLINK_DURATION          25

OneWire ow(DS18B20_BUS_PIN);
DS18B20Sensor ds18b20(&ow);

#define MAX_TEMP_SENSORS 4
#define ADDR_SIZE 8
byte addresses[ADDR_SIZE * MAX_TEMP_SENSORS];
#define ADDR(i) (&addresses[i * ADDR_SIZE])
byte temp_sensors;
word temperature[MAX_TEMP_SENSORS];
word temperatureReported[MAX_TEMP_SENSORS];
word temperatureCalibration[MAX_TEMP_SENSORS];

unsigned long lastSetSwitchValue = 0;
byte switchValue = 1;
byte lastFanLevel = 1;
byte lastFanLevelReported = 1;
byte lastHeating = 0;
byte lastHeatingReported = 0;

unsigned long fanRelayOnTimer = 0;
unsigned long heatingRelayOnTimer = 0;
unsigned long relayTimer1 = 0;
unsigned long relayTimer2 = 0;
unsigned long fanLevelTimer = 0;
unsigned long fanLevelTimer2 = 0;
unsigned long heatingTimer = 0;
unsigned long heatingTimer2 = 0;
unsigned long temperatureTimer[MAX_TEMP_SENSORS];
unsigned long ledTimer = 0;
unsigned long listConfigTimer = 0;

word status_report_interval_config;
word temperature_report_interval_config;
word temperature_report_threshold_config;
word relay_duration_config;
word enabled_config;
word enabled_switch_config;
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

    pinMode(FAN_LEVEL_1_PIN, INPUT_PULLUP);
    pinMode(FAN_LEVEL_2_PIN, INPUT_PULLUP);
    pinMode(HEATING_PIN, INPUT_PULLUP);

    fetchConfig();
    temp_sensors = ds18b20.findAllSensors(addresses);
}

void fetchConfig() {
    status_report_interval_config = zunoLoadCFGParam(64);
    temperature_report_interval_config = zunoLoadCFGParam(65);
    temperature_report_threshold_config = zunoLoadCFGParam(66);
    relay_duration_config = zunoLoadCFGParam(67);
    enabled_config = zunoLoadCFGParam(68);
    enabled_switch_config = zunoLoadCFGParam(69);
    for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
        temperatureCalibration[i] = zunoLoadCFGParam(70 + i);
    }

    default_config_set_config = zunoLoadCFGParam(96);
    if (default_config_set_config != 1) {
        status_report_interval_config = 30;
        zunoSaveCFGParam(64, status_report_interval_config);

        temperature_report_interval_config = 300;
        zunoSaveCFGParam(65, temperature_report_interval_config);

        temperature_report_threshold_config = 2;
        zunoSaveCFGParam(66, temperature_report_threshold_config);

        relay_duration_config = 500;
        zunoSaveCFGParam(67, relay_duration_config);

        enabled_config = 0;
        zunoSaveCFGParam(68, enabled_config);

        enabled_switch_config = 0;
        zunoSaveCFGParam(69, enabled_switch_config);

        for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
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
        if (enabled_switch_config == 1) {
            handleSwitch(timerNow);
        }
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
    if (param == 69) {
        enabled_switch_config = value;
        if (enabled_switch_config < 0 || enabled_switch_config > 1) {
            enabled_switch_config = 0;
        }
    }
    if (param >= 70 && param <= 73) {
        temperatureCalibration[param - 70] = value;
#ifdef DEBUG
    Serial.print("config ");
    Serial.print("temperatureCalibration[");
    Serial.print(param - 70);
    Serial.print("] = ");
    Serial.println(temperatureCalibration[param - 70]);
#endif
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
        Serial.print("enabled_switch_config: ");
        Serial.println(enabled_switch_config);
        Serial.print("temp sensors: ");
        Serial.println(temp_sensors);
        for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
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
#ifdef DEBUG_2
            Serial.print("r1 on ");
            Serial.println(timerNow);
#endif
        }
    }
}

void fanRelayOff(unsigned long timerNow) {
    if (relayTimer1 > 0 && (timerNow - relayTimer1 > relay_duration_config)) {
        digitalWrite(RELAY_1_PIN, LOW);
        ledBlink(timerNow);
#ifdef DEBUG_2
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
#ifdef DEBUG_2
            Serial.print("r2 on ");
            Serial.println(timerNow);
#endif
        }
    }
}

void heatingRelayOff(unsigned long timerNow) {
    if (relayTimer2 > 0 && (timerNow - relayTimer2 > relay_duration_config)) {
        digitalWrite(RELAY_2_PIN, LOW);
        ledBlink(timerNow);
#ifdef DEBUG_2
        Serial.print("r2 off ");
        Serial.println(timerNow);
#endif
        relayTimer2 = 0;
    }
}

void checkFanLevel(unsigned long timerNow) {
    int fl1 = digitalRead(FAN_LEVEL_1_PIN);
    int fl2 = digitalRead(FAN_LEVEL_2_PIN);
    lastFanLevel = fl1 == 1 && fl2 == 0 ? 3 : (fl1 == 0 && fl2 == 1 ? 2 : 1);

    bool changed = lastFanLevel != lastFanLevelReported;
    unsigned long timer1Diff = timerNow - lastSetSwitchValue;
    unsigned long timer2Diff = timerNow - fanLevelTimer;
    unsigned long repInterval = ((unsigned long)status_report_interval_config) * 1000;

    if (changed && timer1Diff > MIN_STATE_UPDATE_DURATION || timer2Diff > repInterval) {
        lastFanLevelReported = lastFanLevel;
        fanLevelTimer = timerNow;
        ledBlink(timerNow);
        zunoSendReport(2);
#ifdef DEBUG_3
        Serial.print("lastFanLevel: ");
        Serial.println(lastFanLevel);
#endif
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
    int heat = digitalRead(HEATING_PIN);
    lastHeating = heat == 0 ? 10 : 0;

    bool changed = lastHeating != lastHeatingReported;
    unsigned long timer1Diff = timerNow - lastSetSwitchValue;
    unsigned long timer2Diff = timerNow - heatingTimer;
    unsigned long repInterval = ((unsigned long)status_report_interval_config) * 1000;

    if (changed && timer1Diff > MIN_STATE_UPDATE_DURATION || timer2Diff > repInterval) {
        lastHeatingReported = lastHeating;
        heatingTimer = timerNow;
        ledBlink(timerNow);
        zunoSendReport(3);
#ifdef DEBUG_3
        Serial.print("lastHeating: ");
        Serial.println(lastHeating);
#endif
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
    for (byte i = 0; i < temp_sensors && i < MAX_TEMP_SENSORS; i++) {

        unsigned long timerDiff = timerNow - temperatureTimer[i];
        unsigned long repInterval = ((unsigned long)temperature_report_interval_config) * 1000;

        if (timerDiff > MIN_TEMP_UPDATE_DURATION || timerDiff > repInterval) {

            temperature[i] = (ds18b20.getTemperature(ADDR(i)) * 100) + temperatureCalibration[i];
            bool tempChanged = temperature[i] > temperatureReported[i] + temperature_report_threshold_config || temperature[i] < temperatureReported[i] - temperature_report_threshold_config;

            if (tempChanged && timerDiff > MIN_TEMP_UPDATE_DURATION || timerDiff > repInterval) {
                temperatureTimer[i] = timerNow;
                temperatureReported[i] = temperature[i];
                ledBlink(timerNow);
                zunoSendReport(i + 4);
#ifdef DEBUG_3
                Serial.print("temp[");
                Serial.print(i);
                Serial.print("]: ");
                Serial.println(temperature[i]);
#endif
            }
        }
    }
}

void setterSwitch(byte value) {
    switchValue = value;
    lastSetSwitchValue = millis();
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

