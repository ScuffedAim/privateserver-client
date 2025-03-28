//================ Copyright (c) 2015, PG, All rights reserved. =================//
//
// Purpose:		app base class (v3)
//
// $NoKeywords: $appb
//===============================================================================//

#ifndef APP_H
#define APP_H

#include "KeyboardListener.h"
#include "cbase.h"

class Engine;

class App : public KeyboardListener {
   public:
    App() { ; }
    virtual ~App() { ; }

    virtual void draw(Graphics *g) { (void)g; }
    virtual void update() { ; }

    virtual void onKeyDown(KeyboardEvent &e) { (void)e; }
    virtual void onKeyUp(KeyboardEvent &e) { (void)e; }
    virtual void onChar(KeyboardEvent &e) { (void)e; }
    virtual void stealFocus() { ; }

    virtual void onResolutionChanged(Vector2 newResolution) { (void)newResolution; }
    virtual void onDPIChanged() { ; }

    virtual void onFocusGained() { ; }
    virtual void onFocusLost() { ; }

    virtual void onMinimized() { ; }
    virtual void onRestored() { ; }

    virtual bool onShutdown() { return true; }
};

#endif
