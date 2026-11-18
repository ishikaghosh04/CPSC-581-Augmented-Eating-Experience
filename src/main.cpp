// Pass It On - Dish-level lantern, shared-bite glow, and Leftover Roulette
// Pin map for the current prototype.
#include <Arduino.h>
#include <Servo.h>
#include <LiquidCrystal.h>
#include <stdio.h>

// TinyIRReceiver uses pin-change interrupts and no hardware timer, so it keeps
// working while tone()/analogWrite() use Timer2. The full IRremote receiver
// does not, which is why the tiny NEC receiver is used here.
#define IR_RECEIVE_PIN A3
#define NO_LED_RECEIVE_FEEDBACK_CODE
#include <TinyIRReceiver.hpp>

// ---- Pins ----
const uint8_t TRIG = 2, ECHO = 4;
const uint8_t RGB_R = 3, RGB_G = 5, RGB_B = 6;
const uint8_t LCD_RS = 7, LCD_EN = 8;
const uint8_t LCD_D4 = 10, LCD_D5 = 11, LCD_D6 = 12, LCD_D7 = A2;
const uint8_t SERVO_PIN = 9;
const uint8_t SOLO_SWITCH = A4;
const uint8_t BUZZER = 13;
const uint8_t BOWL_PIN[2] = {A0, A1};

// The 16x2 LCD replaces the old 74HC595 LED bar. The bar's pins now drive the
// display, and the dish level is shown as text on the LCD.
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// ---- Calibration ----
float distFull = 5;   // cm from sensor to food when the dish is full
float distEmpty = 30; // cm from sensor to dish bottom when empty
bool calActive = false;
// 0 = next Test sets EMPTY, 1 = next Test sets FULL.
uint8_t calStep = 0;
const unsigned long CAL_DONE_MS = 3000;
unsigned long calDoneUntil = 0;
unsigned long calNextFlash = 0;
bool calFlashOn = true;

// A failed immunity toggle blinks a warning instead of changing the setting.
const unsigned long IMM_FAIL_MS = 4000;
unsigned long immFailUntil = 0;
unsigned long immFailNextFlash = 0;
bool immFailFlashOn = true;

// ---- Thresholds ----
const int CLEAN_PCT = 5;  // at or below this, the dish counts as clean
const int REARM_PCT = 15; // must rise above this (a refill) to celebrate again
bool celebrated = true;

// ---- Roulette roster ----
const uint8_t MAX_PLAYERS = 5;
const uint8_t MIN_PLAYERS = 2;
bool playerOn[MAX_PLAYERS] = {true, true, false, false, false};
bool immune[MAX_PLAYERS] = {false, false, false, false, false};
uint8_t activeCount = 2;
bool immunityMode = false;

int dishPercent = 0;

// ---- LCD state ----
enum LcdMode : uint8_t
{
  LCD_DISH,
  LCD_SPIN,
  LCD_REVEAL,
  LCD_SUMMARY,
  LCD_CAL,
  LCD_CAL_DONE,
  LCD_IMMUNITY,
  LCD_IMM_FAIL
};

LcdMode lcdMode = LCD_DISH;
int lcdLastPct = -1;
uint8_t lcdLastCount = 0;
uint8_t lcdSpinFrame = 0;
unsigned long lcdNextSpin = 0;
unsigned long lcdSummaryUntil = 0;

// ---- Roulette state ----
enum RouletteState : uint8_t
{
  ROULETTE_IDLE,
  ROULETTE_SPIN,
  ROULETTE_REVEAL
};

RouletteState rouletteState = ROULETTE_IDLE;
bool rouletteActive = false;
uint8_t rouletteLeftovers = 0;
uint8_t rouletteDish = 0;
uint8_t rouletteCleanup = 0;
uint8_t rouletteFrame = 0;
uint8_t roulettePhase = 0;
unsigned long rouletteStarted = 0;
unsigned long rouletteNextFrame = 0;
unsigned long roulettePhaseStarted = 0;

// Keep polling bowls while the lantern waits or plays a chime.
void serviceSharedGlow();
void companionGlow(int pct);
void updateRoulette();
void refreshLcd();

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
// about 2 kHz. On real hardware a passive buzzer sounds with this too.
// Do not use tone(): it takes Timer2 and breaks PWM on D3.
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
  const float span = distEmpty - distFull;
  if (span <= 0)
    return 0;

  float d = medianDistance();
  float pct = (distEmpty - d) / span * 100.0;
  return constrain((int)pct, 0, 100);
}

// ---------------- LCD helpers ----------------
void lcdWriteLine(uint8_t row, const char *text)
{
  lcd.setCursor(0, row);

  uint8_t n = 0;
  while (text[n] != '\0' && n < 16)
  {
    lcd.write(text[n]);
    ++n;
  }
  while (n < 16)
  {
    lcd.write(' ');
    ++n;
  }
}

uint8_t activeImmuneCount()
{
  uint8_t n = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; ++i)
  {
    if (playerOn[i] && immune[i])
      ++n;
  }
  return n;
}

void drawDishScreen()
{
  char line[17];
  const uint8_t immuneN = activeImmuneCount();
  if (immuneN > 0)
    snprintf(line, sizeof(line), "Dish %d%% Imm:%d", dishPercent, immuneN);
  else
    snprintf(line, sizeof(line), "Dish %d%%", dishPercent);
  lcdWriteLine(0, line);

  uint8_t pos = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; ++i)
  {
    if (!playerOn[i])
      continue;
    if (pos > 0)
      line[pos++] = ' ';
    line[pos++] = 'P';
    line[pos++] = char('1' + i);
  }

  const char *hint = " P=spin";
  if (pos + 7 <= 16)
  {
    for (uint8_t i = 0; hint[i] != '\0'; ++i)
      line[pos++] = hint[i];
  }
  line[pos] = '\0';
  lcdWriteLine(1, line);
}

void drawImmunityScreen()
{
  lcdWriteLine(0, "Imm 1-5  M=done");

  char line[17];
  uint8_t pos = 0;
  const char *prefix = "Imm:";
  for (uint8_t i = 0; prefix[i] != '\0'; ++i)
    line[pos++] = prefix[i];

  bool any = false;
  for (uint8_t i = 0; i < MAX_PLAYERS; ++i)
  {
    if (!playerOn[i] || !immune[i])
      continue;
    if (pos + 2 > 16)
      break;
    line[pos++] = 'P';
    line[pos++] = char('1' + i);
    any = true;
  }

  if (!any)
  {
    pos = 0;
    const char *none = "Imm:none";
    for (uint8_t i = 0; none[i] != '\0'; ++i)
      line[pos++] = none[i];
  }

  line[pos] = '\0';
  lcdWriteLine(1, line);
}

void drawSpinFrame()
{
  char line[17];
  for (uint8_t i = 0; i < 16; ++i)
    line[i] = ' ';
  line[16] = '\0';

  const uint8_t head = lcdSpinFrame % 16;
  line[head] = 'O';
  line[(head + 15) % 16] = 'o';
  lcdWriteLine(1, line);
  ++lcdSpinFrame;
}

void refreshLcd()
{
  const unsigned long now = millis();

  if (lcdMode == LCD_SUMMARY && now >= lcdSummaryUntil)
  {
    lcdMode = LCD_DISH;
    lcdLastPct = -1;
  }

  if (lcdMode == LCD_CAL_DONE)
  {
    if (now < calDoneUntil)
    {
      if (now >= calNextFlash)
      {
        calNextFlash = now + 300;
        calFlashOn = !calFlashOn;
        lcdWriteLine(0, calFlashOn ? "Calibration" : "");
        lcdWriteLine(1, calFlashOn ? "complete!" : "");
      }
      return;
    }
    lcdMode = LCD_DISH;
    lcdLastPct = -1;
    lcdLastCount = 0;
  }

  if (lcdMode == LCD_IMM_FAIL)
  {
    if (now < immFailUntil)
    {
      if (now >= immFailNextFlash)
      {
        immFailNextFlash = now + 300;
        immFailFlashOn = !immFailFlashOn;
        lcdWriteLine(0, immFailFlashOn ? "Immunity denied" : "");
        lcdWriteLine(1, immFailFlashOn ? "1 must stay free" : "");
      }
      return;
    }
    lcdMode = LCD_IMMUNITY;
    drawImmunityScreen();
    return;
  }

  if (lcdMode == LCD_DISH)
  {
    if (dishPercent != lcdLastPct || activeCount != lcdLastCount)
    {
      drawDishScreen();
      lcdLastPct = dishPercent;
      lcdLastCount = activeCount;
    }
  }
  else if (lcdMode == LCD_SPIN && now >= lcdNextSpin)
  {
    lcdNextSpin = now + 160;
    drawSpinFrame();
  }
}

// Contract: show the dish level on the LCD.
void showLevel(int pct)
{
  dishPercent = pct;
  refreshLcd();
}

// ---------------- Calibration ----------------
void startCalibration()
{
  if (rouletteActive)
  {
    Serial.println(F("Finish the roulette before calibrating"));
    return;
  }

  calActive = true;
  calStep = 0;
  lcdMode = LCD_CAL;
  lcdWriteLine(0, "Calibrate");
  lcdWriteLine(1, "T: set EMPTY");
  Serial.println(F("Calibration: empty dish, press Test"));
}

void captureCalibration()
{
  const float d = medianDistance();

  if (d >= 100)
  {
    Serial.println(F("No echo - check the ultrasonic sensor"));
    lcdWriteLine(1, "No echo! T retry");
    beep(60);
    return;
  }

  if (calStep == 0)
  {
    distEmpty = d;
    calStep = 1;
    lcdWriteLine(1, "T: set FULL");
    Serial.print(F("EMPTY set to "));
    Serial.print(d);
    Serial.println(F(" cm"));
    beep(60);
    return;
  }

  if (d > distEmpty - 2)
  {
    Serial.println(F("FULL must be 2+ cm closer than EMPTY - try again"));
    lcdWriteLine(1, "Too far! T retry");
    beep(60);
    return;
  }

  distFull = d;
  calActive = false;
  lcdMode = LCD_CAL_DONE;
  calDoneUntil = millis() + CAL_DONE_MS;
  calNextFlash = millis() + 300;
  calFlashOn = true;
  lcdWriteLine(0, "Calibration");
  lcdWriteLine(1, "complete!");
  Serial.print(F("FULL set to "));
  Serial.print(d);
  Serial.println(F(" cm"));
  Serial.println(F("Calibration complete"));
  beep(120);
}

void handleTestKey()
{
  if (calActive)
    captureCalibration();
  else
    startCalibration();
}

// Lantern: warm and bright when plenty is left, softens as the dish empties.
void lanternGlow(int pct, int pulse = 0)
{
  const int red = map(pct, 0, 100, 40, 255);
  const int green = map(pct, 0, 100, 15, 110);
  analogWrite(RGB_R, map(pulse, 0, 100, red, 255));
  analogWrite(RGB_G, map(pulse, 0, 100, green, 220));
  analogWrite(RGB_B, 0);
}

// ---------------- Shared-bite glow ----------------
const int LIFT_ADC = 420; // brighter: analog value falls
const int REST_ADC = 580; // darker: analog value rises
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

  // The roulette owns the RGB LED and servo while it is running.
  // Sensors still update above, so bowl lifts are not lost.
  if (rouletteActive)
    return;

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

  if (pulseActive && !dishClean)
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
const bool solomode = false;

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

// The roulette leaves the servo alone, so companion writes are muted while a
// spin or reveal is on screen.
void writeServo(int angle)
{
  if (!rouletteActive)
    companionServo.write(angle);
}

void companionNod()
{
  movement = 1;
  movementStarted = millis();
  lastNod = movementStarted;
  writeServo(110);
}

void beginCompanion()
{
  companionServo.attach(SERVO_PIN);
  writeServo(90);

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
  const int dinerCount = solomode ? 1 : 2;

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
      writeServo(65);
      Serial.println(F("Waiting for the table"));
    }
  }
  else if (waiting)
  {
    waiting = false;
    movement = 0;
    writeServo(90);
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
  const unsigned long elapsed = millis() - movementStarted;

  if (movement == 1 && elapsed >= 350)
  {
    writeServo(90);
    movement = 0;
  }

  if (movement == 2)
  {
    if (elapsed >= 1200)
    {
      writeServo(waiting ? 65 : 90);
      movement = 0;
    }
    else
    {
      writeServo((elapsed / 200) % 2 == 0 ? 75 : 105);
    }
  }

  if (waiting || movement != 0)
    return;

  // Solo mode mirrors lifts. Group mode uses the longest interval.
  if (soloMode)
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
    if (!solomode &&
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

// ---------------- Leftover Roulette ----------------
const unsigned long ROULETTE_SPIN_MS = 2400;
// Reveal the chores first and save the leftovers prize for the finale.
const unsigned long ROULETTE_REVEAL_MS[3] = {2000, 2000, 2600};

// Remote key codes from the Wokwi IR remote (NEC, address 0).
const uint8_t IR_CMD_PLUS = 2;
const uint8_t IR_CMD_MINUS = 152;
const uint8_t IR_CMD_PLAY = 168;
const uint8_t IR_CMD_TEST = 34;
const uint8_t IR_CMD_MENU = 226;
const uint8_t IR_NUMBER_CMD[MAX_PLAYERS] = {48, 24, 122, 16, 56};

uint8_t buildActiveList(uint8_t *out)
{
  uint8_t n = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; ++i)
  {
    if (playerOn[i])
      out[n++] = i;
  }
  return n;
}

void printRoster()
{
  Serial.print(F("Players: "));
  Serial.print(activeCount);
  Serial.print(F(" ("));

  bool first = true;
  for (uint8_t i = 0; i < MAX_PLAYERS; ++i)
  {
    if (!playerOn[i])
      continue;
    if (!first)
      Serial.print(' ');
    Serial.print('P');
    Serial.print(i + 1);
    first = false;
  }
  Serial.println(')');
}

void printPlayer(uint8_t slot)
{
  Serial.print('P');
  Serial.print(slot + 1);
}

void printImmunity()
{
  Serial.print(F("Immune: "));
  uint8_t n = 0;
  for (uint8_t i = 0; i < MAX_PLAYERS; ++i)
  {
    if (!playerOn[i] || !immune[i])
      continue;
    Serial.print('P');
    Serial.print(i + 1);
    Serial.print(' ');
    ++n;
  }
  if (n == 0)
    Serial.print(F("none"));
  Serial.println();
}

void enterImmunityMode()
{
  if (rouletteActive)
  {
    Serial.println(F("Finish the roulette first"));
    beep(40);
    return;
  }

  immunityMode = true;
  lcdMode = LCD_IMMUNITY;
  drawImmunityScreen();
  Serial.println(F("Immunity edit: 1-5 toggle, M exits"));
  printRoster();
  printImmunity();
}

void exitImmunityMode()
{
  immunityMode = false;
  lcdMode = LCD_DISH;
  lcdLastPct = -1;
  lcdLastCount = 0;
  refreshLcd();
  printImmunity();
}

void toggleImmunity(uint8_t slot)
{
  if (!playerOn[slot])
  {
    Serial.print(F("Player "));
    Serial.print(slot + 1);
    Serial.println(F(" is not active"));
    beep(40);
    return;
  }

  // At least one active player must stay non-immune, otherwise the duties
  // would have no eligible players.
  if (!immune[slot] && activeImmuneCount() + 1 >= activeCount)
  {
    Serial.println(F("Set failed: at least 1 player must stay non-immune"));
    beep(120);
    lcdMode = LCD_IMM_FAIL;
    immFailUntil = millis() + IMM_FAIL_MS;
    immFailNextFlash = millis() + 300;
    immFailFlashOn = true;
    lcdWriteLine(0, "Immunity denied");
    lcdWriteLine(1, "1 must stay free");
    return;
  }

  immune[slot] = !immune[slot];
  lcdMode = LCD_IMMUNITY; // a successful toggle cancels any fail flash
  drawImmunityScreen();
  Serial.print(F("Player "));
  Serial.print(slot + 1);
  Serial.println(immune[slot] ? F(" is immune") : F(" immunity cleared"));
}

void rosterUpdated()
{
  lcdMode = LCD_DISH;
  lcdLastPct = -1;
  lcdLastCount = 0;
  refreshLcd();
  printRoster();
}

// Keep at least one active player non-immune. If removing the last eligible
// player would leave the whole table immune, clear the highest active
// player's immunity instead.
void enforceNonImmune()
{
  if (activeCount == 0 || activeImmuneCount() < activeCount)
    return;

  for (int i = MAX_PLAYERS - 1; i >= 0; --i)
  {
    if (!playerOn[i])
      continue;
    immune[i] = false;
    Serial.print(F("P"));
    Serial.print(i + 1);
    Serial.println(F(" immunity cleared to keep one player eligible"));
    return;
  }
}

void addPlayer()
{
  if (activeCount >= MAX_PLAYERS)
  {
    Serial.println(F("Already at 5 players"));
    beep(50);
    return;
  }

  for (uint8_t i = 0; i < MAX_PLAYERS; ++i)
  {
    if (!playerOn[i])
    {
      playerOn[i] = true;
      ++activeCount;
      rosterUpdated();
      return;
    }
  }
}

void removePlayer()
{
  if (activeCount <= MIN_PLAYERS)
  {
    Serial.println(F("Need at least 2 players"));
    beep(50);
    return;
  }

  for (int i = MAX_PLAYERS - 1; i >= 0; --i)
  {
    if (playerOn[i])
    {
      playerOn[i] = false;
      immune[i] = false;
      --activeCount;
      enforceNonImmune();
      rosterUpdated();
      return;
    }
  }
}

void togglePlayer(uint8_t slot)
{
  if (playerOn[slot])
  {
    if (activeCount <= MIN_PLAYERS)
    {
      Serial.println(F("Need at least 2 players"));
      beep(50);
      return;
    }
    playerOn[slot] = false;
    immune[slot] = false;
    --activeCount;
    enforceNonImmune();
  }
  else
  {
    if (activeCount >= MAX_PLAYERS)
    {
      Serial.println(F("Already at 5 players"));
      beep(50);
      return;
    }
    playerOn[slot] = true;
    ++activeCount;
  }
  rosterUpdated();
}

void shufflePlayers(uint8_t *list, uint8_t count)
{
  for (uint8_t i = count; i > 1; --i)
  {
    const uint8_t j = (uint8_t)random(i);
    const uint8_t swap = list[i - 1];
    list[i - 1] = list[j];
    list[j] = swap;
  }
}

// The leftovers prize is a lottery for everyone, immune or not. Immunity only
// protects a player from the two duties. Duties are drawn from non-immune
// players, and the leftovers winner is skipped when enough others remain.
void chooseRouletteResult()
{
  uint8_t list[MAX_PLAYERS];
  const uint8_t n = buildActiveList(list);
  rouletteLeftovers = list[(uint8_t)random(n)];

  uint8_t eligible[MAX_PLAYERS];
  uint8_t e = 0;
  for (uint8_t i = 0; i < n; ++i)
  {
    if (!immune[list[i]])
      eligible[e++] = list[i];
  }

  if (e == 0)
  {
    // Safety net: the editor and roster keep one player non-immune, so this
    // should not happen. Waive immunity rather than leave the table stuck.
    for (uint8_t i = 0; i < n; ++i)
      eligible[i] = list[i];
    e = n;
    Serial.println(F("All players immune - immunity waived for this spin"));
  }

  if (n == 2)
  {
    if (e == 1)
    {
      // Only one eligible player, so both duties land on them.
      rouletteDish = eligible[0];
      rouletteCleanup = eligible[0];
    }
    else
    {
      rouletteDish = eligible[(uint8_t)random(e)];
      rouletteCleanup = eligible[(uint8_t)random(e)];
    }
    return;
  }

  uint8_t duty[MAX_PLAYERS];
  uint8_t dutyCount = 0;
  for (uint8_t i = 0; i < e; ++i)
  {
    if (eligible[i] != rouletteLeftovers)
      duty[dutyCount++] = eligible[i];
  }

  if (dutyCount == 0)
  {
    // The only eligible player won the leftovers, so they take both duties.
    duty[dutyCount++] = rouletteLeftovers;
  }

  shufflePlayers(duty, dutyCount);
  rouletteDish = duty[0];
  rouletteCleanup = dutyCount > 1 ? duty[1] : duty[0];
}

uint8_t rouletteRolePlayer(uint8_t phase)
{
  if (phase == 0)
    return rouletteDish;
  if (phase == 1)
    return rouletteCleanup;
  return rouletteLeftovers;
}

void setRouletteColor(int red, int green, int blue)
{
  analogWrite(RGB_R, red);
  analogWrite(RGB_G, green);
  analogWrite(RGB_B, blue);
}

void enterRevealPhase(uint8_t phase)
{
  rouletteState = ROULETTE_REVEAL;
  roulettePhase = phase;
  roulettePhaseStarted = millis();

  const uint8_t who = rouletteRolePlayer(phase);

  lcdMode = LCD_REVEAL;
  lcdWriteLine(0, phase == 0 ? "Dish duty" : phase == 1 ? "Cleanup duty"
                                                         : "Leftovers");
  char line[17];
  snprintf(line, sizeof(line), "Player P%d", who + 1);
  lcdWriteLine(1, line);

  Serial.print(phase == 0 ? F("Dish duty: ") : phase == 1 ? F("Cleanup duty: ")
                                                           : F("Leftovers: "));
  printPlayer(who);
  Serial.println();

  if (phase == 0)
  {
    setRouletteColor(0, 80, 255); // blue duty
    beep(70);
  }
  else if (phase == 1)
  {
    setRouletteColor(0, 200, 70); // green duty
    beep(70);
  }
  else
  {
    setRouletteColor(255, 170, 0); // gold prize
    beep(120);
  }
}

void finishRoulette()
{
  rouletteState = ROULETTE_IDLE;
  rouletteActive = false;
  // The roulette never moved the servo. Drop any nod it interrupted so the
  // companion cannot finish a stale movement after the result appears.
  movement = 0;

  lcdMode = LCD_SUMMARY;
  lcdSummaryUntil = millis() + 8000;
  char line[17];
  snprintf(line, sizeof(line), "Dish P%d Clean P%d",
           rouletteDish + 1, rouletteCleanup + 1);
  lcdWriteLine(0, line);
  snprintf(line, sizeof(line), "Leftovers P%d", rouletteLeftovers + 1);
  lcdWriteLine(1, line);

  Serial.println(F("Roulette done"));
}

void spinRoulette()
{
  if (calActive)
    return;

  // Ignore presses during the roll, but allow a re-spin from the reveal.
  if (rouletteActive && rouletteState == ROULETTE_SPIN)
    return;

  chooseRouletteResult();

  rouletteState = ROULETTE_SPIN;
  rouletteActive = true;
  rouletteFrame = 0;
  roulettePhase = 0;
  rouletteStarted = millis();
  rouletteNextFrame = rouletteStarted;

  lcdMode = LCD_SPIN;
  lcdSpinFrame = 0;
  lcdNextSpin = 0;
  lcdWriteLine(0, "Spinning...");
  lcdWriteLine(1, "");

  Serial.println(F("Roulette: spin"));
  printRoster();
  printImmunity();
}

void handleRemoteCommand(uint8_t command)
{
  if (immunityMode)
  {
    if (command == IR_CMD_MENU)
    {
      exitImmunityMode();
      return;
    }

    for (uint8_t i = 0; i < MAX_PLAYERS; ++i)
    {
      if (command == IR_NUMBER_CMD[i])
      {
        toggleImmunity(i);
        return;
      }
    }
    return; // ignore other keys while editing immunity
  }

  if (command == IR_CMD_TEST)
  {
    handleTestKey();
    return;
  }

  if (calActive)
    return;

  if (command == IR_CMD_PLAY)
  {
    spinRoulette();
    return;
  }

  if (command == IR_CMD_MENU)
  {
    enterImmunityMode();
    return;
  }

  if (rouletteActive)
  {
    Serial.println(F("Roster locked during the roulette"));
    beep(40);
    return;
  }

  if (command == IR_CMD_PLUS)
  {
    addPlayer();
    return;
  }

  if (command == IR_CMD_MINUS)
  {
    removePlayer();
    return;
  }

  for (uint8_t i = 0; i < MAX_PLAYERS; ++i)
  {
    if (command == IR_NUMBER_CMD[i])
    {
      togglePlayer(i);
      return;
    }
  }
}
void pollSerialRemote()
{
  if (Serial.available() == 0)
    return;

  const int key = Serial.read();

  if (key >= '1' && key <= '5')
  {
    handleRemoteCommand(IR_NUMBER_CMD[key - '1']);
    return;
  }

  switch (key)
  {
    case 'p':
    case 'P':
      handleRemoteCommand(IR_CMD_PLAY);
      break;

    case 't':
    case 'T':
      handleRemoteCommand(IR_CMD_TEST);
      break;

    case 'm':
    case 'M':
      handleRemoteCommand(IR_CMD_MENU);
      break;

    case '+':
      handleRemoteCommand(IR_CMD_PLUS);
      break;

    case '-':
      handleRemoteCommand(IR_CMD_MINUS);
      break;
  }
}
void pollRemote()
{
  if (!TinyIRReceiverDecode())
    return;

  const uint8_t command = TinyIRReceiverData.Command;
  const uint8_t flags = TinyIRReceiverData.Flags;

  if (flags & IRDATA_FLAGS_IS_REPEAT)
    return;

  Serial.print(F("IR: "));
  Serial.println(command);
  handleRemoteCommand(command);
}

void updateRoulette()
{
  pollRemote();
  pollSerialRemote();

  if (!rouletteActive)
    return;

  const unsigned long now = millis();

  if (rouletteState == ROULETTE_SPIN)
  {
    if (now - rouletteNextFrame >= 80)
    {
      rouletteNextFrame = now;
      ++rouletteFrame;

      // Colour chase on the RGB LED. The servo stays still during the roll.
      switch ((rouletteFrame / 3) % 6)
      {
      case 0:
        setRouletteColor(255, 0, 0);
        break;
      case 1:
        setRouletteColor(255, 120, 0);
        break;
      case 2:
        setRouletteColor(255, 220, 0);
        break;
      case 3:
        setRouletteColor(0, 220, 80);
        break;
      case 4:
        setRouletteColor(0, 80, 255);
        break;
      default:
        setRouletteColor(160, 0, 255);
        break;
      }

      if (rouletteFrame % 2 == 0)
        beep(6);
    }

    if (now - rouletteStarted >= ROULETTE_SPIN_MS)
      enterRevealPhase(0);
    return;
  }

  if (rouletteState == ROULETTE_REVEAL &&
      now - roulettePhaseStarted >= ROULETTE_REVEAL_MS[roulettePhase])
  {
    if (roulettePhase >= 2)
      finishRoulette();
    else
      enterRevealPhase(roulettePhase + 1);
  }
}

// ---------------- Arduino ----------------
void setup()
{
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(SOLO_SWITCH, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.clear();

  soloMode = digitalRead(SOLO_SWITCH) == LOW;

  for (int i = 0; i < 2; ++i)
  {
    pinMode(BOWL_PIN[i], INPUT);
    const bool lifted = analogRead(BOWL_PIN[i]) <= LIFT_ADC;
    bowls[i] = {lifted, lifted, millis(), false, 0};
  }

  randomSeed(((unsigned long)analogRead(A0) << 16) ^
             ((unsigned long)analogRead(A1) << 8) ^ micros());

  Serial.begin(9600);

  if (!initPCIInterruptForTinyIRReceiver())
    Serial.println(F("IR interrupt unavailable"));
  else
    Serial.println(F("IR ready"));

  refreshLcd();

  beginCompanion();
  Serial.println(F("Dish-level lantern ready"));
  printRoster();
  printImmunity();
}

void loop()
{
  updateRoulette();

  // While the roulette owns the outputs, keep polling the bowl sensors but
  // skip the blocking ultrasonic read so the animation stays smooth.
  if (rouletteActive)
  {
    refreshLcd();
    serviceSharedGlow();
    delay(1);
    return;
  }

  updateCompanion();

  const int pct = readDishLevel();
  showLevel(pct);
  serviceSharedGlow();

  if (pct <= CLEAN_PCT && !celebrated)
  {
    chime();
    celebrated = true;
  }
  if (pct > REARM_PCT)
    celebrated = false; // re-arm after a refill

  Serial.print(F("level%: "));
  Serial.println(pct);
  waitWithGlow(150);
}
