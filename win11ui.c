#include "win11ui.h"
#include <dwmapi.h>  
#include <shellapi.h>


#pragma comment(lib, "Dwmapi.lib")

#define RAWBIT_TRAY_ICON_ID 1
#define RAWBIT_TRAY_MESSAGE (WM_USER + 1)


typedef struct {
    int AccentState;
    int Flags;
    int Color;
    int AnimationId;
} ACCENT_POLICY;

typedef struct {
    int Attribute;
    void* Data;
    SIZE_T Size;
} WINDOWCOMPOSITIONATTRIBDATA;

enum { WCA_ACCENT_POLICY = 19 };
enum { ACCENT_ENABLE_BLURBEHIND = 3 };

typedef BOOL(WINAPI* SetWindowCompositionAttributeFn)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

void win11ui_apply_rounded_corners(HWND window_handle)
{
    const DWORD DWMWCP_ROUND = 2; // DWM_WINDOW_CORNER_PREFERENCE_ROUND
    DwmSetWindowAttribute(window_handle, 33, &DWMWCP_ROUND, sizeof(DWMWCP_ROUND));
}

void win11ui_apply_blur(HWND window_handle)
{
    HMODULE hUser32 = LoadLibraryA("user32.dll");
    SetWindowCompositionAttributeFn fnSetAttr =
        (SetWindowCompositionAttributeFn)GetProcAddress(hUser32, "SetWindowCompositionAttribute");

    if (fnSetAttr)
    {
        ACCENT_POLICY policy = { ACCENT_ENABLE_BLURBEHIND, 0, 0, 0 };
        WINDOWCOMPOSITIONATTRIBDATA data = { WCA_ACCENT_POLICY, &policy, sizeof(policy) };
        fnSetAttr(window_handle, &data);
    }
}

void win11ui_enable_dark_mode(void)
{
    HMODULE hUxtheme = LoadLibraryA("uxtheme.dll");
    typedef BOOL(WINAPI* AllowDarkFn)(BOOL);
    AllowDarkFn fnAllowDark = (AllowDarkFn)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));

    if (fnAllowDark)
        fnAllowDark(TRUE);
}


void tray_icon_add(HWND window_handle)
{
    NOTIFYICONDATAA nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.hWnd = window_handle;
    nid.uID = RAWBIT_TRAY_ICON_ID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = RAWBIT_TRAY_MESSAGE;
    HICON icon_tray = LoadImageA(NULL, "ico/rawBit 16.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
    nid.hIcon = icon_tray;

    lstrcpyA(nid.szTip, "rawBit");

    Shell_NotifyIconA(NIM_ADD, &nid);
}

void tray_icon_remove(HWND window_handle)
{
    NOTIFYICONDATAA nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.hWnd = window_handle;
    nid.uID = RAWBIT_TRAY_ICON_ID;

    Shell_NotifyIconA(NIM_DELETE, &nid);
}
