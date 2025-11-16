#include "platform/win32/launcher_window.h"

#include <CommCtrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <windowsx.h>

#include "config.h"
#include "debug.h"
#include "resources.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Shell32.lib")

namespace
{
    const wchar_t* kWindowClassName = L"rawbit_launcher_window";
    const UINT kOpenButtonId = 1001;
    const UINT kTrayCommandOpen = 2001;
    const UINT kTrayCommandQuit = 2002;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_MICA_EFFECT
#define DWMWA_MICA_EFFECT 1029
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

#ifndef WCA_ACCENT_POLICY
#define WCA_ACCENT_POLICY 19
#endif
#ifndef ACCENT_ENABLE_HOSTBACKDROP
#define ACCENT_ENABLE_HOSTBACKDROP 5
#endif
#ifndef ACCENT_ENABLE_ACRYLICBLURBEHIND
#define ACCENT_ENABLE_ACRYLICBLURBEHIND 4
#endif

struct AccentPolicy
{
    int state;
    int flags;
    unsigned int gradient_color;
    int animation_id;
};

struct WindowCompositionAttribData
{
    int attribute;
    void* data;
    SIZE_T size;
};

typedef BOOL(WINAPI* SetWindowCompositionAttributeFn)(HWND, WindowCompositionAttribData*);

    LRESULT CALLBACK launcher_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    // Add this function near the top with apply_mica
    void disable_mica(HWND hwnd)
    {
        const DWORD backdrop = 1; // DWMSBT_NONE
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

        const MARGINS margins = { 0, 0, 0, 0 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
   
    void apply_mica(HWND hwnd)
    {
        if(!hwnd)
        {
            return;
        }

        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

        const DWORD backdrop = DWMSBT_MAINWINDOW;
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

        const DWORD corner_pref = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner_pref, sizeof(corner_pref));

        BOOL enable_mica = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, &enable_mica, sizeof(enable_mica));

        const MARGINS margins = { -1, -1, -1, -1 };

        DwmExtendFrameIntoClientArea(hwnd, &margins);

        BOOL trueValue = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &trueValue, sizeof(trueValue));

     

    }

    void draw_open_button(const LauncherWindow* launcher, DRAWITEMSTRUCT* draw)
    {
        if(!launcher || !draw)
        {
            return;
        }

        HDC dc = draw->hDC;
        RECT rc = draw->rcItem;

        const bool pressed = (draw->itemState & ODS_SELECTED) != 0;
        const bool focused = (draw->itemState & ODS_FOCUS) != 0;

        const COLORREF bg_normal = RGB(22, 22, 26);
        const COLORREF bg_hover = RGB(32, 32, 36);
        const COLORREF bg_pressed = RGB(18, 18, 22);
        const COLORREF border_color = RGB(64, 64, 72);

        COLORREF fill_color = bg_normal;
        if(pressed)
        {
            fill_color = bg_pressed;
        }
        else if(draw->itemState & ODS_HOTLIGHT)
        {
            fill_color = bg_hover;
        }

        HBRUSH fill_brush = CreateSolidBrush(fill_color);
        HPEN border_pen = CreatePen(PS_SOLID, 1, border_color);
        HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(dc, fill_brush));
        HPEN old_pen = static_cast<HPEN>(SelectObject(dc, border_pen));
        RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
        SelectObject(dc, old_brush);
        SelectObject(dc, old_pen);
        DeleteObject(fill_brush);
        DeleteObject(border_pen);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(240, 240, 240));

        const wchar_t* text = L"Open rawBit interface";
        DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if(focused)
        {
            RECT focus_rc = rc;
            InflateRect(&focus_rc, -6, -6);
            DrawFocusRect(dc, &focus_rc);
        }
    }

    void layout_controls(LauncherWindow* launcher, RECT client)
    {
        if(!launcher)
        {
            return;
        }

        const int margin = 24;
        const int spacing = 12;
        const int button_height = 48;
        const int button_width = (client.right - client.left) - (margin * 2);

        int y = margin;

        if(launcher->primary_text)
        {
            MoveWindow(launcher->primary_text, margin, y, button_width, 32, TRUE);
            y += 32 + spacing / 2;
        }

        if(launcher->secondary_text)
        {
            MoveWindow(launcher->secondary_text, margin, y, button_width, 48, TRUE);
            y += 48 + spacing;
        }

        if(launcher->open_button)
        {
            MoveWindow(launcher->open_button, margin, y, button_width, button_height, TRUE);
        }
    }

    void show_tray_menu(LauncherWindow* launcher)
    {
        if(!launcher || !launcher->tray_menu)
        {
            return;
        }

        POINT cursor;
        GetCursorPos(&cursor);
        SetForegroundWindow(launcher->window);
        TrackPopupMenu(launcher->tray_menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, launcher->window, nullptr);
    }

    ATOM register_window_class(HINSTANCE instance)
    {
        static ATOM atom = 0;
        if(atom != 0)
        {
            return atom;
        }

        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = launcher_window_proc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_ICON_LARGE));
        wc.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_ICON_SMALL));
        wc.lpszClassName = kWindowClassName;
        atom = RegisterClassExW(&wc);
        return atom;
    }

    void set_window_pointer(HWND hwnd, LauncherWindow* launcher)
    {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(launcher));
    }

    LauncherWindow* from_window(HWND hwnd)
    {
        return reinterpret_cast<LauncherWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    void update_secondary_text(LauncherWindow* launcher)
    {
        if(!launcher || !launcher->secondary_text)
        {
            return;
        }

        wchar_t buffer[256];
        swprintf_s(buffer,
            L"Click the button to open the control interface in your browser.\nhttp://127.0.0.1:%hu/",
            launcher->config.http_port);
        SetWindowTextW(launcher->secondary_text, buffer);
    }

    void update_fonts(LauncherWindow* launcher)
    {
        if(!launcher)
        {
            return;
        }

        if(launcher->text_font)
        {
            DeleteObject(launcher->text_font);
            launcher->text_font = nullptr;
        }
        if(launcher->button_font)
        {
            DeleteObject(launcher->button_font);
            launcher->button_font = nullptr;
        }

        LOGFONTW base_font;
        ZeroMemory(&base_font, sizeof(base_font));
        base_font.lfHeight = -16;
        wcscpy_s(base_font.lfFaceName, L"Segoe UI");

        launcher->text_font = CreateFontIndirectW(&base_font);
        base_font.lfWeight = FW_SEMIBOLD;
        base_font.lfHeight = -18;
        launcher->button_font = CreateFontIndirectW(&base_font);

        if(launcher->primary_text && launcher->text_font)
        {
            SendMessageW(launcher->primary_text, WM_SETFONT, reinterpret_cast<WPARAM>(launcher->text_font), TRUE);
        }
        if(launcher->secondary_text && launcher->text_font)
        {
            SendMessageW(launcher->secondary_text, WM_SETFONT, reinterpret_cast<WPARAM>(launcher->text_font), TRUE);
        }
        if(launcher->open_button && launcher->button_font)
        {
            SendMessageW(launcher->open_button, WM_SETFONT, reinterpret_cast<WPARAM>(launcher->button_font), TRUE);
        }
    }

    void invoke_callback(LauncherWindow* launcher, LauncherCommand command)
    {
        if(launcher && launcher->config.command_callback && !launcher->suppress_close_command)
        {
            launcher->config.command_callback(launcher->config.callback_user_data, command);
        }
    }

    LRESULT handle_create(HWND hwnd, LPCREATESTRUCTW create_info)
    {
        LauncherWindow* launcher = reinterpret_cast<LauncherWindow*>(create_info->lpCreateParams);
        if(!launcher)
        {
            return -1;
        }
        launcher->window = hwnd;
        set_window_pointer(hwnd, launcher);

        launcher->primary_text = CreateWindowExW(
            0, L"STATIC", L"rawBit engine is running.", WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, hwnd, nullptr, launcher->config.instance, nullptr);

        launcher->secondary_text = CreateWindowExW(
            0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, hwnd, nullptr, launcher->config.instance, nullptr);

        launcher->open_button = CreateWindowExW(
            0, L"BUTTON", L"Open rawBit interface", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kOpenButtonId)),
            launcher->config.instance, nullptr);

        update_secondary_text(launcher);
        update_fonts(launcher);
        apply_mica(hwnd);

        RECT rc;
        GetClientRect(hwnd, &rc);
        layout_controls(launcher, rc);


        launcher->tray_menu = CreatePopupMenu();
        if(launcher->tray_menu)
        {
            AppendMenuW(launcher->tray_menu, MF_STRING, kTrayCommandOpen, L"Open interface");
            AppendMenuW(launcher->tray_menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(launcher->tray_menu, MF_STRING, kTrayCommandQuit, L"Quit");
        }

        TrayIconConfig tray_cfg;
        ZeroMemory(&tray_cfg, sizeof(tray_cfg));
        tray_cfg.instance = launcher->config.instance;
        tray_cfg.window = hwnd;
        tray_cfg.icon_resource_id = IDI_ICON_TRAY;
        tray_cfg.callback_message = launcher->tray_message_id;
        tray_cfg.tooltip = L"rawBit";

        if(tray_icon_init(&launcher->tray_icon, &tray_cfg) == 0)
        {
            launcher->tray_initialized = 1;
        }

        return 0;
    }

    LRESULT CALLBACK launcher_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        LauncherWindow* launcher = from_window(hwnd);

        switch(msg)
        {
            // In launcher_window_proc, add this case

            case WM_ACTIVATE:
            {
                // Re-apply DWM attributes when the window is activated
                // to ensure Mica is consistently applied.
                if (wparam == WA_ACTIVE || wparam == WA_CLICKACTIVE)
                {
                    apply_mica(hwnd);
                }
                else if (wparam == WA_INACTIVE)
                {
                    // Optional: Disable effect when inactive
                    disable_mica(hwnd);
                }
                return 0; // Important: Return 0 to indicate you handled it
            }

            case WM_NCCREATE:
            {
                CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
                if(cs && cs->lpCreateParams)
                {
                    set_window_pointer(hwnd, reinterpret_cast<LauncherWindow*>(cs->lpCreateParams));
                }
                return TRUE;
            }

            case WM_NCPAINT:
                return 0;
                

            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                // FIX 2: Fill the background with a solid color.
                // DWM will replace this with the Mica effect, but this makes
                // hit-testing reliable for window dragging.
                FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));

                EndPaint(hwnd, &ps);
                return 0;
            }


            case WM_CREATE:
                return handle_create(hwnd, reinterpret_cast<LPCREATESTRUCTW>(lparam));

            case WM_DESTROY:
                if(launcher)
                {
                    if(launcher->tray_initialized)
                    {
                        tray_icon_shutdown(&launcher->tray_icon);
                        launcher->tray_initialized = 0;
                    }
                    if(launcher->tray_menu)
                    {
                        DestroyMenu(launcher->tray_menu);
                        launcher->tray_menu = nullptr;
                    }
                    if(launcher->text_font)
                    {
                        DeleteObject(launcher->text_font);
                        launcher->text_font = nullptr;
                    }
                    if(launcher->button_font)
                    {
                        DeleteObject(launcher->button_font);
                        launcher->button_font = nullptr;
                    }
                    launcher->window = nullptr;
                }
                break;

            case WM_CLOSE:
                if(launcher && launcher->suppress_close_command)
                {
                    DestroyWindow(hwnd);
                    return 0;
                }
                ShowWindow(hwnd, SW_MINIMIZE);
                return 0;

            case WM_COMMAND:
                if(LOWORD(wparam) == kOpenButtonId && HIWORD(wparam) == BN_CLICKED)
                {
                    invoke_callback(launcher, LauncherCommand_OpenInterface);
                    return 0;
                }
                if(LOWORD(wparam) == kTrayCommandOpen)
                {
                    invoke_callback(launcher, LauncherCommand_OpenInterface);
                    return 0;
                }
                if(LOWORD(wparam) == kTrayCommandQuit)
                {
                    invoke_callback(launcher, LauncherCommand_Quit);
                    return 0;
                }
                break;

            case WM_CTLCOLORSTATIC:
                if(launcher)
                {
                    HDC dc = reinterpret_cast<HDC>(wparam);
                    HWND target = reinterpret_cast<HWND>(lparam);
                    if(target == launcher->primary_text || target == launcher->secondary_text)
                    {
                        SetTextColor(dc, RGB(240, 240, 240));
                        SetBkMode(dc, TRANSPARENT);
                        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
                    }
                }
                break;

            case WM_DRAWITEM:
                if(launcher && wparam == kOpenButtonId)
                {
                    draw_open_button(launcher, reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
                    return TRUE;
                }
                break;

            case WM_NCHITTEST:
            {
                POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
                ScreenToClient(hwnd, &pt);

                HWND child = ChildWindowFromPointEx(
                    hwnd, pt,
                    CWP_SKIPINVISIBLE | CWP_SKIPDISABLED
                );

                // If clicking on nothing -> drag
                if (!child || child == hwnd)
                    return HTCAPTION;

                // Otherwise it's a normal client area
                return HTCLIENT;
            }



            case WM_SIZE:
                if(launcher)
                {
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    layout_controls(launcher, rc);
                }
                break;

            case WM_ERASEBKGND:
                return 1;


            default:
                if(launcher && msg == launcher->tray_message_id)
                {
                    if(lparam == WM_LBUTTONUP)
                    {
                        invoke_callback(launcher, LauncherCommand_OpenInterface);
                        return 0;
                    }
                    if(lparam == WM_RBUTTONUP)
                    {
                        show_tray_menu(launcher);
                        return 0;
                    }
                }
                break;
        }

        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

int launcher_window_init(LauncherWindow* launcher, const LauncherWindowConfig* config)
{
    if(!launcher || !config || !config->instance)
    {
        return -1;
    }

    ZeroMemory(launcher, sizeof(*launcher));
    launcher->config = *config;
    launcher->tray_message_id = WM_APP + 42;

    if(register_window_class(config->instance) == 0)
    {
        return -2;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW ,
        kWindowClassName,
        APP_TITLE_W,
        WS_POPUP | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        420,
        260,
        nullptr,
        nullptr,
        config->instance,
        launcher);

    if(!hwnd)
    {
        return -3;
    }



    // Remove the default caption and border styles
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER);
    SetWindowLongW(hwnd, GWL_STYLE, style);

   
    // Get the current extended style
    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);

    // Explicitly remove the layered style that conflicts with Mica
    exStyle &= ~WS_EX_LAYERED;

    // Also remove any default window edges
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);

    // Apply the new, corrected extended style
    SetWindowLongW(hwnd, GWL_EXSTYLE, exStyle);

 

    SetWindowPos(hwnd, nullptr, 0,0,0,0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    launcher->window = hwnd;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return 0;
}

void launcher_window_destroy(LauncherWindow* launcher)
{
    if(!launcher)
    {
        return;
    }

    launcher->suppress_close_command = 1;
    if(launcher->window)
    {
        DestroyWindow(launcher->window);
        launcher->window = nullptr;
    }
}

void launcher_window_show(const LauncherWindow* launcher)
{
    if(!launcher || !launcher->window)
    {
        return;
    }
    ShowWindow(launcher->window, SW_SHOW);
    SetForegroundWindow(launcher->window);
}

void launcher_window_open_browser(const LauncherWindow* launcher)
{
    if(!launcher)
    {
        return;
    }

    wchar_t url[128];
    swprintf_s(url, L"http://127.0.0.1:%hu/", launcher->config.http_port);
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}
