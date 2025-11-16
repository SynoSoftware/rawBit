#pragma once

#include <windows.h>
#include <stdint.h>

struct HttpServerConfig
{
    unsigned short port;
    unsigned int poll_interval_ms;
    wchar_t web_root[MAX_PATH];
};

struct HttpServer
{
    HttpServerConfig config;
    HANDLE thread_handle;
    HANDLE stop_event;
    volatile LONG running;
    char web_root_utf8[MAX_PATH];
};

void http_server_config_default(HttpServerConfig* config);
int http_server_init(HttpServer* server, const HttpServerConfig* config);
void http_server_shutdown(HttpServer* server);
unsigned short http_server_port(const HttpServer* server);
