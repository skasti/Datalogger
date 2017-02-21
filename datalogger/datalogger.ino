#include "SD.h"
#include <Wire.h>
#include "RTClib.h"
#include "UBX.h"
#include "states.h"
#include "gps.h"

#define DEBUG Serial

#if defined(__AVR_ATmega2560__)
  #define GPS Serial1
  bool debug = true;
#else
  #define GPS Serial
  bool debug = false;
#endif

State currentState = FIXING;

int inputs[] = { A0,A1,A2,A3,A6,A7 };

int sdCardPin = 53;
bool sdCardInitialized = false;

File logFile;

const uint16_t VALUE_COUNT = 6;

struct logLine
{
    uint32_t        micros;
    uint16_t        speed;
    uint16_t        sAcc; //Speed accuracy extimate
    long            lon;
    long            lat;
    long            alt;
    unsigned long   hAcc; //Horizontal accuracy estimate
    unsigned long   vAcc; //Vertical accuracy estimate
    uint8_t         fixType; //Position Fix Type
    uint16_t        values[VALUE_COUNT];
};

NAV_PVT     pvt;

int flushInterval = 5000;
int flushCounter = 0;

Gps gps;

void setup() {
  gps.setup();
  
  while (!gps.hasTimeFix())
  {
    if (millis() > 60000) //abort if this takes more than 1 min
      break;

    gps.update();
  }

  DateTime now = DateTime(pvt.year, pvt.month, pvt.day, pvt.hour, pvt.min, pvt.sec);

  uint32_t ms = micros();
  uint32_t unixTime = now.unixtime();

  for (int i = 0; i < 6; i++) {
    pinMode(inputs[i], INPUT);  
  }
  
  pinMode(sdCardPin, OUTPUT);
  sdCardInitialized = SD.begin(sdCardPin);

  String date = String(now.month())+"-";
  date = date + now.day();
  
  String extension = ".log";
  
  int counter = 0;
  
  String filename = date + "-" + counter;
  
  while (SD.exists(filename + extension)) {
    counter++;
    filename = date + "-" + counter;
  }

  if (sdCardInitialized) {
    logFile = SD.open(filename + extension,FILE_WRITE);

    if (logFile) {
      logFile.write((const uint8_t *)&ms, sizeof(ms));
      logFile.write((const uint8_t *)&unixTime, sizeof(unixTime));
      logFile.write((const uint8_t *)&VALUE_COUNT, sizeof(VALUE_COUNT));
      logFile.flush();
    } else {

    }
  }
}

void loop() {  
  gps.update();
  pvt = gps.getLatest();

  unsigned long ms = micros();
  
  struct logLine line;
  line.micros = ms;

  line.speed = pvt.gSpeed;
  line.sAcc = pvt.sAcc;
  line.lon = pvt.lon;
  line.lat = pvt.lat;
  line.alt = pvt.alt;
  line.hAcc = pvt.hAcc;
  line.vAcc = pvt.vAcc;
  line.fixType = pvt.fixType;

  for (int i = 0; i < VALUE_COUNT; i++) {
    line.values[i] = analogRead(inputs[i]);
  }

  logFile.write((const uint8_t *)&line, sizeof(line));

  flushCounter++;

  if (flushCounter > flushInterval)
  {
    flushCounter = 0;
    logFile.flush();
  }

  delay(4);
}
