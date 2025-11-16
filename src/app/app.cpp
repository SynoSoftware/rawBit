#include "app/app.h"

#include <string.h>
#include <windows.h>

#include "config.h"
#include "debug.h"

static void launcher_command_handler(void* user_data, LauncherCommand command);

static RawBitAppConfig build_default_config(const RawBitAppConfig* config)
{
    RawBitAppConfig result;
    result.http_port = 32145;
    result.engine_tick_ms = 500;

    if(config)
    {
        if(config->http_port != 0)
        {
            result.http_port = config->http_port;
        }
        if(config->engine_tick_ms != 0)
        {
            result.engine_tick_ms = config->engine_tick_ms;
        }
    }
    if(result.engine_tick_ms < 250)
    {
        result.engine_tick_ms = 250;
    }
    return result;
}

int rawbit_app_init(RawBitApp* app, const RawBitAppConfig* config, HINSTANCE instance)
{
    if(!app)
    {
        return -1;
    }

    ZeroMemory(app, sizeof(*app));
    app->instance = instance;
    app->config = build_default_config(config);

    EngineSessionConfig engine_cfg;
    engine_session_config_default(&engine_cfg);
    engine_cfg.alert_interval_ms = app->config.engine_tick_ms;
    if(engine_session_init(&app->engine, &engine_cfg) != 0)
    {
        DebugOut("rawbit_app: Engine session failed to start.\n");
        rawbit_app_shutdown(app);
        return -2;
    }
    app->engine_initialized = 1;

    HttpServerConfig http_cfg;
    http_server_config_default(&http_cfg);
    http_cfg.port = app->config.http_port;
    http_cfg.broadcast_interval_ms = app->config.engine_tick_ms;
    http_cfg.engine = &app->engine;
    if(http_server_init(&app->http, &http_cfg) != 0)
    {
        DebugOut("rawbit_app: HTTP server failed to start.\n");
        rawbit_app_shutdown(app);
        return -3;
    }
    app->http_initialized = 1;

    LauncherWindowConfig launcher_cfg;
    ZeroMemory(&launcher_cfg, sizeof(launcher_cfg));
    launcher_cfg.instance = app->instance;
    launcher_cfg.http_port = app->config.http_port;
    launcher_cfg.command_callback = launcher_command_handler;
    launcher_cfg.callback_user_data = app;

    if(launcher_window_init(&app->launcher, &launcher_cfg) != 0)
    {
        DebugOut("rawbit_app: Launcher window failed to initialize.\n");
        rawbit_app_shutdown(app);
        return -4;
    }
    app->launcher_initialized = 1;

    return 0;
}

void rawbit_app_shutdown(RawBitApp* app)
{
    if(!app)
    {
        return;
    }

    if(app->launcher_initialized)
    {
        launcher_window_destroy(&app->launcher);
        app->launcher_initialized = 0;
    }

    if(app->http_initialized)
    {
        http_server_shutdown(&app->http);
        app->http_initialized = 0;
    }

    if(app->engine_initialized)
    {
        engine_session_shutdown(&app->engine);
        app->engine_initialized = 0;
    }
}

void rawbit_app_open_interface(const RawBitApp* app)
{
    if(!app || !app->launcher_initialized)
    {
        return;
    }
    launcher_window_open_browser(&app->launcher);
}

unsigned short rawbit_app_http_port(const RawBitApp* app)
{
    if(!app)
    {
        return 0;
    }
    return app->config.http_port;
}

static void launcher_command_handler(void* user_data, LauncherCommand command)
{
    RawBitApp* app = reinterpret_cast<RawBitApp*>(user_data);
    if(!app)
    {
        return;
    }

    switch(command)
    {
        case LauncherCommand_OpenInterface:
            rawbit_app_open_interface(app);
            break;
        case LauncherCommand_Quit:
            PostQuitMessage(0);
            break;
        default:
            break;
    }
}
