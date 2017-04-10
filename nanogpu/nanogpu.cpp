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

    // Unwritten EEPROM locations have the value 255, 
    // so this means EEPROM has not been initiated
    if (eepromState == 255)
    {
        initEEPROM();
        setStatus("ROM Init");
    }
    else
    {
        readEEPROM();
        setStatus("ROM Read");
    }
}

void NanoGpu::setStatus(const char newStatus[])
{
    bool eos = false;
    for (int i = 0; i < 10; i++)
    {
        if (eos || i == 9)
        {
            status[i] = 0x00;
        }
        else 
        {
            char c = newStatus[i];
        
            if (c != 0x00)
            {
                status[i] = c;
            }
            else
            {
                status[i] = 0x00;
                eos = true;
            }
        }        
    }
}

void NanoGpu::initEEPROM()
{
    for (int i = 0; i < VALUES_COUNT; i++)
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
    for (int i = 0; i < VALUES_COUNT; i++)
    {
        int minIndex = i * sizeof(uint16_t) * 2;
        int maxIndex = minIndex + sizeof(uint16_t);

        EEPROM.get(minIndex, mins[i]);
        EEPROM.get(maxIndex, maxs[i]);
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
    while ( Serial.available() ) 
    {
        byte c = Serial.read();
        
        if ( fpos < sizeof(GPU_HEADER)) 
        {

            if ( c == GPU_HEADER[fpos])
                fpos++;
            else
                fpos = 0;
        }
        else if (fpos == sizeof(GPU_HEADER)) 
        {
            package = c;

            switch (package)
            {
                case PKG_MODE_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_MODE) + CHECKSUM_LENGTH;
                    break;
                case PKG_STATUS_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_STATUS) + CHECKSUM_LENGTH;
                    break;
                case PKG_VALUES_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_VALUES) + CHECKSUM_LENGTH;
                    break;
                case PKG_CALIBRATE_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_CALIBRATE) + CHECKSUM_LENGTH;
                    break;
                case PKG_STORE_CALIBRATION_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_STORE_CALIBRATION) + CHECKSUM_LENGTH;
                    break;
                case PKG_READ_CALIBRATION_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_READ_CALIBRATION) + CHECKSUM_LENGTH;
                    break;
                case PKG_SIGNAL_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_SIGNAL) + CHECKSUM_LENGTH;
                    break;
                default:
                    fpos = 0;
                    break;
            }
        }
        else
        {
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
                            case PKG_SIGNAL_ID:
                                updateSignal();
                                break;
                            default:
                                break;
                        }

                        Serial.write(0x00);
                    }
                    else
                    {
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

    if (calibrateIndex >= VALUES_COUNT)
    {
        calibrateIndex = 0xFF;
        setStatus("CAL STOP");
    }
    else
    {
        setStatus("CAL START");
        
        mins[calibrateIndex] = 1000;
        maxs[calibrateIndex] = 0;
    }
}

void NanoGpu::storeCalibration()
{
    PKG_STORE_CALIBRATION package = *((PKG_STORE_CALIBRATION*)(&buffer));

    if (package.channel >= VALUES_COUNT)
        return;

    int minIndex = package.channel * sizeof(uint16_t) * 2;
    int maxIndex = minIndex + sizeof(uint16_t);

    EEPROM.put(minIndex, mins[package.channel]);
    EEPROM.put(maxIndex, maxs[package.channel]);
}

void NanoGpu::readCalibration()
{
    PKG_READ_CALIBRATION package = *((PKG_READ_CALIBRATION*)(&buffer));

    if (package.channel >= VALUES_COUNT)
        return;

    int minIndex = package.channel * sizeof(uint16_t) * 2;
    int maxIndex = minIndex + sizeof(uint16_t);

    EEPROM.get(minIndex, mins[package.channel]);
    EEPROM.get(maxIndex, maxs[package.channel]);
}

void NanoGpu::updateValues() 
{
    PKG_VALUES valuePackage = *((PKG_VALUES*)(&buffer));

    for (int i = 0; i < VALUES_COUNT; i++)
    {
        values[i] = valuePackage.values[i];

        if (calibrateIndex == i)
        {
            if (values[i] < mins[i])
            {
                mins[i] = values[i];
                setStatus("MINS");
            }            
            else if (values[i] > maxs[i])
            {
                maxs[i] = values[i];
                setStatus("MAXS");
            }            
        }
    }
}

void NanoGpu::updateStatus() 
{
    PKG_STATUS statusPackage = *((PKG_STATUS*)(&buffer));

    setStatus(statusPackage.characters);
}

void NanoGpu::updateSignal()
{
    PKG_SIGNAL signalPackage = *((PKG_SIGNAL*)(&buffer));
    signalStrength = signalPackage.signalStrength;
}

void NanoGpu::renderValues()
{
    int width = 128 / VALUES_COUNT - 2;

    for (int i = 0; i < VALUES_COUNT; i++)
    {
        if (mins[i] < maxs[i])
        {
            int16_t value = values[i];
            
            if (value < mins[i])
                value = mins[i];

            double realValue = value - mins[i];
            int interval = maxs[i] - mins[i];
            double multiplier = 50.0 / (double)interval;

            int height = (int)(realValue * multiplier);
            display.drawBox(i * (width + 2), 57-height, width, height+1);
        }
    }
}

void NanoGpu::renderStatusText()
{
    display.setFont(u8g2_font_ncenB10_tr);
    display.drawStr(5,25,status);    
}

void NanoGpu::renderSignal()
{
    display.drawBox(0, 0, signalStrength / 2, 5);
}

void NanoGpu::update()
{
    if (Serial.available())
        processSerial();

    if (millis() > nextDrawTime)
    {
        nextDrawTime = millis() + 20;
        display.clearBuffer();    

        renderSignal();
        // display.drawBox(fpos, 0, payloadSize - fpos, 1);

        // for (int i = 0; i < 128; i+= 2)
        // {
        //     display.drawBox(i, 58, 1, 2);
        // }

        // display.drawBox(checksum[0] * 2, 60, 2, 1);
        // display.drawBox(buffer[payloadSize - 2] * 2, 61, 2, 1);
        // display.drawBox(checksum[1] * 2, 62, 2, 1);
        // display.drawBox(buffer[payloadSize - 1] * 2, 63, 2, 1);

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
}