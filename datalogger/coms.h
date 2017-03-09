#ifndef COMS_H
#define COMS_H

#include <inttypes.h>

const unsigned char GPU_HEADER[] = { 0xAC, 0xDC, 0xFC };
const unsigned char PKG_VALUES_ID = 0x01;
const unsigned char PKG_STATUS_ID = 0x02;
const unsigned char PKG_MODE_ID = 0x03;

struct PKG_VALUES {
    uint16_t values[10];
};

struct PKG_STATUS {
    char characters[20];
};

struct PKG_MODE {
    uint8_t mode;
};

enum GPU_MODE {
    STATUSTEXT = 0x01,
    VALUES = 0x02
};

#endif