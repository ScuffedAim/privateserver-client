//================ Copyright (c) 2019, PG, All rights reserved. =================//
//
// Purpose:		windows sdl environment
//
// $NoKeywords: $sdlwinenv
//===============================================================================//

#ifdef _WIN32

#ifndef WINSDLENVIRONMENT_H
#define WINSDLENVIRONMENT_H

#include "SDLEnvironment.h"

#ifdef MCENGINE_FEATURE_SDL

#include <windows.h>

class WinSDLEnvironment : public SDLEnvironment {
   public:
    WinSDLEnvironment();
    virtual ~WinSDLEnvironment() { ; }

    // system
    virtual OS getOS();
    virtual void sleep(unsigned int us);
    virtual void openURLInDefaultBrowser(UString url);

    // user
    virtual UString getUsername();
    virtual std::string getUserDataPath();

    // file IO
    virtual bool directoryExists(std::string directoryName);
    virtual bool createDirectory(std::string directoryName);
    virtual std::vector<UString> getFilesInFolder(UString folder);
    virtual std::vector<UString> getFoldersInFolder(UString folder);
    virtual std::vector<UString> getLogicalDrives();
    virtual std::string getFolderFromFilePath(std::string filepath);
    virtual std::string getFileNameFromFilePath(std::string filePath);

    // dialogs & message boxes
    virtual UString openFileWindow(const char *filetypefilters, UString title, UString initialpath);
    virtual UString openFolderWindow(UString title, UString initialpath);

   private:
    void path_strip_filename(TCHAR *Path);
};

#endif

#endif

#endif
