#pragma once

#include <windows.h>

#define RAWBIT_TRAY_MESSAGE (WM_USER + 1)
#define RAWBIT_TRAY_ICON_ID 1
typedef struct
{
    int AccentState;
    int Flags;
    int Color;
    int AnimationId;
} ACCENT_POLICY;

typedef struct
{
    int Attribute;
    void* Data;
    SIZE_T Size;
} WINDOWCOMPOSITIONATTRIBDATA;

enum { WCA_ACCENT_POLICY = 19 };
enum { ACCENT_ENABLE_BLURBEHIND = 3 };

typedef BOOL(WINAPI* SetWindowCompositionAttributeFn)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

// Tray icon functions
void tray_icon_add(HWND window_handle, HINSTANCE hInstance);
void tray_icon_remove(HWND window_handle);
void win11ui_apply_rounded_corners(HWND window_handle);
void win11ui_apply_blur(HWND window_handle);
void win11ui_enable_dark_mode(void);
