#ifndef NANOGPU_CLIENT_H
#define NANOGPU_CLIENT_H

#include <inttypes.h>
#include "coms.h"

const bool ISDEBUG = false;

class NanoGpuClient{
    private:
        void calcCheckSum(unsigned char* CK, unsigned char* payload, int payloadSize);
        void sendPacket(unsigned char packetId, unsigned char* payload, int payloadSize, bool confirm);
        
    public:
        void setup();
        void sendValues(uint16_t* values);
        void sendStatus(unsigned char* status);
        void sendMode(GPU_MODE mode);
};

#endif