#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

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
#define BUZZER_ENABLED 1 //0 - пищалка выключена, 1 - пищалка включена
#define DAWN_DURATION 20 // (минут) это за сколько минут ДО будильника лента начнёт разгораться

RTC_DS3231 rtc;

// ===== ВРЕМЯ =====
int hrs;
int mins;

int alm_hrs;
int alm_mins;

// ===== EEPROM =====
#define EEPROM_ALM_H 0
#define EEPROM_ALM_M 1

GyverTM1637 disp(CLK, DIO);
Encoder enc(ENC_CLK, ENC_DT, ENC_SW);

// ===== ПЕРЕМЕННЫЕ =====
bool isAlarmMode = false;
bool alarmTriggered = false;

int brightness = 0;

// ===== SETUP =====
void setup() {

  Wire.begin();
  rtc.begin();

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

  // текущее время
  DateTime now = rtc.now();
  hrs = now.hour();
  mins = now.minute();

  // будильник
  alm_hrs = EEPROM.read(EEPROM_ALM_H);
  alm_mins = EEPROM.read(EEPROM_ALM_M);

  if (alm_hrs > 23) alm_hrs = 8;
  if (alm_mins > 59) alm_mins = 0;

  disp.displayClock(hrs, mins);
}

// ===== LOOP =====
void loop() {

  enc.tick();

  isAlarmMode = !digitalRead(MODE_SWITCH);

  handleEncoder();
  updateTime();
  checkAlarm();

  // выключение будильника кнопкой
  if (enc.isClick()) {

    if (alarmTriggered) {

      noTone(BUZZ_PIN);
      analogWrite(MOSFET_PIN, 0);

      brightness = 0;
      alarmTriggered = false;
    }
  }

  delay(5);
}

// ===== ЭНКОДЕР =====
void handleEncoder() {

  int dir = 0;

  if (enc.isRight()) dir = 1;
  if (enc.isLeft()) dir = -1;

  if (isAlarmMode) {

    alm_mins += dir;

    if (enc.isRightH()) alm_hrs++;
    if (enc.isLeftH()) alm_hrs--;

    normalizeTime(alm_hrs, alm_mins);

    EEPROM.update(EEPROM_ALM_H, alm_hrs);
    EEPROM.update(EEPROM_ALM_M, alm_mins);

    disp.displayClock(alm_hrs, alm_mins);
  }

  else {

    if (dir != 0 || enc.isRightH() || enc.isLeftH()) {

      mins += dir;

      if (enc.isRightH()) hrs++;
      if (enc.isLeftH()) hrs--;

      normalizeTime(hrs, mins);

      DateTime now = rtc.now();

      rtc.adjust(DateTime(
        now.year(),
        now.month(),
        now.day(),
        hrs,
        mins,
        now.second()
      ));
    }

    disp.displayClock(hrs, mins);
  }
}

// ===== ВРЕМЯ =====
void updateTime() {

  DateTime now = rtc.now();

  hrs = now.hour();
  mins = now.minute();

  if (!isAlarmMode)
    disp.displayClock(hrs, mins);
}

// ===== БУДИЛЬНИК =====
void checkAlarm() {

  int now = hrs * 60 + mins;
  int alarmTime = alm_hrs * 60 + alm_mins;

  int dawnStart = alarmTime - DAWN_DURATION;
  if (dawnStart < 0) dawnStart += 1440;

  bool inDawn;

  if (dawnStart < alarmTime)
    inDawn = (now >= dawnStart && now < alarmTime);
  else
    inDawn = (now >= dawnStart || now < alarmTime);

  // рассвет
  if (inDawn) {

    int dawnProgress;

    if (dawnStart < alarmTime)
      dawnProgress = now - dawnStart;
    else {

      if (now >= dawnStart)
        dawnProgress = now - dawnStart;
      else
        dawnProgress = (1440 - dawnStart) + now;
    }

    float k = (float)dawnProgress / DAWN_DURATION;

    if (k > 1) k = 1;

    brightness = k * 255;

    analogWrite(MOSFET_PIN, brightness);
  }

  // будильник
  if (now == alarmTime && !alarmTriggered) {

    analogWrite(MOSFET_PIN, 255);

    if (BUZZER_ENABLED)
      tone(BUZZ_PIN, 1000);

    alarmTriggered = true;
  }

  // сброс
  if (now != alarmTime && alarmTriggered) {

    if (now == (alarmTime + 1) % 1440) {

      alarmTriggered = false;

      if (BUZZER_ENABLED)
        noTone(BUZZ_PIN);
    }
  }

  // ночь
  if (!inDawn && now != alarmTime) {

    if (brightness > 0) {

      brightness--;
      analogWrite(MOSFET_PIN, brightness);
      delay(5);
    }
    else
      analogWrite(MOSFET_PIN, 0);
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