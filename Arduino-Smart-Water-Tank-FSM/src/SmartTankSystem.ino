#include <LiquidCrystal.h>

LiquidCrystal lcd(13, 11, 7, 6, 4, 3);

const int TRIG_PIN   = 9;
const int ECHO_PIN   = 10;
const int MOTOR_PIN  = 5;
const int BUZZER_PIN = 8;
const int BTN_PIN    = 2;

const unsigned long CYCLE_MS = 120000;
const unsigned long ALLOW_MS = 30000;

const float DIST_FULL  = 5.0;
const float DIST_EMPTY = 30.0;

const int LOW_LEVEL  = 30;
const int HIGH_LEVEL = 90;

unsigned long lastCheck = 0;
int lastLevel = 0;
int failCount = 0;

bool forcedMode = false;
bool lastBtn = HIGH;

enum State { IDLE, WAITING, AUTO_FILL, FORCED_FILL, SAFETY_STOP };
State state = IDLE;

long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;
  return (long)(duration * 0.034 / 2);
}

int levelPercent(long d) {
  if (d < 0) return -1;
  if (d < DIST_FULL)  d = (long)DIST_FULL;
  if (d > DIST_EMPTY) d = (long)DIST_EMPTY;

  float pct = (DIST_EMPTY - d) * 100.0 / (DIST_EMPTY - DIST_FULL);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (int)(pct + 0.5);
}

bool allowedNow() {
  return (millis() % CYCLE_MS) < ALLOW_MS;
}

void pumpOn()  { digitalWrite(MOTOR_PIN, HIGH); }
void pumpOff() { digitalWrite(MOTOR_PIN, LOW);  }

void beepShort() {
  tone(BUZZER_PIN, 1200, 120);
  delay(140);
}

void showLCD(int lvl, bool allowed) {
  lcd.setCursor(0, 0);
  lcd.print("Lvl:");
  if (lvl < 0) lcd.print("--");
  else {
    if (lvl < 100) lcd.print(" ");
    if (lvl < 10)  lcd.print(" ");
    lcd.print(lvl);
  }
  lcd.print("% ");
  lcd.print(forcedMode ? "F" : "A");
  lcd.print(allowed ? " ON " : " OFF");

  lcd.setCursor(0, 1);
  switch (state) {
    case IDLE:        lcd.print("IDLE            "); break;
    case WAITING:     lcd.print("WAIT SCHEDULE   "); break;
    case AUTO_FILL:   lcd.print("AUTO FILLING    "); break;
    case FORCED_FILL: lcd.print("FORCED FILLING  "); break;
    case SAFETY_STOP: lcd.print("SAFETY STOP!    "); break;
  }
}

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);

  pumpOff();
  noTone(BUZZER_PIN);

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Smart Tank Sys");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(900);
  lcd.clear();
}

void loop() {
  bool btn = digitalRead(BTN_PIN);
  if (lastBtn == HIGH && btn == LOW) {
    forcedMode = !forcedMode;
    beepShort();
    beepShort();
    delay(200);
  }
  lastBtn = btn;

  long dist = readDistanceCM();
  int lvl = levelPercent(dist);
  bool allowed = allowedNow();

  bool needsFill = (lvl >= 0 && lvl < LOW_LEVEL);
  bool isFull    = (lvl >= HIGH_LEVEL);

  if (state == SAFETY_STOP) {
    pumpOff();
    tone(BUZZER_PIN, 1700, 250);
    showLCD(lvl, allowed);
    delay(250);
    return;
  }

  if (isFull) {
    pumpOff();
    state = IDLE;
  } else if (forcedMode && needsFill) {
    pumpOn();
    if (state != FORCED_FILL) {
      state = FORCED_FILL;
      lastLevel = lvl;
      lastCheck = millis();
      failCount = 0;
    }
  } else if (!forcedMode && needsFill && allowed) {
    pumpOn();
    if (state != AUTO_FILL) {
      state = AUTO_FILL;
      lastLevel = lvl;
      lastCheck = millis();
      failCount = 0;
    }
  } else if (needsFill && !forcedMode && !allowed) {
    pumpOff();
    state = WAITING;
  } else {
    pumpOff();
    state = IDLE;
  }

  bool pumpRunning = (state == AUTO_FILL || state == FORCED_FILL);
  if (pumpRunning) {
    if (millis() - lastCheck > 8000) {
      if (lvl <= lastLevel) failCount++;
      else failCount = 0;

      lastLevel = lvl;
      lastCheck = millis();

      if (failCount >= 2) {
        state = SAFETY_STOP;
        pumpOff();
      }
    }
  } else {
    failCount = 0;
    lastLevel = lvl;
    lastCheck = millis();
  }

  showLCD(lvl, allowed);
  delay(200);
}
