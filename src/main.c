#include <windows.h>
#include <windowsx.h>
#include <stdio.h>

#include "win11ui.h"
#include "resources.h"
#include "debug.h"
#include "window.h"
#include "config.h"

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    HICON icon_small = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON_SMALL));
    HICON icon_large = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON_LARGE));
    if(!icon_small)
    {
        MessageBoxW(NULL, L"Failed to load small icon", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    if(!icon_large)
    {
        MessageBoxW(NULL, L"Failed to load large icon", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"rawbit_window_class";
    wc.hbrBackground = NULL;
    wc.hIconSm = icon_small;
    wc.hIcon = icon_large;

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"rawbit_window_class",
        APP_TITLE_W,
        WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 300,
        NULL, NULL, hInstance, NULL
    );

    if(!hwnd)
    {
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    win11ui_enable_dark_mode();
    win11ui_apply_rounded_corners(hwnd);
    win11ui_apply_blur(hwnd);

    SetWindowTextW(hwnd, APP_TITLE_W);

    ShowWindow(hwnd, nCmdShow);
    tray_icon_add(hwnd, hInstance);




    MSG msg;
    while(GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
