#include "GyverEncoder.h"
#include "GyverTM1637.h"

// ===== ПИНЫ =====
#define CLK 12
#define DIO 11

#define ENC_CLK 8
#define ENC_DT 9
#define ENC_SW 10

#define MODE_SWITCH 4
#define MOSFET_PIN 3
#define BUZZ_PIN 7

// ===== НАСТРОЙКИ =====
#define BUZZER_ENABLED 0   // 🔊 0 = звук ВЫКЛ, 1 = звук ВКЛ

#define DAWN_DURATION 20   // минут рассвета
#define TIME_SPEED 60000   // 1 минута = 60 секунд

// ===== ВРЕМЯ =====
int hrs = 12;
int mins = 0;

int alm_hrs = 8;
int alm_mins = 0;

// ===== ОБЪЕКТЫ =====
GyverTM1637 disp(CLK, DIO);
Encoder enc(ENC_CLK, ENC_DT, ENC_SW);

// ===== ПЕРЕМЕННЫЕ =====
bool isAlarmMode = false;
bool alarmTriggered = false;

unsigned long timer = 0;

// рассвет
unsigned long dawnTimer = 0;
int brightness = 0;

// энкодер
int lastDir = 0;

// ===== SETUP =====
void setup() {
  pinMode(MOSFET_PIN, OUTPUT);
  analogWrite(MOSFET_PIN, 0);

  pinMode(MODE_SWITCH, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT);

  disp.clear();
  disp.brightness(7);

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  enc.setType(TYPE2);
  enc.setTickMode(MANUAL);
}

// ===== LOOP =====
void loop() {
  enc.tick();

  isAlarmMode = !digitalRead(MODE_SWITCH);

  handleEncoder();
  updateTime();
  checkAlarm();

  delay(10);
}

// ===== ЭНКОДЕР =====
void handleEncoder() {
  int dir = 0;

  if (enc.isRight()) dir = 1;
  if (enc.isLeft()) dir = -1;

  // анти-дребезг: сброс направления если нет вращения
  if (dir == 0) {
    lastDir = 0;
  }
  
  if (isAlarmMode) {
    alm_mins += dir;

    if (enc.isRightH()) alm_hrs++;
    if (enc.isLeftH()) alm_hrs--;

    normalizeTime(alm_hrs, alm_mins);
    disp.displayClock(alm_hrs, alm_mins);

  } else {
    mins += dir;

    if (enc.isRightH()) hrs++;
    if (enc.isLeftH()) hrs--;

    normalizeTime(hrs, mins);
    disp.displayClock(hrs, mins);
  }
}

// ===== ВРЕМЯ =====
void updateTime() {
  if (millis() - timer >= TIME_SPEED) {
    timer = millis();
    mins++;
    normalizeTime(hrs, mins);
  }
}

// ===== БУДИЛЬНИК =====
void checkAlarm() {
  int now = hrs * 60 + mins;
  int alarmTime = alm_hrs * 60 + alm_mins;

  int dawnStart = alarmTime - DAWN_DURATION;
  if (dawnStart < 0) dawnStart += 1440;

  bool inDawn;
  if (dawnStart < alarmTime) {
    inDawn = (now >= dawnStart && now < alarmTime);
  } else {
    inDawn = (now >= dawnStart || now < alarmTime);
  }

// 🌅 РАССВЕТ
if (inDawn) {
  int dawnProgress;

  if (dawnStart < alarmTime) {
    dawnProgress = now - dawnStart;
  } else {
    if (now >= dawnStart)
      dawnProgress = now - dawnStart;
    else
      dawnProgress = (1440 - dawnStart) + now;
  }

  // переводим минуты в миллисекунды внутри минуты
  float minuteFraction = (millis() % TIME_SPEED) / (float)TIME_SPEED;

  float totalProgress = dawnProgress + minuteFraction;

  float k = totalProgress / DAWN_DURATION;
  if (k > 1.0) k = 1.0;

  brightness = (int)(k * 255);

  analogWrite(MOSFET_PIN, brightness);
}

  // ⏰ БУДИЛЬНИК (ОДИН РАЗ)
  if (now == alarmTime && !alarmTriggered) {
    if (brightness < 255) {
    brightness += 2; // плавное добивание до максимума
    if (brightness > 255) brightness = 255;
    analogWrite(MOSFET_PIN, brightness);
  }
    // 🔊 ЗВУК (если включен)
    if (BUZZER_ENABLED) {
      tone(BUZZ_PIN, 1000);
    }

    alarmTriggered = true;
  }

  // сброс через минуту
  if (now != alarmTime && alarmTriggered) {
    if (now == (alarmTime + 1) % 1440) {
      alarmTriggered = false;

      if (BUZZER_ENABLED) {
        noTone(BUZZ_PIN);
      }
    }
  }

  // 😴 остальное время
  if (!inDawn && now != alarmTime) {
    if (brightness > 0) {
      brightness--;
      analogWrite(MOSFET_PIN, brightness);
      delay(5);
    } else {
      analogWrite(MOSFET_PIN, 0);
    }
  }
}

// ===== НОРМАЛИЗАЦИЯ =====
void normalizeTime(int &h, int &m) {
  while (m > 59) {
    m -= 60;
    h++;
  }
  while (m < 0) {
    m += 60;
    h--;
  }

  while (h > 23) h -= 24;
  while (h < 0) h += 24;
}