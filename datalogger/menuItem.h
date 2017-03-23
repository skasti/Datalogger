#ifndef MENUITEM_H
#define MENUITEM_H

#include <inttypes.h>

class MenuItem
{
    uint16_t id;
    uint16_t parentId;
    uint16_t nextId;
    char* text;

    public:
        MenuItem();
        MenuItem(uint16_t id, uint16_t parentId, uint16_t nextId, char text[]);
        uint16_t getId();
        uint16_t getParentId();
        uint16_t getNextId();
        char* getText();
        void setText(char newText[]);
};

#endif