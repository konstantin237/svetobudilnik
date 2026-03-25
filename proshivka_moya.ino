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
#define BUZZ_ENABLE 1
#define DAWN_DURATION 20   // минут

#define TIME_SPEED 60000   // 1 минута = 60 сек

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

// для плавного рассвета
unsigned long dawnTimer = 0;
int brightness = 0;

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
  enc.setTickMode(AUTO);
}

// ===== LOOP =====
void loop() {
  enc.tick();

  isAlarmMode = !digitalRead(MODE_SWITCH);

  // ===== НАСТРОЙКА =====
  if (isAlarmMode) {
    if (enc.isRight()) alm_mins++;
    if (enc.isLeft()) alm_mins--;

    if (enc.isRightH()) alm_hrs++;
    if (enc.isLeftH()) alm_hrs--;

    normalizeTime(alm_hrs, alm_mins);
    disp.displayClock(alm_hrs, alm_mins);

  } else {
    if (enc.isRight()) mins++;
    if (enc.isLeft()) mins--;

    if (enc.isRightH()) hrs++;
    if (enc.isLeftH()) hrs--;

    normalizeTime(hrs, mins);
    disp.displayClock(hrs, mins);
  }

  // ===== ХОД ВРЕМЕНИ =====
  if (millis() - timer >= TIME_SPEED) {
    timer = millis();
    mins++;
    normalizeTime(hrs, mins);
  }

  checkAlarm();

  delay(50);
}

// ===== БУДИЛЬНИК =====
void checkAlarm() {
  int now = hrs * 60 + mins;
  int alarmTime = alm_hrs * 60 + alm_mins;

  int dawnStart = alarmTime - DAWN_DURATION;
  if (dawnStart < 0) dawnStart += 1440;

  // проверка попадания в интервал (с учётом перехода через 00:00)
  bool inDawn;
  if (dawnStart < alarmTime) {
    inDawn = (now >= dawnStart && now < alarmTime);
  } else {
    inDawn = (now >= dawnStart || now < alarmTime);
  }

  // 🌅 РАССВЕТ
  if (inDawn) {
    if (millis() - dawnTimer > (DAWN_DURATION * 60000UL / 255)) {
      dawnTimer = millis();
      brightness++;
      if (brightness > 255) brightness = 255;
      analogWrite(MOSFET_PIN, brightness);
    }
  }

  // ⏰ БУДИЛЬНИК
  else if (now == alarmTime && !alarmTriggered) {
    analogWrite(MOSFET_PIN, 255);

    if (BUZZ_ENABLE) {
      tone(BUZZ_PIN, 1000);
    }

    alarmTriggered = true;
  }

  // сброс флага
  if (now != alarmTime) {
    alarmTriggered = false;
  }

  // 😴 остальное время
  if (!inDawn && now != alarmTime) {
    brightness = 0;
    analogWrite(MOSFET_PIN, 0);
    noTone(BUZZ_PIN);
  }
}

// ===== НОРМАЛИЗАЦИЯ =====
void normalizeTime(int &h, int &m) {
  if (m > 59) {
    m = 0;
    h++;
  }
  if (m < 0) {
    m = 59;
    h--;
  }
  if (h > 23) h = 0;
  if (h < 0) h = 23;
}