#ifndef NEXTIONDISPLAY_H
#define NEXTIONDISPLAY_H

#include <inttypes.h>

#if defined(__AVR_ATmega2560__)
#define Nextion Serial3
#else
#define Nextion Serial
#endif

class NextionDIsplay {
    public:
        void debug(char text[]);
        void sendCommand(char command[]);
};

#endif