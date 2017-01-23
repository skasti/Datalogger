#include "SD.h"
#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;

int inputs[] = { A0,A1,A2,A3,A6,A7 };

int sdCardPin = 10;
bool sdCardInitialized = false;

File logFile;

const uint16_t VALUE_COUNT = 8;

struct logLine
{
    uint32_t micros;
    uint16_t values[VALUE_COUNT];
};


const unsigned char UBX_HEADER[] = { 0xB5, 0x62 };

struct NAV_VELNED {
  unsigned char cls;
  unsigned char id;
  unsigned short len;
  unsigned long iTOW;
  long velN;
  long velE;
  long velD;
  unsigned long spd;
  unsigned long gSpd;
  long hdg;
  unsigned long sAcc;
  unsigned long hAcc;
};

NAV_VELNED velned;
unsigned long groundSpeed = 0;

int flushInterval = 5000;
int flushCounter = 0;

void calcChecksum(unsigned char* CK) {
  memset(CK, 0, 2);
  for (int i = 0; i < (int)sizeof(NAV_VELNED); i++) {
    CK[0] += ((unsigned char*)(&velned))[i];
    CK[1] += CK[0];
  }
}

bool processGPS() {
  static int fpos = 0;
  static unsigned char checksum[2];
  const int payloadSize = sizeof(NAV_VELNED);

  while ( Serial.available() ) {
    byte c = Serial.read();
    if ( fpos < 2 ) {
      if ( c == UBX_HEADER[fpos] )
        fpos++;
      else
        fpos = 0;
    }
    else {
      if ( (fpos-2) < payloadSize )
        ((unsigned char*)(&velned))[fpos-2] = c;

      fpos++;

      if ( fpos == (payloadSize+2) ) {
        calcChecksum(checksum);
      }
      else if ( fpos == (payloadSize+3) ) {
        if ( c != checksum[0] )
          fpos = 0;
      }
      else if ( fpos == (payloadSize+4) ) {
        fpos = 0;
        if ( c == checksum[1] ) {
          return true;
        }
      }
      else if ( fpos > (payloadSize+4) ) {
        fpos = 0;
      }
    }
  }
  return false;
}

void setup() {
  Serial.begin(38400);
  
  if (! rtc.begin()) {
    while (1);
  }

  if (rtc.lostPower()) {
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  DateTime now = rtc.now();

  for (int i = 0; i < 6; i++) {
    pinMode(inputs[i], INPUT);  
  }
  
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

  //filename = "data.log";


  if (sdCardInitialized) {
    logFile = SD.open(filename + extension,FILE_WRITE);

    if (logFile) {
      uint32_t ms = micros();
      uint32_t unixTime = now.unixtime();
      logFile.write((const uint8_t *)&ms, sizeof(ms));
      logFile.write((const uint8_t *)&unixTime, sizeof(unixTime));
      logFile.write((const uint8_t *)&VALUE_COUNT, sizeof(VALUE_COUNT));
      logFile.flush();
    } else {
      
    }
  }
}

void loop() {  
  if ( processGPS() ) {
    groundSpeed = velned.gSpd;
  }

  unsigned long ms = micros();
  
  struct logLine line;
  line.micros = ms;

  for (int i = 0; i < 6; i++) {
    line.values[i] = analogRead(inputs[i]);
  }

  line.values[6] = (uint16_t)(velned.sAcc);
  line.values[7] = (uint16_t)(groundSpeed);

  logFile.write((const uint8_t *)&line, sizeof(line));

  flushCounter++;

  if (flushCounter > flushInterval)
  {
    flushCounter = 0;
    logFile.flush();
  }

  delay(4);
}
