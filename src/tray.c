#include "window.h"
#include "debug.h"
#include "win11ui.h"
#include "tray.h"
#include "resources.h"
#include <windows.h>
#include <windowsx.h>

void tray_icon_add(HWND window_handle, HINSTANCE hInstance)
{
    NOTIFYICONDATAA nid = { 0 };
    nid.cbSize = sizeof(nid);
    nid.hWnd = window_handle;
    nid.uID = RAWBIT_TRAY_ICON_ID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = RAWBIT_TRAY_MESSAGE;

    HICON icon_tray = LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_ICON_TRAY));
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
