#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h> // << ADDED for malloc, free

// --- Project Specific Headers ---
// Make sure these headers exist and declare the functions/defines used below.
#include "win11ui.h"   // For win11ui_* functions
#include "resources.h" // For IDI_ICON_SMALL, IDI_ICON_LARGE
#include "debug.h"     // For InitializeDebugOutput, DebugOut, CleanupDebugOutput
#include "window.h"    // Should declare: window_proc, tray_icon_add (or separate tray header)
#include "config.h"    // Should define: APP_TITLE_W (or define it below)
#include "torrent.h"   // << ADDED for Torrent type, torrent_load, torrent_free

// --- Placeholder - Replace if APP_TITLE_W is defined in config.h ---
#ifndef APP_TITLE_W
#define APP_TITLE_W L"RawBit Torrent" // Example title
#warning "APP_TITLE_W not found in included headers, using placeholder."
#endif
// --- Make sure window_proc is declared, likely in window.h ---
// LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    InitializeDebugOutput(); // Initialize debug locking

    // Ensure resource IDs from resources.h are correct
    HICON icon_small = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON_SMALL));
    HICON icon_large = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON_LARGE));
    if(!icon_small || !icon_large)
    {
        MessageBoxW(NULL, L"Failed to load icons", L"Error", MB_OK | MB_ICONERROR);
        CleanupDebugOutput();
        return 1;
    }

    // --- Window Class ---
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = window_proc; // 'window_proc' must be defined somewhere and linked.
    wc.hInstance = hInstance;
    wc.lpszClassName = L"rawbit_window_class";
    wc.hbrBackground = NULL; // Using custom drawing or transparent background
    wc.hIconSm = icon_small;
    wc.hIcon = icon_large;
    // wc.hCursor = LoadCursor(NULL, IDC_ARROW); // May want a cursor

    if(!RegisterClassExW(&wc))
    {
        MessageBoxW(NULL, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        CleanupDebugOutput();
        return 1;
    }

    // --- Create Window ---
    // Using the original window styles you provided
    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,              // Ensures window appears on taskbar
        L"rawbit_window_class",       // Registered class name
        APP_TITLE_W,                  // Text for window title (defined above or in config.h)
        WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX, // Original style flags
        CW_USEDEFAULT, CW_USEDEFAULT, // Default position
        800, 300,                     // Original dimensions
        NULL,                         // No parent window
        NULL,                         // No menu
        hInstance,                    // Application instance handle
        NULL                          // No additional creation data
    );

    if(!hwnd)
    {
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        // Consider UnregisterClassW here if appropriate
        CleanupDebugOutput();
        return 1;
    }

    // --- Apply Custom UI (Assuming these functions exist and are linked) ---
    win11ui_enable_dark_mode();
    win11ui_apply_rounded_corners(hwnd);
    win11ui_apply_blur(hwnd);

    // --- Final Window Setup ---
    // SetWindowTextW(hwnd, APP_TITLE_W); // Title already set in CreateWindowExW
    ShowWindow(hwnd, nCmdShow); // Ensure window is shown correctly per OS request
    // UpdateWindow(hwnd); // Usually called to force initial paint

    // Assuming tray_icon_add is defined and linked (perhaps in window.c?)
    tray_icon_add(hwnd, hInstance);

    // --- Allocate Torrent struct on the HEAP ---
    Torrent* t_ptr = (Torrent*)malloc(sizeof(Torrent));
    if(!t_ptr)
    {
        MessageBoxW(NULL, L"Failed to allocate memory for Torrent struct", L"Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
        // *** NOTE: 'CleanupDebugOutput' MUST be defined in debug.c and linked ***
        CleanupDebugOutput();
        return 1;
    }

    // --- Load the torrent data ---
    // *** NOTE: 'torrent_load' function MUST be defined in torrent.c and linked ***
    // *** NOTE: 'torrent_load' CALLS functions that MUST be defined in torrent_parse.c ***
    int load_result = torrent_load("example.torrent", t_ptr);

    if(load_result == 0)
    {
        // *** NOTE: 'DebugOut' MUST be defined in debug.c and linked ***
        DebugOut("Torrent loaded successfully in WinMain:\n");
        DebugOut("  Name: %s\n", t_ptr->name);
        // --- Print Info Hash ---
        char info_hash_hex[41];
        for(int i = 0; i < 20; ++i) sprintf_s(info_hash_hex + i * 2, 3, "%02x", t_ptr->info_hash[i]);
        DebugOut("  Info Hash: %s\n", info_hash_hex);
        // --- Print Trackers ---
        DebugOut("  Announce: %s\n", t_ptr->announce[0] ? t_ptr->announce : "(not set)");
        DebugOut("  Announce List Tiers: %d\n", t_ptr->announce_tier_count);
        for(int i = 0; i < t_ptr->announce_tier_count; ++i)
        {
            DebugOut("    Tier %d:\n", i + 1);
            for(int j = 0; j < t_ptr->announce_list[i].url_count; ++j)
            {
                DebugOut("      - %s\n", t_ptr->announce_list[i].urls[j]);
            }
        }
        // --- Print Piece Info ---
        DebugOut("  Piece Length: %d\n", t_ptr->piece_length);
        // >>> THE FIX IS HERE <<<
        DebugOut("  Piece Count: %d\n", t_ptr->piece_count); // Was 't->', now 't_ptr->'
        // >>>--------------- <<<
        DebugOut("  Total Size: %lld bytes\n", t_ptr->total_size);

        // --- Print ALL Files ---
        DebugOut("  File Count: %d\n", t_ptr->file_count);
        DebugOut("  Files:\n");
        for(int i = 0; i < t_ptr->file_count; ++i)
        {
            TorrentFile* f = &t_ptr->files[i];
            DebugOut("    [%d] Length: %lld bytes\n", i, f->length);
            char full_path_buffer[1024] = { 0 };
            for(int j = 0; j < f->path_depth; ++j)
            {
                // strcat_s is safer than strcat
                strcat_s(full_path_buffer, sizeof(full_path_buffer), f->path[j]);
                if(j < f->path_depth - 1)
                {
                    strcat_s(full_path_buffer, sizeof(full_path_buffer), "\\");
                }
            }
            DebugOut("        Path: %s\n", full_path_buffer);
        }
    }
    else // load_result != 0
    {
        MessageBoxW(NULL, L"Failed to load or parse torrent file. Check Debug Output.", L"Error", MB_OK | MB_ICONERROR);
    }

    // --- Main Message Loop ---
    MSG msg = { 0 }; // Good practice to initialize msg
    while(GetMessageW(&msg, NULL, 0, 0) > 0) // Use > 0 check
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    int loop_result = (int)msg.wParam; // Get exit code

    // --- Cleanup ---
    if(t_ptr)
    {
        if(load_result == 0)
        {
            // *** NOTE: 'torrent_free' function MUST be defined in torrent.c and linked ***
            torrent_free(t_ptr); // Frees t_ptr->pieces
        }
        free(t_ptr); // Free the Torrent struct itself
        t_ptr = NULL;
    }

    // *** NOTE: 'CleanupDebugOutput' MUST be defined in debug.c and linked ***
    CleanupDebugOutput();

    return loop_result;
}
