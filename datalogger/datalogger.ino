#include "SD.h"
#include <Wire.h>
#include "RTClib.h"
#include "UBX.h"
#include "states.h"
#include "gps.h"
#include "nanogpu-client.h"
#include "SparkFunMLX90614.h"
#include "menu.h"

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

int upPin = 24;
int downPin = 26;
int enterPin = 28;

File logFile;

const uint16_t VALUE_COUNT = 14;

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

int drawInterval = 20;
int drawCounter = 0;

Gps gps;
NanoGpuClient gpuClient;
uint8_t irIds[] = {0x10,0x11,0x12,0x13,0x14,0x15};
IRTherm ir[6];
int16_t temp[6] = {0,0,0,0,0,0};
bool irEnabled[6] = {false, false, false, false, false, false};
int currentIR = 0;

bool isLogging = false;
bool isMenu = true;
bool isCalibrating = false;
int calibrateIndex = 0xFF;

int t = 2;

MenuItem menuItems[] = {
  MenuItem(0x1000, 0x0000, 0x1000, "Start Log"),
    MenuItem(0x1100, 0x1000, 0x0000, "OK"),
    MenuItem(0x1200, 0x1000, 0x0000, "Abort"),
  MenuItem(0x2000, 0x0000, 0x2000, "Calibrate"),
    MenuItem(0x2100, 0x2000, 0x2100, "Channel"),
      MenuItem(0x2110, 0x2100, 0x2000, "Ch 1"),
      MenuItem(0x2120, 0x2100, 0x2000, "Ch 2"),
      MenuItem(0x2130, 0x2100, 0x2000, "Ch 3"),
      MenuItem(0x2140, 0x2100, 0x2000, "Ch 4"),
      MenuItem(0x2150, 0x2100, 0x2000, "Ch 5"),
      MenuItem(0x2160, 0x2100, 0x2000, "Ch 6"),
      MenuItem(0x2170, 0x2100, 0x2000, "Ch 7"),
      MenuItem(0x2180, 0x2100, 0x2000, "Ch 8"),
    MenuItem(0x2200, 0x2000, 0x2200, "Start"),
      MenuItem(0x2210, 0x2200, 0x2000, "OK"),
      MenuItem(0x2220, 0x2200, 0x2000, "Abort"),
    MenuItem(0x2300, 0x2000, 0x2300, "Stop"),
      MenuItem(0x2310, 0x2300, 0x2000, "OK"),
      MenuItem(0x2320, 0x2300, 0x2000, "Abort"),
    MenuItem(0x2400, 0x2000, 0x2400, "Save"),
      MenuItem(0x2410, 0x2400, 0x2000, "OK"),
      MenuItem(0x2420, 0x2400, 0x2000, "Abort"),
    MenuItem(0x2500, 0x2000, 0x2500, "Reset"),
      MenuItem(0x2510, 0x2500, 0x2000, "OK"),
      MenuItem(0x2520, 0x2500, 0x2000, "Abort"),
    MenuItem(0x2600, 0x2000, 0x0000, "BACK"),
  MenuItem(0x3000, 0x0000, 0x0000, "Toggle UI")
};

Menu menu(upPin,downPin,enterPin, menuItems, 27);

void sendDebug(char text[])
{
  gpuClient.sendStatus(text);
  Serial.println(text);
}

void setup() {
  digitalWrite(13,HIGH);
  delay(250);
  digitalWrite(13,LOW);
  delay(250);
  digitalWrite(13,HIGH);
  delay(250);
  digitalWrite(13,LOW);
  delay(250);
  digitalWrite(13,HIGH);
  delay(250);
  digitalWrite(13,LOW);
  delay(250);
  digitalWrite(13,HIGH);
  delay(250);
  digitalWrite(13,LOW);
  delay(250);
  digitalWrite(13,HIGH);
  delay(250);
  gps.setup();
  gpuClient.setup();
  Serial.begin(9600);
  digitalWrite(13,LOW);

  gpuClient.sendMode(STATUSTEXT);
  delay(10);  
  sendDebug("IR INIT");

  for (int i = 0; i < 6; i++) 
  {
    if (digitalRead(enterPin) == LOW)
    {
      sendDebug("SKIP");
      delay(500);
      break;
    }

    ir[i].begin(irIds[i]);
    ir[i].setUnit(TEMP_C);

    if (ir[i].read())
    {
      irEnabled[i] = true;
    }
  }

  digitalWrite(13,HIGH);
  delay(250);
  digitalWrite(13,LOW);

  for (int i = 0; i < 10; i++) 
  {
    pinMode(inputs[i], INPUT);  
  }

  sendDebug("GPS FIX");
  while (!gps.hasTimeFix())
  {
    if (millis() > 60000) //abort if this takes more than 1 min
      break;

    if (digitalRead(enterPin) == LOW)
    {
      sendDebug("SKIP");
      delay(500);
      break;
    }

    gps.update();
  }

  if (!gps.hasTimeFix()) 
  {
    sendDebug("FAILED");
    delay(1000);
  }

  digitalWrite(13,HIGH);
  delay(250);
  digitalWrite(13,LOW);

  initSD();
}

void initSD()
{
  pinMode(sdCardPin, OUTPUT);
  sdCardInitialized = SD.begin(sdCardPin);
  if (sdCardInitialized)
  {
    sendDebug("SD INIT");
    delay(500);
  }
  else
  {
    sendDebug("SD FAIL");
    delay(1000);
  }
}

void updateIRTemps()
{
  if (irEnabled[currentIR] && ir[currentIR].read()) {
    temp[currentIR] = ir[currentIR].object();
  }  

  currentIR++;

  if (currentIR > 5)
    currentIR = 0;
}

bool flush()
{
  if (!isLogging)
    return false;

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
    drawCounter = 0;
    gpuClient.sendValues(line.values);
    return true;
  }

  return false;
}

bool initLogFile()
{
  if (!sdCardInitialized)
    initSD();    

  if (!sdCardInitialized)
  {
    sendDebug("NO CARD");
    delay(1000);
    return false;
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
    sendDebug("FILENAME");
    counter++;
    filename = date + "-" + counter;
  }

  delay(500);
  
  logFile = SD.open(filename + extension,FILE_WRITE);

  if (logFile) {
    logFile.write((const uint8_t *)&ms, sizeof(ms));
    logFile.write((const uint8_t *)&unixTime, sizeof(unixTime));
    logFile.write((const uint8_t *)&VALUE_COUNT, sizeof(VALUE_COUNT));
    logFile.flush();
    return true;
  } else {
    sendDebug("NO FILE");
    delay(1000);
    return false;
  }
}

void toggleLogging()
{
  if (isLogging)
  {
    logFile.flush();
    logFile.close();
    isLogging = false;
    isMenu = true;
    gpuClient.sendMode(STATUSTEXT);
    menu.setText(0x1000, "Start Log");
    digitalWrite(13, LOW);
  }
  else
  {
    if (!initLogFile())
      return;

    gpuClient.sendMode(VALUES);
    isMenu = false;
    isLogging = true;
    menu.setText(0x1000, "Stop Log");
    digitalWrite(13, HIGH);
  }
}

void toggleUI()
{
  if (isMenu)
  {
    gpuClient.sendMode(VALUES);
    isMenu = false;
  }
  else
  {
    gpuClient.sendMode(STATUSTEXT);
    isMenu = true;
  }
}

void channelSelect(int channel)
{
  if (isCalibrating)
  {
    sendDebug("IN USE");
    delay(500);
    return;
  }

  calibrateIndex = channel;
}

void startCalibration()
{
  if (isCalibrating)
    return;
    
  isCalibrating = true;
  gpuClient.sendCalibrate(calibrateIndex);
}

void stopCalibration()
{
  if (!isCalibrating)
    return;
    
  isCalibrating = false;
  gpuClient.sendCalibrate(0xFF);
}

void storeCalibration()
{    
  gpuClient.sendStoreCalibration(calibrateIndex);
}

void resetCalibration()
{    
  gpuClient.sendResetCalibration(calibrateIndex);
}

void loop()
{
  //Stuff that always should be done:
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
      line.values[i] = analogRead(inputs[i-6]);
    }    
  }

  if (isLogging)
  {
    logFile.write((const uint8_t *)&line, sizeof(line));
  }

  if (!draw() || !isLogging) {
    updateIRTemps();
  } 

  uint16_t menuAction = menu.update();

  switch(menuAction)
  {
    case 0x1100: //Toggle logging
      toggleLogging();
      break;
    case 0x3000: //Toggle UI
      toggleUI();
      break;

    case 0x2110: //Select Ch 1
      channelSelect(6);
      break;
    case 0x2120: //Select Ch 2
      channelSelect(7);
      break;
    case 0x2130: //Select Ch 3
      channelSelect(8);
      break;
    case 0x2140: //Select Ch 4
      channelSelect(9);
      break;
    case 0x2150: //Select Ch 5
      channelSelect(10);
      break;
    case 0x2160: //Select Ch 6
      channelSelect(11);
      break;
    case 0x2170: //Select Ch 7
      channelSelect(12);
      break;
    case 0x2180: //Select Ch 8
      channelSelect(13);
      break;

    case 0x2210: //Start Calibration
      startCalibration();
      break;

    case 0x2310: //Stop Calibration
      stopCalibration();
      break;

    case 0x2410: //Stop Calibration
      storeCalibration();
      break;

    case 0x2510: //Stop Calibration
      resetCalibration();
      break;
    
    default:
      break;
  }

  if (isMenu)
    menu.render(gpuClient);

  delay(3);
}
