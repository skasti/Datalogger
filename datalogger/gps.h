#ifndef GPS_H
#define GPS_H

#include "UBX.h"

#if defined(__AVR_ATmega2560__)
  #define GPS Serial1
#else
  #define GPS Serial
#endif

class Gps 
{
    NAV_PVT pvtBuffer, latestPVT;

    private:
        void calcChecksum (unsigned char* CK);
        bool process();

    public:
        void setup();
        void update();
        NAV_PVT getLatest();
        bool has3DFix();
        bool hasTimeFix();
};

#endif