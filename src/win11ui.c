#include "win11ui.h"
#include <dwmapi.h>  
#include <shellapi.h>
#include "resources.h"

#pragma comment(lib, "Dwmapi.lib")

#define RAWBIT_TRAY_ICON_ID 1
#define RAWBIT_TRAY_MESSAGE (WM_USER + 1)




void win11ui_apply_rounded_corners(HWND window_handle)
{
    const DWORD DWMWCP_ROUND = 2; // DWM_WINDOW_CORNER_PREFERENCE_ROUND
    DwmSetWindowAttribute(window_handle, 33, &DWMWCP_ROUND, sizeof(DWMWCP_ROUND));
}

void win11ui_apply_blur(HWND window_handle)
{
    HMODULE hUser32 = LoadLibraryA("user32.dll");
    if(!hUser32) return;

    SetWindowCompositionAttributeFn fnSetAttr =
        (SetWindowCompositionAttributeFn)GetProcAddress(hUser32, "SetWindowCompositionAttribute");

    if(fnSetAttr)
    {
        ACCENT_POLICY policy = { ACCENT_ENABLE_BLURBEHIND, 0, 0, 0 };
        WINDOWCOMPOSITIONATTRIBDATA data = { WCA_ACCENT_POLICY, &policy, sizeof(policy) };
        fnSetAttr(window_handle, &data);
    }

    FreeLibrary(hUser32);
}


void win11ui_enable_dark_mode(void)
{
    HMODULE hUxtheme = LoadLibraryA("uxtheme.dll");
    if(!hUxtheme) return;

    typedef BOOL(WINAPI* AllowDarkFn)(BOOL);
    AllowDarkFn fnAllowDark = (AllowDarkFn)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));

    if(fnAllowDark)
        fnAllowDark(TRUE);

    FreeLibrary(hUxtheme);
}


