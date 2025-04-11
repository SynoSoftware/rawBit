#pragma once

#include <windows.h>

// Tray icon functions
void tray_icon_add(HWND window_handle);
void tray_icon_remove(HWND window_handle);
void win11ui_apply_rounded_corners(HWND window_handle);
void win11ui_apply_blur(HWND window_handle);
void win11ui_enable_dark_mode(void);
