#pragma once
#include <stdint.h>
#include <stddef.h>

#define MAX_FILES_PER_TORRENT 256
#define MAX_PATH_COMPONENTS    16
#define MAX_FILENAME_LEN      256
#define MAX_ANNOUNCE_URL_LEN  512
#define MAX_ANNOUNCE_TIERS    10    // Max tracker tiers in announce-list
#define MAX_TRACKERS_PER_TIER 10    // Max trackers per tier

typedef struct TorrentFile
{
    // ... (same as before)
    long long length;
    char path[MAX_PATH_COMPONENTS][MAX_FILENAME_LEN];
    int path_depth;
} TorrentFile;

// Structure to hold tiered announce URLs
typedef struct AnnounceTier
{
    char urls[MAX_TRACKERS_PER_TIER][MAX_ANNOUNCE_URL_LEN];
    int url_count;
} AnnounceTier;

typedef struct Torrent
{
    // --- Core Info ---
    char name[MAX_FILENAME_LEN];
    unsigned char info_hash[20];      // <<< ADDED: SHA-1 hash of the bencoded info dict
    long long total_size;
    int piece_length;
    unsigned char* pieces;            // Dynamic buffer for piece hashes (SHA-1 sums)
    int piece_count;

    // --- File Structure ---
    int file_count;                   // 0 for single file before processing, 1 after; >1 for multi
    TorrentFile files[MAX_FILES_PER_TORRENT]; // Array for file details

    // --- Tracker Info ---
    char announce[MAX_ANNOUNCE_URL_LEN]; // Primary announce URL (optional if announce-list exists)
    AnnounceTier announce_list[MAX_ANNOUNCE_TIERS]; // <<< ADDED: Tiered list support
    int announce_tier_count;          // <<< ADDED: Number of tiers in announce_list

    // --- Optional Info (Add as needed) ---
    // char comment[512];
    // char created_by[128];
    // long long creation_date; // Unix timestamp
    // char encoding[32];

} Torrent;

// Load torrent data *and calculate info_hash*
int torrent_load(const char* path, Torrent* t);

void torrent_free(Torrent* t); // (Definition should already exist in torrent.c)

// --- Hashing ---
// You need to implement or link this function externally.
// It takes a pointer to the data and its length, and fills the 20-byte hash buffer.
// Returns 0 on success, -1 on error.
int calculate_sha1(const unsigned char* data, size_t data_len, unsigned char hash_out[20]);