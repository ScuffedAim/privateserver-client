#ifdef _WIN32

#include "WinEnvironment.h"

#include <Commdlg.h>
#include <Lmcons.h>
#include <Shlobj.h>
#include <shellapi.h>
#include <tchar.h>

#include <filesystem>
#include <string>

#include "ConVar.h"
#include "Engine.h"
#include "Mouse.h"
#include "NullGraphicsInterface.h"
#include "WinContextMenu.h"
#include "WinGL3Interface.h"
#include "WinGLLegacyInterface.h"
#include "cbase.h"

using namespace std;

bool g_bCursorVisible = true;

bool WinEnvironment::bResizable = true;
std::vector<McRect> WinEnvironment::vMonitors;
int WinEnvironment::iNumCoresForProcessAffinity = -1;
HHOOK WinEnvironment::hKeyboardHook = NULL;

WinEnvironment::WinEnvironment(HWND hwnd, HINSTANCE hinstance) : Environment() {
    this->hwnd = hwnd;
    this->hInstance = hinstance;

    this->mouseCursor = LoadCursor(NULL, IDC_ARROW);
    this->bFullScreen = false;
    this->vWindowSize = getWindowSize();
    this->bCursorClipped = false;

    this->iDPIOverride = -1;
    this->bIsRestartScheduled = false;

    // init
    enumerateMonitors();

    if(this->vMonitors.size() < 1) {
        debugLog("WARNING: No monitors found! Adding default monitor ...\n");
        this->vMonitors.push_back(McRect(0, 0, this->vWindowSize.x, this->vWindowSize.y));
    }

    // convar callbacks
    cv_win_processpriority.setCallback(fastdelegate::MakeDelegate(this, &WinEnvironment::onProcessPriorityChange));
    cv_win_disable_windows_key.setCallback(
        fastdelegate::MakeDelegate(this, &WinEnvironment::onDisableWindowsKeyChange));

    setProcessPriority(cv_win_processpriority.getInt());
}

WinEnvironment::~WinEnvironment() { enableWindowsKey(); }

void WinEnvironment::update() {
    this->bIsCursorInsideWindow =
        McRect(0, 0, engine->getScreenWidth(), engine->getScreenHeight()).contains(getMousePos());

    // auto reset cursor
    if(!this->bHasCursorTypeChanged) {
        setCursor(CURSORTYPE::CURSOR_NORMAL);
    }
    this->bHasCursorTypeChanged = false;
}

Graphics *WinEnvironment::createRenderer() {
    return new WinGLLegacyInterface(this->hwnd);
    // return new WinGL3Interface(this->hwnd);
}

ContextMenu *WinEnvironment::createContextMenu() { return new WinContextMenu(); }

Environment::OS WinEnvironment::getOS() { return Environment::OS::WINDOWS; }

void WinEnvironment::shutdown() { SendMessage(this->hwnd, WM_CLOSE, 0, 0); }

void WinEnvironment::restart() {
    this->bIsRestartScheduled = true;
    shutdown();
}

void WinEnvironment::sleep(unsigned int us) { Sleep(us / 1000); }

UString WinEnvironment::getUsername() {
    const DWORD idiot_msvc_len = UNLEN + 1;
    DWORD username_len = idiot_msvc_len;
    wchar_t username[idiot_msvc_len];

    if(GetUserNameW(username, &username_len)) return UString(username);

    return UString("");
}

std::string WinEnvironment::getUserDataPath() {
    char path[PATH_MAX];

    if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) return std::string(path);

    return std::string("");
}

std::string WinEnvironment::getExecutablePath() {
    char path[MAX_PATH];

    if(GetModuleFileName(NULL, path, MAX_PATH)) return std::string(path);

    return std::string("");
}

bool WinEnvironment::fileExists(std::string filename) {
    WIN32_FIND_DATA FindFileData;
    HANDLE handle = FindFirstFile(filename.c_str(), &FindFileData);
    if(handle == INVALID_HANDLE_VALUE)
        return std::ifstream(std::filesystem::u8path(filename.c_str())).good();
    else {
        FindClose(handle);
        return true;
    }
}

bool WinEnvironment::directoryExists(std::string filename) {
    DWORD dwAttrib = GetFileAttributes(filename.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool WinEnvironment::createDirectory(std::string directoryName) { return CreateDirectory(directoryName.c_str(), NULL); }

bool WinEnvironment::renameFile(std::string oldFileName, std::string newFileName) {
    return MoveFile(oldFileName.c_str(), newFileName.c_str());
}

bool WinEnvironment::deleteFile(std::string filePath) { return DeleteFile(filePath.c_str()); }

UString WinEnvironment::getClipBoardText() {
    UString result = "";
    HANDLE clip = NULL;
    if(OpenClipboard(NULL)) {
        clip = GetClipboardData(CF_UNICODETEXT);
        wchar_t *pchData = (wchar_t *)GlobalLock(clip);

        if(pchData != NULL) result = UString(pchData);

        GlobalUnlock(clip);
        CloseClipboard();
    }
    return result;
}

void WinEnvironment::setClipBoardText(UString text) {
    if(text.length() < 1) return;

    if(OpenClipboard(NULL) && EmptyClipboard()) {
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, ((text.length() + 1) * sizeof(WCHAR)));

        if(hMem == NULL) {
            debugLog("ERROR: hglbCopy == NULL!\n");
            CloseClipboard();
            return;
        }

        memcpy(GlobalLock(hMem), text.wc_str(), (text.length() + 1) * sizeof(WCHAR));
        GlobalUnlock(hMem);

        SetClipboardData(CF_UNICODETEXT, hMem);

        CloseClipboard();
    }
}

void WinEnvironment::openURLInDefaultBrowser(UString url) {
    ShellExecuteW(this->hwnd, L"open", url.wc_str(), NULL, NULL, SW_SHOW);
}

void WinEnvironment::openDirectory(std::string path) {
    UString wpath(path.c_str());
    ShellExecuteW(this->hwnd, L"open", wpath.wc_str(), NULL, NULL, SW_SHOW);
}

UString WinEnvironment::openFileWindow(const char *filetypefilters, UString title, UString initialpath) {
    disableFullscreen();

    OPENFILENAME fn;
    ZeroMemory(&fn, sizeof(fn));

    char fileNameBuffer[255];

    // fill it
    fn.lStructSize = sizeof(fn);
    fn.hwndOwner = NULL;
    fn.lpstrFile = fileNameBuffer;
    fn.lpstrFile[0] = '\0';
    fn.nMaxFile = sizeof(fileNameBuffer);
    fn.lpstrFilter = filetypefilters;
    fn.nFilterIndex = 1;
    fn.lpstrFileTitle = NULL;
    fn.nMaxFileTitle = 0;
    fn.lpstrTitle = title.length() > 1 ? title.toUtf8() : NULL;
    fn.lpstrInitialDir = initialpath.length() > 1 ? initialpath.toUtf8() : NULL;
    fn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_ENABLESIZING;

    // open the dialog
    GetOpenFileName(&fn);

    return fn.lpstrFile;
}

UString WinEnvironment::openFolderWindow(UString title, UString initialpath) {
    disableFullscreen();

    OPENFILENAME fn;
    ZeroMemory(&fn, sizeof(fn));

    char fileNameBuffer[255];

    fileNameBuffer[0] = 's';
    fileNameBuffer[1] = 'k';
    fileNameBuffer[2] = 'i';
    fileNameBuffer[3] = 'n';
    fileNameBuffer[4] = '.';
    fileNameBuffer[5] = 'i';
    fileNameBuffer[6] = 'n';
    fileNameBuffer[7] = 'i';
    fileNameBuffer[8] = '\0';

    // fill it
    fn.lStructSize = sizeof(fn);
    fn.hwndOwner = NULL;
    fn.lpstrFile = fileNameBuffer;
    /// fn.lpstrFile[0] = '\0';
    fn.nMaxFile = sizeof(fileNameBuffer);
    fn.nFilterIndex = 1;
    fn.lpstrFileTitle = NULL;
    fn.lpstrTitle = title.length() > 1 ? title.toUtf8() : NULL;
    fn.lpstrInitialDir = initialpath.length() > 1 ? initialpath.toUtf8() : NULL;
    fn.Flags = OFN_PATHMUSTEXIST | OFN_ENABLESIZING;

    // open the dialog
    GetOpenFileName(&fn);

    return fn.lpstrFile;
}

std::vector<std::string> WinEnvironment::getFilesInFolder(std::string folder) {
    // Since we want to avoid wide strings in the codebase as much as possible,
    // we convert wide paths to UTF-8 (as they fucking should be).
    // We can't just use FindFirstFileA, because then any path with unicode
    // characters will fail to open!
    // Keep in mind that windows can't handle the way too modern 1993 UTF-8, so
    // you have to use std::filesystem::u8path() or convert it back to a wstring
    // before using the windows API.

    folder.append("*.*");
    WIN32_FIND_DATAW data;
    std::wstring buffer;
    std::vector<std::string> files;

    int size = MultiByteToWideChar(CP_UTF8, 0, folder.c_str(), folder.length(), NULL, 0);
    std::wstring wfile(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, folder.c_str(), folder.length(), (LPWSTR)wfile.c_str(), wfile.length());

    HANDLE handle = FindFirstFileW(wfile.c_str(), &data);

    while(true) {
        std::wstring filename(data.cFileName);
        if(filename == buffer) break;

        buffer = filename;

        if(filename.length() > 0) {
            if((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                int size = WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), filename.length(), NULL, 0, NULL, NULL);
                std::string utf8filename(size, 0);
                WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), size, (LPSTR)utf8filename.c_str(), size, NULL, NULL);
                files.push_back(utf8filename);
            }
        }

        FindNextFileW(handle, &data);
    }

    FindClose(handle);
    return files;
}

std::vector<std::string> WinEnvironment::getFoldersInFolder(std::string folder) {
    // Since we want to avoid wide strings in the codebase as much as possible,
    // we convert wide paths to UTF-8 (as they fucking should be).
    // We can't just use FindFirstFileA, because then any path with unicode
    // characters will fail to open!
    // Keep in mind that windows can't handle the way too modern 1993 UTF-8, so
    // you have to use std::filesystem::u8path() or convert it back to a wstring
    // before using the windows API.

    folder.append("*.*");
    WIN32_FIND_DATAW data;
    std::wstring buffer;
    std::vector<std::string> folders;

    int size = MultiByteToWideChar(CP_UTF8, 0, folder.c_str(), folder.length(), NULL, 0);
    std::wstring wfolder(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, folder.c_str(), folder.length(), (LPWSTR)wfolder.c_str(), wfolder.length());

    HANDLE handle = FindFirstFileW(wfolder.c_str(), &data);

    while(true) {
        std::wstring filename(data.cFileName);
        if(filename == buffer) break;

        buffer = filename;

        if(filename.length() > 0 && filename.compare(L".") != 0 && filename.compare(L"..") != 0) {
            if((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                int size = WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), filename.length(), NULL, 0, NULL, NULL);
                std::string utf8filename(size, 0);
                WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), size, (LPSTR)utf8filename.c_str(), size, NULL, NULL);
                folders.push_back(utf8filename);
            }
        }

        FindNextFileW(handle, &data);
    }

    FindClose(handle);
    return folders;
}

std::vector<UString> WinEnvironment::getLogicalDrives() {
    std::vector<UString> drives;
    DWORD dwDrives = GetLogicalDrives();

    for(int i = 0; i < 26; i++) {
        // 26 letters in [A..Z] range
        if(dwDrives & 1) {
            UString driveName = UString::format("%c", 'A' + i);
            UString driveExecName = driveName;
            driveExecName.append(":/");

            UString driveNameForGetDriveFunction = driveName;
            driveNameForGetDriveFunction.append(":\\");

            DWORD attributes = GetFileAttributes(driveNameForGetDriveFunction.toUtf8());

            // debugLog("checking %s, type = %i, free clusters = %lu\n", driveNameForGetDriveFunction.toUtf8(),
            // GetDriveType(driveNameForGetDriveFunction.toUtf8()), attributes);

            // check if the drive is valid, and if there is media in it (e.g. ignore empty dvd drives)
            if(GetDriveType(driveNameForGetDriveFunction.toUtf8()) > DRIVE_NO_ROOT_DIR &&
               attributes != INVALID_FILE_ATTRIBUTES) {
                if(driveExecName.length() > 0) drives.push_back(driveExecName);
            }
        }

        dwDrives >>= 1;  // next bit
    }

    return drives;
}

std::string WinEnvironment::getFolderFromFilePath(std::string filepath) {
    char *aString = (char *)filepath.c_str();
    path_strip_filename(aString);
    return aString;
}

std::string WinEnvironment::getFileExtensionFromFilePath(std::string filepath, bool includeDot) {
    int idx = filepath.find_last_of('.');
    if(idx != -1)
        return filepath.substr(idx + 1);
    else
        return std::string("");
}

std::string WinEnvironment::getFileNameFromFilePath(std::string filePath) {
    if(filePath.length() < 1) return filePath;

    const size_t lastSlashIndex = filePath.find_last_of('/');
    const size_t lastBackSlashIndex = filePath.find_last_of('\\');
    size_t idx = 0;
    if(lastSlashIndex != std::string::npos) idx = lastSlashIndex + 1;
    if(lastBackSlashIndex != std::string::npos) idx = max(idx, lastBackSlashIndex + 1);

    return filePath.substr(idx);
}

void WinEnvironment::showMessageInfo(UString title, UString message) {
    bool wasFullscreen = this->bFullScreen;
    handleShowMessageFullscreen();
    MessageBoxW(this->hwnd, message.wc_str(), title.wc_str(), MB_ICONINFORMATION | MB_OK);
    if(wasFullscreen) {
        maximize();
        enableFullscreen();
    }
}

void WinEnvironment::showMessageWarning(UString title, UString message) {
    bool wasFullscreen = this->bFullScreen;
    handleShowMessageFullscreen();
    MessageBoxW(this->hwnd, message.wc_str(), title.wc_str(), MB_ICONWARNING | MB_OK);
    if(wasFullscreen) {
        maximize();
        enableFullscreen();
    }
}

void WinEnvironment::showMessageError(UString title, UString message) {
    bool wasFullscreen = this->bFullScreen;
    handleShowMessageFullscreen();
    MessageBoxW(this->hwnd, message.wc_str(), title.wc_str(), MB_ICONERROR | MB_OK);
    if(wasFullscreen) {
        maximize();
        enableFullscreen();
    }
}

void WinEnvironment::showMessageErrorFatal(UString title, UString message) {
    bool wasFullscreen = this->bFullScreen;
    handleShowMessageFullscreen();
    MessageBox(this->hwnd, message.toUtf8(), title.toUtf8(), MB_ICONSTOP | MB_OK);
    if(wasFullscreen) {
        maximize();
        enableFullscreen();
    }
}

void WinEnvironment::focus() { SetForegroundWindow(this->hwnd); }

void WinEnvironment::center() {
    RECT clientRect;
    GetClientRect(this->hwnd, &clientRect);

    // get nearest monitor and center on that, build windowed pos + size
    const McRect desktopRect = getDesktopRect();
    int width = std::abs(clientRect.right - clientRect.left);
    int height = std::abs(clientRect.bottom - clientRect.top);
    int xPos = desktopRect.getX() + (desktopRect.getWidth() / 2) - (int)(width / 2);
    int yPos = desktopRect.getY() + (desktopRect.getHeight() / 2) - (int)(height / 2);

    // calculate window size for client size (to respect borders etc.)
    RECT serverRect;
    serverRect.left = xPos;
    serverRect.top = yPos;
    serverRect.right = xPos + width;
    serverRect.bottom = yPos + height;
    AdjustWindowRect(&serverRect, getWindowStyleWindowed(), FALSE);

    // set window pos as prev pos, apply it
    xPos = serverRect.left;
    yPos = serverRect.top;
    width = std::abs(serverRect.right - serverRect.left);
    height = std::abs(serverRect.bottom - serverRect.top);
    this->vLastWindowPos.x = xPos;
    this->vLastWindowPos.y = yPos;
    MoveWindow(this->hwnd, xPos, yPos, width, height, FALSE);  // non-client width/height!
}

void WinEnvironment::minimize() { ShowWindow(this->hwnd, SW_MINIMIZE); }

void WinEnvironment::maximize() { ShowWindow(this->hwnd, SW_MAXIMIZE); }

void WinEnvironment::enableFullscreen() {
    if(this->bFullScreen) return;

    // backup prev window pos
    RECT rect;
    GetWindowRect(this->hwnd, &rect);
    this->vLastWindowPos.x = rect.left;
    this->vLastWindowPos.y = rect.top;

    // backup prev client size
    RECT clientRect;
    GetClientRect(this->hwnd, &clientRect);
    this->vLastWindowSize.x = std::abs(clientRect.right - clientRect.left);
    this->vLastWindowSize.y = std::abs(clientRect.bottom - clientRect.top);

    // get nearest monitor, build fullscreen resolution
    const McRect desktopRect = getDesktopRect();
    const int width = desktopRect.getWidth();
    const int height = desktopRect.getHeight() + (this->bFullscreenWindowedBorderless ? 1 : 0);

    // and apply everything (move + resize)
    SetWindowLongPtr(this->hwnd, GWL_STYLE, WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE);
    MoveWindow(this->hwnd, (int)(desktopRect.getX()), (int)(desktopRect.getY()), width, height, FALSE);

    this->bFullScreen = true;
}

void WinEnvironment::disableFullscreen() {
    if(!this->bFullScreen) return;

    // Fetch window size from cvar string
    f32 width = 1280.f;
    f32 height = 720.f;
    {
        UString res = cv_windowed_resolution.getString();
        if(res.length() >= 7) {
            std::vector<UString> resolution = res.split("x");
            if(resolution.size() == 2) {
                int w = resolution[0].toFloat();
                int h = resolution[1].toFloat();
                if(w >= 300 && h >= 240) {
                    width = (f32)w;
                    height = (f32)h;
                }
            }
        }
    }

    // clamp prev window client size to monitor
    const McRect desktopRect = getDesktopRect();
    this->vLastWindowSize.x = min(width, desktopRect.getWidth());
    this->vLastWindowSize.y = min(height, desktopRect.getHeight());

    // request window size based on prev client size
    RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = this->vLastWindowSize.x;
    rect.bottom = this->vLastWindowSize.y;
    AdjustWindowRect(&rect, getWindowStyleWindowed(), FALSE);

    // build new size, set it as the current size
    this->vWindowSize.x = std::abs(rect.right - rect.left);
    this->vWindowSize.y = std::abs(rect.bottom - rect.top);

    // HACKHACK: double MoveWindow is a workaround for a windows bug (otherwise overscale would get clamped to taskbar)
    SetWindowLongPtr(this->hwnd, GWL_STYLE, getWindowStyleWindowed());
    MoveWindow(this->hwnd, (int)this->vLastWindowPos.x, (int)this->vLastWindowPos.y, (int)this->vWindowSize.x,
               (int)this->vWindowSize.y,
               FALSE);  // non-client width/height!
    MoveWindow(this->hwnd, (int)this->vLastWindowPos.x, (int)this->vLastWindowPos.y, (int)this->vWindowSize.x,
               (int)this->vWindowSize.y,
               FALSE);  // non-client width/height!

    this->bFullScreen = false;
}

void WinEnvironment::setWindowTitle(UString title) { SetWindowText(this->hwnd, title.toUtf8()); }

void WinEnvironment::setWindowPos(int x, int y) {
    SetWindowPos(this->hwnd, this->hwnd, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
}

void WinEnvironment::setWindowSize(int width, int height) {
    // backup last window pos
    RECT rect;
    GetWindowRect(this->hwnd, &rect);
    this->vLastWindowPos.x = rect.left;
    this->vLastWindowPos.y = rect.top;

    // remember prev client size
    this->vLastWindowSize.x = width;
    this->vLastWindowSize.y = height;

    // request window size based on client size
    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;
    AdjustWindowRect(&rect, getWindowStyleWindowed(), FALSE);

    // build new size, set it as the current size
    this->vWindowSize.x = std::abs(rect.right - rect.left);
    this->vWindowSize.y = std::abs(rect.bottom - rect.top);

    MoveWindow(this->hwnd, (int)this->vLastWindowPos.x, (int)this->vLastWindowPos.y, (int)this->vWindowSize.x,
               (int)this->vWindowSize.y,
               FALSE);  // non-client width/height!
}

void WinEnvironment::setWindowResizable(bool resizable) {
    this->bResizable = resizable;
    SetWindowLongPtr(this->hwnd, GWL_STYLE, getWindowStyleWindowed());
}

void WinEnvironment::setMonitor(int monitor) {
    monitor = clamp<int>(monitor, 0, this->vMonitors.size() - 1);
    if(monitor == getMonitor()) return;

    const McRect desktopRect = this->vMonitors[monitor];
    const bool wasFullscreen = this->bFullScreen;

    if(wasFullscreen) disableFullscreen();

    // build new window size, clamp to monitor size (otherwise the borders would be hidden offscreen)
    RECT windowRect;
    GetWindowRect(this->hwnd, &windowRect);
    const Vector2 windowSize = Vector2(std::abs((int)(windowRect.right - windowRect.left)),
                                       std::abs((int)(windowRect.bottom - windowRect.top)));
    const int width = min((int)windowSize.x, (int)desktopRect.getWidth());
    const int height = min((int)windowSize.y, (int)desktopRect.getHeight());

    // move and resize, force center
    MoveWindow(this->hwnd, desktopRect.getX(), desktopRect.getY(), width, height, FALSE);  // non-client width/height!
    center();

    if(wasFullscreen) enableFullscreen();
}

Vector2 WinEnvironment::getWindowPos() {
    POINT p;
    p.x = 0;
    p.y = 0;
    ClientToScreen(this->hwnd,
                   &p);  // this respects the window border, because the engine only works in client coordinates
    return Vector2(p.x, p.y);
}

Vector2 WinEnvironment::getWindowSize() {
    RECT clientRect;
    GetClientRect(this->hwnd, &clientRect);
    return Vector2(clientRect.right, clientRect.bottom);
}

int WinEnvironment::getMonitor() {
    const McRect desktopRect = getDesktopRect();

    for(size_t i = 0; i < this->vMonitors.size(); i++) {
        if(((int)this->vMonitors[i].getX()) == ((int)desktopRect.getX()) &&
           ((int)this->vMonitors[i].getY()) == ((int)desktopRect.getY()))
            return i;
    }

    debugLog("WARNING: Environment::getMonitor() found no matching monitor, returning default monitor ...\n");
    return 0;
}

std::vector<McRect> WinEnvironment::getMonitors() { return this->vMonitors; }

Vector2 WinEnvironment::getNativeScreenSize() {
    const McRect desktopRect = getDesktopRect();
    return Vector2(desktopRect.getWidth(), desktopRect.getHeight());
}

McRect WinEnvironment::getVirtualScreenRect() {
    return McRect(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                  GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
}

McRect WinEnvironment::getDesktopRect() {
    HMONITOR monitor = MonitorFromWindow(this->hwnd, MONITOR_DEFAULTTONEAREST);

    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);

    GetMonitorInfo(monitor, &info);

    return McRect(info.rcMonitor.left, info.rcMonitor.top, std::abs(info.rcMonitor.left - info.rcMonitor.right),
                  std::abs(info.rcMonitor.top - info.rcMonitor.bottom));
}

int WinEnvironment::getDPI() {
    if(this->iDPIOverride > 0) return this->iDPIOverride;

    HDC hdc = GetDC(this->hwnd);
    if(hdc != NULL) {
        const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(this->hwnd, hdc);
        return clamp<int>(dpi, 96, 96 * 4);  // sanity clamp
    } else
        return 96;
}

bool WinEnvironment::isCursorInWindow() { return this->bIsCursorInsideWindow; }

bool WinEnvironment::isCursorClipped() { return this->bCursorClipped; }

bool WinEnvironment::isCursorVisible() { return g_bCursorVisible; }

Vector2 WinEnvironment::getMousePos() {
    POINT mpos;
    GetCursorPos(&mpos);
    ScreenToClient(this->hwnd, &mpos);
    return Vector2(mpos.x, mpos.y);
}

McRect WinEnvironment::getCursorClip() { return this->cursorClip; }

CURSORTYPE WinEnvironment::getCursor() { return this->cursorType; }

void WinEnvironment::setCursor(CURSORTYPE cur) {
    this->bHasCursorTypeChanged = true;

    if(cur == this->cursorType) return;

    this->cursorType = cur;

    switch(cur) {
        case CURSORTYPE::CURSOR_NORMAL:
            this->mouseCursor = LoadCursor(NULL, IDC_ARROW);
            break;
        case CURSORTYPE::CURSOR_WAIT:
            this->mouseCursor = LoadCursor(NULL, IDC_WAIT);
            break;
        case CURSORTYPE::CURSOR_SIZE_H:
            this->mouseCursor = LoadCursor(NULL, IDC_SIZEWE);
            break;
        case CURSORTYPE::CURSOR_SIZE_V:
            this->mouseCursor = LoadCursor(NULL, IDC_SIZENS);
            break;
        case CURSORTYPE::CURSOR_SIZE_HV:
            this->mouseCursor = LoadCursor(NULL, IDC_SIZENESW);
            break;
        case CURSORTYPE::CURSOR_SIZE_VH:
            this->mouseCursor = LoadCursor(NULL, IDC_SIZENWSE);
            break;
        case CURSORTYPE::CURSOR_TEXT:
            this->mouseCursor = LoadCursor(NULL, IDC_IBEAM);
            break;
        default:
            this->mouseCursor = LoadCursor(NULL, IDC_ARROW);
            break;
    }

    SetCursor(this->mouseCursor);
}

void WinEnvironment::setCursorVisible(bool visible) {
    g_bCursorVisible = visible;  // notify main_Windows.cpp (for client area cursor invis)

    int check = ShowCursor(visible);
    if(!visible) {
        for(int i = 0; i <= check; i++) {
            ShowCursor(visible);
        }
    } else if(check < 0) {
        for(int i = check; i <= 0; i++) {
            ShowCursor(visible);
        }
    }
}

void WinEnvironment::setMousePos(int x, int y) {
    POINT temp;
    temp.x = (LONG)x;
    temp.y = (LONG)y;
    ClientToScreen(this->hwnd, &temp);
    SetCursorPos((int)temp.x, (int)temp.y);
}

void WinEnvironment::setCursorClip(bool clip, McRect rect) {
    this->bCursorClipped = clip;
    this->cursorClip = rect;

    if(clip) {
        RECT windowRect;

        if(rect.getWidth() == 0 && rect.getHeight() == 0) {
            RECT clientRect;
            GetClientRect(this->hwnd, &clientRect);

            POINT topLeft;
            topLeft.x = 0;
            topLeft.y = 0;
            ClientToScreen(this->hwnd, &topLeft);

            POINT bottomRight;
            bottomRight.x = clientRect.right;
            bottomRight.y = clientRect.bottom;
            ClientToScreen(this->hwnd, &bottomRight);

            windowRect.left = topLeft.x;
            windowRect.top = topLeft.y;
            windowRect.right = bottomRight.x;
            windowRect.bottom = bottomRight.y;

            this->cursorClip = McRect(0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top);
        }

        // TODO: custom rect (only fullscreen works atm)

        ClipCursor(&windowRect);
    } else
        ClipCursor(NULL);
}

UString WinEnvironment::keyCodeToString(KEYCODE keyCode) {
    UINT scanCode = MapVirtualKeyW(keyCode, MAPVK_VK_TO_VSC);

    WCHAR keyNameString[256];
    switch(keyCode) {
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_INSERT:
        case VK_DELETE:
        case VK_DIVIDE:
        case VK_NUMLOCK:
            scanCode |= 0x100;
            break;
    }

    if(!GetKeyNameTextW(scanCode << 16, keyNameString, 256))
        return UString::format("%lu", keyCode);  // fallback to raw number (better than having an empty string)

    return UString(keyNameString);
}

bool WinEnvironment::setProcessPriority(int priority) {
    return SetPriorityClass(GetCurrentProcess(), priority < 1 ? NORMAL_PRIORITY_CLASS : HIGH_PRIORITY_CLASS);
}

bool WinEnvironment::setProcessAffinity(int affinity) {
    HANDLE currentProcess = GetCurrentProcess();
    DWORD_PTR dwProcessAffinityMask;
    DWORD_PTR dwSystemAffinityMask;
    if(SUCCEEDED(GetProcessAffinityMask(currentProcess, &dwProcessAffinityMask, &dwSystemAffinityMask))) {
        // count cores and print current mask
        debugLog("dwProcessAffinityMask = ");
        DWORD_PTR dwProcessAffinityMaskTemp = dwProcessAffinityMask;
        int numCores = 0;
        while(dwProcessAffinityMaskTemp) {
            if(dwProcessAffinityMaskTemp & 1) {
                numCores++;
                debugLog("1");
            } else
                debugLog("0");

            dwProcessAffinityMaskTemp >>= 1;
        }
        if(this->iNumCoresForProcessAffinity < 1) this->iNumCoresForProcessAffinity = numCores;

        switch(affinity) {
            case 0:  // first core (e.g. 1111 -> 1000, 11 -> 10, etc.)
                dwProcessAffinityMask >>= this->iNumCoresForProcessAffinity - 1;
                break;
            case 1:  // last core (e.g. 1111 -> 0001, 11 -> 01, etc.)
                dwProcessAffinityMask >>= this->iNumCoresForProcessAffinity - 1;
                dwProcessAffinityMask <<= this->iNumCoresForProcessAffinity - 1;
                break;
            default:  // reset, all cores (e.g. ???? -> 1111, ?? -> 11, etc.)
                for(int i = 0; i < this->iNumCoresForProcessAffinity; i++) {
                    dwProcessAffinityMask |= (1 << i);
                }
                break;
        }
        debugLog("\ndwProcessAffinityMask = %i\n", dwProcessAffinityMask);

        if(FAILED(SetProcessAffinityMask(currentProcess, dwProcessAffinityMask))) {
            debugLog("Couldn't SetProcessAffinityMask(), GetLastError() = %i!\n", GetLastError());
            return false;
        }
    } else {
        debugLog("Couldn't GetProcessAffinityMask(), GetLastError() = %i!\n", GetLastError());
        return false;
    }

    return true;
}

void WinEnvironment::disableWindowsKey() {
    if(this->hKeyboardHook != NULL) return;

    this->hKeyboardHook =
        SetWindowsHookEx(WH_KEYBOARD_LL, WinEnvironment::lowLevelKeyboardProc, GetModuleHandle(NULL), 0);
}

void WinEnvironment::enableWindowsKey() {
    if(this->hKeyboardHook == NULL) return;

    UnhookWindowsHookEx(this->hKeyboardHook);
    this->hKeyboardHook = NULL;
}

// helper functions

void WinEnvironment::path_strip_filename(TCHAR *Path) {
    size_t Len = _tcslen(Path);

    if(Len == 0) return;

    size_t Idx = Len - 1;
    while(TRUE) {
        TCHAR Chr = Path[Idx];
        if(Chr == TEXT('\\') || Chr == TEXT('/')) {
            if(Idx == 0 || Path[Idx - 1] == ':') Idx++;

            break;
        } else if(Chr == TEXT(':')) {
            Idx++;
            break;
        } else {
            if(Idx == 0)
                break;
            else
                Idx--;
            ;
        };
    };

    Path[Idx] = TEXT('\0');
}

void WinEnvironment::handleShowMessageFullscreen() {
    if(this->bFullScreen) {
        disableFullscreen();

        // HACKHACK: force minimize + focus if in fullscreen
        // minimizing allows us to view MessageBox()es at all, and focus brings it into the foreground (because minimize
        // also unfocuses)
        minimize();
        focus();
    }
}

void WinEnvironment::enumerateMonitors() {
    this->vMonitors.clear();
    EnumDisplayMonitors(NULL, NULL, WinEnvironment::monitorEnumProc, 0);
}

void WinEnvironment::onProcessPriorityChange(UString oldValue, UString newValue) {
    setProcessPriority(newValue.toInt());
}

void WinEnvironment::onDisableWindowsKeyChange(UString oldValue, UString newValue) {
    if(newValue.toInt())
        disableWindowsKey();
    else
        enableWindowsKey();
}

long WinEnvironment::getWindowStyleWindowed() {
    long style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

    if(!WinEnvironment::bResizable) style = style & (~WS_SIZEBOX);

    return style;
}

BOOL CALLBACK WinEnvironment::monitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &monitorInfo);

    const bool isPrimaryMonitor = (monitorInfo.dwFlags & MONITORINFOF_PRIMARY);
    const McRect monitorRect =
        McRect(lprcMonitor->left, lprcMonitor->top, std::abs(lprcMonitor->left - lprcMonitor->right),
               std::abs(lprcMonitor->top - lprcMonitor->bottom));
    if(isPrimaryMonitor)
        WinEnvironment::vMonitors.insert(WinEnvironment::vMonitors.begin(), monitorRect);
    else
        WinEnvironment::vMonitors.push_back(monitorRect);

    if(cv_debug_env.getBool())
        debugLog("Monitor %i: (right = %ld, bottom = %ld, left = %ld, top = %ld), isPrimaryMonitor = %i\n",
                 WinEnvironment::vMonitors.size(), lprcMonitor->right, lprcMonitor->bottom, lprcMonitor->left,
                 lprcMonitor->top, (int)isPrimaryMonitor);

    return TRUE;
}

LRESULT CALLBACK WinEnvironment::lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if(nCode < 0 || nCode != HC_ACTION) return CallNextHookEx(WinEnvironment::hKeyboardHook, nCode, wParam, lParam);

    bool ignoreKeyStroke = false;
    KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;
    switch(wParam) {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            ignoreKeyStroke = ((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN)) && (p->flags == 1);
            break;
        }
    }

    if(ignoreKeyStroke)
        return 1;
    else
        return CallNextHookEx(WinEnvironment::hKeyboardHook, nCode, wParam, lParam);
}

#endif
