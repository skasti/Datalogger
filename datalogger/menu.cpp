#include "menu.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

Menu::Menu(int upPin, int downPin, int enterPin, MenuItem* items, int numItems)
{
    this->upPin = upPin;
    this->downPin = downPin;
    this->enterPin = enterPin;

    pinMode(upPin, INPUT_PULLUP);
    pinMode(downPin, INPUT_PULLUP);
    pinMode(enterPin, INPUT_PULLUP);

    pinMode(13, OUTPUT);

    this->allItems = items;
    this->numItems = numItems;

    this->setParent(0x0000);
}

void Menu::updateCurrentMenu()
{
    int num = 0;

    for (int i = 0; i < numItems; i++)
    {
        if (allItems[i].getParentId() == currentParent)
            num++;
    }

    currentMenu = new MenuItem[num];
    int index = 0;

    for (int i = 0; i < numItems; i++)
    {
        if (allItems[i].getParentId() == currentParent)
        {
            currentMenu[index++] = allItems[i];
        }
    }

    currentMenuSize = num;
}

void Menu::setParent(uint16_t newParentId)
{
    currentParent = newParentId;
    updateCurrentMenu();
    currentItemId = currentMenu[0].getId();
    currentItemIndex = 0;
}

void Menu::setItem(uint16_t newItemId)
{
    MenuItem* newItem = getItem(newItemId);

    currentParent = newItem->getParentId();
    updateCurrentMenu();
    currentItemId = newItemId;

    for (int i = 0; i < currentMenuSize; i++)
    {
        if (currentMenu[i].getId() == newItemId)
        {
            currentItemIndex = i;
            return;
        }
    }
}


MenuItem* Menu::getCurrentItem()
{
    return getItem(currentItemId);
}

MenuItem* Menu::getItem(uint16_t id)
{
    for (int i = 0; i < numItems; i++)
    {
        if (allItems[i].getId() == id)
            return &allItems[i];
    }
}

char* Menu::getCurrentText()
{
    return getCurrentItem()->getText();
}

void Menu::up()
{
    if (currentItemIndex == 0)
        return;
    else if (currentItemIndex < 0)
    {
        currentItemIndex = 0;
        currentItemId = currentMenu[0].getId();
    }

    currentItemIndex--;
    currentItemId = currentMenu[currentItemIndex].getId();
}

void Menu::down()
{
    if (currentItemIndex == currentMenuSize - 1)
        return;
    else if (currentItemIndex >= currentMenuSize)
    {
        currentItemIndex = 0;
        currentItemId = currentMenu[0].getId();
        return;
    }

    currentItemIndex++;
    currentItemId = currentMenu[currentItemIndex].getId();
}

uint16_t Menu::enter()
{
    MenuItem* now = getCurrentItem();
    uint16_t nextId = now->getNextId();

    if (nextId == 0x0000) //Return to main menu
    {
        setParent(0x0000);
    }
    else
    {
        setParent(nextId);
    }

    return now->getId();
}

uint16_t Menu::update()
{
    prevUp = upState;
    prevDown = downState;
    prevEnter = enterState;

    upState = digitalRead(upPin);
    downState = digitalRead(downPin);
    enterState = digitalRead(enterPin);

    if (upState == LOW && prevUp == HIGH) 
    {
        up();
        Serial.print("UP: ");
        Serial.println(currentItemIndex);
    }
    if (downState == LOW && prevDown == HIGH) 
    {
        down();
        Serial.print("DOWN: ");
        Serial.println(currentItemIndex);
    }
    if (enterState == LOW && prevEnter == HIGH) 
    {
        uint16_t result =  enter();     
        
        Serial.print("ENTER: ");
        Serial.println(currentParent);

        return result;
    }

    return 0x0000;
}

void Menu::render(NanoGpuClient gpu)
{
    if (millis() > nextRender)
    {
        gpu.sendStatus(getCurrentText());
        nextRender = millis() + 60;
    }
}

void Menu::setText(uint16_t itemId, char newText[])
{
    getItem(itemId)->setText(newText);
}