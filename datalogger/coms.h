#ifndef COMS_H
#define COMS_H

#include <inttypes.h>

const uint8_t CHECKSUM_LENGTH = 3;
const uint8_t VALUES_COUNT = 14;

const unsigned char GPU_HEADER[] = { 0xAC, 0xDC, 0xFC };
const unsigned char PKG_VALUES_ID = 0x01;
const unsigned char PKG_STATUS_ID = 0x02;
const unsigned char PKG_MODE_ID = 0x03;
const unsigned char PKG_CALIBRATE_ID = 0x04;
const unsigned char PKG_STORE_CALIBRATION_ID = 0x05;
const unsigned char PKG_READ_CALIBRATION_ID = 0x06;

struct PKG_VALUES {
    uint16_t values[VALUES_COUNT];
};

struct PKG_STATUS {
    char characters[10];
};

struct PKG_MODE {
    uint8_t mode;
};

struct PKG_CALIBRATE {
    uint8_t channel;
};

struct PKG_STORE_CALIBRATION {
    uint8_t channel;
};

struct PKG_READ_CALIBRATION {
    uint8_t channel;
};

enum GPU_MODE {
    STATUSTEXT = 0x01,
    VALUES = 0x02
};

#endif