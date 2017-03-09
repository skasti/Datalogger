#include "nanogpu-client.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#define GPU Serial3

void NanoGpuClient::setup()
{
    GPU.begin(9600);
}

void NanoGpuClient::calcCheckSum(unsigned char* CK, unsigned char* payload, int payloadSize)
{    
    for (int i = 0; i < payloadSize; i++) {
        CK[0] += payload[i];
        CK[1] += CK[0];
    }
}

void NanoGpuClient::sendPacket(unsigned char packetId, unsigned char* payload, int payloadSize, bool confirm)
{
    unsigned char checksum[2];
    memset(checksum, 0, 2);
    
    calcCheckSum(checksum, payload, payloadSize);

    unsigned char response = 0xFF;
    int tries = 0;

    do
    {
        tries++;

        GPU.write(GPU_HEADER, sizeof(GPU_HEADER));
        GPU.write(packetId);
        GPU.write(payload, payloadSize);
        GPU.write(checksum, 2);

        if (ISDEBUG)
        {
            Serial.write(GPU_HEADER, sizeof(GPU_HEADER));
            Serial.write(packetId);
            Serial.write(payload, payloadSize);
            Serial.write(checksum, 2);
        }

        if (confirm)
        {
            while (GPU.available() == 0) {}

            response = GPU.read();

            if (tries > 3)
                break;
        }
        else
        {
            response = 0x00;
        }
    }
    while(response != 0x00);
}

void NanoGpuClient::sendMode(GPU_MODE mode)
{
    sendPacket(PKG_MODE_ID, (unsigned char*)&mode, sizeof(PKG_MODE), true);    
}

void NanoGpuClient::sendStatus(unsigned char* status)
{
    sendPacket(PKG_STATUS_ID, (unsigned char*)status, sizeof(PKG_STATUS), true);    
}

void NanoGpuClient::sendValues(uint16_t* values)
{
    sendPacket(PKG_VALUES_ID, (unsigned char*)values, sizeof(PKG_VALUES), false);    
}