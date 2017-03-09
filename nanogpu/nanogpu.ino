#include <Wire.h>
#include <U8g2lib.h>
#include "nanogpu.h"

NanoGpu Gpu;

void setup() {
    Gpu.setup();
}

void loop() {
    Gpu.update();
    delay(5);
}