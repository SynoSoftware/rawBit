#pragma once

#include <windows.h>
#include <stdint.h>

#include "engine/engine_session.h"
#include "net/http_server.h"
#include "platform/win32/launcher_window.h"

struct RawBitAppConfig
{
    unsigned short http_port;
    unsigned int engine_tick_ms;
};

struct RawBitApp
{
    EngineSession engine;
    HttpServer http;
    LauncherWindow launcher;
    RawBitAppConfig config;
    HINSTANCE instance;
    int engine_initialized;
    int http_initialized;
    int launcher_initialized;
};

int rawbit_app_init(RawBitApp* app, const RawBitAppConfig* config, HINSTANCE instance);
void rawbit_app_shutdown(RawBitApp* app);
void rawbit_app_open_interface(const RawBitApp* app);
unsigned short rawbit_app_http_port(const RawBitApp* app);

