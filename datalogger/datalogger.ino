#include "SD.h"
#include <Wire.h>
#include "RTClib.h"
#include "UBX.h"
#include "states.h"
#include "gps.h"
#include "SparkFunMLX90614.h"
#include <EEPROM.h>
#include <WString.h>
#include "nextionDisplay.h"


#define DEBUG Serial

#if defined(__AVR_ATmega2560__)
  #define GPS Serial1
  bool debug = true;
#else
  #define GPS Serial
  bool debug = false;
#endif

const int AUTOSTART_EEPROM = 0;
bool autoStart = true;
unsigned int autoStartMode = 1; //0 = speed, 1 = fixType, 2 = Power (always log)
unsigned long autoStartSpeedThreshold = 30 * 0.277 * 1000; // 30kph -> m/s -> mm/s
unsigned int autoStartFixType = 2; //3 = Full 3D
unsigned long autoStartDurationThreshold = 2000; // 2sec;
unsigned long autoStartStart = 0;

unsigned long ms = 0;
unsigned long nextLogTime = 0;
unsigned long nextDrawTime = 0;
unsigned long nextInputUpdate = 0;
unsigned long nextBlink = 0;
unsigned long nextSignal = 0;
unsigned long nextFlush = 0;
bool blinkState = false;

unsigned long signalInterval = 1000000;
unsigned long blinkInterval = 500000;
unsigned long drawInterval = 100000; // 100ms;
unsigned long loggingDrawInterval = 250000; // 250ms;
unsigned long logInterval = 4000; // 4ms;
unsigned long inputUpdateInterval = 20000; // 20ms;
unsigned long flushInterval = 10000000; // 10sec;

int inputs[] = { A0,A1,A2,A3,A4,A5,A6,A7 };

int sdCardPin = 53;
bool sdCardInitialized = false;

int toggleLoggingPin = 40;
int enterPin = 40;
int redLedPin = 45;
int greenLedPin = 42;
int blueLedPin = 44;

int toggleLoggingState, prevToggleLoggingState;
unsigned long toggleLoggingStart = 0;
bool loggingToggled = false;

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

Gps gps;
NextionDisplay display;

String pollValuesCmd = String("pollValues");
String toggleAutoCmd = String("toggleAuto");
String getAutoCmd = String("getAuto");

uint8_t irIds[] = {0x10,0x11,0x12,0x13,0x14,0x15};
IRTherm ir[6];
int16_t temp[6] = {0,0,0,0,0,0};
bool irEnabled[6] = {false, false, false, false, false, false};
int currentIR = 0;

bool isLogging = false;
bool isMenu = true;
bool isCalibrating = false;
int calibrateIndex = 0xFF;

char* irNames[] = {
  "IR0: RL",
  "IR1: RM",
  "IR2: RR",
  "IR3: FL",
  "IR4: FM",
  "IR5: FR"
};

char* channelComponentNames[] = {
  "t0", "t1", "t2",  //Rear temps (RL, RM, RR)
  "t3", "t4", "t5", //Front temps (FL, FM, FR)
  "a0", "a1", "a2", "a3", //First set of analogs
  "a4", "a5", "a6", "a7", //Second set of analogs
};


void sendDebug(char text[])
{
  Serial.println(text);
  display.debug(text);
}

void setup() {

  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);

  analogWrite(redLedPin,40);

  gps.setup();

  display.setup();

  Serial.begin(9600);

  EEPROM.write(AUTOSTART_EEPROM, 0);  
  autoStart = (EEPROM.read(AUTOSTART_EEPROM) == 0);

  digitalWrite(13,LOW);

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
      sendDebug(irNames[i]);
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

  updateToggleLoggingButton();

  digitalWrite(13,HIGH);
  delay(250);
  digitalWrite(13,LOW);

  initSD();
  analogWrite(redLedPin,LOW);
  analogWrite(greenLedPin, 40);
}

void getGPSFix()
{
  if (gps.hasTimeFix())
    return;

  unsigned long giveUpTime = millis() + 60000;
  int enterState = digitalRead(enterPin);
  int previousEnterState = enterState;

  sendDebug("GPS FIX");
  while (!gps.hasTimeFix())
  {
    previousEnterState = enterState;
    enterState = digitalRead(enterPin);
    
    if (millis() > giveUpTime) //abort if this takes more than 1 min
    {
      sendDebug("TIMEOUT");
      delay(500);
      break;
    }

    if (enterState == LOW && previousEnterState == HIGH)
    {
      sendDebug("SKIP");
      delay(500);
      break;
    }

    gps.update();

    delay(10);
  }
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
  unsigned long start = millis();
  if (irEnabled[currentIR] && ir[currentIR].read()) {
    // int prev = temp[currentIR];
    temp[currentIR] = ir[currentIR].object();
    int duration = millis() - start;

    if (duration > 10)
    {
      sendDebug("Disable IR");
      irEnabled[currentIR] = false;
    }
  }
  currentIR++;

  if (currentIR > 5)
    currentIR = 0;
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

  getGPSFix();

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
  sendDebug("ToggleLog");
  if (isLogging)
  {
    logFile.flush();
    logFile.close();
    isLogging = false;
    digitalWrite(13, LOW);
  }
  else
  {
    if (!initLogFile())
      return;

    isLogging = true;
    digitalWrite(13, HIGH);
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

void updateToggleLoggingButton()
{
  prevToggleLoggingState = toggleLoggingState;
  toggleLoggingState = digitalRead(toggleLoggingPin);

  if (toggleLoggingState == LOW && prevToggleLoggingState == HIGH)
  {
    toggleLoggingStart = millis();
    loggingToggled = false;
  } 
  else if (toggleLoggingState == LOW) {
    if ((millis() - toggleLoggingStart > 2000) && !loggingToggled)
    {
      toggleLogging();
      loggingToggled = true;
    }
  }
}

void updateAutoStart()
{
  if (isLogging)
    return;

  if (!autoStart)
    return;

  if (autoStartMode == 0)
  {
    if (pvt.gSpeed < autoStartSpeedThreshold)
      autoStartStart = 0;
    else if (autoStartStart > 0)
    {
      if (millis() - autoStartStart > autoStartDurationThreshold)
        toggleLogging();
    }
    else
      autoStartStart = millis();
  } else if (autoStartMode == 1)
  {
    if (pvt.fixType < autoStartFixType)
      autoStartStart = 0;
    else if (autoStartStart > 0)
    {
      if (millis() - autoStartStart > autoStartDurationThreshold)
        toggleLogging();
    }
    else
      autoStartStart = millis();
  } else if (autoStartMode == 2)
  { 
    if (autoStartStart > 0)
    {
      if (millis() - autoStartStart > autoStartDurationThreshold)
        toggleLogging();
    }
    else
      autoStartStart = millis();
  }
}

void sendAutoStart()
{
  if (autoStart)
    display.sendValue("autoStartBtn", "Auto ON");
  else
    display.sendValue("autoStartBtn", "Auto OFF");
}

void toggleAutoStart()
{
  autoStart = !autoStart;

  if (autoStart)
    EEPROM.write(AUTOSTART_EEPROM, 0);
  else
    EEPROM.write(AUTOSTART_EEPROM, 0xFF);

  sendAutoStart();
}

void loop()
{
  //Stuff that always should be done:
  ms = micros();
  gps.update();

  if (ms > nextLogTime)
  {
    pvt = gps.getLatest();
    line.micros = ms;

    line.speed = pvt.gSpeed;
    line.sAcc = pvt.sAcc;
    line.lon = pvt.lon;
    line.lat = pvt.lat;
    line.alt = pvt.alt;
    line.hAcc = pvt.hAcc;
    line.vAcc = pvt.vAcc;
    line.fixType = pvt.fixType;

    int prev = 0;
    for (int i = 0; i < VALUE_COUNT; i++) {
      // prev = line.values[i];

      if (i < 6)
      {
        line.values[i] = temp[i];
      } else {        
        line.values[i] = analogRead(inputs[i-6]);
      }

      // if (prev != line.values[i])
      // {
      //   display.sendValue(channelComponentNames[i], line.values[i]);
      // }
    }

    if (isLogging)
    {
      logFile.write((const uint8_t *)&line, sizeof(line));

      if (ms > nextFlush)
      {
        nextFlush = ms + flushInterval;
        logFile.flush();
      }
    }

    nextLogTime = ms + logInterval;
  }

  if (!isLogging) {
    updateIRTemps();
  }

  if (display.hasCommand())
  {
    String command = display.getCommand();

    if (command.equals(pollValuesCmd))
    {
      for (int i = 0; i < VALUE_COUNT; i++)
        display.sendValue(channelComponentNames[i], line.values[i]);
    }
    else if (command.equals(toggleAutoCmd))
    {
      toggleAutoStart();
    }
    else if (command.equals(getAutoCmd))
    {
      sendAutoStart();
    }
  }

  if (ms > nextInputUpdate)
  {
    nextInputUpdate = ms + inputUpdateInterval;

    updateToggleLoggingButton();  
    updateAutoStart();
  }

  if (ms > nextBlink)
  {
    blinkState = !blinkState;

    int ledPin = greenLedPin;
    
    if (isLogging)
    {
      ledPin = blueLedPin;
      digitalWrite(greenLedPin, LOW);
    }
    else
    {
      digitalWrite(blueLedPin, LOW);
    }

    if (blinkState)
    {
      if ((ledPin >= 44) && (ledPin <= 46))
        analogWrite(ledPin, 40);
      else
        digitalWrite(ledPin, HIGH);
    }
    else
       digitalWrite(ledPin, LOW);

    nextBlink = ms + blinkInterval;
  }  
}
