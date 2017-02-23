#ifndef NANOGPU_H
#define NANOGPU_H
#include <Wire.h>
#include <U8g2lib.h>
#include <inttypes.h>

enum GPU_MODE {
    STATUSTEXT = 0x01,
    VALUES = 0x02
};

class NanoGpu {
    int dot_x = 0;
    int dot_y = 1;
    int dot_w = 1;
    int dot_h = 1;

    int fpos = 0;
    int payloadSize = 3;
    byte package = 0x00;
    unsigned char buffer[64];
    U8G2_SH1106_128X64_NONAME_F_HW_I2C display = U8G2_SH1106_128X64_NONAME_F_HW_I2C(U8G2_R0);

    GPU_MODE mode = STATUSTEXT;    
    char status[20] = "IDLE";
    uint16_t values[10] = {0,0,0,0,0,0,0,0,0,0};

    private:
        void updateMode();
        void updateStatus();
        void updateValues(); 
        void processSerial();
        void renderStatusText();
        void renderValues();

    public:
        void setup();
        void update();
};

#endif