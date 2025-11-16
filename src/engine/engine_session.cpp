#include "engine/engine_session.h"

#include "debug.h"

namespace
{
    DWORD WINAPI engine_session_thread(LPVOID context)
    {
        EngineSession* session = reinterpret_cast<EngineSession*>(context);
        if(!session)
        {
            return 0;
        }

        while(true)
        {
            const DWORD wait_result = WaitForSingleObject(session->stop_event, session->config.alert_interval_ms);
            if(wait_result != WAIT_TIMEOUT)
            {
                break;
            }

            DebugOut("engine_session: tick.\n");
            // TODO: poll libtorrent alerts and issue events.
        }

        InterlockedExchange(&session->running, 0);
        return 0;
    }
}

void engine_session_config_default(EngineSessionConfig* config)
{
    if(!config)
    {
        return;
    }
    config->alert_interval_ms = 500;
}

int engine_session_init(EngineSession* session, const EngineSessionConfig* config)
{
    if(!session)
    {
        return -1;
    }

    ZeroMemory(session, sizeof(*session));

    EngineSessionConfig local_config;
    engine_session_config_default(&local_config);
    if(config)
    {
        local_config = *config;
    }
    if(local_config.alert_interval_ms < 250)
    {
        local_config.alert_interval_ms = 250;
    }
    session->config = local_config;

    session->stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if(!session->stop_event)
    {
        return -2;
    }

    session->running = 1;
    session->thread_handle = CreateThread(nullptr, 0, engine_session_thread, session, 0, nullptr);
    if(!session->thread_handle)
    {
        CloseHandle(session->stop_event);
        session->stop_event = nullptr;
        session->running = 0;
        return -3;
    }

    DebugOut("engine_session: started (interval=%u ms).\n", session->config.alert_interval_ms);
    return 0;
}

void engine_session_shutdown(EngineSession* session)
{
    if(!session)
    {
        return;
    }

    if(session->stop_event)
    {
        SetEvent(session->stop_event);
    }

    if(session->thread_handle)
    {
        WaitForSingleObject(session->thread_handle, 3000);
        CloseHandle(session->thread_handle);
        session->thread_handle = nullptr;
    }

    if(session->stop_event)
    {
        CloseHandle(session->stop_event);
        session->stop_event = nullptr;
    }

    session->running = 0;
    DebugOut("engine_session: stopped.\n");
}

