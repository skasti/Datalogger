#include "menuItem.h"

MenuItem::MenuItem()
{
    //DONT USE
}

MenuItem::MenuItem(uint16_t id, uint16_t parentId, uint16_t nextId, char text[])
{
    this->id = id;
    this->parentId = parentId;
    this->nextId = nextId;
    this->text = text;
}

uint16_t MenuItem::getId()
{
    return id;
}

uint16_t MenuItem::getParentId()
{
    return parentId;
}

uint16_t MenuItem::getNextId()
{
    return nextId;
}

char* MenuItem::getText()
{
    return text;
}

void MenuItem::setText(char newText[])
{
    text = newText;
}