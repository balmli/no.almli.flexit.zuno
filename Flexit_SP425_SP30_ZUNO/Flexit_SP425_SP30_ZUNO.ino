
#include "ZUNO_DS18B20.h"
#include "EEPROM.h"

#define DEBUG_x

#define RELAY_1_PIN                 9
#define RELAY_2_PIN                 10
#define RELAY_3_PIN                 11
#define DS18B20_BUS_PIN             12
#define FAN_LEVEL_1_PIN             A1
#define FAN_LEVEL_2_PIN             A2
#define HEATING_PIN                 A3
#define LED_PIN                     LED_BUILTIN

#define MIN_UPDATE_DURATION         30000
#define MIN_STATE_UPDATE_DURATION   1000
#define MIN_TEMP_UPDATE_DURATION    30000
#define LED_BLINK_DURATION          25

OneWire ow(DS18B20_BUS_PIN);
DS18B20Sensor ds18b20(&ow);

#define MAX_TEMP_SENSORS            4
#define ADDR_SIZE                   8
byte addresses[ADDR_SIZE * MAX_TEMP_SENSORS];
#define ADDR(i) (&addresses[i * ADDR_SIZE])
byte temp_sensors;
word temperature[MAX_TEMP_SENSORS];
word temperatureReported[MAX_TEMP_SENSORS];
word temperatureCalibration[MAX_TEMP_SENSORS];

byte mode = 1;
byte modeChanged = 0;
unsigned long lastSetMode = 0;
byte lastFanLevel = 1;
byte lastFanLevelReported = 99;
byte lastHeating = 0;
byte lastHeatingReported = 99;

unsigned long relayTimer = 0;
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
word enabled_mode_config;
word default_config_set_config;

ZUNO_SETUP_SLEEPING_MODE(ZUNO_SLEEPING_MODE_ALWAYS_AWAKE);
ZUNO_SETUP_PRODUCT_ID(0xC0, 0xC0);
ZUNO_SETUP_CFGPARAMETER_HANDLER(config_parameter_changed);

ZUNO_SETUP_CHANNELS(
    ZUNO_SWITCH_MULTILEVEL(getterMode, setterMode),

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
    pinMode(RELAY_1_PIN, OUTPUT);
    pinMode(RELAY_2_PIN, OUTPUT);
    pinMode(RELAY_3_PIN, OUTPUT);
    digitalWrite(RELAY_1_PIN, HIGH);
    digitalWrite(RELAY_2_PIN, HIGH);
    digitalWrite(RELAY_3_PIN, HIGH);
    pinMode(LED_PIN, OUTPUT);

    pinMode(FAN_LEVEL_1_PIN, INPUT_PULLUP);
    pinMode(FAN_LEVEL_2_PIN, INPUT_PULLUP);
    pinMode(HEATING_PIN, INPUT_PULLUP);

    Serial.begin();
    fetchConfig();
    temp_sensors = ds18b20.findAllSensors(addresses);
    fetchMode();
}

void fetchConfig() {
    status_report_interval_config = zunoLoadCFGParam(64);
    temperature_report_interval_config = zunoLoadCFGParam(65);
    temperature_report_threshold_config = zunoLoadCFGParam(66);
    relay_duration_config = zunoLoadCFGParam(67);
    enabled_config = zunoLoadCFGParam(68);
    enabled_mode_config = zunoLoadCFGParam(69);
    for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
        temperatureCalibration[i] = zunoLoadCFGParam(70 + i);
    }

    default_config_set_config = zunoLoadCFGParam(96);
    if (default_config_set_config != 1) {
        status_report_interval_config = 600;
        zunoSaveCFGParam(64, status_report_interval_config);

        temperature_report_interval_config = 600;
        zunoSaveCFGParam(65, temperature_report_interval_config);

        temperature_report_threshold_config = 20;
        zunoSaveCFGParam(66, temperature_report_threshold_config);

        relay_duration_config = 250;
        zunoSaveCFGParam(67, relay_duration_config);

        enabled_config = 0;
        zunoSaveCFGParam(68, enabled_config);

        enabled_mode_config = 0;
        zunoSaveCFGParam(69, enabled_mode_config);

        for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
            temperatureCalibration[i] = 0;
            zunoSaveCFGParam(70 + i, temperatureCalibration[i]);
        }

        default_config_set_config = 1;
        zunoSaveCFGParam(96, default_config_set_config);
    }
}

void fetchMode() {
    dword addr = 0x800;
    mode = EEPROM.read(addr);
    if (mode < 1 || mode > 13) {
        mode = 1;
    }
    modeChanged = 1;
    zunoSendReport(1);
}

void storeMode() {
    dword addr = 0x800;
    EEPROM.write(addr, mode);
}

void loop() {
    if (enabled_config == 1) {
        unsigned long timerNow = millis();
        if (enabled_mode_config == 1) {
            handleMode(timerNow);
        }
        getFanLevel(timerNow);
        getHeating(timerNow);
        reportMode(timerNow);
        ledBlinkOff(timerNow);
        reportFanLevel(timerNow);
        ledBlinkOff(timerNow);
        reportHeating(timerNow);
        ledBlinkOff(timerNow);
        reportTemperatures(timerNow);
        ledBlinkOff(timerNow);
#ifdef DEBUG
        listConfig(timerNow);
#endif
    }
}

void config_parameter_changed(byte param, word value) {
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
        enabled_mode_config = value;
        if (enabled_mode_config < 0 || enabled_mode_config > 1) {
            enabled_mode_config = 0;
        }
    }
    if (param >= 70 && param <= 73) {
        temperatureCalibration[param - 70] = value;
    }

#ifdef DEBUG
    Serial.print("config ");
    Serial.print(param);
    Serial.print(" = ");
    Serial.println(value);
#endif
}

#ifdef DEBUG
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
        Serial.print("enabled_config: ");
        Serial.println(enabled_config);
        Serial.print("enabled_mode_config: ");
        Serial.println(enabled_mode_config);
        Serial.print("temp sensors: ");
        Serial.println(temp_sensors);
        for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
            Serial.print("temperatureCalibration[");
            Serial.print(i);
            Serial.print("]: ");
            Serial.println(temperatureCalibration[i]);
        }
    }
}
#endif

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

void handleMode(unsigned long timerNow) {
    if ((modeChanged == 1) && ((timerNow - relayTimer) > relay_duration_config)) {
        relayTimer = timerNow;
        ledBlink(timerNow);

        byte r1 = ((mode % 10) == 2) ? LOW : HIGH;
        digitalWrite(RELAY_1_PIN, r1);

        byte r2 = ((mode % 10) == 3) ? LOW : HIGH;
        digitalWrite(RELAY_2_PIN, r2);

        byte r3 = ((mode - mode % 10) == 10) ? LOW : HIGH;
        digitalWrite(RELAY_3_PIN, r3);

        storeMode();
        modeChanged = 0;
        delay(25);

#ifdef DEBUG_3
        Serial.print(timerNow / 1000);
        Serial.print(": r1: ");
        Serial.print(r1);
        Serial.print(", r2: ");
        Serial.print(r2);
        Serial.print(", r3: ");
        Serial.println(r3);
#endif
    }
}

void getFanLevel(unsigned long timerNow) {
    int fl1 = digitalRead(FAN_LEVEL_1_PIN);
    int fl2 = digitalRead(FAN_LEVEL_2_PIN);
    lastFanLevel = fl1 == 1 && fl2 == 0 ? 3 : (fl1 == 0 && fl2 == 1 ? 2 : 1);

#ifdef DEBUG_2
    if (timerNow - fanLevelTimer2 > 2000) {
        fanLevelTimer2 = timerNow;
        Serial.print(timerNow / 1000);
        Serial.print(": fl1: ");
        Serial.print(fl1);
        Serial.print(", fl2: ");
        Serial.println(fl2);
        Serial.print(", fan level: ");
        Serial.print(lastFanLevel);
        Serial.print(", mode: ");
        Serial.println(mode);
    }
#endif
}

void getHeating(unsigned long timerNow) {
    int heat = digitalRead(HEATING_PIN);
    lastHeating = heat == 0 ? 10 : 0;

#ifdef DEBUG_2
    if (timerNow - heatingTimer2 > 2000) {
        heatingTimer2 = timerNow;
        Serial.print(timerNow / 1000);
        Serial.print(", heat: ");
        Serial.print(heat);
        Serial.print(", heating: ");
        Serial.print(lastHeating);
        Serial.print(", mode: ");
        Serial.println(mode);
    }
#endif
}

void reportMode(unsigned long timerNow) {
    bool changed = lastFanLevel != lastFanLevelReported || lastHeating != lastHeatingReported;
    unsigned long timer1Diff = timerNow - lastSetMode;
    if (changed && timer1Diff > MIN_STATE_UPDATE_DURATION) {
        byte mode2 = mode;
        mode = lastFanLevel + lastHeating;
        ledBlink(timerNow);
        zunoSendReport(1);
#ifdef DEBUG_4
        Serial.print(timerNow / 1000);
        Serial.print(": mode: ");
        Serial.println(mode);
#endif
        delay(100);
        mode = mode2;
    }
}

void reportFanLevel(unsigned long timerNow) {
    bool changed = lastFanLevel != lastFanLevelReported;
    unsigned long timer1Diff = timerNow - lastSetMode;
    unsigned long timer2Diff = timerNow - fanLevelTimer;
    unsigned long repInterval = ((unsigned long)status_report_interval_config) * 1000;

    if (changed && timer1Diff > MIN_STATE_UPDATE_DURATION || timer2Diff > repInterval) {
        lastFanLevelReported = lastFanLevel;
        fanLevelTimer = timerNow;
        ledBlink(timerNow);
        zunoSendReport(2);
#ifdef DEBUG_4
        Serial.print(timerNow / 1000);
        Serial.print(": lastFanLevel: ");
        Serial.println(lastFanLevel);
#endif
    }
}

void reportHeating(unsigned long timerNow) {
    bool changed = lastHeating != lastHeatingReported;
    unsigned long timer1Diff = timerNow - lastSetMode;
    unsigned long timer2Diff = timerNow - heatingTimer;
    unsigned long repInterval = ((unsigned long)status_report_interval_config) * 1000;

    if (changed && timer1Diff > MIN_STATE_UPDATE_DURATION || timer2Diff > repInterval) {
        lastHeatingReported = lastHeating;
        heatingTimer = timerNow;
        ledBlink(timerNow);
        zunoSendReport(3);
#ifdef DEBUG_4
        Serial.print(timerNow / 1000);
        Serial.print(": lastHeating: ");
        Serial.println(lastHeating);
#endif
    }
}

void reportTemperatures(unsigned long timerNow) {
    for (byte i = 0; i < temp_sensors && i < MAX_TEMP_SENSORS; i++) {

        unsigned long timerDiff = timerNow - temperatureTimer[i];
        unsigned long repInterval = ((unsigned long)temperature_report_interval_config) * 1000;

        if (timerDiff > MIN_TEMP_UPDATE_DURATION || timerDiff > repInterval) {

            temperature[i] = ((word)(ds18b20.getTemperature(ADDR(i)) * 100)) + temperatureCalibration[i];
            bool tempChanged = temperature[i] > temperatureReported[i] + temperature_report_threshold_config || temperature[i] < temperatureReported[i] - temperature_report_threshold_config;

            if (tempChanged && timerDiff > MIN_TEMP_UPDATE_DURATION || timerDiff > repInterval) {
                temperatureTimer[i] = timerNow;
                temperatureReported[i] = temperature[i];
                ledBlink(timerNow);
                zunoSendReport(i + 4);
#ifdef DEBUG_3
                Serial.print(timerNow / 1000);
                Serial.print(": temp[");
                Serial.print(i);
                Serial.print("]: ");
                Serial.println(temperature[i]);
#endif
            }
        }
    }
}

void setterMode(byte value) {
    mode = value;
    modeChanged = 1;
    lastSetMode = millis();
}

byte getterMode(void) {
    return mode;
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


