//================ Copyright (c) 2019, PG, All rights reserved. =================//
//
// Purpose:		windows sdl opengl es 2.x interface
//
// $NoKeywords: $winsdlgles2i
//===============================================================================//

#ifdef _WIN32

#ifndef WINSDLGLES2INTERFACE_H
#define WINSDLGLES2INTERFACE_H

#include "cbase.h"

#if defined(MCENGINE_FEATURE_SDL) && defined(MCENGINE_FEATURE_OPENGLES)

#include "SDLGLES2Interface.h"

class WinSDLGLES2Interface : public SDLGLES2Interface {
   public:
    WinSDLGLES2Interface(SDL_Window *window);
    virtual ~WinSDLGLES2Interface();
};

#endif

#endif

#endif
