#include "net/http_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>

#include <string>

extern "C" {
#include "mongoose.h"
}

#include "engine/engine_session.h"
#include "debug.h"

namespace
{
    static void determine_web_root(wchar_t* dest, size_t dest_count, const wchar_t* requested)
    {
        if(!dest || dest_count == 0)
        {
            return;
        }
        dest[0] = L'\0';

        if(requested && requested[0] != L'\0')
        {
            wcsncpy_s(dest, dest_count, requested, _TRUNCATE);
            return;
        }

        wchar_t module_path[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, module_path, MAX_PATH);
        if(len == 0 || len >= MAX_PATH)
        {
            return;
        }

        while(len > 0)
        {
            if(module_path[len - 1] == L'\\' || module_path[len - 1] == L'/')
            {
                module_path[len - 1] = L'\0';
                break;
            }
            --len;
        }

        wcsncpy_s(dest, dest_count, module_path, _TRUNCATE);

        size_t current_len = wcslen(dest);
        if(current_len > 0 && dest[current_len - 1] != L'\\')
        {
            wcscat_s(dest, dest_count, L"\\");
        }
        wcscat_s(dest, dest_count, L"webui\\dist");
    }

    static void wide_to_utf8(const wchar_t* source, char* dest, size_t dest_len)
    {
        if(!dest || dest_len == 0)
        {
            return;
        }
        dest[0] = '\0';
        if(!source || source[0] == L'\0')
        {
            return;
        }
        WideCharToMultiByte(CP_UTF8, 0, source, -1, dest, static_cast<int>(dest_len), nullptr, nullptr);
    }

    static void reset_snapshot(EngineSessionSnapshot* snapshot)
    {
        if(!snapshot)
        {
            return;
        }
        snapshot->torrents.clear();
        snapshot->stats.torrent_count = 0;
        snapshot->stats.active_count = 0;
        snapshot->stats.download_rate = 0;
        snapshot->stats.upload_rate = 0;
    }

    static void collect_snapshot(const HttpServer* server, EngineSessionSnapshot* snapshot)
    {
        if(!snapshot)
        {
            return;
        }
        reset_snapshot(snapshot);
        if(server && server->config.engine)
        {
            engine_session_snapshot(server->config.engine, snapshot);
        }
    }

    static void append_json_escape(std::string& out, const char* text)
    {
        out.push_back('"');
        if(text)
        {
            while(*text)
            {
                const unsigned char ch = static_cast<unsigned char>(*text);
                switch(ch)
                {
                    case '\\': out.append("\\\\"); break;
                    case '"': out.append("\\\""); break;
                    case '\b': out.append("\\b"); break;
                    case '\f': out.append("\\f"); break;
                    case '\n': out.append("\\n"); break;
                    case '\r': out.append("\\r"); break;
                    case '\t': out.append("\\t"); break;
                    default:
                        if(ch < 0x20)
                        {
                            const char hex[] = "0123456789abcdef";
                            out.append("\\u00");
                            out.push_back(hex[(ch >> 4) & 0x0F]);
                            out.push_back(hex[ch & 0x0F]);
                        }
                        else
                        {
                            out.push_back(static_cast<char>(ch));
                        }
                        break;
                }
                ++text;
            }
        }
        out.push_back('"');
    }

    static void append_uint(std::string& out, unsigned long long value)
    {
        char buffer[32];
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "%llu", value);
        out.append(buffer);
    }

    static void append_float(std::string& out, float value)
    {
        char buffer[32];
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "%.4f", value);
        out.append(buffer);
    }

    static void append_stats_fields(std::string& out, const EngineSessionSnapshot& snapshot, unsigned short port)
    {
        out.append("\"port\":");
        append_uint(out, port);
        out.append(",\"torrent_count\":");
        append_uint(out, snapshot.stats.torrent_count);
        out.append(",\"active\":");
        append_uint(out, snapshot.stats.active_count);
        out.append(",\"download_rate\":");
        append_uint(out, snapshot.stats.download_rate);
        out.append(",\"upload_rate\":");
        append_uint(out, snapshot.stats.upload_rate);
    }

    static void build_session_payload(const HttpServer* server, const EngineSessionSnapshot& snapshot, std::string& out)
    {
        out.clear();
        out.reserve(256);
        out.push_back('{');
        append_stats_fields(out, snapshot, server->config.port);
        out.push_back('}');
    }

    static void build_torrents_payload(const HttpServer* server, const EngineSessionSnapshot& snapshot, std::string& out)
    {
        out.clear();
        out.reserve(512);
        out.append("{\"stats\":{");
        append_stats_fields(out, snapshot, server->config.port);
        out.append("},\"torrents\":[");
        for(size_t i = 0; i < snapshot.torrents.size(); ++i)
        {
            if(i != 0)
            {
                out.push_back(',');
            }
            const EngineTorrentStatus& status = snapshot.torrents[i];
            out.append("{\"id\":");
            append_uint(out, status.id);
            out.append(",\"name\":");
            append_json_escape(out, status.name);
            out.append(",\"magnet\":");
            append_json_escape(out, status.magnet_uri);
            out.append(",\"progress\":");
            append_float(out, status.progress);
            out.append(",\"size\":");
            append_uint(out, status.size_bytes);
            out.append(",\"downloaded\":");
            append_uint(out, status.downloaded_bytes);
            out.append(",\"download_rate\":");
            append_uint(out, status.download_rate);
            out.append(",\"upload_rate\":");
            append_uint(out, status.upload_rate);
            out.append(",\"paused\":");
            out.append(status.is_paused ? "true" : "false");
            out.append(",\"complete\":");
            out.append(status.is_complete ? "true" : "false");
            out.push_back('}');
        }
        out.append("]}");
    }

    static void respond_json(struct mg_connection* connection, int code, const std::string& body)
    {
        mg_http_reply(connection, code,
            "Content-Type: application/json\r\nCache-Control: no-cache\r\n",
            "%s", body.c_str());
    }

    static void respond_error(struct mg_connection* connection, int code, const char* message)
    {
        mg_http_reply(connection, code,
            "Content-Type: application/json\r\nCache-Control: no-cache\r\n",
            "{ \"error\": \"%s\" }\n", message ? message : "unknown");
    }

    static void respond_ok(struct mg_connection* connection)
    {
        mg_http_reply(connection, 200,
            "Content-Type: application/json\r\nCache-Control: no-cache\r\n",
            "{ \"status\": \"ok\" }\n");
    }

    static bool parse_torrent_path(const struct mg_http_message* message, unsigned int* out_id, std::string& action)
    {
        if(!message)
        {
            return false;
        }

        std::string uri(message->uri.ptr, message->uri.len);
        const std::string prefix = "/api/torrents/";
        if(uri.rfind(prefix, 0) != 0)
        {
            return false;
        }

        const size_t id_start = prefix.length();
        const size_t slash_pos = uri.find('/', id_start);
        std::string id_str = uri.substr(id_start, slash_pos - id_start);
        if(id_str.empty())
        {
            return false;
        }

        char* end_ptr = nullptr;
        const unsigned long value = strtoul(id_str.c_str(), &end_ptr, 10);
        if(value == 0 || (end_ptr && *end_ptr != '\0'))
        {
            return false;
        }

        if(out_id)
        {
            *out_id = static_cast<unsigned int>(value);
        }

        if(slash_pos != std::string::npos && slash_pos + 1 < uri.length())
        {
            action = uri.substr(slash_pos + 1);
        }
        else
        {
            action.clear();
        }

        return true;
    }

    static bool extract_json_string(struct mg_str json, const char* path, char* buffer, size_t buffer_len)
    {
        if(!buffer || buffer_len == 0)
        {
            return false;
        }
        buffer[0] = '\0';

        char* value = mg_json_get_str(json, path);
        if(!value)
        {
            return false;
        }

        const errno_t copy_result = strncpy_s(buffer, buffer_len, value, _TRUNCATE);
        free(value);
        return copy_result == 0 && buffer[0] != '\0';
    }

    static bool http_method_is(const struct mg_http_message* message, const char* method)
    {
        if(!message || !method)
        {
            return false;
        }
        const struct mg_str expected = mg_str(method);
        return mg_strcmp(message->method, expected) == 0;
    }

    static bool http_uri_matches(const struct mg_http_message* message, const char* pattern)
    {
        if(!message || !pattern)
        {
            return false;
        }
        const struct mg_str uri = mg_str_n(message->uri.ptr, message->uri.len);
        const struct mg_str pat = mg_str(pattern);
        return mg_match(uri, pat, nullptr);
    }

    static void handle_add_torrent(struct mg_connection* connection, HttpServer* server, const struct mg_http_message* message)
    {
        if(!server->config.engine)
        {
            respond_error(connection, 503, "engine-unavailable");
            return;
        }

        char magnet[512];
        char name[128];
        magnet[0] = '\0';
        name[0] = '\0';

        if(!extract_json_string(message->body, "$.magnet", magnet, sizeof(magnet)))
        {
            respond_error(connection, 400, "missing-magnet");
            return;
        }

        EngineAddTorrentOptions options;
        ZeroMemory(&options, sizeof(options));
        options.magnet_uri = magnet;
        if(extract_json_string(message->body, "$.name", name, sizeof(name)))
        {
            options.display_name = name;
        }
        options.size_bytes = 0;
        double size_value = 0;
        if(mg_json_get_num(message->body, "$.size", &size_value) && size_value > 0)
        {
            options.size_bytes = static_cast<unsigned long long>(size_value);
        }

        unsigned int torrent_id = 0;
        if(engine_session_add_torrent(server->config.engine, &options, &torrent_id) != 0)
        {
            respond_error(connection, 500, "add-failed");
            return;
        }

        mg_http_reply(connection, 200,
            "Content-Type: application/json\r\nCache-Control: no-cache\r\n",
            "{ \"status\": \"ok\", \"id\": %u }\n", torrent_id);
    }

    static void handle_modify_torrent(struct mg_connection* connection, HttpServer* server, unsigned int torrent_id, const std::string& action)
    {
        if(!server->config.engine)
        {
            respond_error(connection, 503, "engine-unavailable");
            return;
        }

        int result = -1;
        if(action == "pause")
        {
            result = engine_session_pause_torrent(server->config.engine, torrent_id);
        }
        else if(action == "resume")
        {
            result = engine_session_resume_torrent(server->config.engine, torrent_id);
        }
        else
        {
            respond_error(connection, 404, "unknown-action");
            return;
        }

        if(result != 0)
        {
            respond_error(connection, 404, "not-found");
            return;
        }

        respond_ok(connection);
    }

    static void handle_remove_torrent(struct mg_connection* connection, HttpServer* server, unsigned int torrent_id)
    {
        if(!server->config.engine)
        {
            respond_error(connection, 503, "engine-unavailable");
            return;
        }

        if(engine_session_remove_torrent(server->config.engine, torrent_id) != 0)
        {
            respond_error(connection, 404, "not-found");
            return;
        }

        respond_ok(connection);
    }

    static void handle_session_request(struct mg_connection* connection, HttpServer* server)
    {
        EngineSessionSnapshot snapshot;
        reset_snapshot(&snapshot);
        collect_snapshot(server, &snapshot);

        std::string body;
        build_session_payload(server, snapshot, body);
        respond_json(connection, 200, body);
    }

    static void handle_torrents_request(struct mg_connection* connection, HttpServer* server, const struct mg_http_message* message)
    {
        std::string action;
        unsigned int torrent_id = 0;
        const bool has_id = parse_torrent_path(message, &torrent_id, action);

        if(!has_id && http_method_is(message, "GET"))
        {
            EngineSessionSnapshot snapshot;
            reset_snapshot(&snapshot);
            collect_snapshot(server, &snapshot);

            std::string body;
            build_torrents_payload(server, snapshot, body);
            respond_json(connection, 200, body);
            return;
        }

        if(!has_id && http_method_is(message, "POST"))
        {
            handle_add_torrent(connection, server, message);
            return;
        }

        if(has_id && http_method_is(message, "DELETE"))
        {
            handle_remove_torrent(connection, server, torrent_id);
            return;
        }

        if(has_id && http_method_is(message, "POST"))
        {
            handle_modify_torrent(connection, server, torrent_id, action);
            return;
        }

        respond_error(connection, 405, "unsupported-method");
    }

    static bool handle_api_request(struct mg_connection* connection, HttpServer* server, struct mg_http_message* message)
    {
        if(http_uri_matches(message, "/api/session"))
        {
            handle_session_request(connection, server);
            return true;
        }

        if(http_uri_matches(message, "/api/torrents"))
        {
            handle_torrents_request(connection, server, message);
            return true;
        }

        if(http_uri_matches(message, "/api/torrents/*"))
        {
            handle_torrents_request(connection, server, message);
            return true;
        }

        if(http_uri_matches(message, "/ws"))
        {
            mg_ws_upgrade(connection, message, nullptr);
            return true;
        }

        return false;
    }

    static void maybe_broadcast_updates(struct mg_mgr* mgr, HttpServer* server)
    {
        if(!server || !mgr || !server->config.engine)
        {
            return;
        }

        const unsigned int interval = server->config.broadcast_interval_ms ? server->config.broadcast_interval_ms : 1000;
        const ULONGLONG now = GetTickCount64();
        if(server->last_broadcast_tick != 0 && (now - server->last_broadcast_tick) < interval)
        {
            return;
        }
        server->last_broadcast_tick = now;

        EngineSessionSnapshot snapshot;
        reset_snapshot(&snapshot);
        collect_snapshot(server, &snapshot);

        std::string payload;
        build_torrents_payload(server, snapshot, payload);

        for(struct mg_connection* conn = mgr->conns; conn != nullptr; conn = conn->next)
        {
            if(conn->is_websocket)
            {
                mg_ws_send(conn, payload.c_str(), payload.size(), WEBSOCKET_OP_TEXT);
            }
        }
    }

    static void handle_http_event(struct mg_connection* connection, int event, void* event_data)
    {
        if(!connection)
        {
            return;
        }

        HttpServer* server = reinterpret_cast<HttpServer*>(connection->fn_data);
        if(!server)
        {
            return;
        }

        switch(event)
        {
            case MG_EV_HTTP_MSG:
            {
                struct mg_http_message* message = reinterpret_cast<struct mg_http_message*>(event_data);
                if(handle_api_request(connection, server, message))
                {
                    return;
                }

                struct mg_http_serve_opts opts;
                memset(&opts, 0, sizeof(opts));
                opts.root_dir = server->web_root_utf8[0] ? server->web_root_utf8 : ".";
                mg_http_serve_dir(connection, message, &opts);
                break;
            }

            case MG_EV_WS_OPEN:
            {
                DebugOut("http_server: WebSocket client connected.\n");
                EngineSessionSnapshot snapshot;
                reset_snapshot(&snapshot);
                collect_snapshot(server, &snapshot);
                std::string payload;
                build_torrents_payload(server, snapshot, payload);
                mg_ws_send(connection, payload.c_str(), payload.size(), WEBSOCKET_OP_TEXT);
                break;
            }

            case MG_EV_WS_MSG:
            {
                // Ignore incoming messages for now; UI is write-only.
                break;
            }

            default:
                break;
        }
    }

    DWORD WINAPI http_server_thread(LPVOID context)
    {
        HttpServer* server = reinterpret_cast<HttpServer*>(context);
        if(!server)
        {
            return 0;
        }

        struct mg_mgr mgr;
        mg_mgr_init(&mgr);

        char address[64];
        _snprintf_s(address, sizeof(address), _TRUNCATE, "http://127.0.0.1:%hu", server->config.port);

        struct mg_connection* listener = mg_http_listen(
            &mgr, address, handle_http_event, server);
        if(!listener)
        {
            DebugOut("http_server: Failed to listen on %s\n", address);
            mg_mgr_free(&mgr);
            InterlockedExchange(&server->running, 0);
            return 0;
        }

        server->last_broadcast_tick = GetTickCount64();
        DebugOut("http_server: Listening on %s root=%s\n", address, server->web_root_utf8);

        while(WaitForSingleObject(server->stop_event, 0) != WAIT_OBJECT_0)
        {
            mg_mgr_poll(&mgr, server->config.poll_interval_ms);
            maybe_broadcast_updates(&mgr, server);
        }

        mg_mgr_free(&mgr);
        InterlockedExchange(&server->running, 0);
        return 0;
    }
}

void http_server_config_default(HttpServerConfig* config)
{
    if(!config)
    {
        return;
    }

    config->port = 32145;
    config->poll_interval_ms = 250;
    config->broadcast_interval_ms = 750;
    config->web_root[0] = L'\0';
    config->engine = nullptr;
}

int http_server_init(HttpServer* server, const HttpServerConfig* config)
{
    if(!server)
    {
        return -1;
    }

    ZeroMemory(server, sizeof(*server));

    const wchar_t* requested_root = nullptr;
    if(config && config->web_root[0] != L'\0')
    {
        requested_root = config->web_root;
    }

    HttpServerConfig local_config;
    http_server_config_default(&local_config);
    if(config)
    {
        local_config = *config;
    }
    if(local_config.poll_interval_ms < 50)
    {
        local_config.poll_interval_ms = 50;
    }
    if(local_config.broadcast_interval_ms < 200)
    {
        local_config.broadcast_interval_ms = 200;
    }
    if(local_config.port == 0)
    {
        local_config.port = 1;
    }

    determine_web_root(local_config.web_root, MAX_PATH, requested_root);
    server->config = local_config;
    wide_to_utf8(server->config.web_root, server->web_root_utf8, sizeof(server->web_root_utf8));

    server->stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if(!server->stop_event)
    {
        return -2;
    }

    server->running = 1;
    server->thread_handle = CreateThread(nullptr, 0, http_server_thread, server, 0, nullptr);
    if(!server->thread_handle)
    {
        CloseHandle(server->stop_event);
        server->stop_event = nullptr;
        server->running = 0;
        return -3;
    }

    return 0;
}

void http_server_shutdown(HttpServer* server)
{
    if(!server)
    {
        return;
    }

    if(server->stop_event)
    {
        SetEvent(server->stop_event);
    }

    if(server->thread_handle)
    {
        WaitForSingleObject(server->thread_handle, 3000);
        CloseHandle(server->thread_handle);
        server->thread_handle = nullptr;
    }

    if(server->stop_event)
    {
        CloseHandle(server->stop_event);
        server->stop_event = nullptr;
    }

    server->running = 0;
}

unsigned short http_server_port(const HttpServer* server)
{
    if(!server)
    {
        return 0;
    }
    return server->config.port;
}
