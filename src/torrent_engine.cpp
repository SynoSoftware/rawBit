#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "torrent_engine.h"
#include "debug.h"
}

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_info.hpp>

namespace
{
    namespace lt = libtorrent;

    std::string path_to_narrow(const std::filesystem::path& path)
    {
#if defined(_WIN32)
        return path.u8string();
#else
        return path.string();
#endif
    }

    void copy_error(char* buffer, size_t buffer_len, const std::string& message)
    {
        if(!buffer || buffer_len == 0)
        {
            return;
        }
#if defined(_MSC_VER)
        strncpy_s(buffer, buffer_len, message.c_str(), _TRUNCATE);
#else
        std::strncpy(buffer, message.c_str(), buffer_len - 1);
        buffer[buffer_len - 1] = '\0';
#endif
    }

    std::filesystem::path default_download_path()
    {
        std::filesystem::path base = std::filesystem::current_path();
        return base / std::filesystem::path("downloads");
    }
}

struct TorrentEngine
{
    std::unique_ptr<lt::session> session;
};

extern "C" TorrentEngine* torrent_engine_create(void)
{
    try
    {
        lt::settings_pack pack;
        pack.set_bool(lt::settings_pack::enable_dht, true);
        pack.set_bool(lt::settings_pack::enable_upnp, true);
        pack.set_bool(lt::settings_pack::enable_lsd, true);
        pack.set_bool(lt::settings_pack::enable_natpmp, true);
        pack.set_int(lt::settings_pack::alert_mask,
            lt::alert_category::status |
            lt::alert_category::error |
            lt::alert_category::storage |
            lt::alert_category::tracker |
            lt::alert_category::connect);

        auto engine = std::make_unique<TorrentEngine>();
        engine->session = std::make_unique<lt::session>(pack);
        DebugOut("torrent_engine: libtorrent session created.\n");
        return engine.release();
    }
    catch(const std::exception& ex)
    {
        DebugOut("torrent_engine: Failed to create session (%s).\n", ex.what());
        return nullptr;
    }
}

extern "C" void torrent_engine_destroy(TorrentEngine* engine)
{
    if(!engine)
    {
        return;
    }

    engine->session.reset();
    delete engine;
    DebugOut("torrent_engine: destroyed session.\n");
}

extern "C" int torrent_engine_add_torrent_file(TorrentEngine* engine,
    const char* torrent_path,
    const char* download_directory,
    char* error_buffer,
    size_t error_buffer_len)
{
    if(!engine || !engine->session)
    {
        copy_error(error_buffer, error_buffer_len, "Session not initialized");
        return -1;
    }
    if(!torrent_path || torrent_path[0] == '\0')
    {
        copy_error(error_buffer, error_buffer_len, "Torrent path is empty");
        return -2;
    }

    std::filesystem::path torrent_file(torrent_path);
    if(!std::filesystem::exists(torrent_file))
    {
        std::string message = "Torrent file not found: ";
        message += torrent_path;
        copy_error(error_buffer, error_buffer_len, message);
        DebugOut("torrent_engine: %s\n", message.c_str());
        return -3;
    }

    std::filesystem::path target_dir;
    if(download_directory && download_directory[0] != '\0')
    {
        target_dir = std::filesystem::path(download_directory);
    }
    else
    {
        target_dir = default_download_path();
    }

    std::string target_dir_string = path_to_narrow(target_dir);

    std::error_code fs_error;
    std::filesystem::create_directories(target_dir, fs_error);
    if(fs_error)
    {
        DebugOut("torrent_engine: Failed to create download directory '%s' (%s).\n",
            target_dir_string.c_str(), fs_error.message().c_str());
    }

    lt::error_code ec;
    const std::string torrent_file_string = path_to_narrow(torrent_file);
    auto info = std::make_shared<lt::torrent_info>(torrent_file_string, ec);
    if(ec)
    {
        copy_error(error_buffer, error_buffer_len, ec.message());
        DebugOut("torrent_engine: Failed to load torrent '%s' (%s).\n", torrent_path, ec.message().c_str());
        return -4;
    }

    lt::add_torrent_params params;
    params.ti = info;
    params.save_path = target_dir_string;
    params.flags |= lt::torrent_flags::auto_managed;
    params.flags &= ~lt::torrent_flags::paused;

    try
    {
        engine->session->async_add_torrent(std::move(params));
    }
    catch(const std::exception& ex)
    {
        copy_error(error_buffer, error_buffer_len, ex.what());
        DebugOut("torrent_engine: async_add_torrent threw (%s).\\n", ex.what());
        return -5;
    }
    DebugOut("torrent_engine: queued '%s' for download into '%s'.\n",
        info->name().c_str(), params.save_path.c_str());

    return 0;
}

extern "C" void torrent_engine_drain_alerts(TorrentEngine* engine,
    TorrentAlertHandler handler,
    void* user_data)
{
    if(!engine || !engine->session)
    {
        return;
    }

    std::vector<lt::alert*> alerts;
    engine->session->pop_alerts(&alerts);
    for(const lt::alert* alert : alerts)
    {
        const std::string message = alert->message();
        if(handler)
        {
            handler(message.c_str(), alert->category(), user_data);
        }
        else
        {
            DebugOut("libtorrent: %s\n", message.c_str());
        }
    }
}
