// Pass It On - Dish-level lantern and shared-bite glow
// Pin map follows the team contract.
#include <Arduino.h>
#include <Servo.h>
// ---- Pins (team contract) ----
const uint8_t TRIG = 2, ECHO = 4;
const uint8_t RGB_R = 3, RGB_G = 5, RGB_B = 6;
const uint8_t DATA = 7, LATCH = 8, CLOCK = 12;
const uint8_t CAL_BTN = 11, BUZZER = 13;

// ---- Calibration (placeholders: measure the real dish on Tuesday) ----
float distFull = 5;   // cm from sensor to food when the dish is full
float distEmpty = 30; // cm from sensor to dish bottom when empty
uint8_t calStep = 0;  // 0 = next press sets FULL, 1 = next press sets EMPTY

// ---- Thresholds ----
const int CLEAN_PCT = 5;  // at or below this, the dish counts as clean
const int REARM_PCT = 15; // must rise above this (a refill) to celebrate again
bool celebrated = true;

// Keep polling bowls while the existing lantern waits or plays a chime.
void serviceSharedGlow();
void companionGlow(int pct);

void waitWithGlow(unsigned long duration)
{
  const unsigned long start = millis();
  while (millis() - start < duration)
  {
    serviceSharedGlow();
    delay(1);
  }
}

// ---------------- Buzzer ----------------
// Wokwi's buzzer only sounds when the pin toggles quickly, so we toggle at
// about 2 kHz. On real hardware an active buzzer sounds with this too.
// Do not use tone(): it takes Timer2 and breaks PWM on D3 and D11.
void beep(unsigned int ms)
{
  const unsigned long start = millis();
  uint8_t cycles = 0;
  while (millis() - start < ms)
  {
    digitalWrite(BUZZER, HIGH);
    delayMicroseconds(250);
    digitalWrite(BUZZER, LOW);
    delayMicroseconds(250);
    if (++cycles % 8 == 0)
      serviceSharedGlow();
  }
}

void chime()
{ // clean-dish celebration (blocks ~0.7 s)
  for (int i = 0; i < 3; i++)
  {
    beep(120);
    waitWithGlow(120);
  }
}

// ---------------- Ultrasonic ----------------
float readDistanceCm()
{
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long t = pulseIn(ECHO, HIGH, 30000);    // 30 ms timeout
  return t == 0 ? 400.0 : t * 0.0343 / 2; // timeout = "very far"
}

float medianDistance()
{ // median of 5 readings
  float r[5];
  for (int i = 0; i < 5; i++)
  {
    r[i] = readDistanceCm();
    waitWithGlow(10);
  }
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4 - i; j++)
      if (r[j] > r[j + 1])
      {
        float t = r[j];
        r[j] = r[j + 1];
        r[j + 1] = t;
      }
  return r[2];
}

// Contract: returns 0-100 (100 = full dish)
int readDishLevel()
{
  float d = medianDistance();
  float pct = (distEmpty - d) / (distEmpty - distFull) * 100.0;
  return constrain((int)pct, 0, 100);
}

// ---------------- Outputs ----------------
// Contract: show dish level (how much is left) on the 8-LED bar via the 74HC595
void showLevel(int pct)
{
  int n = (pct * 8 + 50) / 100; // 0..8 LEDs lit
  byte mask = (1 << n) - 1;
  if (n >= 8)
    mask = 0xFF;
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLOCK, MSBFIRST, mask);
  digitalWrite(LATCH, HIGH);
}

// Lantern: warm and bright when plenty is left, softens as the dish empties
void lanternGlow(int pct, int pulse = 0)
{
  const int red = map(pct, 0, 100, 40, 255);
  const int green = map(pct, 0, 100, 15, 110);
  analogWrite(RGB_R, map(pulse, 0, 100, red, 255));
  analogWrite(RGB_G, map(pulse, 0, 100, green, 220));
  analogWrite(RGB_B, 0);
}

// ---------------- Calibration ----------------
void checkCalibrate()
{
  static bool last = HIGH;
  bool now = digitalRead(CAL_BTN);
  if (last == HIGH && now == LOW)
  {
    float d = medianDistance();
    if (calStep == 1 && d < distFull + 2)
    {
      Serial.println("EMPTY must be 2+ cm farther than FULL - press again");
    }
    else
    {
      if (calStep == 0)
        distFull = d;
      else
        distEmpty = d;
      calStep ^= 1;
      Serial.print(calStep == 1 ? "FULL set to " : "EMPTY set to ");
      Serial.print(d);
      Serial.println(" cm");
    }
    beep(60);
  }
  last = now;
}

// ---------------- Shared-bite glow ----------------

// Pins from the team contract (not used by Part 3, listed to avoid clashes)
const uint8_t SERVO_PIN = 9;           // companion servo
const uint8_t ROULETTE_BTN = 10;       // roulette button
const uint8_t BOWL_PIN[2] = {A0, A1};  // photoresistors, bowl A / bowl B
const uint8_t DINER_LED[2] = {A2, A3}; // diner LEDs A / B

const uint8_t SOLO_SWITCH = A4; // LOW = solo (bowl A only)
const int LIFT_ADC = 420;       // brighter: analog value falls
const int REST_ADC = 580;       // darker: analog value rises
const unsigned long STABLE_MS = 45;
const unsigned long SYNC_MS = 20000;
const unsigned long PULSE_MS = 900;

struct Bowl
{
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
bool bowlLifted(int i)
{
  return i >= 0 && i < 2 && bowls[i].lifted;
}

// Contract: request a warm pulse without blocking.
void pulseShared()
{
  pulseStarted = millis();
  pulseActive = true;
  Serial.println(soloMode ? "Solo glow" : "Shared glow");
}

void serviceSharedGlow()
{
  const unsigned long now = millis();
  const bool solo = digitalRead(SOLO_SWITCH) == LOW;
  if (solo != soloMode)
  {
    soloMode = solo;
    Serial.println(soloMode ? "Mode: SOLO" : "Mode: GROUP");
    for (Bowl &bowl : bowls)
      bowl.pending = false;
  }

  for (int i = 0; i < 2; ++i)
  {
    Bowl &bowl = bowls[i];
    const int light = analogRead(BOWL_PIN[i]);
    const bool sensedLift = bowl.lifted ? light < REST_ADC : light <= LIFT_ADC;
    if (sensedLift != bowl.candidate)
    {
      bowl.candidate = sensedLift;
      bowl.candidateSince = now;
    }
    if (bowl.candidate != bowl.lifted && now - bowl.candidateSince >= STABLE_MS)
    {
      bowl.lifted = bowl.candidate;
      if (bowl.lifted)
      {
        Serial.println(i == 0 ? "Bowl A lifted" : "Bowl B lifted");
        Serial.println(now);

        bowl.liftedAt = now;
        bowl.pending = !soloMode;
        if (soloMode && i == 0)
          pulseShared();
      }
    }
    if (bowl.pending && now - bowl.liftedAt >= SYNC_MS)
      bowl.pending = false;
  }

  if (bowls[0].pending && bowls[1].pending)
  {
    for (Bowl &bowl : bowls)
      bowl.pending = false;
    pulseShared();
  }

  const unsigned long elapsed = millis() - pulseStarted;
  if (pulseActive && elapsed >= PULSE_MS)
    pulseActive = false;
  int strength = 0;
  if (pulseActive)
  {
    const unsigned long attack = 180;
    strength = elapsed < attack
                   ? elapsed * 100 / attack
                   : (PULSE_MS - elapsed) * 100 / (PULSE_MS - attack);
  }

  const bool dishClean = dishPercent <= CLEAN_PCT;

  if (pulseActive && ! dishClean)
  {
    lanternGlow(dishPercent, strength);
  }
  else
  {
    companionGlow(dishPercent);
  }
}

// ---------------- Pace-matching companion ----------------
Servo companionServo;

// false = two diners; true = solo demo using bowl A
const bool SOLO_MODE = false;

const unsigned long DEFAULT_INTERVAL = 6000;
const unsigned long WAIT_AFTER = 20000;
const unsigned long PLAY_AFTER = 45000;
const unsigned long SENSOR_DEBOUNCE = 80;

struct DinerState
{
  bool rawLifted = false;
  bool stableLifted = false;
  bool started = false;
  bool hasInterval = false;

  unsigned long rawChangedAt = 0;
  unsigned long lastBite = 0;
  unsigned long averageInterval = DEFAULT_INTERVAL;
};

DinerState diners[2];

bool mealStarted = false;
bool waiting = false;
bool playfulDone = false;

unsigned long lastNod = 0;
unsigned long movementStarted = 0;

// 0 = still, 1 = nodding, 2 = playful wiggle
uint8_t movement = 0;

void companionNod()
{
  movement = 1;
  movementStarted = millis();
  lastNod = movementStarted;
  companionServo.write(110);
}

void beginCompanion()
{
  companionServo.attach(SERVO_PIN);
  companionServo.write(90);

  // Read the starting positions so an already exposed sensor
  // does not immediately create a bowl-lift event.
  for (int i = 0; i < 2; i++)
  {
    diners[i].rawLifted = bowlLifted(i);
    diners[i].stableLifted = diners[i].rawLifted;
    diners[i].rawChangedAt = millis();
  }

  Serial.println(F("Companion ready"));
}

void updateCompanion()
{
  const unsigned long now = millis();
  const int dinerCount = SOLO_MODE ? 1 : 2;

  bool biteDetected = false;
  bool someonePaused = false;
  bool everyonePaused = true;
  unsigned long tableInterval = 0;

  for (int i = 0; i < dinerCount; i++)
  {
    DinerState &diner = diners[i];
    const bool lifted = bowlLifted(i);

    // Wait for a stable sensor change.
    if (lifted != diner.rawLifted)
    {
      diner.rawLifted = lifted;
      diner.rawChangedAt = now;
    }

    if (diner.rawLifted != diner.stableLifted &&
        now - diner.rawChangedAt >= SENSOR_DEBOUNCE)
    {
      diner.stableLifted = diner.rawLifted;

      // Count only the change from bowl down to bowl up.
      if (diner.stableLifted &&
          (!diner.started || now - diner.lastBite >= 400))
      {
        if (diner.started)
        {
          const unsigned long interval = now - diner.lastBite;

          if (interval < WAIT_AFTER)
          {
            if (diner.hasInterval)
            {
              diner.averageInterval =
                  (7UL * diner.averageInterval + 3UL * interval) / 10UL;
            }
            else
            {
              diner.averageInterval = interval;
              diner.hasInterval = true;
            }
          }
          else
          {
            // Start estimating again after a conversation pause.
            diner.hasInterval = false;
            diner.averageInterval = DEFAULT_INTERVAL;
          }
        }

        diner.started = true;
        diner.lastBite = now;
        biteDetected = true;

        Serial.print(i == 0 ? F("A") : F("B"));
        Serial.println(F(" bowl lifted"));
      }
    }

    // A diner joins the pace calculation after their first lift.
    if (diner.started)
    {
      const unsigned long idleTime = now - diner.lastBite;

      if (idleTime >= WAIT_AFTER)
      {
        someonePaused = true;
      }

      if (idleTime < PLAY_AFTER)
      {
        everyonePaused = false;
      }

      if (diner.averageInterval > tableInterval)
      {
        tableInterval = diner.averageInterval;
      }
    }
  }

  if (!mealStarted)
  {
    if (!biteDetected)
      return;

    mealStarted = true;
    companionNod();
    Serial.println(F("Companion wakes"));
  }

  if (biteDetected)
  {
    playfulDone = false;
  }

  // A participating diner has paused, so stop regular nods.
  if (someonePaused)
  {
    if (!waiting)
    {
      waiting = true;
      movement = 0;
      companionServo.write(65);
      Serial.println(F("Waiting for the table"));
    }
  }
  else if (waiting)
  {
    waiting = false;
    movement = 0;
    companionServo.write(90);
    lastNod = now;
    Serial.println(F("Eating resumes"));
  }

  // Play one wiggle after everyone participating pauses for 45 seconds.
  if (everyonePaused && !playfulDone)
  {
    playfulDone = true;
    beep(80);
    movement = 2;
    movementStarted = millis();
    Serial.println(F("Playful wiggle + chime"));
  }

  // Finish each movement using elapsed time.
  const unsigned long elapsed = now - movementStarted;

  if (movement == 1 && elapsed >= 350)
  {
    companionServo.write(90);
    movement = 0;
  }

  if (movement == 2)
  {
    if (elapsed >= 1200)
    {
      companionServo.write(waiting ? 65 : 90);
      movement = 0;
    }
    else
    {
      companionServo.write((elapsed / 200) % 2 == 0 ? 75 : 105);
    }
  }

  if (waiting || movement != 0)
    return;

  // Solo mode mirrors lifts. Group mode uses the longest interval.
  if (SOLO_MODE)
  {
    if (biteDetected)
      companionNod();
  }
  else
  {
    tableInterval = max(tableInterval, 3000UL);

    if (now - lastNod >= tableInterval)
    {
      companionNod();

      Serial.print(F("Companion interval (ms): "));
      Serial.println(tableInterval);
    }
  }
}

// Roulette: spin, land on a person, play the buzzer chime
void spinRoulette()
{
  // TODO: read D10 button, move servo, pick who takes the leftovers
  Serial.println("[stub] spinRoulette()");
}
// ==============================================================
// Set the companion's belly light using its current behaviour.
void companionGlow(int pct)
{
  // Give the existing dish lantern the empty-dish display.
  if (pct <= CLEAN_PCT)
  {
    lanternGlow(pct);
    return;
  }

  // A triangle wave gives a gentle four-second breathing cycle.
  unsigned long phase = millis() % 4000UL;
  int breath = phase <= 2000UL ? phase : 4000UL - phase;

  int red = 255;
  int green = 120;
  int blue = 30;
  int brightness;

  if (!mealStarted)
  {
    // Sleeping: dim, slowly breathing warm light.
    brightness = map(breath, 0, 2000, 5, 25);
  }
  else if (waiting)
  {
    // Conversation pause: a soft, warm pulse.
    green = 180;
    blue = 100;
    brightness = map(breath, 0, 2000, 20, 70);
  }
  else
  {
    bool differentPaces = false;

    // Compare speeds only after both diners have usable intervals.
    if (!SOLO_MODE &&
        diners[0].hasInterval &&
        diners[1].hasInterval)
    {
      unsigned long faster = min(
          diners[0].averageInterval,
          diners[1].averageInterval);

      unsigned long slower = max(
          diners[0].averageInterval,
          diners[1].averageInterval);

      // Amber when the slower interval is over 1.5 times the faster.
      differentPaces = slower > faster + faster / 2;
    }

    if (!differentPaces)
    {
      lanternGlow(pct);
      return;
    }

    // Soft amber cue while the servo keeps following the slower pace.
    green = 160;
    blue = 0;
    brightness = 100;
  }

  // Multiply first, then divide, to retain brightness precision.
  analogWrite(RGB_R, red * brightness / 255);
  analogWrite(RGB_G, green * brightness / 255);
  analogWrite(RGB_B, blue * brightness / 255);
}
// ---------------- Arduino ----------------
void setup()
{
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(DATA, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(CAL_BTN, INPUT_PULLUP);
  pinMode(SOLO_SWITCH, INPUT_PULLUP);
  soloMode = digitalRead(SOLO_SWITCH) == LOW;
  for (int i = 0; i < 2; ++i)
  {
    pinMode(BOWL_PIN[i], INPUT);
    const bool lifted = analogRead(BOWL_PIN[i]) <= LIFT_ADC;
    bowls[i] = {lifted, lifted, millis(), false, 0};
  }
  Serial.begin(9600);
  beginCompanion();
  Serial.println("Dish-level lantern ready");
}

void loop()
{
  updateCompanion();
  checkCalibrate();

  int pct = readDishLevel();
  dishPercent = pct;
  showLevel(pct);
  serviceSharedGlow();

  if (pct <= CLEAN_PCT && !celebrated)
  {
    chime();
    celebrated = true;
  }
  if (pct > REARM_PCT)
    celebrated = false; // re-arm after a refill

  Serial.print("level%: ");
  Serial.println(pct);
  waitWithGlow(150);
}
