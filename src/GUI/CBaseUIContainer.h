#pragma once
#include "CBaseUIElement.h"

class CBaseUIContainer : public CBaseUIElement {
   public:
    CBaseUIContainer(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = "");
    virtual ~CBaseUIContainer();

    void clear();
    void empty();

    void draw_debug(Graphics *g);
    virtual void draw(Graphics *g);
    virtual void mouse_update(bool *propagate_clicks);

    void onKeyUp(KeyboardEvent &e);
    void onKeyDown(KeyboardEvent &e);
    void onChar(KeyboardEvent &e);

    CBaseUIContainer *addBaseUIElement(CBaseUIElement *element, float xPos, float yPos);
    CBaseUIContainer *addBaseUIElement(CBaseUIElement *element);
    CBaseUIContainer *addBaseUIElementBack(CBaseUIElement *element, float xPos, float yPos);
    CBaseUIContainer *addBaseUIElementBack(CBaseUIElement *element);

    CBaseUIContainer *insertBaseUIElement(CBaseUIElement *element, CBaseUIElement *index);
    CBaseUIContainer *insertBaseUIElementBack(CBaseUIElement *element, CBaseUIElement *index);

    CBaseUIContainer *removeBaseUIElement(CBaseUIElement *element);
    CBaseUIContainer *deleteBaseUIElement(CBaseUIElement *element);

    CBaseUIElement *getBaseUIElement(UString name);

    inline const std::vector<CBaseUIElement *> &getElements() const { return this->vElements; }

    virtual void onMoved() { this->update_pos(); }
    virtual void onResized() { this->update_pos(); }

    virtual bool isBusy();
    virtual bool isActive();

    void onMouseDownOutside();

    virtual void onFocusStolen();
    virtual void onEnabled();
    virtual void onDisabled();

    void update_pos();

   protected:
    std::vector<CBaseUIElement *> vElements;
};
