#include "platform/win32/tray_icon.h"

#include <shellapi.h>
#include <string.h>
#include <wchar.h>

int tray_icon_init(TrayIcon* icon, const TrayIconConfig* config)
{
    if(!icon || !config || !config->window)
    {
        return -1;
    }

    ZeroMemory(icon, sizeof(*icon));
    ZeroMemory(&icon->notify_data, sizeof(icon->notify_data));

    icon->notify_data.cbSize = sizeof(icon->notify_data);
    icon->notify_data.hWnd = config->window;
    icon->notify_data.uID = 1;
    icon->notify_data.uFlags = NIF_MESSAGE;
    icon->notify_data.uCallbackMessage = config->callback_message;

    if(config->icon_resource_id != 0 && config->instance)
    {
        icon->notify_data.hIcon = static_cast<HICON>(
            LoadImageW(config->instance, MAKEINTRESOURCEW(config->icon_resource_id), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
        if(icon->notify_data.hIcon)
        {
            icon->notify_data.uFlags |= NIF_ICON;
        }
    }

    if(config->tooltip && config->tooltip[0] != L'\0')
    {
        icon->notify_data.uFlags |= NIF_TIP;
        wcsncpy_s(icon->notify_data.szTip,
            sizeof(icon->notify_data.szTip) / sizeof(icon->notify_data.szTip[0]),
            config->tooltip, _TRUNCATE);
    }

    if(!Shell_NotifyIconW(NIM_ADD, &icon->notify_data))
    {
        if(icon->notify_data.hIcon)
        {
            DestroyIcon(icon->notify_data.hIcon);
            icon->notify_data.hIcon = nullptr;
        }
        return -2;
    }

    icon->is_visible = 1;
    return 0;
}

void tray_icon_shutdown(TrayIcon* icon)
{
    if(!icon)
    {
        return;
    }

    if(icon->is_visible)
    {
        Shell_NotifyIconW(NIM_DELETE, &icon->notify_data);
        icon->is_visible = 0;
    }

    if(icon->notify_data.hIcon)
    {
        DestroyIcon(icon->notify_data.hIcon);
        icon->notify_data.hIcon = nullptr;
    }

    ZeroMemory(&icon->notify_data, sizeof(icon->notify_data));
}
