#include "net/http_server.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

extern "C" {
#include "mongoose.h"
}

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

    static void handle_http_event(struct mg_connection* connection, int event, void* event_data, void* user_data)
    {
        HttpServer* server = reinterpret_cast<HttpServer*>(user_data);
        if(!server)
        {
            return;
        }

        if(event == MG_EV_HTTP_MSG)
        {
            struct mg_http_message* message = reinterpret_cast<struct mg_http_message*>(event_data);
            if(mg_http_match_uri(message, "/api/session"))
            {
                mg_http_reply(connection, 200,
                    "Content-Type: application/json\r\nCache-Control: no-cache\r\n",
                    "{ \"port\": %hu }\n", server->config.port);
                return;
            }

            struct mg_http_serve_opts opts;
            memset(&opts, 0, sizeof(opts));
            opts.root_dir = server->web_root_utf8[0] ? server->web_root_utf8 : ".";
            mg_http_serve_dir(connection, message, &opts);
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

        DebugOut("http_server: Listening on %s root=%s\n", address, server->web_root_utf8);

        while(WaitForSingleObject(server->stop_event, 0) != WAIT_OBJECT_0)
        {
            mg_mgr_poll(&mgr, server->config.poll_interval_ms);
        }

        mg_mgr_free(&mgr);
        InterlockedExchange(&server->running, 0);
        return 0;
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
}

void http_server_config_default(HttpServerConfig* config)
{
    if(!config)
    {
        return;
    }

    config->port = 32145;
    config->poll_interval_ms = 250;
    config->web_root[0] = L'\0';
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
