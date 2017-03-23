#include "nanogpu-client.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

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

            if (tries > 5)
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

void NanoGpuClient::sendStatus(char status[])
{
    PKG_STATUS packet;

    bool eos = false;
    for (int i = 0; i < sizeof(PKG_STATUS); i++)
    {
        if (eos)
        {
            packet.characters[i] = 0x00;
        }
        else if (status[i] != 0x00)
        {
            packet.characters[i] = status[i];
        }
        else 
        {
            packet.characters[i] = 0x00;
            eos = true;
        }
    }

    packet.characters[sizeof(PKG_STATUS) - 1] = 0x00;

    sendPacket(PKG_STATUS_ID, (unsigned char*)&packet, sizeof(PKG_STATUS), true);    
}

void NanoGpuClient::sendValues(uint16_t* values)
{
    sendPacket(PKG_VALUES_ID, (unsigned char*)values, sizeof(PKG_VALUES), false);    
}

void NanoGpuClient::sendCalibrate(uint8_t channel)
{
    sendPacket(PKG_CALIBRATE_ID, (unsigned char*)&channel, sizeof(PKG_CALIBRATE), true);    
}

void NanoGpuClient::sendStoreCalibration(uint8_t channel)
{
    sendPacket(PKG_STORE_CALIBRATION_ID, (unsigned char*)&channel, sizeof(PKG_STORE_CALIBRATION), true);    
}

void NanoGpuClient::sendResetCalibration(uint8_t channel)
{
    sendPacket(PKG_READ_CALIBRATION_ID, (unsigned char*)&channel, sizeof(PKG_READ_CALIBRATION), true);    
}