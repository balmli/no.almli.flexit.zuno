

int FAN_LEVEL_1_PIN = 7;
int FAN_LEVEL_2_PIN = 8;
int HEATING_PIN = 9;

int FAN_MIN_PIN = 10;
int FAN_NORMAL_PIN = 11;
int FAN_MAX_PIN = 12;
int HEATING_LED_PIN = 13;

void setup() {
  pinMode(FAN_LEVEL_1_PIN, INPUT_PULLUP);
  pinMode(FAN_LEVEL_2_PIN, INPUT_PULLUP);
  pinMode(HEATING_PIN, INPUT_PULLUP);
  
  pinMode(FAN_MIN_PIN, OUTPUT);
  pinMode(FAN_NORMAL_PIN, OUTPUT);
  pinMode(FAN_MAX_PIN, OUTPUT);
  pinMode(HEATING_LED_PIN, OUTPUT);
}

void loop() {
  int fanLevel1 = digitalRead(FAN_LEVEL_1_PIN);
  int fanLevel2 = digitalRead(FAN_LEVEL_2_PIN);
  int heating = digitalRead(HEATING_PIN);

  digitalWrite(FAN_MIN_PIN, fanLevel1 == 1 && fanLevel2 == 1 ? HIGH : LOW);
  digitalWrite(FAN_NORMAL_PIN, fanLevel1 == 0 ? HIGH : LOW);
  digitalWrite(FAN_MAX_PIN, fanLevel2 == 0 ? HIGH : LOW);
  digitalWrite(HEATING_LED_PIN, 1 - heating);

  delay(20);
}

