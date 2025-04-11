#include <windows.h>
#include "win11ui.h"


LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "rawbit_window_class";

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0,
        "rawbit_window_class",
        "rawBit",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        NULL, NULL, hInstance, NULL
    );
    win11ui_enable_dark_mode();
    win11ui_apply_rounded_corners(hwnd);
    win11ui_apply_blur(hwnd);


    ShowWindow(hwnd, nCmdShow);
    tray_icon_add(hwnd);


    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        tray_icon_remove(hwnd);
        PostQuitMessage(0);
        return 0;
    }



    return DefWindowProcA(hwnd, msg, wparam, lparam);
}
