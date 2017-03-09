#include "nanogpu.h"
#include "coms.h"
#include <EEPROM.h>

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

void NanoGpu::setup()
{
    Serial.begin(9600);
    display.begin();

    int eepromState = EEPROM.read(0);

    //Unwritten EEPROM locations have the value 255, 
    //so this means EEPROM has not been initiated
    if (eepromState == 255)
        initEEPROM();
    else
        readEEPROM();
}

void NanoGpu::initEEPROM()
{
    for (int i = 0; i < 10; i++)
    {
        int minIndex = i * sizeof(uint16_t) * 2;
        int maxIndex = minIndex + sizeof(uint16_t);

        EEPROM.put(minIndex, mins[i]);
        EEPROM.put(maxIndex, maxs[i]);
    }

    EEPROM.write(0,0);
}

void NanoGpu::readEEPROM()
{
    for (int i = 0; i < 10; i++)
    {
        int minIndex = i * sizeof(uint16_t) * 2;
        int maxIndex = minIndex + sizeof(uint16_t);

        EEPROM.get(minIndex, mins[i]);
        EEPROM.get(maxIndex, maxs[i]);

        int interval = maxs[i] - mins[i];
        double multiplier = (double)interval / 50.0;
        multipliers[i] = multiplier;
    }
}

bool NanoGpu::checksumIsValid()
{
    checksum[0] = 0x00;
    checksum[1] = 0x00;

    int realPayload = payloadSize - sizeof(GPU_HEADER) - 1;

    for (int i = 0; i < realPayload; i++) 
    {
        if (i < realPayload - 2)
        {
            checksum[0] += buffer[i];
            checksum[1] += checksum[0];
        }
        else if (buffer[i] != checksum[i - (realPayload - 2)])
        {
            return false;
        }
    }
    
    return true;
};

void NanoGpu::processSerial()
{
    dot_h = 1;
    dot_x = 0;
    dot_w = 1;

    while ( Serial.available() ) 
    {
        byte c = Serial.read();
        dot_x++;

        if (dot_x > 128)
            dot_x = 0;
        
        if ( fpos < sizeof(GPU_HEADER)) 
        {
            dot_y = 1;

            if ( c == GPU_HEADER[fpos])
                fpos++;
            else
                fpos = 0;
        }
        else if (fpos == sizeof(GPU_HEADER)) 
        {
            package = c;
            dot_y = 2;
        
            switch (package)
            {
                case PKG_MODE_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_MODE) + 3;
                    break;
                case PKG_STATUS_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_STATUS) + 3;
                    break;
                case PKG_VALUES_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_VALUES) + 3;
                    break;
                case PKG_CALIBRATE_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_CALIBRATE) + 3;
                    break;
                case PKG_STORE_CALIBRATION_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_STORE_CALIBRATION) + 3;
                    break;
                case PKG_READ_CALIBRATION_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_READ_CALIBRATION) + 3;
                    break;
                default:
                    fpos = 0;
                    break;
            }

            dot_w = 1 + package;
        }
        else
        {
            dot_y = 3;
            if (fpos < payloadSize)
            {
                buffer[fpos-sizeof(GPU_HEADER) - 1] = c;
                fpos++;

                if (fpos == payloadSize)
                {
                    fpos = 0;

                    if (checksumIsValid())
                    {
                        switch (package)
                        {
                            case PKG_MODE_ID:
                                updateMode();
                                break;
                            case PKG_STATUS_ID:
                                updateStatus();
                                break;
                            case PKG_VALUES_ID:
                                updateValues();
                                break;
                            case PKG_CALIBRATE_ID:
                                updateCalibration();
                                break;
                            case PKG_STORE_CALIBRATION_ID:
                                storeCalibration();
                                break;
                            case PKG_READ_CALIBRATION_ID:
                                readCalibration();
                                break;
                            default:
                                break;
                        }

                        Serial.write(0x00);
                    }
                    else
                    {
                        dot_h = 5;
                        Serial.write(0xFF);
                    }

                    return;
                }
            } 
            else
            {
                fpos = 0;
                Serial.write(0xEE);
                return;
            }
        }
    }
    Serial.write(0xDD);
}

void NanoGpu::updateMode()
{
    PKG_MODE modePackage = *((PKG_MODE*)(&buffer));
    mode = (GPU_MODE) modePackage.mode;
}

void NanoGpu::updateCalibration()
{
    PKG_CALIBRATE package = *((PKG_CALIBRATE*)(&buffer));

    calibrateIndex = package.channel;

    if (calibrateIndex >= 10)
        calibrateIndex = 255;
    else
    {
        mins[calibrateIndex] = 1000;
        maxs[calibrateIndex] = 0;
    }
}

void NanoGpu::storeCalibration()
{
    PKG_STORE_CALIBRATION package = *((PKG_STORE_CALIBRATION*)(&buffer));

    if (package.channel >= 10)
        return;

    int minIndex = package.channel * sizeof(uint16_t) * 2;
    int maxIndex = minIndex + sizeof(uint16_t);

    EEPROM.put(minIndex, mins[package.channel]);
    EEPROM.put(maxIndex, maxs[package.channel]);
}

void NanoGpu::readCalibration()
{
    PKG_READ_CALIBRATION package = *((PKG_READ_CALIBRATION*)(&buffer));

    if (package.channel >= 10)
        return;

    int minIndex = package.channel * sizeof(uint16_t) * 2;
    int maxIndex = minIndex + sizeof(uint16_t);

    EEPROM.get(minIndex, mins[package.channel]);
    EEPROM.get(maxIndex, maxs[package.channel]);

    int interval = maxs[package.channel] - mins[package.channel];
    double multiplier = (double)interval / 50.0;
    multipliers[package.channel] = multiplier;
}

void NanoGpu::updateValues() 
{
    PKG_VALUES valuePackage = *((PKG_VALUES*)(&buffer));

    for (int i = 0; i < 10; i++)
    {
        values[i] = valuePackage.values[i];

        if (calibrateIndex == i)
        {
            if (values[i] < mins[i])
                mins[i] = values[i];
            
            if (values[i] > maxs[i])
                maxs[i] = values[i];

            int interval = maxs[i] - mins[i];
            double multiplier = (double)interval / 50.0;
            multipliers[i] = multiplier;
        }
    }
}

void NanoGpu::updateStatus() 
{
    PKG_STATUS statusPackage = *((PKG_STATUS*)(&buffer));

    bool eos = false;

    for (int i = 0; i < 20; i++)
    {
        char c = statusPackage.characters[i];

        if (eos)
        {
            status[i] = 0x00;
        }
        else if (c != 0x00)
        {
            status[i] = c;
        }
        else
        {
            eos = true;
        }
    }
}

void NanoGpu::renderValues()
{
    for (int i = 0; i < 10; i++)
    {
        int realValue = values[i] - mins[i];
        int interval = maxs[i] - mins[i];
        double multiplier = (double)interval / 50.0;

        int height = (int)((double)realValue * multiplier);
        display.drawBox(i * 12, 57-height, 10, height+1);
    }
}

void NanoGpu::renderStatusText()
{
    display.setFont(u8g2_font_ncenB14_tr);
    display.drawStr(5,25,status);    
}

void NanoGpu::update()
{
    if (Serial.available())
        processSerial();

    display.clearBuffer();    
    display.drawBox(fpos, 0, payloadSize - fpos, 1);
    display.drawBox(dot_x, dot_y, dot_w, dot_h);

    for (int i = 0; i < 128; i+= 2)
    {
        display.drawBox(i, 58, 1, 2);
    }

    display.drawBox(checksum[0] * 2, 60, 2, 1);
    display.drawBox(buffer[payloadSize - 2] * 2, 61, 2, 1);
    display.drawBox(checksum[1] * 2, 62, 2, 1);
    display.drawBox(buffer[payloadSize - 1] * 2, 63, 2, 1);

    switch(mode)
    {
        case STATUSTEXT:
            renderStatusText();
            break;
        case VALUES:
            renderValues();
            break;
        default:
            mode = STATUSTEXT;
            break;
    }

    display.sendBuffer();
}