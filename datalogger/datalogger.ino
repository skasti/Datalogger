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
    uint32_t  micros;
    uint16_t  speed;
    uint16_t  sAcc; //Speed accuracy extimate
    long      lon;
    long      lat;
    long      alt;
    unsigned long   hAcc; //Horizontal accuracy estimate
    unsigned long   vAcc; //Vertical accuracy estimate
    uint16_t  values[VALUE_COUNT];
};


const unsigned char UBX_HEADER[] = { 0xB5, 0x62 };

struct NAV_PVT {
  unsigned char   cls;
  unsigned char   id;
  unsigned short  len;
  unsigned long   iTOW; //GPS Time of week
  uint16_t        year;
  uint8_t         month;
  uint8_t         day;
  uint8_t         hour;
  uint8_t         min;
  uint8_t         sec;
  
  unsigned char   valid; 
  //valid Bitflags: 
  //4 = fullyResolved, 
  //2 = validDate, 
  //1 = validTime

  unsigned long   tAcc; //Time accuracy extimate (UTC)
  long            nano; //fraction of second range.
  uint8_t         fixType;
  //fixType values:
  //0: no fix
  //1: dead reckoning only
  //2: 2D-fix
  //3: 3D-fix
  //4: GNSS + dead reckoning
  //5: Time only

  unsigned char   flags; //fix status flags
  unsigned char   flags2; //additional flags
  uint8_t         numSV; //Number of satellites
  long            lon; //Longitude. Scaling: 1e-7
  long            lat; //Latitude. Scaling: 1e-7
  long            height; //Height above ellipsoid
  long            alt; //Height above mean sea level
  unsigned long   hAcc; //Horizontal accuracy estimate
  unsigned long   vAcc; //Vertical accuracy estimate
  long            velN; //NED North Velocity
  long            velE; //NED East Velocity
  long            velD; //NED Down Velocity
  long            gSpeed; //Ground speed (2-D)
  long            headMot; //Heading of motion
  unsigned long   sAcc; //Speed accuracy estimate
  unsigned long   headAcc; //Heading accuracy estimate
  uint16_t        pDOP; //Position DOP
  uint8_t         reserved1[6]; //Reserved
  long            headVeh; //heading of vehicle (2-D)
  uint8_t         reserved2[4]; //Reserved
};

NAV_PVT     pvtBuffer;
NAV_PVT     pvt;

int flushInterval = 5000;
int flushCounter = 0;

void calcChecksum(unsigned char* CK) {
  memset(CK, 0, 2);
  for (int i = 0; i < (int)sizeof(NAV_PVT); i++) {
    CK[0] += ((unsigned char*)(&pvtBuffer))[i];
    CK[1] += CK[0];
  }
}

bool processGPS() {
  static int fpos = 0;
  static unsigned char checksum[2];
  const int payloadSize = sizeof(NAV_PVT);

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
        ((unsigned char*)(&pvtBuffer))[fpos-2] = c;

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
    pvt = pvtBuffer;
  }

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
