#include <windows.h>
#include <CommCtrl.h>

#include "app/app.h"
#include "config.h"
#include "debug.h"

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    InitializeDebugOutput();

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    if(!InitCommonControlsEx(&icc))
    {
        MessageBoxW(nullptr, L"Failed to initialize common controls.", APP_TITLE_W, MB_OK | MB_ICONERROR);
        CleanupDebugOutput();
        return -1;
    }

    RawBitApp app;
    RawBitAppConfig config;
    config.http_port = 32145;
    config.engine_tick_ms = 500;

    int init_result = rawbit_app_init(&app, &config, instance);
    if(init_result != 0)
    {
        MessageBoxW(nullptr, L"Failed to initialize rawBit components.", APP_TITLE_W, MB_OK | MB_ICONERROR);
        rawbit_app_shutdown(&app);
        CleanupDebugOutput();
        return init_result;
    }

    launcher_window_show(&app.launcher);

    MSG msg;
    while(GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    rawbit_app_shutdown(&app);
    CleanupDebugOutput();
    return static_cast<int>(msg.wParam);
}

