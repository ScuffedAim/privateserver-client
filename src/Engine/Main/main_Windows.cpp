#ifdef _WIN32

// clang-format off
// Include order matters
#include "cbase.h"
#include <dwmapi.h>
#include <shellapi.h>
// clang-format on

#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Downloader.h"  // for extract_beatmapset
#include "File.h"
#include "MainMenu.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "Skin.h"
#include "SongBrowser/SongBrowser.h"

// NEXTRAWINPUTBLOCK macro requires this
typedef uint64_t QWORD;

#ifdef MCENGINE_WINDOWS_TOUCH_SUPPORT
#include <winuser.h>
typedef BOOL(WINAPI *PGPI)(UINT32 pointerId, POINTER_INFO *pointerInfo);
PGPI g_GetPointerInfo = (PGPI)GetProcAddress(GetModuleHandle(TEXT("user32.dll")), "GetPointerInfo");
#ifndef TWF_WANTPALM
#define TWF_WANTPALM 0x00000002
#endif
#endif

#ifdef MCENGINE_WINDOWS_REALTIMESTYLUS_SUPPORT
#include "WinRealTimeStylus.h"
#endif

#define MI_WP_SIGNATURE 0xFF515700
#define SIGNATURE_MASK 0xFFFFFF00
#define IsPenEvent(dw) (((dw) & SIGNATURE_MASK) == MI_WP_SIGNATURE)

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

#include <iostream>

#include "ConVar.h"
#include "Engine.h"
#include "Mouse.h"
#include "Profiler.h"
#include "Timer.h"
#include "WinEnvironment.h"
#include "WinGLLegacyInterface.h"

#define WINDOW_TITLE L"scuffedaim"
#define WM_NEOSU_PROTOCOL (WM_USER + 1)

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define WINDOW_WIDTH_MIN 100
#define WINDOW_HEIGHT_MIN 100

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#define WM_NCUAHDRAWCAPTION 0x00AE
#define WM_NCUAHDRAWFRAME 0x00AF

extern bool g_bCursorVisible;  // set by WinEnvironment, for client area cursor invis
extern bool g_bARBMultisampleSupported;

Engine *g_engine = NULL;

bool g_bRunning = true;
bool g_bUpdate = true;
bool g_bDraw = true;

bool g_bMinimized = false;                       // for fps_max_background
bool g_bHasFocus = false;                        // for fps_max_background
bool g_bIsCursorVisible = true;                  // local variable
bool g_bSupportsPerMonitorDpiAwareness = false;  // local variable

std::vector<unsigned int> g_vTouches;

extern "C" {  // force switch to the high performance gpu in multi-gpu systems (mostly laptops)
__declspec(dllexport) DWORD NvOptimusEnablement =
    0x00000001;  // http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance =
    0x00000001;  // https://community.amd.com/thread/169965
}

static void handle_osk(const char *osk_path) {
    Skin::unpack(osk_path);

    auto folder_name = env->getFileNameFromFilePath(osk_path);
    folder_name.erase(folder_name.size() - 4);  // remove .osk extension

    cv_skin.setValue(env->getFileNameFromFilePath(folder_name).c_str());
    osu->optionsMenu->updateSkinNameLabel();
}

static void handle_osz(const char *osz_path) {
    File osz(osz_path);
    i32 set_id = extract_beatmapset_id(osz.readFile(), osz.getFileSize());
    if(set_id < 0) {
        // special case: legacy fallback behavior for invalid beatmapSetID, try to parse the ID from the
        // path
        auto mapset_name = UString(env->getFileNameFromFilePath(osz_path).c_str());
        const std::vector<UString> tokens = mapset_name.split(" ");
        for(auto token : tokens) {
            i32 id = token.toInt();
            if(id > 0) set_id = id;
        }
    }
    if(set_id == -1) {
        osu->getNotificationOverlay()->addToast("Beatmapset doesn't have a valid ID.");
        return;
    }

    std::string mapset_dir = MCENGINE_DATA_DIR "maps\\";
    mapset_dir.append(std::to_string(set_id));
    mapset_dir.append("\\");
    if(!env->directoryExists(mapset_dir)) {
        env->createDirectory(mapset_dir);
    }
    if(!extract_beatmapset(osz.readFile(), osz.getFileSize(), mapset_dir)) {
        osu->getNotificationOverlay()->addToast("Failed to extract beatmapset");
        return;
    }

    db->addBeatmapSet(mapset_dir);
    if(!osu->getSongBrowser()->selectBeatmapset(set_id)) {
        osu->getNotificationOverlay()->addToast("Failed to import beatmapset");
        return;
    }

    // prevent song browser from picking main menu song after database loads
    // (we just loaded and selected another song, so previous no longer applies)
    SAFE_DELETE(osu->mainMenu->preloaded_beatmapset);
}

static void handle_neosu_url(const char *url) {
    if(!strcmp(url, "neosu://run")) {
        // nothing to do
        return;
    }

    if(strstr(url, "neosu://join_lobby/") == url) {
        // TODO @kiwec: lobby id
        return;
    }

    if(strstr(url, "neosu://select_map/") == url) {
        // TODO @kiwec: beatmapset + md5 combo
        return;
    }

    if(strstr(url, "neosu://spectate/") == url) {
        // TODO @kiwec: user id
        return;
    }

    if(strstr(url, "neosu://watch_replay/") == url) {
        // TODO @kiwec: replay md5
        return;
    }
}

void handle_cmdline_args(const char *args) {
    if(args[0] == '-') return;
    if(strlen(args) < 4) return;

    if(strstr(args, "neosu://") == args) {
        handle_neosu_url(args);
    } else {
        auto extension = env->getFileExtensionFromFilePath(args);
        if(!extension.compare("osz")) {
            handle_osz(args);
        } else if(!extension.compare("osk") || !extension.compare("zip")) {
            handle_osk(args);
        }
    }
}

static void register_neosu_file_associations() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // Register neosu as an application
    HKEY neosu_key;
    i32 err = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\neosu", 0, NULL, REG_OPTION_NON_VOLATILE,
                              KEY_WRITE, NULL, &neosu_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as an application. Error: %d (root)\n", err);
        return;
    }
    RegSetValueExW(neosu_key, L"", 0, REG_SZ, (BYTE *)L"neosu", 12);
    RegSetValueExW(neosu_key, L"URL Protocol", 0, REG_SZ, (BYTE *)L"", 2);

    HKEY app_key;
    err = RegCreateKeyExW(neosu_key, L"Application", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &app_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as an application. Error: %d (app)\n", err);
        RegCloseKey(neosu_key);
        return;
    }
    RegSetValueExW(app_key, L"ApplicationName", 0, REG_SZ, (BYTE *)L"neosu", 12);
    RegCloseKey(app_key);

    HKEY cmd_key;
    wchar_t command[MAX_PATH + 10];
    swprintf_s(command, _countof(command), L"\"%s\" \"%%1\"", exePath);
    err = RegCreateKeyExW(neosu_key, L"shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                          &cmd_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as an application. Error: %d (command)\n", err);
        RegCloseKey(neosu_key);
        return;
    }
    RegSetValueExW(cmd_key, L"", 0, REG_SZ, (BYTE *)command, (wcslen(command) + 1) * sizeof(wchar_t));
    RegCloseKey(cmd_key);

    RegCloseKey(neosu_key);

    // Register neosu as .osk handler
    HKEY osk_key;
    err = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\.osk\\OpenWithProgids", 0, NULL,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &osk_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as .osk format handler. Error: %d\n", err);
        return;
    }
    RegSetValueEx(osk_key, "neosu", 0, REG_SZ, (BYTE *)L"", 2);
    RegCloseKey(osk_key);

    // Register neosu as .osr handler
    HKEY osr_key;
    err = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\.osr\\OpenWithProgids", 0, NULL,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &osr_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as .osr format handler. Error: %d\n", err);
        return;
    }
    RegSetValueEx(osr_key, "neosu", 0, REG_SZ, (BYTE *)L"", 2);
    RegCloseKey(osr_key);

    // Register neosu as .osz handler
    HKEY osz_key;
    err = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Classes\\.osz\\OpenWithProgids", 0, NULL,
                         REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &osz_key, NULL);
    if(err != ERROR_SUCCESS) {
        debugLog("Failed to register neosu as .osz format handler. Error: %d\n", err);
        return;
    }
    RegSetValueEx(osz_key, "neosu", 0, REG_SZ, (BYTE *)L"", 2);
    RegCloseKey(osz_key);
}

//****************//
//	Message loop  //
//****************//

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_NCCREATE:
            if(g_bSupportsPerMonitorDpiAwareness) {
                typedef BOOL(WINAPI * EPNCDS)(HWND);
                EPNCDS g_EnableNonClientDpiScaling =
                    (EPNCDS)GetProcAddress(GetModuleHandle(TEXT("user32.dll")), "EnableNonClientDpiScaling");
                if(g_EnableNonClientDpiScaling != NULL) g_EnableNonClientDpiScaling(hwnd);
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);

        case WM_COPYDATA: {
            PCOPYDATASTRUCT cds = (PCOPYDATASTRUCT)lParam;
            if(cds->dwData != WM_NEOSU_PROTOCOL) return 0;

            i32 argc = 0;
            char **argv = (char **)cds->lpData;
            for(i32 i = 0; i < argc; i++) {
                handle_cmdline_args(argv[i]);
            }

            return 1;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);

            for(UINT i = 0; i < fileCount; i++) {
                UINT pathLength = DragQueryFileW(hDrop, i, NULL, 0);
                wchar_t *filePath = new wchar_t[pathLength + 1];
                DragQueryFileW(hDrop, i, filePath, pathLength + 1);

                // Convert filepath to UTF-8
                int size = WideCharToMultiByte(CP_UTF8, 0, filePath, pathLength, NULL, 0, NULL, NULL);
                std::string utf8filepath(size, 0);
                WideCharToMultiByte(CP_UTF8, 0, filePath, size, (LPSTR)utf8filepath.c_str(), size, NULL, NULL);
                delete[] filePath;

                handle_cmdline_args(utf8filepath.c_str());
            }

            DragFinish(hDrop);

            return 0;
        }

        // graceful shutdown request
        case WM_DESTROY:
            if(g_engine != NULL) g_engine->shutdown();  // this will in turn trigger a WM_CLOSE
            return 0;

        // alt-f4, window X button press, right click > close, "exit" ConCommand and WM_DESTROY will all send WM_CLOSE
        case WM_CLOSE:
            if(g_bRunning) {
                g_bRunning = false;
                if(g_engine != NULL) g_engine->onShutdown();
            }
            return 0;

        // paint nothing on repaint
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
        }
            return 0;

        // focus and active
        case WM_ACTIVATE:
            if(g_bRunning) {
                if(!HIWORD(wParam))  // if we are not minimized
                {
                    // if (g_engine != NULL)
                    //	printf("WndProc() : WM_ACTIVATE, not minimized\n");

                    g_bUpdate = true;
                    g_bDraw = true;
                } else {
                    // if (g_engine != NULL)
                    //	printf("WndProc() : WM_ACTIVATE, minimized\n");

                    /// g_bUpdate = false;
                    g_bDraw = false;
                    g_bHasFocus = false;
                }
            }
            break;

        // focus
        case WM_SETFOCUS:
            g_bHasFocus = true;
            if(g_bRunning && g_engine != NULL) {
                if(!g_engine->hasFocus()) g_engine->onFocusGained();
            }
            break;

        case WM_KILLFOCUS:
            g_bHasFocus = false;
            if(g_bRunning && g_engine != NULL) {
                if(g_engine->hasFocus()) g_engine->onFocusLost();
            }
            break;

        // OnKeyDown
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            if(g_engine != NULL) {
                g_engine->onKeyboardKeyDown(wParam);
                return 0;
            }
            break;

        // OnKeyUp
        case WM_SYSKEYUP:
        case WM_KEYUP:
            if(g_engine != NULL) {
                g_engine->onKeyboardKeyUp(wParam);
                return 0;
            }
            break;

        // OnCharDown
        case WM_CHAR:
            if(g_engine != NULL) {
                g_engine->onKeyboardChar(wParam);
                return 0;
            }
            break;

        // left mouse button, inject as keyboard key as well
        case WM_LBUTTONDOWN:
            if(g_engine != NULL && (!cv_win_realtimestylus.getBool() ||
                                    !IsPenEvent(GetMessageExtraInfo())))  // if realtimestylus support is enabled, all
                                                                          // clicks are handled by it and not here
            {
                g_engine->onMouseLeftChange(true);
                g_engine->onKeyboardKeyDown(VK_LBUTTON);
            }
            SetCapture(hwnd);
            break;
        case WM_LBUTTONUP:
            if(g_engine != NULL) {
                g_engine->onMouseLeftChange(false);
                g_engine->onKeyboardKeyUp(VK_LBUTTON);
            }
            ReleaseCapture();
            break;

        // middle mouse button, inject as keyboard key as well
        case WM_MBUTTONDOWN:
            if(g_engine != NULL) {
                g_engine->onMouseMiddleChange(true);
                g_engine->onKeyboardKeyDown(VK_MBUTTON);
            }
            SetCapture(hwnd);
            break;
        case WM_MBUTTONUP:
            if(g_engine != NULL) {
                g_engine->onMouseMiddleChange(false);
                g_engine->onKeyboardKeyUp(VK_MBUTTON);
            }
            ReleaseCapture();
            break;

        // right mouse button, inject as keyboard key as well
        case WM_RBUTTONDOWN:
            if(g_engine != NULL && (!cv_win_realtimestylus.getBool() ||
                                    !IsPenEvent(GetMessageExtraInfo())))  // if realtimestylus support is enabled, all
                                                                          // pen clicks are handled by it and not here
            {
                g_engine->onMouseRightChange(true);
                g_engine->onKeyboardKeyDown(VK_RBUTTON);
            }
            SetCapture(hwnd);
            break;
        case WM_RBUTTONUP:
            if(g_engine != NULL) {
                g_engine->onMouseRightChange(false);
                g_engine->onKeyboardKeyUp(VK_RBUTTON);
            }
            ReleaseCapture();
            break;

        // mouse sidebuttons (4 and 5), inject them as keyboard keys as well
        case WM_XBUTTONDOWN:
            if(g_engine != NULL) {
                const DWORD fwButton = GET_XBUTTON_WPARAM(wParam);
                if(fwButton == XBUTTON1) {
                    g_engine->onMouseButton4Change(true);
                    g_engine->onKeyboardKeyDown(VK_XBUTTON1);
                } else if(fwButton == XBUTTON2) {
                    g_engine->onMouseButton5Change(true);
                    g_engine->onKeyboardKeyDown(VK_XBUTTON2);
                }
            }
            return TRUE;
        case WM_XBUTTONUP:
            if(g_engine != NULL) {
                const DWORD fwButton = GET_XBUTTON_WPARAM(wParam);
                if(fwButton == XBUTTON1) {
                    g_engine->onMouseButton4Change(false);
                    g_engine->onKeyboardKeyUp(VK_XBUTTON1);
                } else if(fwButton == XBUTTON2) {
                    g_engine->onMouseButton5Change(false);
                    g_engine->onKeyboardKeyUp(VK_XBUTTON2);
                }
            }
            return TRUE;

        // cursor visibility handling (for client area)
        case WM_SETCURSOR:
            if(!g_bCursorVisible) {
                if(LOWORD(lParam) == HTCLIENT)  // hide if in client area
                {
                    if(g_bIsCursorVisible) {
                        g_bIsCursorVisible = false;

                        int check = ShowCursor(false);
                        for(int i = 0; i <= check; i++) {
                            ShowCursor(false);
                        }
                    }
                } else if(!g_bIsCursorVisible)  // show if not in client area (e.g. window border)
                {
                    g_bIsCursorVisible = true;

                    int check = ShowCursor(true);
                    if(check < 0) {
                        for(int i = check; i <= 0; i++) {
                            ShowCursor(true);
                        }
                    }
                }
            }

            // this logic below would mean that we have to handle the cursor when moving from resizing into the client
            // area seems very annoying; unfinished
            /*
            if (LOWORD(lParam) == HTCLIENT) // if we are inside the client area, we handle the cursor
                    return TRUE;
            else
                    break; // if not, let DefWindowProc do its thing
            */

            // avoid cursor flicker when using non-normal cursor set by engine
            if(g_engine != NULL && g_engine->getEnvironment()->getCursor() != CURSORTYPE::CURSOR_NORMAL) return TRUE;

            break;

#ifdef MCENGINE_WINDOWS_TOUCH_SUPPORT
        case WM_POINTERUP:
        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERENTER:
        case WM_POINTERLEAVE:
        case WM_POINTERCAPTURECHANGED:
        case WM_POINTERWHEEL:
        case WM_POINTERHWHEEL:
            if(g_engine != NULL && g_GetPointerInfo != NULL) {
                POINTER_INFO pointerInfo;
                unsigned long id = LOWORD(wParam);
                if(g_GetPointerInfo(LOWORD(wParam), &pointerInfo)) {
                    if(pointerInfo.pointerFlags & POINTER_FLAG_PRIMARY) {
                        // bit of a hack, but it should work fine
                        // convert to fake raw tablet coordinates (0 to 65536)
                        int rawAbsoluteX = ((float)pointerInfo.ptPixelLocation.x /
                                            (float)g_engine->getEnvironment()->getNativeScreenSize().x) *
                                           65536;
                        int rawAbsoluteY = ((float)pointerInfo.ptPixelLocation.y /
                                            (float)g_engine->getEnvironment()->getNativeScreenSize().y) *
                                           65536;
                        g_engine->onMouseRawMove(rawAbsoluteX, rawAbsoluteY, true, true);
                    }

                    if(pointerInfo.pointerFlags & POINTER_FLAG_DOWN) {
                        bool contains = false;
                        for(size_t i = 0; i < g_vTouches.size(); i++) {
                            if(g_vTouches[i] == id) {
                                contains = true;
                                break;
                            }
                        }

                        if(!contains) {
                            bool already = g_vTouches.size() > 0;
                            g_vTouches.push_back(id);

                            if(already)
                                g_engine->onMouseRightChange(true);
                            else
                                g_engine->onMouseLeftChange(true);
                        }
                    } else if(pointerInfo.pointerFlags & POINTER_FLAG_UP) {
                        for(size_t i = 0; i < g_vTouches.size(); i++) {
                            if(g_vTouches[i] == id) {
                                g_vTouches.erase(g_vTouches.begin() + i);
                                i--;
                            }
                        }

                        bool still = g_vTouches.size() > 0;

                        if(still) g_engine->onMouseRightChange(false);
                        // WTF: this is already called by WM_LBUTTONUP, fuck touch
                        // else
                        //	g_engine->onMouseLeftChange(false);
                    }
                }
            }
            break;
#endif

        // raw input handling (only for mouse movement atm)
        case WM_INPUT: {
            RAWINPUT raw;
            UINT dwSize = sizeof(raw);

            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &dwSize, sizeof(RAWINPUTHEADER));

            if(raw.header.dwType == RIM_TYPEMOUSE) {
                if(g_engine != NULL) {
                    g_engine->onMouseRawMove(raw.data.mouse.lLastX, raw.data.mouse.lLastY,
                                             (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE),
                                             (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP));
                }
            }
        } break;

        // vertical mouse wheel
        case WM_MOUSEWHEEL:
            if(g_engine != NULL) g_engine->onMouseWheelVertical(GET_WHEEL_DELTA_WPARAM(wParam));
            break;

        // horizontal mouse wheel
        case WM_MOUSEHWHEEL:
            if(g_engine != NULL) g_engine->onMouseWheelHorizontal(GET_WHEEL_DELTA_WPARAM(wParam));
            break;

        // minimizing/maximizing
        case WM_SYSCOMMAND:
            switch(wParam) {
                case SC_MINIMIZE:
                    g_bDraw = false;
                    /// g_bUpdate = false;
                    g_bMinimized = true;
                    if(g_engine != NULL) g_engine->onMinimized();
                    break;
                case SC_MAXIMIZE:
                    g_bMinimized = false;
                    if(g_engine != NULL) {
                        RECT rect;
                        GetClientRect(hwnd, &rect);
                        g_engine->onResolutionChange(Vector2(rect.right, rect.bottom));
                        g_engine->onMaximized();
                    }
                    break;
                case SC_RESTORE:
                    g_bUpdate = true;
                    g_bDraw = true;
                    g_bMinimized = false;
                    if(g_engine != NULL) g_engine->onRestored();
                    break;

                // ignore ALT key opening the window context menu
                case SC_KEYMENU:
                    if((lParam >> 16) <= 0)  // still allow for mouse tho
                        return 0;
                    break;
            }
            break;

        // resolution change
        case WM_DISPLAYCHANGE:
        case WM_SIZE:
            /// case WM_EXITSIZEMOVE: // this would fire resize events for every pixel change, destroying performance if
            /// e.g. RenderTargets are resized in onResolutionChange()
            if(g_engine != NULL && g_bUpdate) {
                RECT rect;
                GetClientRect(hwnd, &rect);
                g_engine->requestResolutionChange(Vector2(rect.right, rect.bottom));
            }
            break;

        // DPI scaling change (e.g. because user changed scaling in settings, or moved the window to a monitor with
        // different scaling)
        case WM_DPICHANGED:
            if(g_engine != NULL) {
                WinEnvironment *winEnv = dynamic_cast<WinEnvironment *>(g_engine->getEnvironment());
                if(winEnv != NULL) {
                    winEnv->setDPIOverride(HIWORD(wParam));
                    g_engine->onDPIChange();

                    RECT *const prcNewWindow = (RECT *)lParam;
                    SetWindowPos(hwnd, NULL, prcNewWindow->left, prcNewWindow->top,
                                 prcNewWindow->right - prcNewWindow->left, prcNewWindow->bottom - prcNewWindow->top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
            }
            break;

        // resize limit
        case WM_GETMINMAXINFO: {
            WINDOWPLACEMENT wPos;
            { wPos.length = sizeof(WINDOWPLACEMENT); }
            GetWindowPlacement(hwnd, &wPos);

            // min
            LPMINMAXINFO pMMI = (LPMINMAXINFO)lParam;

            // printf("before: %i %i %i %i\n", (int)pMMI->ptMinTrackSize.x, (int)pMMI->ptMinTrackSize.y,
            // (int)pMMI->ptMaxTrackSize.x, (int)pMMI->ptMaxTrackSize.y); printf("window pos: left = %i, top = %i,
            // bottom = %i, right = %i\n", (int)wPos.rcNormalPosition.left, (int)wPos.rcNormalPosition.top,
            // (int)wPos.rcNormalPosition.bottom, (int)wPos.rcNormalPosition.right);

            pMMI->ptMinTrackSize.x = WINDOW_WIDTH_MIN;
            pMMI->ptMinTrackSize.y = WINDOW_HEIGHT_MIN;

            // NOTE: this is only required for OpenGL and custom renderers
            // allow dynamic overscale (offscreen window borders/decorations)
            // this also clamps all user-initiated resolution changes to the resolution of the monitor the window is on

            // HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            //  HACKHACK: use center instead of MonitorFromWindow() in order to workaround windows display scaling
            //  bullshit bug
            POINT centerOfWindow;
            {
                centerOfWindow.x =
                    wPos.rcNormalPosition.left + (wPos.rcNormalPosition.right - wPos.rcNormalPosition.left) / 2;
                centerOfWindow.y =
                    wPos.rcNormalPosition.top + (wPos.rcNormalPosition.bottom - wPos.rcNormalPosition.top) / 2;
            }
            HMONITOR monitor = MonitorFromPoint(centerOfWindow, MONITOR_DEFAULTTONEAREST);
            {
                MONITORINFO info;
                info.cbSize = sizeof(MONITORINFO);

                if(GetMonitorInfo(monitor, &info) >= 0) {
                    RECT clientRect;
                    RECT windowRect;

                    GetClientRect(hwnd, &clientRect);
                    GetWindowRect(hwnd, &windowRect);

                    const LONG decorationsSumWidth = (windowRect.right - windowRect.left) - clientRect.right;
                    const LONG decorationsSumHeight = (windowRect.bottom - windowRect.top) - clientRect.bottom;

                    // printf("window rect: left = %i, top = %i, bottom = %i, right = %i\n", (int)windowRect.left,
                    // (int)windowRect.top, (int)windowRect.bottom, (int)windowRect.right); printf("client rect: left =
                    // %i, top = %i, bottom = %i, right = %i\n", (int)clientRect.left, (int)clientRect.top,
                    // (int)clientRect.bottom, (int)clientRect.right); printf("monitor width = %i, height = %i\n",
                    // (int)std::abs(info.rcMonitor.left - info.rcMonitor.right), (int)std::abs(info.rcMonitor.top -
                    // info.rcMonitor.bottom)); printf("decorations: width = %i, height = %i\n",
                    // (int)decorationsSumWidth, (int)decorationsSumHeight);

                    pMMI->ptMaxTrackSize.x = std::abs(info.rcMonitor.left - info.rcMonitor.right) + decorationsSumWidth;
                    pMMI->ptMaxTrackSize.y =
                        std::abs(info.rcMonitor.top - info.rcMonitor.bottom) + decorationsSumHeight;
                }
            }

            // printf("after: %i %i %i %i\n", (int)pMMI->ptMinTrackSize.x, (int)pMMI->ptMinTrackSize.y,
            // (int)pMMI->ptMaxTrackSize.x, (int)pMMI->ptMaxTrackSize.y);
        }
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

//*******************//
//	Window creation  //
//*******************//

HWND createWinWindow(HINSTANCE hInstance) {
    HWND hwnd;

    // window style
    LONG_PTR style = WinEnvironment::getWindowStyleWindowed();
    LONG_PTR exStyle = WS_EX_WINDOWEDGE;

    style &= ~WS_VISIBLE;  // always start out invisible, we have a ShowWindow() call later anyway

    int xPos = (GetSystemMetrics(SM_CXSCREEN) / 2) - (WINDOW_WIDTH / 2);
    int yPos = (GetSystemMetrics(SM_CYSCREEN) / 2) - (WINDOW_HEIGHT / 2);
    int width = WINDOW_WIDTH;
    int height = WINDOW_HEIGHT;

    RECT clientArea;
    clientArea.left = xPos;
    clientArea.top = yPos;
    clientArea.right = xPos + width;
    clientArea.bottom = yPos + height;
    AdjustWindowRect(&clientArea, style, FALSE);

    xPos = clientArea.left;
    yPos = clientArea.top;
    width = clientArea.right - clientArea.left;
    height = clientArea.bottom - clientArea.top;

    // create the window
    hwnd = CreateWindowExW(exStyle, WINDOW_TITLE, WINDOW_TITLE, style, xPos, yPos, width, height, NULL, NULL, hInstance,
                           NULL);
    if(hwnd == NULL) {
        MessageBox(NULL, "Couldn't CreateWindowEx()!", "Fatal Error", MB_ICONEXCLAMATION | MB_OK);
        return NULL;
    }

    // NOTE: Hardcoded "1" from resource.rc
    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    return hwnd;
}

//********************//
//	Main entry point  //
//********************//

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
#ifdef _MSC_VER
    // When building with MSVC, vprintf() is not returning the correct value unless we have a console allocated.
    FILE *dummy;
#ifdef _DEBUG
    AllocConsole();
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
#else
    freopen_s(&dummy, "NUL", "w", stdout);
    freopen_s(&dummy, "NUL", "w", stderr);
#endif
#endif

    // fix working directory when dropping files onto neosu.exe
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t *last_slash = wcsrchr(exePath, L'\\');
    if(last_slash != NULL) *last_slash = L'\0';
    SetCurrentDirectoryW(exePath);

    // Convert args to UTF-8
    i32 argc = 0;
    char **argv = NULL;
    LPWSTR *wargv = CommandLineToArgvW(pCmdLine, &argc);
    argv = new char *[argc];
    for(i32 i = 0; i < argc; i++) {
        int size = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = new char[size + 1];
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], size, argv[i], size, NULL, NULL);
    }

    // register .osk, .osr, .osz, and neosu:// protocol
    register_neosu_file_associations();

    // if a neosu instance is already running, send it a message then quit
    HWND existing_window = FindWindowW(L"neosu", NULL);
    if(existing_window) {
        for(i32 i = 0; i < argc; i++) {
            COPYDATASTRUCT cds;
            cds.dwData = WM_NEOSU_PROTOCOL;
            cds.cbData = strlen(argv[i]) + 1;
            cds.lpData = argv[i];
            SendMessage(existing_window, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
        }

        SetForegroundWindow(existing_window);

        return 0;
    }

    // disable IME text input
    if(wcsstr(pCmdLine, L"-noime") != NULL) {
        typedef BOOL(WINAPI * pfnImmDisableIME)(DWORD);

        HMODULE hImm32 = LoadLibrary("imm32.dll");
        if(hImm32 != NULL) {
            pfnImmDisableIME pImmDisableIME = (pfnImmDisableIME)GetProcAddress(hImm32, "ImmDisableIME");
            if(pImmDisableIME == NULL)
                FreeLibrary(hImm32);
            else {
                pImmDisableIME(-1);
                FreeLibrary(hImm32);
            }
        }
    }

    // enable fancy themed windows controls (v6+), requires McEngine.exe.manifest AND linking to comctl32, for fucks
    // sake only noticeable in MessageBox()-es and a few other dialogs
    {
        INITCOMMONCONTROLSEX icc;
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_WIN95_CLASSES;
        InitCommonControlsEx(&icc);
    }

    // if supported (>= Windows Vista), enable DPI awareness so that GetSystemMetrics returns correct values
    // without this, on e.g. 150% scaling, the screen pixels of a 1080p monitor would be reported by
    // GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN) as only 720p! also on even newer systems (>= Windows 8.1) we can get
    // WM_DPICHANGED notified
    if(wcsstr(pCmdLine, L"-nodpi") == NULL) {
        // Windows 8.1+
        // per-monitor dpi scaling
        {
            HMODULE shcore = GetModuleHandle(TEXT("shcore.dll"));
            if(shcore != NULL) {
                typedef HRESULT(WINAPI * PSPDAN)(int);
                PSPDAN g_SetProcessDpiAwareness = (PSPDAN)GetProcAddress(shcore, "SetProcessDpiAwareness");
                if(g_SetProcessDpiAwareness != NULL) {
                    const HRESULT result = g_SetProcessDpiAwareness(2);  // 2 == PROCESS_PER_MONITOR_DPI_AWARE
                    g_bSupportsPerMonitorDpiAwareness = (result == S_OK || result == E_ACCESSDENIED);
                }
            }
        }

        if(!g_bSupportsPerMonitorDpiAwareness) {
            // Windows Vista+
            // system-wide dpi scaling
            {
                auto g_SetProcessDPIAware = GetProcAddress(GetModuleHandle(TEXT("user32.dll")), "SetProcessDPIAware");
                if(g_SetProcessDPIAware != NULL) g_SetProcessDPIAware();
            }
        }
    }

    // prepare window class
    WNDCLASSEXW wc;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)CreateSolidBrush(0x00000000);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WINDOW_TITLE;
    // wc.hIconSm			= LoadIcon(hInstance, MAKEINTRESOURCE(1)); // NOTE: load icon named "1" in
    // /Main/WinIcon.rc file, must link to WinIcon.o which was created with windres.exe
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    // register window class
    if(!RegisterClassExW(&wc)) {
        MessageBox(NULL, "Couldn't RegisterClassEx()!", "Fatal Error", MB_ICONEXCLAMATION | MB_OK);
        return -1;
    }

    // create the window
    HWND hwnd = createWinWindow(hInstance);
    if(hwnd == NULL) {
        printf("FATAL ERROR: hwnd == NULL!!!\n");
        MessageBox(NULL, "Couldn't createWinWindow()!", "Fatal Error", MB_ICONEXCLAMATION | MB_OK);
        return -1;
    }

    // get the screen refresh rate, and set fps_max to that as default
    {
        DEVMODE lpDevMode;
        memset(&lpDevMode, 0, sizeof(DEVMODE));
        lpDevMode.dmSize = sizeof(DEVMODE);
        lpDevMode.dmDriverExtra = 0;

        if(EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &lpDevMode)) {
            float displayFrequency = static_cast<float>(lpDevMode.dmDisplayFrequency);
            /// printf("Display Refresh Rate is %.2f Hz, setting fps_max to %i.\n\n", displayFrequency,
            /// (int)displayFrequency);
            cv_fps_max.setValue((int)displayFrequency);
            cv_fps_max.setDefaultFloat((int)displayFrequency);
        }
    }

    // initialize raw input
    {
        RAWINPUTDEVICE Rid[1];
        Rid[0].usUsagePage = ((USHORT)0x01);
        Rid[0].usUsage = ((USHORT)0x02);
        Rid[0].dwFlags = /*RIDEV_INPUTSINK | RIDEV_DEVNOTIFY*/ 0;
        Rid[0].hwndTarget = hwnd;
        if(RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])) == FALSE) {
            printf("WARNING: Couldn't RegisterRawInputDevices(), GetLastError() = %i\n", (int)GetLastError());
            MessageBox(NULL, "Couldn't RegisterRawInputDevices()!", "Warning", MB_ICONEXCLAMATION | MB_OK);
        }
    }

#ifdef MCENGINE_WINDOWS_REALTIMESTYLUS_SUPPORT
    // initialize RealTimeStylus (COM)
    {
        HRESULT hr = CoInitialize(NULL);
        if(hr == S_OK || hr == S_FALSE)  // if we initialized successfully, or if we are already initialized
        {
            if(!InitRealTimeStylus(hInstance, hwnd))
                printf("WARNING: Couldn't InitRealTimeStylus()! RealTimeStylus is not going to work.\n");
        } else
            printf("WARNING: Couldn't CoInitialize()! RealTimeStylus is not going to work.\n");
    }
#endif

#ifdef MCENGINE_WINDOWS_TOUCH_SUPPORT
    // initialize touch support/settings
    // http://robertinventor.com/bmwiki/How_to_disable_guestures_etc._for_multi_touch_playable_on_screen_keyboards
    {
        LPCTSTR tabletAtom = "MicrosoftTabletPenServiceProperty";
        unsigned short pressAndHoldAtomID = GlobalAddAtom(tabletAtom);
        if(pressAndHoldAtomID != 0) {
            DWORD dwHwndTabletProperty = TABLET_DISABLE_PRESSANDHOLD | TABLET_DISABLE_PENTAPFEEDBACK |
                                         TABLET_DISABLE_PENBARRELFEEDBACK | TABLET_DISABLE_FLICKS |
                                         TABLET_DISABLE_SMOOTHSCROLLING | TABLET_DISABLE_FLICKFALLBACKKEYS |
                                         TABLET_ENABLE_MULTITOUCHDATA;

            SetProp(hwnd, tabletAtom, (HANDLE)dwHwndTabletProperty);
        }

        typedef BOOL(WINAPI * pfnRegisterTouchWindow)(HWND, ULONG);
        pfnRegisterTouchWindow pRegisterTouchWindow =
            (pfnRegisterTouchWindow)GetProcAddress(GetModuleHandle(TEXT("user32.dll")), "RegisterTouchWindow");
        if(pRegisterTouchWindow != NULL) pRegisterTouchWindow(hwnd, TWF_WANTPALM);
    }
#endif

    // create timers
    Timer *frameTimer = new Timer();
    frameTimer->start();
    frameTimer->update();

    Timer *deltaTimer = new Timer();
    deltaTimer->start();
    deltaTimer->update();

    // NOTE: it seems that focus events get lost between CreateWindow() above and ShowWindow() here
    // can be simulated by sleeping and alt-tab for testing.
    // seems to be a Windows bug? if you switch to the window after all of this, no activate or focus events will be
    // received!
    // Sleep(1000);

    // make the window visible
    ShowWindow(hwnd, nCmdShow);

    // initialize engine
    WinEnvironment *environment = new WinEnvironment(hwnd, hInstance);
    g_engine = new Engine(environment, argc, argv);
    g_engine->loadApp();

    g_bHasFocus = g_bHasFocus && (GetForegroundWindow() == hwnd);   // HACKHACK: workaround (1), see above
    bool wasLaunchedInBackgroundAndWaitingForFocus = !g_bHasFocus;  // HACKHACK: workaround (2), see above

    if(g_bHasFocus) g_engine->onFocusGained();

    DragAcceptFiles(hwnd, TRUE);

    frameTimer->update();
    deltaTimer->update();

    // main loop
    MSG msg;
    msg.message = WM_NULL;
    unsigned long tickCounter = 0;
    UINT currentRawInputBufferNumBytes = 0;
    unsigned char *currentRawInputBuffer = NULL;
    while(g_bRunning) {
        VPROF_MAIN();

        // handle windows message queue
        {
            VPROF_BUDGET("Windows", VPROF_BUDGETGROUP_WNDPROC);

            if(cv_win_mouse_raw_input_buffer.getBool()) {
                UINT minRawInputBufferNumBytes = 0;
                UINT hr = GetRawInputBuffer(NULL, &minRawInputBufferNumBytes, sizeof(RAWINPUTHEADER));
                if(hr != (UINT)-1 && minRawInputBufferNumBytes > 0) {
                    // resize buffer up to 1 MB sanity limit (if we lagspike this could easily be hit, 8000 Hz polling
                    // rate will produce ~0.12 MB per second)
                    const UINT numAlignmentBytes = 8;
                    const UINT rawInputBufferNumBytes =
                        std::clamp<UINT>(minRawInputBufferNumBytes * numAlignmentBytes * 1024, 1, 1024 * 1024);
                    if(currentRawInputBuffer == NULL || currentRawInputBufferNumBytes < rawInputBufferNumBytes) {
                        currentRawInputBufferNumBytes = rawInputBufferNumBytes;
                        {
                            if(currentRawInputBuffer != NULL) delete[] currentRawInputBuffer;
                        }
                        currentRawInputBuffer = new unsigned char[currentRawInputBufferNumBytes];
                    }

                    // grab and go through all buffered RAWINPUT events
                    hr = GetRawInputBuffer((RAWINPUT *)currentRawInputBuffer, &currentRawInputBufferNumBytes,
                                           sizeof(RAWINPUTHEADER));
                    if(hr != (UINT)-1) {
                        RAWINPUT *currentRawInput = (RAWINPUT *)currentRawInputBuffer;
                        for(; hr > 0; hr--)  // (hr = number of rawInputs)
                        {
                            if(currentRawInput->header.dwType == RIM_TYPEMOUSE) {
                                const LONG lastX =
                                    ((RAWINPUT *)(&((BYTE *)currentRawInput)[numAlignmentBytes]))->data.mouse.lLastX;
                                const LONG lastY =
                                    ((RAWINPUT *)(&((BYTE *)currentRawInput)[numAlignmentBytes]))->data.mouse.lLastY;
                                const USHORT usFlags =
                                    ((RAWINPUT *)(&((BYTE *)currentRawInput)[numAlignmentBytes]))->data.mouse.usFlags;

                                g_engine->onMouseRawMove(lastX, lastY, (usFlags & MOUSE_MOVE_ABSOLUTE),
                                                         (usFlags & MOUSE_VIRTUAL_DESKTOP));
                            }

                            currentRawInput = NEXTRAWINPUTBLOCK(currentRawInput);
                        }
                    }
                }

                // handle all remaining non-WM_INPUT messages
                while(PeekMessageW(&msg, NULL, 0U, WM_INPUT - 1, PM_REMOVE) != 0 ||
                      PeekMessageW(&msg, NULL, WM_INPUT + 1, 0U, PM_REMOVE) != 0) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            } else {
                while(PeekMessageW(&msg, NULL, 0U, 0U, PM_REMOVE) != 0) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
        }

        // HACKHACK: focus bug workaround (3), see above
        {
            if(wasLaunchedInBackgroundAndWaitingForFocus) {
                const bool actuallyGotFocusEvenThoughThereIsNoFocusOrActivateEvent = (GetForegroundWindow() == hwnd);
                if(actuallyGotFocusEvenThoughThereIsNoFocusOrActivateEvent) {
                    wasLaunchedInBackgroundAndWaitingForFocus = false;

                    g_bHasFocus = true;
                    if(!g_engine->hasFocus()) g_engine->onFocusGained();
                }
            }
        }

        // update
        {
            deltaTimer->update();
            g_engine->setFrameTime(deltaTimer->getDelta());

            if(g_bUpdate) g_engine->onUpdate();
        }

        const bool inBackground = (g_bMinimized || !g_bHasFocus);

        // draw
        {
            const unsigned long interleaved = cv_fps_max_background_interleaved.getInt();
            if(g_bDraw && (!inBackground || interleaved < 2 || tickCounter % interleaved == 0)) {
                g_engine->onPaint();
            }
        }

        tickCounter++;

        // delay the next frame
        {
            VPROF_BUDGET("FPSLimiter", VPROF_BUDGETGROUP_SLEEP);

            frameTimer->update();

            if((!cv_fps_unlimited.getBool() && cv_fps_max.getInt() > 0) || inBackground) {
                double delayStart = frameTimer->getElapsedTime();
                double delayTime;
                if(inBackground)
                    delayTime = (1.0 / (double)cv_fps_max_background.getFloat()) - frameTimer->getDelta();
                else
                    delayTime = (1.0 / (double)cv_fps_max.getFloat()) - frameTimer->getDelta();

                const bool didSleep = delayTime > 0.0;
                while(delayTime > 0.0) {
                    if(inBackground)  // real waiting (very inaccurate, but very good for little background cpu
                                      // utilization)
                        Sleep((int)((1.0f / cv_fps_max_background.getFloat()) * 1000.0f));
                    else           // more or less "busy" waiting, but giving away the rest of the timeslice at least
                        Sleep(0);  // yes, there is a zero in there

                    // decrease the delayTime by the time we spent in this loop
                    // if the loop is executed more than once, note how delayStart now gets the value of the previous
                    // iteration from getElapsedTime() this works because the elapsed time is only updated in update().
                    // now we can easily calculate the time the Sleep() took and subtract it from the delayTime
                    delayStart = frameTimer->getElapsedTime();
                    frameTimer->update();
                    delayTime -= (frameTimer->getElapsedTime() - delayStart);
                }

                if(!didSleep && cv_fps_max_yield.getBool()) Sleep(0);  // yes, there is a zero in there
            } else if(cv_fps_unlimited_yield.getBool())
                Sleep(0);  // yes, there is a zero in there
        }
    }

    // uninitialize RealTimeStylus (COM)
#ifdef MCENGINE_WINDOWS_REALTIMESTYLUS_SUPPORT
    UninitRealTimeStylus();
    CoUninitialize();
#endif

    // release the timers
    SAFE_DELETE(frameTimer);
    SAFE_DELETE(deltaTimer);

    const bool isRestartScheduled = environment->isRestartScheduled();

    // release engine
    SAFE_DELETE(g_engine);

    // finally, destroy the window
    DestroyWindow(hwnd);

    // handle potential restart
    if(isRestartScheduled) {
        wchar_t full_path[MAX_PATH];
        GetModuleFileNameW(NULL, full_path, MAX_PATH);
        STARTUPINFOW startupInfo = {sizeof(STARTUPINFOW)};
        PROCESS_INFORMATION processInfo;
        CreateProcessW(full_path, NULL, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &startupInfo,
                       &processInfo);
    }

    return 0;
}

#endif
