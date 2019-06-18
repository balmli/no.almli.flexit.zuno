
#include "ZUNO_DS18B20.h"

#define RELAY_1_PIN         9
#define RELAY_2_PIN         10
#define DS18B20_BUS_PIN     11
#define FAN_LEVEL_1_PIN     A1
#define FAN_LEVEL_2_PIN     A2
#define HEATING_PIN         A3
#define LED_PIN             LED_BUILTIN

#define MIN_SWITCH_DURATION     1000
#define MIN_UPDATE_DURATION     30000
#define DEFAULT_RELAY_DURATION  500
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

byte switchValue = 0;
byte lastFanLevel = 0;
byte lastHeating = 0;

unsigned long lastChangedSwitch = 0;
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
    fetchConfig();
    number_of_sensors = ds18b20.findAllSensors(addresses);
}

void fetchConfig() {
    status_report_interval_config = zunoLoadCFGParam(64);
    if (status_report_interval_config > 32767) {
        status_report_interval_config = 30;
        zunoSaveCFGParam(64, status_report_interval_config);
    }
    temperature_report_interval_config = zunoLoadCFGParam(65);
    if (temperature_report_interval_config > 32767) {
        temperature_report_interval_config = 60;
        zunoSaveCFGParam(65, temperature_report_interval_config);
    }
    temperature_report_threshold_config = zunoLoadCFGParam(66);
    if (temperature_report_threshold_config > 100) {
        temperature_report_threshold_config = 2;
        zunoSaveCFGParam(66, temperature_report_threshold_config);
    }
    relay_duration_config = zunoLoadCFGParam(67);
    if (relay_duration_config > 5000) {
        relay_duration_config = 500;
        zunoSaveCFGParam(67, relay_duration_config);
    }
    for (byte i = 0; i < TEMP_SENSORS; i++) {
        temperatureCalibration[i] = zunoLoadCFGParam(70 + i);
        if (temperatureCalibration[i] > 400) {
            temperatureCalibration[i] = 0;
            zunoSaveCFGParam(70 + i, temperatureCalibration[i]);
        }
    }
}

void loop() {
    unsigned long timerNow = millis();
    //handleSwitch(timerNow);
    checkFanLevel(timerNow);
    checkHeating(timerNow);
    checkTemperatures(timerNow);
    ledBlinkOff(timerNow);
    listConfig(timerNow);
}

void config_parameter_changed(byte param, word value) {
    Serial.print("config ");
    Serial.print(param);
    Serial.print(" = ");
    Serial.println(value);

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
    if (param >= 70 && param >= 73) {
        temperatureCalibration[param - 70] = value;
    }
}

void listConfig(unsigned long timerNow) {
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
        for (byte i = 0; i < TEMP_SENSORS; i++) {
            Serial.print("temperatureCalibration[");
            Serial.print(i);
            Serial.print("]: ");
            Serial.println(temperatureCalibration[i]);
        }
    }
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
        }
    }
}

void fanRelayOff(unsigned long timerNow) {
    if (relayTimer1 > 0 && (timerNow - relayTimer1 > DEFAULT_RELAY_DURATION)) {
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
        }
    }
}

void heatingRelayOff(unsigned long timerNow) {
    if (relayTimer2 > 0 && (timerNow - relayTimer2 > DEFAULT_RELAY_DURATION)) {
        digitalWrite(RELAY_2_PIN, LOW);
        ledBlink(timerNow);
        Serial.print("r2 off ");
        Serial.println(timerNow);
        relayTimer2 = 0;
    }
}

void checkFanLevel(unsigned long timerNow) {
    int fl1 = analogRead(FAN_LEVEL_1_PIN);
    int fl2 = analogRead(FAN_LEVEL_2_PIN);
    byte curFanLevel = fl1 < 400 && fl2 > 400 ? 3 : (fl1 > 400 && fl2 < 400 ? 2 : 1);
    if ((curFanLevel != lastFanLevel && ((timerNow - fanLevelTimer) > MIN_UPDATE_DURATION)) ||
        ((timerNow - fanLevelTimer) > status_report_interval_config * 1000)) {
        lastFanLevel = curFanLevel;
        fanLevelTimer = timerNow;
        ledBlink(timerNow);
        zunoSendReport(2);
        Serial.print("fan level: ");
        Serial.println(lastFanLevel);
    }
    if (timerNow - fanLevelTimer2 > 2000) {
        fanLevelTimer2 = timerNow;
        Serial.print("fl1: ");
        Serial.print(fl1);
        Serial.print(", fl2: ");
        Serial.println(fl2);
    }
}

void checkHeating(unsigned long timerNow) {
    int heat = analogRead(HEATING_PIN);
    byte curHeating = heat > 400 ? 10 : 0;
    if ((curHeating != lastHeating && ((timerNow - heatingTimer) > MIN_UPDATE_DURATION)) ||
        ((timerNow - heatingTimer) > status_report_interval_config * 1000)) {
        lastHeating = curHeating;
        heatingTimer = timerNow;
        ledBlink(timerNow);
        zunoSendReport(3);
        Serial.print("heating: ");
        Serial.println(lastHeating);
    }
    if (timerNow - heatingTimer2 > 2000) {
        heatingTimer2 = timerNow;
        Serial.print("heat: ");
        Serial.println(heat);
    }
}

void checkTemperatures(unsigned long timerNow) {
    if (timerNow - tempTimer > MIN_UPDATE_DURATION) {
        tempTimer = timerNow;
        for (byte i = 0; i < TEMP_SENSORS; i++) {
            temperature[i] = 1600 + i * 200 + (timerNow % 100) + temperatureCalibration[i];
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

