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
        DestroyWindow(hwnd); // Clean up window before exiting
        CleanupDebugOutput();
        return 1; // Exit if allocation fails
    }
    // It's good practice to initialize memory after malloc, although torrent_load does memset
    // memset(t_ptr, 0, sizeof(Torrent));

    // --- Load the torrent data ---
    // Use a real path or process lpCmdLine if you intend to load via command line
    int load_result = torrent_load("example.torrent", t_ptr);

    if(load_result == 0)
    {
        // Successfully loaded, print debug info
        DebugOut("Torrent loaded successfully in WinMain:\n");
        DebugOut("  Name: %s\n", t_ptr->name);
        DebugOut("  Announce: %s\n", t_ptr->announce);
        DebugOut("  Piece Length: %d\n", t_ptr->piece_length);
        DebugOut("  Piece Count: %d\n", t_ptr->piece_count);
        DebugOut("  Total Size: %lld\n", t_ptr->total_size);
        DebugOut("  File Count: %d\n", t_ptr->file_count);
        if(t_ptr->file_count > 0 && t_ptr->files[0].path_depth > 0) // Check path_depth too
        {
            DebugOut("  First File Path[0]: %s\n", t_ptr->files[0].path[0]);
            DebugOut("  First File Length: %lld\n", t_ptr->files[0].length);
        }
        // Application logic would use t_ptr here
    }
    else
    {
        // Failed to load, show error message
        MessageBoxW(NULL, L"Failed to load or parse torrent file. Check Debug Output.", L"Error", MB_OK | MB_ICONERROR);
        // Application logic might decide to continue running the UI or exit here
    }

    // --- Main Message Loop ---
    MSG msg;
    // Standard message loop: GetMessage returns > 0 for messages, 0 for WM_QUIT, -1 for error
    while(GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    // When loop terminates, msg.wParam usually contains the exit code from PostQuitMessage
    int loop_result = (int)msg.wParam;


    // --- Cleanup ---
    // Free torrent data (if allocated and loaded successfully)
    if(t_ptr)
    {
        if(load_result == 0) // Only free internal data if loading was successful
        {
            torrent_free(t_ptr); // Frees t_ptr->pieces
        }
        // Always free the Torrent struct itself if allocation succeeded
        free(t_ptr);
        t_ptr = NULL;
    }

    // Other cleanup (e.g., remove tray icon?)
    // tray_icon_remove(hwnd, ...); // If you have such a function

    // DestroyWindow(hwnd); // Usually implicitly destroyed when closed by user leading to WM_DESTROY -> PostQuitMessage
    // UnregisterClassW(L"rawbit_window_class", hInstance); // Optional: unregister class before exit

    CleanupDebugOutput(); // Cleanup debug locking

    return loop_result; // Return the exit code
}