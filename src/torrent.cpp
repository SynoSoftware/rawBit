#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>

extern "C" {
#include "torrent.h"
#include "debug.h"
}

#include <libtorrent/announce_entry.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/info_hash.hpp>
#include <libtorrent/torrent_info.hpp>

namespace
{
    namespace lt = libtorrent;

    constexpr std::size_t kSha1Length = 20;

    std::string to_utf8(const std::filesystem::path& path)
    {
#if defined(_WIN32)
        auto u8 = path.u8string();
        return std::string(u8.begin(), u8.end());
#else
        return path.string();
#endif
    }

    void copy_string(const std::string& from, char* dest, std::size_t dest_size)
    {
        if(dest == nullptr || dest_size == 0)
        {
            return;
        }
#if defined(_MSC_VER)
        strncpy_s(dest, dest_size, from.c_str(), _TRUNCATE);
#else
        std::strncpy(dest, from.c_str(), dest_size - 1);
        dest[dest_size - 1] = '\0';
#endif
    }

    void populate_piece_hashes(const lt::torrent_info& info, Torrent* torrent)
    {
        const int pieces = info.num_pieces();
        torrent->piece_count = pieces;
        if(pieces <= 0)
        {
            torrent->pieces = nullptr;
            return;
        }

        const std::size_t total_bytes = static_cast<std::size_t>(pieces) * kSha1Length;
        torrent->pieces = static_cast<unsigned char*>(std::malloc(total_bytes));
        if(!torrent->pieces)
        {
            torrent->piece_count = 0;
            DebugOut("torrent_load: Failed to allocate %zu bytes for piece hashes.\n", total_bytes);
            return;
        }

        unsigned char* write_ptr = torrent->pieces;
        for(int i = 0; i < pieces; ++i)
        {
            const lt::sha1_hash hash = info.hash_for_piece(i);
            std::memcpy(write_ptr, hash.data(), kSha1Length);
            write_ptr += kSha1Length;
        }
    }

    void populate_trackers(const lt::torrent_info& info, Torrent* torrent)
    {
        const auto& trackers = info.trackers();
        if(trackers.empty())
        {
            torrent->announce[0] = '\0';
            torrent->announce_tier_count = 0;
            return;
        }

        copy_string(trackers.front().url, torrent->announce, sizeof(torrent->announce));

        std::array<int, MAX_ANNOUNCE_TIERS> tier_map{};
        tier_map.fill(std::numeric_limits<int>::max());
        int tier_count = 0;

        for(const auto& entry : trackers)
        {
            int tier_value = entry.tier >= 0 ? entry.tier : 0;
            int tier_index = -1;
            for(int i = 0; i < tier_count; ++i)
            {
                if(tier_map[static_cast<std::size_t>(i)] == tier_value)
                {
                    tier_index = i;
                    break;
                }
            }

            if(tier_index == -1)
            {
                if(tier_count >= MAX_ANNOUNCE_TIERS)
                {
                    DebugOut("torrent_load: Tracker tiers exceed limit (%d), skipping '%s'.\n", MAX_ANNOUNCE_TIERS, entry.url.c_str());
                    continue;
                }
                tier_index = tier_count;
                tier_map[static_cast<std::size_t>(tier_count)] = tier_value;
                ++tier_count;
            }

            AnnounceTier* tier = &torrent->announce_list[tier_index];
            if(tier->url_count >= MAX_TRACKERS_PER_TIER)
            {
                DebugOut("torrent_load: Tier %d tracker count exceeds limit (%d), skipping '%s'.\n", tier_index, MAX_TRACKERS_PER_TIER, entry.url.c_str());
                continue;
            }

            copy_string(entry.url, tier->urls[tier->url_count], sizeof(tier->urls[tier->url_count]));
            ++tier->url_count;
        }

        torrent->announce_tier_count = tier_count;
    }

    void populate_files(const lt::torrent_info& info, Torrent* torrent)
    {
        const lt::file_storage& files = info.files();
        const int total_files = files.num_files();
        torrent->file_count = std::min(total_files, MAX_FILES_PER_TORRENT);
        if(total_files > MAX_FILES_PER_TORRENT)
        {
            DebugOut("torrent_load: Warning - torrent contains %d files, truncating to %d entries.\n", total_files, MAX_FILES_PER_TORRENT);
        }

        for(int i = 0; i < torrent->file_count; ++i)
        {
            TorrentFile* dest = &torrent->files[i];
            dest->length = files.file_size(i);
            std::string path = files.file_path(i);
            dest->path_depth = 0;

            std::size_t start = 0;
            while(start < path.size() && dest->path_depth < MAX_PATH_COMPONENTS)
            {
                std::size_t next = path.find_first_of("/\\", start);
                const std::size_t len = (next == std::string::npos) ? path.size() - start : next - start;
                if(len > 0)
                {
                    std::string component = path.substr(start, len);
                    copy_string(component, dest->path[dest->path_depth], sizeof(dest->path[dest->path_depth]));
                    ++dest->path_depth;
                }
                if(next == std::string::npos)
                {
                    break;
                }
                start = next + 1;
            }
        }
    }
}

extern "C" int torrent_load(const char* path, Torrent* torrent)
{
    if(!path || !torrent)
    {
        DebugOut("torrent_load: Invalid arguments (path or torrent is NULL).\n");
        return -1;
    }

    lt::error_code ec;
    lt::load_torrent_limits limits;
    std::string filename(path);
    std::unique_ptr<lt::torrent_info> info = std::make_unique<lt::torrent_info>(filename, limits, ec);
    if(ec)
    {
        DebugOut("torrent_load: Failed to parse '%s' (%s).\n", path, ec.message().c_str());
        return -2;
    }

    std::memset(torrent, 0, sizeof(*torrent));

    copy_string(info->name(), torrent->name, sizeof(torrent->name));
    torrent->total_size = info->total_size();
    torrent->piece_length = info->piece_length();

    const lt::sha1_hash info_hash = info->info_hash();
    std::memcpy(torrent->info_hash, info_hash.data(), kSha1Length);

    populate_piece_hashes(*info, torrent);
    populate_files(*info, torrent);
    populate_trackers(*info, torrent);

    DebugOut("torrent_load: Loaded '%s' (%lld bytes, %d files).\n", torrent->name, torrent->total_size, torrent->file_count);
    return 0;
}

extern "C" void torrent_free(Torrent* torrent)
{
    if(!torrent)
    {
        return;
    }

    if(torrent->pieces)
    {
        std::free(torrent->pieces);
        torrent->pieces = nullptr;
    }
}
