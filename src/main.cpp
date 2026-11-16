// Pass It On - Part 3: Dish-level lantern (food waste)
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

// ---------------- Buzzer ----------------
// Wokwi's buzzer only sounds when the pin toggles quickly, so we toggle at
// about 2 kHz. On real hardware an active buzzer sounds with this too.
// Do not use tone(): it takes Timer2 and breaks PWM on D3 and D11.
void beep(unsigned int ms) {
  unsigned long end = millis() + ms;
  while (millis() < end) {
    digitalWrite(BUZZER, HIGH); delayMicroseconds(250);
    digitalWrite(BUZZER, LOW);  delayMicroseconds(250);
  }
}

void chime() {                       // clean-dish celebration (blocks ~0.7 s)
  for (int i = 0; i < 3; i++) {
    beep(120);
    delay(120);
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
  for (int i = 0; i < 5; i++) { r[i] = readDistanceCm(); delay(10); }
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
void lanternGlow(int pct) {
  analogWrite(RGB_R, map(pct, 0, 100, 40, 255));
  analogWrite(RGB_G, map(pct, 0, 100, 15, 110));
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

// ================= STUBS FOR TEAMMATES' PARTS =================
// Placeholders so the merged sketch compiles and the contract names exist.
// Each owner replaces the body of their own function(s). Keep the signatures.

// Pins from the team contract (not used by Part 3, listed to avoid clashes)
const uint8_t SERVO_PIN = 9;                 // companion servo
const uint8_t ROULETTE_BTN = 10;             // roulette button
const uint8_t BOWL_PIN[2] = {A0, A1};        // photoresistors, bowl A / bowl B
const uint8_t DINER_LED[2] = {A2, A3};       // diner LEDs A / B

// Shared-bite glow: true if bowl i is currently lifted
bool bowlLifted(int i) {
  (void)i;
  // TODO: compare analogRead(BOWL_PIN[i]) to a threshold
  return false;
}

// Shared-bite glow: warm RGB pulse when bowls lift within the sync window
void pulseShared() {
  // TODO: pulse the centerpiece RGB (D3/D5/D6)
  Serial.println("[stub] pulseShared()");
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
  Serial.begin(9600);
  Serial.println("Dish-level lantern ready");
}

void loop() {
  checkCalibrate();

  int pct = readDishLevel();
  showLevel(pct);
  lanternGlow(pct);

  if (pct <= CLEAN_PCT && !celebrated) { chime(); celebrated = true; }
  if (pct > REARM_PCT) celebrated = false;       // re-arm after a refill

  Serial.print("level%: ");
  Serial.println(pct);
  delay(150);
}
