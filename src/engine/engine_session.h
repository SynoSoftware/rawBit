#pragma once

#include <windows.h>
#include <stdint.h>

struct EngineSessionConfig
{
    unsigned int alert_interval_ms;
};

struct EngineSession
{
    EngineSessionConfig config;
    HANDLE thread_handle;
    HANDLE stop_event;
    volatile LONG running;
};

void engine_session_config_default(EngineSessionConfig* config);
int engine_session_init(EngineSession* session, const EngineSessionConfig* config);
void engine_session_shutdown(EngineSession* session);

