#include "gps.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "RTClib.h"

void Gps::setup() 
{
    GPS.begin(38400);

    process();
};

void Gps::calcChecksum(unsigned char* CK)
{
    memset(CK, 0, 2);
    for (int i = 0; i < (int)sizeof(NAV_PVT); i++) {
        CK[0] += ((unsigned char*)(&pvtBuffer))[i];
        CK[1] += CK[0];
    }
};

bool Gps::process()
{
    static int fpos = 0;
    static unsigned char checksum[2];
    const int payloadSize = sizeof(NAV_PVT);

    while ( GPS.available() ) {
        byte c = GPS.read();
        if ( fpos < 2 ) {
            if ( c == UBX_HEADER[fpos] )
                fpos++;
            else
                fpos = 0;
        }
        else {
            if ( (fpos-2) < payloadSize )
            {
                ((unsigned char*)(&pvtBuffer))[fpos-2] = c;
            }

            fpos++;

            if ( fpos == (payloadSize+2) ) 
            {
                calcChecksum(checksum);
            }
            else if ( fpos == (payloadSize+3) ) 
            {
                if ( c != checksum[0] )
                    fpos = 0;
            }
            else if ( fpos == (payloadSize+4) ) 
            {
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
};

void Gps::update()
{
    if (process())
        latestPVT = pvtBuffer;
};

NAV_PVT Gps::getLatest()
{
    return latestPVT;
}

bool Gps::has3DFix()
{
    //Fixtype 3 = Full 3D fix
    return latestPVT.fixType == 3;
}

bool Gps::hasTimeFix()
{
    //Not sure how best to do this. Thus far this seems like a good option.
    DateTime compileDate = DateTime(F(__DATE__), F(__TIME__));
    return latestPVT.year >= compileDate.year();
}