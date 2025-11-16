#include "engine/engine_session.h"

#include <algorithm>
#include <new>
#include <string>

#include <wchar.h>
#include <string.h>

#include "debug.h"

namespace
{
    struct EngineTorrentEntry
    {
        unsigned int id;
        std::string name;
        std::string magnet_uri;
        unsigned long long size_bytes;
        unsigned long long downloaded_bytes;
        unsigned int download_rate;
        unsigned int upload_rate;
        float progress;
        bool paused;
        bool complete;
    };

    struct EngineSessionState
    {
        std::vector<EngineTorrentEntry> torrents;
        unsigned int next_torrent_id;
    };

    const unsigned long long kDefaultTorrentSize = 512ull * 1024ull * 1024ull;
    const unsigned int kActiveDownloadRate = 256u * 1024u;

    static EngineSessionState* create_state()
    {
        EngineSessionState* state = new (std::nothrow) EngineSessionState();
        if(state)
        {
            state->next_torrent_id = 1;
        }
        return state;
    }

    static void destroy_state(EngineSessionState* state)
    {
        delete state;
    }

    static EngineSessionState* session_state(EngineSession* session)
    {
        return reinterpret_cast<EngineSessionState*>(session->state);
    }

    static EngineTorrentEntry* find_entry(EngineSessionState* state, unsigned int id)
    {
        if(!state)
        {
            return nullptr;
        }

        for(size_t i = 0; i < state->torrents.size(); ++i)
        {
            if(state->torrents[i].id == id)
            {
                return &state->torrents[i];
            }
        }
        return nullptr;
    }

    static void clamp_string(std::string& value, size_t max_len)
    {
        if(value.length() > max_len)
        {
            value.erase(value.begin() + static_cast<std::string::difference_type>(max_len), value.end());
        }
    }

    static std::string wide_to_utf8(const wchar_t* source)
    {
        if(!source)
        {
            return std::string();
        }
        char buffer[MAX_PATH];
        const int written = WideCharToMultiByte(CP_UTF8, 0, source, -1, buffer, sizeof(buffer), nullptr, nullptr);
        if(written <= 0)
        {
            return std::string();
        }
        return std::string(buffer);
    }

    static std::string extract_file_name_utf8(const wchar_t* path)
    {
        if(!path)
        {
            return std::string();
        }

        const wchar_t* tail = wcsrchr(path, L'\\');
        if(!tail)
        {
            tail = wcsrchr(path, L'/');
        }
        if(tail && tail[1] != L'\0')
        {
            return wide_to_utf8(tail + 1);
        }
        return wide_to_utf8(path);
    }

    static std::string determine_display_name(const EngineAddTorrentOptions* options)
    {
        if(!options)
        {
            return std::string("torrent");
        }
        if(options->display_name && options->display_name[0] != '\0')
        {
            return std::string(options->display_name);
        }
        std::string from_path = extract_file_name_utf8(options->file_path);
        if(!from_path.empty())
        {
            return from_path;
        }
        if(options->magnet_uri && options->magnet_uri[0] != '\0')
        {
            return std::string(options->magnet_uri);
        }
        return std::string("torrent");
    }

    static unsigned long long determine_size_bytes(const EngineAddTorrentOptions* options)
    {
        if(options && options->size_bytes > 0)
        {
            return options->size_bytes;
        }
        return kDefaultTorrentSize;
    }

    static void simulate_progress(EngineTorrentEntry& entry)
    {
        if(entry.paused)
        {
            entry.download_rate = 0;
            entry.upload_rate = entry.complete ? (kActiveDownloadRate / 8) : 0;
            return;
        }

        if(entry.complete)
        {
            entry.download_rate = 0;
            entry.upload_rate = entry.size_bytes > 0 ? (kActiveDownloadRate / 4) : 0;
            return;
        }

        const unsigned long long chunk = entry.size_bytes / 80ull + static_cast<unsigned long long>(kActiveDownloadRate);
        unsigned long long new_downloaded = entry.downloaded_bytes + chunk;
        if(entry.size_bytes > 0 && new_downloaded >= entry.size_bytes)
        {
            new_downloaded = entry.size_bytes;
            entry.complete = true;
            entry.download_rate = 0;
            entry.upload_rate = kActiveDownloadRate / 6;
        }
        else
        {
            entry.download_rate = kActiveDownloadRate;
            entry.upload_rate = kActiveDownloadRate / 12;
        }

        entry.downloaded_bytes = new_downloaded;
        if(entry.size_bytes > 0)
        {
            entry.progress = static_cast<float>(entry.downloaded_bytes) / static_cast<float>(entry.size_bytes);
        }
        else
        {
            entry.progress = entry.complete ? 1.0f : 0.0f;
        }
    }

    static void copy_status(const EngineTorrentEntry& entry, EngineTorrentStatus& status)
    {
        status.id = entry.id;
        strncpy_s(status.name, entry.name.c_str(), _TRUNCATE);
        strncpy_s(status.magnet_uri, entry.magnet_uri.c_str(), _TRUNCATE);
        status.progress = entry.progress;
        status.size_bytes = entry.size_bytes;
        status.downloaded_bytes = entry.downloaded_bytes;
        status.download_rate = entry.download_rate;
        status.upload_rate = entry.upload_rate;
        status.is_paused = entry.paused ? 1 : 0;
        status.is_complete = entry.complete ? 1 : 0;
    }

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

            unsigned int torrent_count = 0;

            EnterCriticalSection(&session->state_lock);
            EngineSessionState* state = session_state(session);
            if(state)
            {
                torrent_count = static_cast<unsigned int>(state->torrents.size());
                for(size_t i = 0; i < state->torrents.size(); ++i)
                {
                    simulate_progress(state->torrents[i]);
                }
            }
            LeaveCriticalSection(&session->state_lock);

            DebugOut("engine_session: tick (%u torrents)\n", torrent_count);
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

    InitializeCriticalSection(&session->state_lock);
    session->state_lock_initialized = 1;

    session->stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if(!session->stop_event)
    {
        DeleteCriticalSection(&session->state_lock);
        session->state_lock_initialized = 0;
        return -2;
    }

    session->state = create_state();
    if(!session->state)
    {
        CloseHandle(session->stop_event);
        session->stop_event = nullptr;
        DeleteCriticalSection(&session->state_lock);
        session->state_lock_initialized = 0;
        return -3;
    }

    session->running = 1;
    session->thread_handle = CreateThread(nullptr, 0, engine_session_thread, session, 0, nullptr);
    if(!session->thread_handle)
    {
        CloseHandle(session->stop_event);
        session->stop_event = nullptr;
        session->running = 0;
        destroy_state(session_state(session));
        session->state = nullptr;
        DeleteCriticalSection(&session->state_lock);
        session->state_lock_initialized = 0;
        return -4;
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

    if(session->state_lock_initialized)
    {
        EnterCriticalSection(&session->state_lock);
        destroy_state(session_state(session));
        session->state = nullptr;
        LeaveCriticalSection(&session->state_lock);
        DeleteCriticalSection(&session->state_lock);
        session->state_lock_initialized = 0;
    }

    session->running = 0;
    DebugOut("engine_session: stopped.\n");
}

int engine_session_add_torrent(EngineSession* session, const EngineAddTorrentOptions* options, unsigned int* out_torrent_id)
{
    if(!session || !options)
    {
        return -1;
    }

    EngineTorrentEntry entry;
    entry.id = 0;
    entry.name = determine_display_name(options);
    clamp_string(entry.name, 120);
    if(options->magnet_uri && options->magnet_uri[0] != '\0')
    {
        entry.magnet_uri = options->magnet_uri;
        clamp_string(entry.magnet_uri, 200);
    }
    entry.size_bytes = determine_size_bytes(options);
    entry.downloaded_bytes = 0;
    entry.download_rate = 0;
    entry.upload_rate = 0;
    entry.progress = 0.0f;
    entry.paused = false;
    entry.complete = false;

    int result = -2;

    EnterCriticalSection(&session->state_lock);
    EngineSessionState* state = session_state(session);
    if(state)
    {
        entry.id = state->next_torrent_id++;
        state->torrents.push_back(entry);
        if(out_torrent_id)
        {
            *out_torrent_id = entry.id;
        }
        result = 0;
    }
    LeaveCriticalSection(&session->state_lock);

    return result;
}

int engine_session_pause_torrent(EngineSession* session, unsigned int torrent_id)
{
    if(!session || torrent_id == 0)
    {
        return -1;
    }

    int result = -2;
    EnterCriticalSection(&session->state_lock);
    EngineSessionState* state = session_state(session);
    EngineTorrentEntry* entry = find_entry(state, torrent_id);
    if(entry)
    {
        entry->paused = true;
        result = 0;
    }
    LeaveCriticalSection(&session->state_lock);
    return result;
}

int engine_session_resume_torrent(EngineSession* session, unsigned int torrent_id)
{
    if(!session || torrent_id == 0)
    {
        return -1;
    }

    int result = -2;
    EnterCriticalSection(&session->state_lock);
    EngineSessionState* state = session_state(session);
    EngineTorrentEntry* entry = find_entry(state, torrent_id);
    if(entry)
    {
        if(!entry->complete)
        {
            entry->paused = false;
        }
        result = 0;
    }
    LeaveCriticalSection(&session->state_lock);
    return result;
}

int engine_session_remove_torrent(EngineSession* session, unsigned int torrent_id)
{
    if(!session || torrent_id == 0)
    {
        return -1;
    }

    int result = -2;
    EnterCriticalSection(&session->state_lock);
    EngineSessionState* state = session_state(session);
    if(state)
    {
        const size_t original = state->torrents.size();
        state->torrents.erase(
            std::remove_if(state->torrents.begin(), state->torrents.end(),
                [torrent_id](const EngineTorrentEntry& entry) { return entry.id == torrent_id; }),
            state->torrents.end());
        if(state->torrents.size() != original)
        {
            result = 0;
        }
    }
    LeaveCriticalSection(&session->state_lock);

    return result;
}

void engine_session_snapshot(EngineSession* session, EngineSessionSnapshot* snapshot)
{
    if(!session || !snapshot)
    {
        return;
    }

    snapshot->torrents.clear();
    snapshot->stats.torrent_count = 0;
    snapshot->stats.active_count = 0;
    snapshot->stats.download_rate = 0;
    snapshot->stats.upload_rate = 0;

    EnterCriticalSection(&session->state_lock);
    EngineSessionState* state = session_state(session);
    if(state)
    {
        snapshot->torrents.reserve(state->torrents.size());
        for(size_t i = 0; i < state->torrents.size(); ++i)
        {
            EngineTorrentStatus status;
            ZeroMemory(&status, sizeof(status));
            copy_status(state->torrents[i], status);
            snapshot->torrents.push_back(status);
        }
    }
    LeaveCriticalSection(&session->state_lock);

    snapshot->stats.torrent_count = static_cast<unsigned int>(snapshot->torrents.size());
    snapshot->stats.active_count = 0;
    snapshot->stats.download_rate = 0;
    snapshot->stats.upload_rate = 0;
    for(size_t i = 0; i < snapshot->torrents.size(); ++i)
    {
        const EngineTorrentStatus& status = snapshot->torrents[i];
        if(status.is_complete == 0 && status.is_paused == 0)
        {
            snapshot->stats.active_count++;
        }
        snapshot->stats.download_rate += status.download_rate;
        snapshot->stats.upload_rate += status.upload_rate;
    }
}
