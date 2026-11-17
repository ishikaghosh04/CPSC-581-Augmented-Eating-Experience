// Pass It On - Dish-level lantern and shared-bite glow
// Pin map follows the team contract.
#include <Arduino.h>

// ---- Pins (team contract) ----
const uint8_t TRIG = 2, ECHO = 4;
const uint8_t RGB_R = 3, RGB_G = 5, RGB_B = 6;
const uint8_t DATA = 7, LATCH = 8, CLOCK = 12;
const uint8_t CAL_BTN = 11, BUZZER = 13;

// ---- Calibration (placeholders: measure the real dish on Tuesday) ----
float distFull = 5;      // cm from sensor to food when the dish is full
float distEmpty = 30;    // cm from sensor to dish bottom when empty
uint8_t calStep = 0;     // 0 = next press sets FULL, 1 = next press sets EMPTY

// ---- Thresholds ----
const int CLEAN_PCT = 5;     // at or below this, the dish counts as clean
const int REARM_PCT = 15;    // must rise above this (a refill) to celebrate again
bool celebrated = false;

// Keep polling bowls while the existing lantern waits or plays a chime.
void serviceSharedGlow();

void waitWithGlow(unsigned long duration) {
  const unsigned long start = millis();
  while (millis() - start < duration) {
    serviceSharedGlow();
    delay(1);
  }
}

// ---------------- Buzzer ----------------
// Wokwi's buzzer only sounds when the pin toggles quickly, so we toggle at
// about 2 kHz. On real hardware an active buzzer sounds with this too.
// Do not use tone(): it takes Timer2 and breaks PWM on D3 and D11.
void beep(unsigned int ms) {
  const unsigned long start = millis();
  uint8_t cycles = 0;
  while (millis() - start < ms) {
    digitalWrite(BUZZER, HIGH); delayMicroseconds(250);
    digitalWrite(BUZZER, LOW);  delayMicroseconds(250);
    if (++cycles % 8 == 0) serviceSharedGlow();
  }
}

void chime() {                       // clean-dish celebration (blocks ~0.7 s)
  for (int i = 0; i < 3; i++) {
    beep(120);
    waitWithGlow(120);
  }
}

// ---------------- Ultrasonic ----------------
float readDistanceCm() {
  digitalWrite(TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long t = pulseIn(ECHO, HIGH, 30000);          // 30 ms timeout
  return t == 0 ? 400.0 : t * 0.0343 / 2;       // timeout = "very far"
}

float medianDistance() {                         // median of 5 readings
  float r[5];
  for (int i = 0; i < 5; i++) { r[i] = readDistanceCm(); waitWithGlow(10); }
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4 - i; j++)
      if (r[j] > r[j + 1]) { float t = r[j]; r[j] = r[j + 1]; r[j + 1] = t; }
  return r[2];
}

// Contract: returns 0-100 (100 = full dish)
int readDishLevel() {
  float d = medianDistance();
  float pct = (distEmpty - d) / (distEmpty - distFull) * 100.0;
  return constrain((int)pct, 0, 100);
}

// ---------------- Outputs ----------------
// Contract: show dish level (how much is left) on the 8-LED bar via the 74HC595
void showLevel(int pct) {
  int n = (pct * 8 + 50) / 100;                  // 0..8 LEDs lit
  byte mask = (1 << n) - 1;
  if (n >= 8) mask = 0xFF;
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLOCK, MSBFIRST, mask);
  digitalWrite(LATCH, HIGH);
}

// Lantern: warm and bright when plenty is left, softens as the dish empties
void lanternGlow(int pct, int pulse = 0) {
  const int red = map(pct, 0, 100, 40, 255);
  const int green = map(pct, 0, 100, 15, 110);
  analogWrite(RGB_R, map(pulse, 0, 100, red, 255));
  analogWrite(RGB_G, map(pulse, 0, 100, green, 220));
  analogWrite(RGB_B, 0);
}

// ---------------- Calibration ----------------
void checkCalibrate() {
  static bool last = HIGH;
  bool now = digitalRead(CAL_BTN);
  if (last == HIGH && now == LOW) {
    float d = medianDistance();
    if (calStep == 0) distFull = d; else distEmpty = d;
    calStep ^= 1;
    Serial.print(calStep == 1 ? "FULL set to " : "EMPTY set to ");
    Serial.print(d);
    Serial.println(" cm");
    beep(60);
  }
  last = now;
}

// ---------------- Shared-bite glow ----------------

// Pins from the team contract (not used by Part 3, listed to avoid clashes)
const uint8_t SERVO_PIN = 9;                 // companion servo
const uint8_t ROULETTE_BTN = 10;             // roulette button
const uint8_t BOWL_PIN[2] = {A0, A1};        // photoresistors, bowl A / bowl B
const uint8_t DINER_LED[2] = {A2, A3};       // diner LEDs A / B

const uint8_t SOLO_SWITCH = A4;              // LOW = solo (bowl A only)
const int LIFT_ADC = 420;                    // brighter: analog value falls
const int REST_ADC = 580;                    // darker: analog value rises
const unsigned long STABLE_MS = 45;
const unsigned long SYNC_MS = 20000;
const unsigned long PULSE_MS = 900;

struct Bowl {
  bool lifted;
  bool candidate;
  unsigned long candidateSince;
  bool pending;
  unsigned long liftedAt;
};

Bowl bowls[2] = {};
bool soloMode = false;
bool pulseActive = false;
unsigned long pulseStarted = 0;
int dishPercent = 0;

// Contract: read the latest debounced state.
bool bowlLifted(int i) {
  return i >= 0 && i < 2 && bowls[i].lifted;
}

// Contract: request a warm pulse without blocking.
void pulseShared() {
  pulseStarted = millis();
  pulseActive = true;
  Serial.println(soloMode ? "Solo glow" : "Shared glow");
}

void serviceSharedGlow() {
  const unsigned long now = millis();
  const bool solo = digitalRead(SOLO_SWITCH) == LOW;
  if (solo != soloMode) {
    soloMode = solo;
    Serial.println(soloMode ? "Mode: SOLO" : "Mode: GROUP");
    for (Bowl &bowl : bowls) bowl.pending = false;
  }

  for (int i = 0; i < 2; ++i) {
    Bowl &bowl = bowls[i];
    const int light = analogRead(BOWL_PIN[i]);
    const bool sensedLift = bowl.lifted ? light < REST_ADC : light <= LIFT_ADC;
    if (sensedLift != bowl.candidate) {
      bowl.candidate = sensedLift;
      bowl.candidateSince = now;
    }
    if (bowl.candidate != bowl.lifted && now - bowl.candidateSince >= STABLE_MS) {
      bowl.lifted = bowl.candidate;
      if (bowl.lifted) {
        Serial.println(i == 0 ? "Bowl A lifted" : "Bowl B lifted");
        Serial.println(now);

        bowl.liftedAt = now;
        bowl.pending = !soloMode;
        if (soloMode && i == 0) pulseShared();
      }
    }
    if (bowl.pending && now - bowl.liftedAt >= SYNC_MS) bowl.pending = false;
  }

  if (bowls[0].pending && bowls[1].pending) {
    for (Bowl &bowl : bowls) bowl.pending = false;
    pulseShared();
  }

  const unsigned long elapsed = millis() - pulseStarted;
  if (pulseActive && elapsed >= PULSE_MS) pulseActive = false;
  int strength = 0;
  if (pulseActive) {
    const unsigned long attack = 180;
    strength = elapsed < attack
        ? elapsed * 100 / attack
        : (PULSE_MS - elapsed) * 100 / (PULSE_MS - attack);
  }
  lanternGlow(dishPercent, strength);
}

// Companion: servo nod, paced to the slowest eater
void companionNod() {
  // TODO: move the servo on D9
  Serial.println("[stub] companionNod()");
}

// Roulette: spin, land on a person, play the buzzer chime
void spinRoulette() {
  // TODO: read D10 button, move servo, pick who takes the leftovers
  Serial.println("[stub] spinRoulette()");
}
// ==============================================================

// ---------------- Arduino ----------------
void setup() {
  pinMode(TRIG, OUTPUT);  pinMode(ECHO, INPUT);
  pinMode(DATA, OUTPUT);  pinMode(LATCH, OUTPUT); pinMode(CLOCK, OUTPUT);
  pinMode(RGB_R, OUTPUT); pinMode(RGB_G, OUTPUT); pinMode(RGB_B, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(CAL_BTN, INPUT_PULLUP);
  pinMode(SOLO_SWITCH, INPUT_PULLUP);
  soloMode = digitalRead(SOLO_SWITCH) == LOW;
  for (int i = 0; i < 2; ++i) {
    pinMode(BOWL_PIN[i], INPUT);
    const bool lifted = analogRead(BOWL_PIN[i]) <= LIFT_ADC;
    bowls[i] = {lifted, lifted, millis(), false, 0};
  }
  Serial.begin(9600);
  Serial.println("Dish-level lantern ready");
}

void loop() {
  checkCalibrate();

  int pct = readDishLevel();
  dishPercent = pct;
  showLevel(pct);
  serviceSharedGlow();

  if (pct <= CLEAN_PCT && !celebrated) { chime(); celebrated = true; }
  if (pct > REARM_PCT) celebrated = false;       // re-arm after a refill

  Serial.print("level%: ");
  Serial.println(pct);
  waitWithGlow(150);
}
