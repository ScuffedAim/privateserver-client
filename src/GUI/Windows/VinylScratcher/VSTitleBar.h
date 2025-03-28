//================ Copyright (c) 2014, PG, All rights reserved. =================//
//
// Purpose:		3D flip bar used for music scrolling/searching/play history
//
// $NoKeywords: $
//===============================================================================//

#ifndef VSTITLEBAR_H
#define VSTITLEBAR_H

#include "CBaseUIElement.h"

class McFont;

class CBaseUIContainer;
class CBaseUIButton;

class VSTitleBar : public CBaseUIElement {
   public:
    typedef fastdelegate::FastDelegate0<> SeekCallback;

   public:
    VSTitleBar(int x, int y, int xSize, McFont *font);
    virtual ~VSTitleBar();

    virtual void draw(Graphics *g);
    virtual void mouse_update(bool *propagate_clicks);

    void setSeekCallback(SeekCallback callback) { this->seekCallback = callback; }
    void setTitle(UString title, bool reverse = false);

    inline bool isSeeking() const { return this->bIsSeeking; }

   protected:
    virtual void onResized();
    virtual void onMoved();
    virtual void onFocusStolen();

   private:
    void drawTitle1(Graphics *g);
    void drawTitle2(Graphics *g);

    SeekCallback seekCallback;

    McFont *font;

    CBaseUIContainer *container;

    CBaseUIButton *title;
    CBaseUIButton *title2;

    float fRot;

    int iFlip;

    bool bIsSeeking;
};

#endif
