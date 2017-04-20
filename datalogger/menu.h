#ifndef MENU_H
#define MENU_H

#include <inttypes.h>
#include "menuItem.h"
#include "nanogpu-client.h"

class Menu
{
    int upPin, downPin, enterPin;
    int prevUp, prevDown, prevEnter;
    int upState, downState, enterState;

    int numItems = 0;
    MenuItem* allItems;
    int currentMenuSize = 0;
    MenuItem* currentMenu;
    int currentItemIndex = 0;
    uint16_t currentItemId = 0x0000;
    uint16_t currentParent = 0x0000;

    long nextRender = 0;
    bool hasChanged = true;

    private:
        void setParent(uint16_t newParent);
        void setItem(uint16_t newItem);

        MenuItem* getCurrentItem();
        MenuItem* getItem(uint16_t id);

        char* getCurrentText();
        void updateCurrentMenu();

        void up();
        void down();
        uint16_t enter();

    public:
        Menu(int upPin, int downPin, int enterPin, MenuItem* items, int numItems);
        uint16_t update();
        void render(NanoGpuClient gpu);
        void setText(uint16_t itemId, char newText[]);
};

#endif