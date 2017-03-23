#ifndef NANOGPU_CLIENT_H
#define NANOGPU_CLIENT_H

#include <inttypes.h>
#include "coms.h"

#if defined(__AVR_ATmega2560__)
  #define GPU Serial2
#else
  #define GPU Serial
#endif


const bool ISDEBUG = false;

class NanoGpuClient{
    private:
        void calcCheckSum(unsigned char* CK, unsigned char* payload, int payloadSize);
        void sendPacket(unsigned char packetId, unsigned char* payload, int payloadSize, bool confirm);
        
    public:
        void setup();
        void sendValues(uint16_t* values);
        void sendStatus(char status[]);
        void sendMode(GPU_MODE mode);
        void sendCalibrate(uint8_t channel);
        void sendStoreCalibration(uint8_t channel);
        void sendResetCalibration(uint8_t channel);
};

#endif