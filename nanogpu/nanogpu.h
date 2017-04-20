#ifndef NANOGPU_H
#define NANOGPU_H
#include <Wire.h>
#include <U8g2lib.h>
#include <inttypes.h>
#include "coms.h"

class NanoGpu {
    int fpos = 0;
    int payloadSize = 3;
    byte package = 0x00;
    unsigned char checksum[2] = {0x00, 0x00};
    unsigned char buffer[40];
    U8G2_SH1106_128X64_NONAME_F_HW_I2C display = U8G2_SH1106_128X64_NONAME_F_HW_I2C(U8G2_R0);

    GPU_MODE mode = STATUSTEXT;    
    char status[10] = "IDLE";
    uint8_t signalStrength = 0x00;
    uint16_t values[VALUES_COUNT] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    uint16_t mins[VALUES_COUNT] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    uint16_t maxs[VALUES_COUNT] = {120,120,120,120,120,120,1000,1000,1000,1000,1000,1000,1000,1000};
    
    uint8_t calibrateIndex = 255;

    unsigned long nextDrawTime = 0;

    private:
        void updateMode();
        void updateCalibration();
        void storeCalibration();
        void readCalibration();
        void updateStatus();
        void updateSignal();
        void updateValues(); 
        bool checksumIsValid();
        void processSerial();
        void renderStatusText();
        void renderValues();
        void renderSignal();

        void readEEPROM();
        void initEEPROM();

    public:
        void setStatus(const char newStatus[]);
        void setup();
        void update();
};

#endif