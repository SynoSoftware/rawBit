#pragma once

#include <windows.h>

struct TrayIconConfig
{
    HINSTANCE instance;
    HWND window;
    UINT icon_resource_id;
    UINT callback_message;
    const wchar_t* tooltip;
};

struct TrayIcon
{
    NOTIFYICONDATAW notify_data;
    int is_visible;
};

int tray_icon_init(TrayIcon* icon, const TrayIconConfig* config);
void tray_icon_shutdown(TrayIcon* icon);
