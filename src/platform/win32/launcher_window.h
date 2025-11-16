#pragma once

#include <windows.h>

#include "platform/win32/tray_icon.h"

enum LauncherCommand
{
    LauncherCommand_OpenInterface = 0,
    LauncherCommand_Quit = 1
};

typedef void (*LauncherCommandCallback)(void* user_data, LauncherCommand command);

struct LauncherWindowConfig
{
    HINSTANCE instance;
    unsigned short http_port;
    LauncherCommandCallback command_callback;
    void* callback_user_data;
};

struct LauncherWindow
{
    LauncherWindowConfig config;
    HWND window;
    HWND primary_text;
    HWND secondary_text;
    HWND open_button;
    HFONT text_font;
    HFONT button_font;
    TrayIcon tray_icon;
    HMENU tray_menu;
    UINT tray_message_id;
    int tray_initialized;
    int suppress_close_command;
};

int launcher_window_init(LauncherWindow* launcher, const LauncherWindowConfig* config);
void launcher_window_destroy(LauncherWindow* launcher);
void launcher_window_show(const LauncherWindow* launcher);
void launcher_window_open_browser(const LauncherWindow* launcher);

