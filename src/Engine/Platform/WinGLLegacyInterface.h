//================ Copyright (c) 2014, PG, All rights reserved. =================//
//
// Purpose:		windows opengl interface
//
// $NoKeywords: $wingli
//===============================================================================//

#ifdef _WIN32

#ifndef WINGLINTERFACE_H
#define WINGLINTERFACE_H

#include "OpenGLLegacyInterface.h"

#ifdef MCENGINE_FEATURE_OPENGL

#include <Windows.h>

class ConVar;

PIXELFORMATDESCRIPTOR getPixelFormatDescriptor();
bool initWinGLMultisample(HDC hDC, HINSTANCE hInstance, HWND hWnd, int factor);

struct FAKE_CONTEXT
{
	HGLRC hglrc;
	HDC hdc;
};

class WinGLLegacyInterface : public OpenGLLegacyInterface
{
public:
	static FAKE_CONTEXT createAndMakeCurrentWGLContext(HWND hwnd, PIXELFORMATDESCRIPTOR pfdIn);

public:
	WinGLLegacyInterface(HWND hwnd);
	virtual ~WinGLLegacyInterface();

	// scene
	void endScene();

	// device settings
	void setVSync(bool vsync);

	// ILLEGAL:
	bool checkGLHardwareAcceleration();
	inline HGLRC getGLContext() const {return m_hglrc;}
	inline HDC getGLHDC() const {return m_hdc;}

private:
	// device context
	HWND m_hwnd;
	HGLRC m_hglrc;
	HDC m_hdc;
};

#endif

#endif

#endif
