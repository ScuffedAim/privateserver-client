//================ Copyright (c) 2015, PG, All rights reserved. =================//
//
// Purpose:		individual engine components to compile with
//
// $NoKeywords: $feat
//===============================================================================//

#ifndef ENGINEFEATURES_H
#define ENGINEFEATURES_H

#ifndef MCENGINE_DATA_DIR
#define MCENGINE_DATA_DIR "./"
#endif

#define MCENGINE_APP Osu

/*
 * std::thread/std::mutex support
 */
#ifndef MCENGINE_FEATURE_MULTITHREADING
#define MCENGINE_FEATURE_MULTITHREADING
#endif

/*
 * pthread support
 */
#ifndef MCENGINE_FEATURE_PTHREADS
#define MCENGINE_FEATURE_PTHREADS
#endif

/*
 * OpenGL graphics (Desktop, legacy + modern)
 */
#ifndef MCENGINE_FEATURE_OPENGL
#define MCENGINE_FEATURE_OPENGL
#endif

/*
 * OpenGL graphics (Mobile, ES/EGL)
 */
//#define MCENGINE_FEATURE_OPENGLES

/*
 * DirectX 11 graphics
 */
//#define MCENGINE_FEATURE_DIRECTX11

/*
 * ENet & CURL networking
 */
//#define MCENGINE_FEATURE_NETWORKING

/*
 * BASS sound
 */
#ifndef MCENGINE_FEATURE_SOUND
#define MCENGINE_FEATURE_SOUND
#endif

/*
 * BASS WASAPI sound (Windows only)
 */
#ifdef _WIN32
//#define MCENGINE_FEATURE_BASS_WASAPI
#endif

/*
 * XInput gamepads
 */
//#define MCENGINE_FEATURE_GAMEPAD

/*
 * OpenCL
 */
//#define MCENGINE_FEATURE_OPENCL

/*
 * Vulkan
 */
//#define MCENGINE_FEATURE_VULKAN

/*
 * OpenVR
 */
//#define MCENGINE_FEATURE_OPENVR

/*
 * SDL2 main + Environment
 */
//#define MCENGINE_FEATURE_SDL

/*
 * SDL2 mixer (audio)
 */
//#define MCENGINE_FEATURE_SDL_MIXER

/*
 * Discord RPC (rich presence)
 */
//#define MCENGINE_FEATURE_DISCORD

/*
 * Steam
 */
//#define MCENGINE_FEATURE_STEAMWORKS

#endif
