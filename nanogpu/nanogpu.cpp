#include "nanogpu.h"
#include "coms.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

void NanoGpu::setup()
{
    Serial.begin(9600);
    display.begin();
}

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
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_MODE) + 1;
                    break;
                case PKG_STATUS_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_STATUS) + 1;
                    break;
                case PKG_VALUES_ID:
                    fpos++;
                    payloadSize = sizeof(GPU_HEADER) + sizeof(PKG_VALUES) + 1;
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
                ((unsigned char*)(&buffer))[fpos-sizeof(GPU_HEADER) - 1] = c;
                fpos++;

                if (fpos == payloadSize)
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
                        default:
                            break;
                    }

                    fpos = 0;
                    return;
                }
            } 
            else
            {
                fpos = 0;
                return;
            }
        }
    }
}

void NanoGpu::updateMode()
{
    PKG_MODE modePackage = *((PKG_MODE*)(&buffer));
    mode = (GPU_MODE) modePackage.mode;
    Serial.print("Changed to mode ");
    Serial.println(mode);
}

void NanoGpu::updateValues() 
{
    PKG_VALUES valuePackage = *((PKG_VALUES*)(&buffer));

    for (int i = 0; i < 10; i++)
    {
        values[i] = valuePackage.values[i];
        Serial.print("values[");
        Serial.print(i);
        Serial.print("] = ");
        Serial.println(valuePackage.values[i]);
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

    Serial.print("Changed status ");
    Serial.println(status);
}

void NanoGpu::renderValues()
{
    for (int i = 0; i < 10; i++)
    {
        int height = (int)((double)values[i] * 0.058);
        display.drawBox(i * 12, 64-height, 10, height+1);
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