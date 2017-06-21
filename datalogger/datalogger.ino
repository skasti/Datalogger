#include "SD.h"
#include <Wire.h>
#include "RTClib.h"
#include "UBX.h"
#include "states.h"
#include "gps.h"
#include "nanogpu-client.h"
#include "SparkFunMLX90614.h"
#include "menu.h"
#include <EEPROM.h>

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
unsigned long autoStartSpeedThreshold = 30 * 0.277 * 1000; // 30kph -> m/s -> mm/s
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

int upPin = 30;
int downPin = 32;
int enterPin = 34;
int toggleUIPin = 36;
int toggleLoggingPin = 40;
int redLedPin = 45;
int greenLedPin = 42;
int blueLedPin = 44;

int toggleUIState, prevToggleUIState, toggleLoggingState, prevToggleLoggingState;
unsigned long toggleLoggingStart = 0;
bool loggingToggled = false;

File logFile;
uint8_t signalStrength = 0x00;
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
  MenuItem(0x3000, 0x0000, 0x0000, "Toggle UI"),
  MenuItem(0x4000, 0x0000, 0x0000, "Auto ON")
};

Menu menu(upPin,downPin,enterPin, menuItems, 28);

void sendDebug(char text[])
{
  gpuClient.sendStatus(text);
  Serial.println(text);
}

void setup() {

  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);

  analogWrite(redLedPin,40);

  gps.setup();
  gpuClient.setup();
  Serial.begin(9600);

  autoStart = (EEPROM.read(AUTOSTART_EEPROM) == 0);
  updateAutoStartMenuItem(); 

  digitalWrite(13,LOW);

  gpuClient.sendSignal(signalStrength);
  
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

  pinMode(toggleUIPin, INPUT_PULLUP);
  pinMode(toggleLoggingPin, INPUT_PULLUP);
  toggleUIState = digitalRead(toggleUIPin);
  updateToggleLoggingButton();

  getGPSFix();

  digitalWrite(13,HIGH);
  delay(250);
  digitalWrite(13,LOW);

  initSD();
  analogWrite(redLedPin,LOW);
  analogWrite(greenLedPin, 40);
}

void updateAutoStartMenuItem()
{
  if (autoStart)
    menu.setText(0x4000, "Auto ON");
  else
    menu.setText(0x4000, "Auto OFF");
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
    temp[currentIR] = ir[currentIR].object();
  }  
  int duration = millis() - start;

  if (duration > 10)
  {
    irEnabled[currentIR] = false;
  }


  currentIR++;

  if (currentIR > 5)
    currentIR = 0;
}

bool draw()
{
  if (ms > nextDrawTime)
  {
    gpuClient.sendValues(line.values);
    nextDrawTime = ms + drawInterval;

    if (isLogging)
      nextDrawTime = ms + loggingDrawInterval;

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
  isMenu = !isMenu;
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

void updateToggleUIButton()
{
  prevToggleUIState = toggleUIState;
  toggleUIState = digitalRead(toggleUIPin);

  if (toggleUIState == LOW && prevToggleUIState == HIGH)
  {
    toggleUI();
  }
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

  if (pvt.gSpeed < autoStartSpeedThreshold)
    autoStartStart = 0;
  else if (autoStartStart > 0)
  {
    if (millis() - autoStartStart > autoStartDurationThreshold)
      toggleLogging();
  }
  else
    autoStartStart = millis();
}

void toggleAutoStart()
{
  autoStart = !autoStart;

  if (autoStart)
    EEPROM.write(AUTOSTART_EEPROM, 0);
  else
    EEPROM.write(AUTOSTART_EEPROM, 0xFF);

  updateAutoStartMenuItem();
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

      if (ms > nextFlush)
      {
        nextFlush = ms + flushInterval;
        logFile.flush();
      }
    }

    nextLogTime = ms + logInterval;
  }

  if (!draw() || !isLogging) {
    updateIRTemps();
  }

  if (ms > nextInputUpdate)
  {
    nextInputUpdate = ms + inputUpdateInterval;

    updateToggleUIButton();
    updateToggleLoggingButton();  
    updateAutoStart();

    if (isMenu)
    {
      uint16_t menuAction = menu.update();

      switch(menuAction)
      {
        case 0x1100: //Toggle logging
          toggleLogging();
          break;
        case 0x3000: //Toggle UI
          toggleUI();
          break;
        case 0x4000: //Toggle Autostart
          toggleAutoStart();
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
      
      if (gpuClient.getMode() != STATUSTEXT)
      {
        gpuClient.sendMode(STATUSTEXT);
      }
    } 
    else
    {
      if (gpuClient.getMode() != VALUES)
        gpuClient.sendMode(VALUES);
    }
  }

  if (isMenu)
    menu.render(gpuClient);

  if (isMenu)
    digitalWrite(13, HIGH);
  else
    digitalWrite(13, LOW);

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

  if (ms > nextSignal)
  {
    nextSignal = ms + signalInterval;

    uint32_t hAcc = pvt.hAcc / 1000;

    if (hAcc <= 1)
      signalStrength = 0xFF;
    else if (hAcc <= 2)
      signalStrength = 0xE0;
    else if (hAcc <= 4)
      signalStrength = 0xC0;
    else if (hAcc <= 8)
      signalStrength = 0xA0;
    else if (hAcc <= 16)
      signalStrength = 0x80;
    else if (pvt.fixType >= 2)
      signalStrength = 0x10;

    gpuClient.sendSignal(signalStrength);
  }
  
}
