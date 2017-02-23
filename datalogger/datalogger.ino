#include "SD.h"
#include <Wire.h>
#include "RTClib.h"
#include "UBX.h"
#include "states.h"
#include "gps.h"
#include <U8g2lib.h>

#define DEBUG Serial

#if defined(__AVR_ATmega2560__)
  #define GPS Serial1
  bool debug = true;
#else
  #define GPS Serial
  bool debug = false;
#endif

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0);

State currentState = FIXING;

int inputs[] = { A0,A1,A2,A3,A4,A5,A6,A7,A8,A9 };

int sdCardPin = 53;
bool sdCardInitialized = false;

File logFile;

const uint16_t VALUE_COUNT = 10;

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

int drawInterval = 0;
int drawCounter = 0;

char fixingStatus[]   = "FIXING              ";
char fixedStatus[]    = "FIXED               ";
char loggingStatus[]  = "LOGGING             ";

Gps gps;
long lastPrintMillis;

void setup() {
  display.begin();
  gps.setup();

  Serial3.begin(9600);

  Serial3.write(0xAC);
  Serial3.write(0xDC);
  Serial3.write(0xFC);
  Serial3.write(0x03);
  Serial3.write(0x01);

  delay(10);

  Serial3.write(0xAC);
  Serial3.write(0xDC);
  Serial3.write(0xFC);
  Serial3.write(0x02);
  Serial3.write(fixingStatus);
  
  while (!gps.hasTimeFix())
  {
    if (millis() > 6000) //abort if this takes more than 1 min
      break;

    gps.update();
  }

  if (gps.hasTimeFix())
  {
    Serial3.write(0xAC);
    Serial3.write(0xDC);
    Serial3.write(0xFC);
    Serial3.write(0x02);
    Serial3.write(fixedStatus);
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
  Serial3.write(0xAC);
  Serial3.write(0xDC);
  Serial3.write(0xFC);
  Serial3.write(0x03);
  Serial3.write(0x02);
  lastPrintMillis = millis();
}

int t = 2;


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

  if (lastPrintMillis + 50 < millis())
  {
    lastPrintMillis = millis();
    Serial3.write(0xAC);
    Serial3.write(0xDC);
    Serial3.write(0xFC);
    Serial3.write(0x01);

    for (int i = 0; i < VALUE_COUNT; i++)
    {
      Serial3.write(line.values[i]);
    }
  }

  delay(4);
}
