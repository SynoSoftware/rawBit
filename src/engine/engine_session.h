#pragma once

#include <windows.h>
#include <stdint.h>

#include <vector>

struct EngineSessionConfig
{
    unsigned int alert_interval_ms;
};

struct EngineSessionStats
{
    unsigned int torrent_count;
    unsigned int active_count;
    unsigned long long download_rate;
    unsigned long long upload_rate;
};

struct EngineTorrentStatus
{
    unsigned int id;
    char name[128];
    char magnet_uri[256];
    float progress;
    unsigned long long size_bytes;
    unsigned long long downloaded_bytes;
    unsigned int download_rate;
    unsigned int upload_rate;
    int is_paused;
    int is_complete;
};

struct EngineSessionSnapshot
{
    EngineSessionStats stats;
    std::vector<EngineTorrentStatus> torrents;
};

struct EngineAddTorrentOptions
{
    const char* magnet_uri;
    const wchar_t* file_path;
    const char* display_name;
    unsigned long long size_bytes;
};

struct EngineSession
{
    EngineSessionConfig config;
    HANDLE thread_handle;
    HANDLE stop_event;
    volatile LONG running;
    CRITICAL_SECTION state_lock;
    int state_lock_initialized;
    void* state;
};

void engine_session_config_default(EngineSessionConfig* config);
int engine_session_init(EngineSession* session, const EngineSessionConfig* config);
void engine_session_shutdown(EngineSession* session);
int engine_session_add_torrent(EngineSession* session, const EngineAddTorrentOptions* options, unsigned int* out_torrent_id);
int engine_session_pause_torrent(EngineSession* session, unsigned int torrent_id);
int engine_session_resume_torrent(EngineSession* session, unsigned int torrent_id);
int engine_session_remove_torrent(EngineSession* session, unsigned int torrent_id);
void engine_session_snapshot(EngineSession* session, EngineSessionSnapshot* snapshot);
