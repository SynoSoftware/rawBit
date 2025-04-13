#pragma once // Use header guards
#include <stdint.h>
#include <stddef.h> // For size_t

#define MAX_FILES_PER_TORRENT 256 // Increased slightly, but still a potential limit
#define MAX_PATH_COMPONENTS    16 // Increased slightly
#define MAX_FILENAME_LEN      256 // Increased path component length

typedef struct TorrentFile
{
    long long length;
    char path[MAX_PATH_COMPONENTS][MAX_FILENAME_LEN]; // Increased component length
    int path_depth;
} TorrentFile;

typedef struct Torrent
{
    char name[MAX_FILENAME_LEN];                // Consistent sizing with path components
    char announce[512];                         // Increased size for potentially long announce URLs
    int piece_length;
    unsigned char* pieces;                      // Dynamically allocated buffer for piece hashes
    int piece_count;
    long long total_size;
    int file_count;
    TorrentFile files[MAX_FILES_PER_TORRENT];   // Still fixed, but increased limit. Dynamic might be better for extreme cases.
} Torrent;

// Function to load torrent data from a file
// Returns 0 on success, negative value on error.
int torrent_load(const char* path, Torrent* t);

// Function to free memory allocated within the Torrent struct (specifically t->pieces)
void torrent_free(Torrent* t);