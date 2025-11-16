#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TorrentEngine TorrentEngine;

typedef void (*TorrentAlertHandler)(const char* message, int category_bits, void* user_data);

TorrentEngine* torrent_engine_create(void);
void torrent_engine_destroy(TorrentEngine* engine);

int torrent_engine_add_torrent_file(TorrentEngine* engine,
    const char* torrent_path,
    const char* download_directory,
    char* error_buffer,
    size_t error_buffer_len);

void torrent_engine_drain_alerts(TorrentEngine* engine,
    TorrentAlertHandler handler,
    void* user_data);

#ifdef __cplusplus
}
#endif
