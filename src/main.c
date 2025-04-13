#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h> // For malloc, free
#include <string.h> // Needed by torrent.c's string functions if called indirectly
#include <CommCtrl.h> // <<<<< ADDED: Need for InitCommonControlsEx

// --- Project Specific Headers ---
// Ensure these paths are correct relative to main.c or are in include paths
#include "win11ui.h"
#include "resources.h"
#include "debug.h"
#include "window.h"      // Declares window_proc
#include "config.h"      // Declares APP_TITLE_W (or use placeholder)
#include "torrent.h"     // Declares Torrent, torrent_load, torrent_free
#include "tray.h"        // Declares tray_icon_add
#include "pagemanager.h" // Declares Page Manager API
#include "page.h"        // Declares Page IDs
// --- Include headers for Page Implementations needed for Registration ---
#include "torrentlistpage.h"
#include "settingspage.h"
#include "aboutpage.h"


// --- Enable Visual Styles / Common Controls v6 ---
// This pragma MUST be present, usually in the file containing WinMain or a common header.
// It directs the linker to include the manifest required for Common Controls v6.
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


// --- Placeholder for APP_TITLE_W (if not defined in config.h) ---
#ifndef APP_TITLE_W
#define APP_TITLE_W L"RawBit Torrent" // Example title
#warning "APP_TITLE_W not found in included headers, using placeholder."
#endif


int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    InitializeDebugOutput();

    // --- Initialize Common Controls v6 (MUST be done ONCE at startup) ---
    // This registers the necessary window classes (like WC_LISTVIEWW).
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES; // Request ListView and standard controls
    if(!InitCommonControlsEx(&icex))
    {
        MessageBoxW(NULL, L"Failed to initialize Windows Common Controls! Cannot start.", L"Fatal Error", MB_OK | MB_ICONERROR);
        CleanupDebugOutput();
        return 1; // Cannot proceed
    }
    DebugOut("WinMain: Common Controls Initialized.\n");


    // --- Load Resources ---
    HICON icon_small = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON_SMALL));
    HICON icon_large = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON_LARGE));
    if(!icon_small || !icon_large)
    {
        MessageBoxW(NULL, L"Failed to load application icons.", L"Error", MB_OK | MB_ICONERROR);
        CleanupDebugOutput(); return 1;
    }

    // --- Register Window Class ---
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = window_proc; // Implemented in window.c
    wc.hInstance = hInstance;
    wc.lpszClassName = L"rawbit_window_class";
    wc.hbrBackground = NULL; // Using custom background drawing
    wc.hIconSm = icon_small;
    wc.hIcon = icon_large;
    // wc.hCursor = LoadCursor(NULL, IDC_ARROW); // Can add default cursor if needed
    if(!RegisterClassExW(&wc))
    {
        MessageBoxW(NULL, L"Failed to register window class.", L"Error", MB_OK | MB_ICONERROR);
        CleanupDebugOutput(); return 1;
    }
    DebugOut("WinMain: Window Class Registered.\n");

    // --- Create Main Application Window ---
    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,           // Ex style for taskbar presence
        L"rawbit_window_class",    // Registered class name
        APP_TITLE_W,               // Window title
        WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME, // Changed to allow system minimize/maximize and sizing
        // WS_OVERLAPPEDWINDOW could also be used for standard frame, but requires adjusting hit-testing etc.
        // Original: WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX
        CW_USEDEFAULT, CW_USEDEFAULT, // Default position
        800, 600,                  // Initial size (height increased)
        NULL, NULL, hInstance, NULL); // No parent, no menu, instance, no extra data
    if(!hwnd)
    {
        MessageBoxW(NULL, L"Failed to create main window.", L"Error", MB_OK | MB_ICONERROR);
        UnregisterClassW(L"rawbit_window_class", hInstance); // Clean up class
        CleanupDebugOutput(); return 1;
    }
    DebugOut("WinMain: Main Window Created (HWND %p).\n", hwnd);
    // Note: WM_CREATE in window_proc is called implicitly during CreateWindowExW


    // --- Initialize Page Manager and Register Pages ---
    // PageManager_Initialize called from window.c: WM_CREATE now handled after createWindow returns in WinMain instead
    // Because WM_CREATE might need instance from main
    PageManager_Initialize(hwnd, hInstance); // Initialize here is correct
    DebugOut("WinMain: Page Manager Initialized.\n");

    if(!PageManager_RegisterPage(PAGE_ID_TORRENTLIST,
                                 TorrentListPage_Create, TorrentListPage_Destroy, TorrentListPage_HandleMessage,
                                 TorrentListPage_Activate, TorrentListPage_Deactivate, TorrentListPage_Resize) ||
       !PageManager_RegisterPage(PAGE_ID_SETTINGS,
                                 SettingsPage_Create, SettingsPage_Destroy, SettingsPage_HandleMessage,
                                 SettingsPage_Activate, SettingsPage_Deactivate, SettingsPage_Resize) ||
       !PageManager_RegisterPage(PAGE_ID_ABOUT,
                                 AboutPage_Create, AboutPage_Destroy, AboutPage_HandleMessage,
                                 AboutPage_Activate, AboutPage_Deactivate, AboutPage_Resize))
    {
        MessageBoxW(hwnd, L"Failed to register UI pages.", L"Initialization Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd); CleanupDebugOutput(); return 1;
    }
    DebugOut("WinMain: UI Pages Registered.\n");


    // --- Switch to Default Page ---
    if(!PageManager_SwitchToPage(PAGE_ID_TORRENTLIST))
    {
        MessageBoxW(hwnd, L"Failed to switch to default UI page.", L"Initialization Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd); CleanupDebugOutput(); return 1;
    }
    DebugOut("WinMain: Switched to Default Page (%d).\n", PAGE_ID_TORRENTLIST);


    // --- Apply Win11 UI Styles ---
    win11ui_enable_dark_mode();         // Ensure uses modern theme APIs if needed
    win11ui_apply_rounded_corners(hwnd);
    win11ui_apply_blur(hwnd);
    DebugOut("WinMain: Win11 UI Styles Applied.\n");


    // --- Show Window & Tray Icon ---
    ShowWindow(hwnd, nCmdShow);        // Make window visible based on parameter
    // UpdateWindow(hwnd);               // Force immediate paint if needed (usually not necessary)
    tray_icon_add(hwnd, hInstance);   // Add icon to system tray


    // --- Load Initial Torrent (Example) ---
    Torrent* t_ptr = (Torrent*)malloc(sizeof(Torrent));
    int load_result = -1;
    if(!t_ptr)
    {
        DebugOut("WinMain: Failed to allocate memory for Torrent struct\n");
        MessageBoxW(hwnd, L"Failed to allocate memory for Torrent struct", L"Memory Error", MB_OK | MB_ICONERROR);
        // Continue running the UI, but loading failed
    }
    else
    {
        load_result = torrent_load("example.torrent", t_ptr);
        if(load_result == 0)
        {
            // Successfully loaded, print details (Full printing code as provided previously)
            DebugOut("Torrent loaded successfully in WinMain:\n");
            DebugOut("  Name: %s\n", t_ptr->name);
            char info_hash_hex[41]; for(int i = 0; i < 20; ++i) sprintf_s(info_hash_hex + i * 2, 3, "%02x", t_ptr->info_hash[i]); DebugOut("  Info Hash: %s\n", info_hash_hex);
            DebugOut("  Announce: %s\n", t_ptr->announce[0] ? t_ptr->announce : "(not set)");
            DebugOut("  Announce List Tiers: %d\n", t_ptr->announce_tier_count); for(int i = 0; i < t_ptr->announce_tier_count; ++i) { DebugOut("    Tier %d:", i + 1); for(int j = 0; j < t_ptr->announce_list[i].url_count; ++j) { DebugOut(" [%s]", t_ptr->announce_list[i].urls[j]); } DebugOut("\n"); }
            DebugOut("  Piece Length: %d\n", t_ptr->piece_length); DebugOut("  Piece Count: %d\n", t_ptr->piece_count); DebugOut("  Total Size: %lld bytes\n", t_ptr->total_size); DebugOut("  File Count: %d\n", t_ptr->file_count); DebugOut("  Files:\n");
            for(int i = 0; i < t_ptr->file_count; ++i) { TorrentFile* f = &t_ptr->files[i]; DebugOut("    [%d] Length: %lld bytes", i, f->length); char fp_buf[1024] = { 0 }; for(int j = 0; j < f->path_depth; ++j) { strcat_s(fp_buf, sizeof(fp_buf), f->path[j]); if(j < f->path_depth - 1) strcat_s(fp_buf, sizeof(fp_buf), "\\"); } DebugOut(" | Path: %s\n", fp_buf); }
            // --- TODO --- Pass t_ptr data to the torrent list page for display
            // e.g., SendMessage(PageManager_GetActivePageHWND(), USER_MSG_ADD_TORRENT, 0, (LPARAM)t_ptr);
            // But be careful about ownership! Copying needed data is safer.

        }
        else
        {
            DebugOut("WinMain: Failed to load/parse 'example.torrent'. Check path/format/debug logs.\n");
            MessageBoxW(hwnd, L"Failed to load initial torrent 'example.torrent'. Check logs.", L"Warning", MB_OK | MB_ICONWARNING);
            // No data loaded, t_ptr will still be freed below.
        }
    }


    // --- Main Message Loop ---
    DebugOut("WinMain: Entering message loop...\n");
    MSG msg = { 0 };
    while(GetMessageW(&msg, NULL, 0, 0) > 0) // Standard loop condition
    {
        // Optional: Add IsDialogMessage check here later if using modeless dialogs/tab key navigation
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DebugOut("WinMain: Exited message loop (wParam=%d).\n", (int)msg.wParam);
    int loop_result = (int)msg.wParam;


    // --- Application Cleanup ---
    // WM_DESTROY in window_proc cleans up pages via PageManager_Cleanup()
    // WM_DESTROY cleans up tray icon
    if(t_ptr)
    { // Free torrent data structure if allocated
        if(load_result == 0)
        { // Only free internal 'pieces' if loading was successful
            torrent_free(t_ptr);
        }
        free(t_ptr); // Free the Torrent struct itself
        t_ptr = NULL;
    }

    // Unregister window class
    UnregisterClassW(L"rawbit_window_class", hInstance);

    CleanupDebugOutput(); // Clean up debugging resources
    return loop_result; // Return exit code from message loop (usually 0)
}