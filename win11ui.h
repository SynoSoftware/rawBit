#pragma once

#include <windows.h>

#define RAWBIT_TRAY_MESSAGE (WM_USER + 1)
#define RAWBIT_TRAY_ICON_ID 1

// Tray icon functions
void tray_icon_add(HWND window_handle);
void tray_icon_remove(HWND window_handle);
void win11ui_apply_rounded_corners(HWND window_handle);
void win11ui_apply_blur(HWND window_handle);
void win11ui_enable_dark_mode(void);
