#include "nextionDisplay.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif


void NextionDisplay::setup() 
{
    Nextion.begin(9600);
}

void NextionDisplay::sendEOL()
{
    Nextion.write(0xFF);
    Nextion.write(0xFF);
    Nextion.write(0xFF);
}

void NextionDisplay::sendCommand(char command[])
{
    Nextion.print(command);
    sendEOL();
}

void NextionDisplay::sendValue(char componentName[], char value[])
{
    Nextion.print(componentName);
    Nextion.print(".txt=\"");
    Nextion.print(value);
    Nextion.print("\"");
    sendEOL();
}

void NextionDisplay::sendValue(char componentName[], int value)
{
    Nextion.print(componentName);
    Nextion.print(".val=");
    Nextion.print(value);
    sendEOL();
}

void NextionDisplay::debug(char text[])
{
    sendValue("debug", text);
}

void NextionDisplay::extractCommand(int length)
{
    char commandBuffer[length + 1];

    for(int i = 1; i < length+2; i++)
        commandBuffer[i- 1] = buffer[i];

    command = String(commandBuffer);
}

bool NextionDisplay::hasCommand()
{
    while(Nextion.available())
    {
        byte c = Nextion.read();
        buffer[bufPos] = c;

        if (c == 0xFF)
        {
            eolCounter++;

            if (eolCounter >= 3)
            {
                buffer[bufPos - 0] = 0x00;
                buffer[bufPos - 1] = 0x00;
                buffer[bufPos - 2] = 0x00;

                if (buffer[0] == 'p')
                {
                    extractCommand(bufPos-3);
                    
                    bufPos = 0;
                    eolCounter = 0;
                    return true;
                }
            }
        }
        else
        {
            eolCounter = 0;
        }

        bufPos++;

        if (bufPos >= 40)
        {
            debug("HICK");
            bufPos = 0;
            eolCounter = 0;
        }
    }

    return false;
}

String NextionDisplay::getCommand()
{
    return command;
}