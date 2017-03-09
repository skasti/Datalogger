#include "SD.h"
#include <Wire.h>
#include "RTClib.h"
#include "UBX.h"
#include "states.h"
#include "gps.h"
#include <U8g2lib.h>
#include "nanogpu-client.h"
#include "SparkFunMLX90614.h"

#define DEBUG Serial

#if defined(__AVR_ATmega2560__)
  #define GPS Serial1
  bool debug = true;
#else
  #define GPS Serial
  bool debug = false;
#endif

State currentState = FIXING;

int inputs[] = { A0,A1,A2,A3,A4,A5,A6,A7,A8,A9 };

int sdCardPin = 53;
bool sdCardInitialized = false;

File logFile;

const uint16_t VALUE_COUNT = 10;

struct LogLine
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

LogLine line;

NAV_PVT     pvt;

int flushInterval = 5000;
int flushCounter = 0;

int drawInterval = 1000;
int drawCounter = 0;

unsigned char fixingStatus[]   = "FIXING              ";
unsigned char fixedStatus[]    = "FIXED               ";
unsigned char loggingStatus[]  = "LOGGING             ";
unsigned char addressStatus[]  = "SET ADDR            ";
unsigned char failedStatus[]   = "FAILED              ";
unsigned char initIRStatus[]   = "INIT IR             ";

Gps gps;
NanoGpuClient gpuClient;
uint8_t irIds[] = {0x10,0x11,0x12,0x13,0x14,0x15};
IRTherm ir[6];
int16_t temp[6] = {0,0,0,0,0,0};
bool irEnabled[6];
int currentIR = 0;

bool statusLogging = false;
int t = 2;

void setup() {
  gps.setup();
  gpuClient.setup();
  Serial.begin(9600);
  
  gpuClient.sendMode(STATUSTEXT);
  delay(10);  
  gpuClient.sendStatus(initIRStatus);

  for (int i = 0; i < 6; i++) {
    ir[i].begin(irIds[i]);
    ir[i].setUnit(TEMP_C);

    if (ir[i].read())
    {
      irEnabled[i] = true;
    }
  }

  for (int i = 0; i < 10; i++) {
    pinMode(inputs[i], INPUT);  
  }
  
  pinMode(sdCardPin, OUTPUT);
  sdCardInitialized = SD.begin(sdCardPin);

  gpuClient.sendStatus(fixingStatus);
  while (!gps.hasTimeFix())
  {
    if (millis() > 12000) //abort if this takes more than 1 min
      break;

    gps.update();
  }

  if (!gps.hasTimeFix()) {
    gpuClient.sendStatus(failedStatus);
    delay(1000);
  }

  DateTime now = DateTime(pvt.year, pvt.month, pvt.day, pvt.hour, pvt.min, pvt.sec);

  uint32_t ms = micros();
  uint32_t unixTime = now.unixtime();

  String date = String(now.month())+"-";
  date = date + now.day();
  
  String extension = ".log";
  
  int counter = 0;
  
  String filename = date + "-" + counter;
  
  while (SD.exists(filename + extension)) {
    counter++;
    filename = date + "-" + counter;
  }

  gpuClient.sendStatus(loggingStatus);

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

void updateIRTemps()
{
  if (irEnabled[currentIR] && ir[currentIR].read()) {
    temp[currentIR] = ir[currentIR].object() * 10;
  }  

  currentIR++;

  if (currentIR > 5)
    currentIR = 0;
}

bool flush()
{
  flushCounter++;

  if (flushCounter > flushInterval)
  {
    flushCounter = 0;
    logFile.flush();
    return true;
  }

  return false;
}

bool draw()
{
  drawCounter++;

  if (drawCounter > drawInterval)
  {
    if (!statusLogging)
    {
      drawInterval = 50;
      statusLogging = true;
      gpuClient.sendMode(VALUES);
    }

    drawCounter = 0;
    gpuClient.sendValues(line.values);
    return true;
  }

  return false;
}

void loop() {  
  gps.update();
  pvt = gps.getLatest();

  unsigned long ms = micros();
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
    if (i < 6)
    {
      line.values[i] = temp[i];
    } else {
      line.values[i] = analogRead(inputs[i]);
    }    
  }

  logFile.write((const uint8_t *)&line, sizeof(line));

  if (!flush() && !draw()) {
    updateIRTemps();
  } 

  delay(3);
}
