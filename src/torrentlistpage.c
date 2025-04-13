#include "torrentlistpage.h"
#include "debug.h"      // Include for DebugOut
#include <CommCtrl.h>    // Required for ListView APIs (WC_LISTVIEWW, ListView_*, LV*)

// Link Common Controls Library (alternative to #pragma comment) - Ensure it's also set in project linker settings
// Or keep using the pragma comment in main.c
// #pragma comment(lib, "Comctl32.lib") // Okay to have multiple times, but one in main.c is sufficient.

// Define a unique ID for the ListView control within this page
#define IDC_TORRENT_LISTVIEW 2001

// Create the Torrent List Page controls
HWND TorrentListPage_Create(HWND hParent, HINSTANCE hInstance)
{
    DebugOut("TorrentListPage: Creating...\n");

    // --- InitCommonControlsEx removed ---
    // Ensure it is called successfully *once* in WinMain before this.

    // --- Create the ListView control ---
    HWND hListView = CreateWindowExW(
        WS_EX_CLIENTEDGE,                // Optional: Add a sunken border look
        WC_LISTVIEWW,                    // Use the standard LISTVIEW class name (registered by InitCommonControlsEx)
        L"",                             // No title text
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, // Styles
        0, 0, 100, 100,                  // Initial Pos/Size (will be set by Resize)
        hParent,                         // Parent window is the container passed by Page Manager
        (HMENU)IDC_TORRENT_LISTVIEW,     // Control ID
        hInstance,                       // App instance
        NULL);                           // No extra data

    if(!hListView)
    {
        // If this fails NOW, it means InitCommonControlsEx likely failed in WinMain,
        // or there's another issue (memory, invalid parent?).
        DebugOut("TorrentListPage: CRITICAL - Failed to create ListView control. Error: %lu\n", GetLastError());
        return NULL; // Critical failure for this page
    }

    // --- Set Colors and Extended Styles ---
    ListView_SetBkColor(hListView, RGB(20, 20, 20));       // Dark background
    ListView_SetTextBkColor(hListView, RGB(20, 20, 20));   // Match text background
    ListView_SetTextColor(hListView, RGB(220, 220, 220)); // Light text
    ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    DebugOut("TorrentListPage: Set colors and extended styles for ListView (HWND %p).\n", hListView);

    // --- Setup ListView Columns (Example) ---
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    lvc.fmt = LVCFMT_LEFT; lvc.cx = 250; lvc.pszText = L"Name"; lvc.iSubItem = 0; ListView_InsertColumn(hListView, 0, &lvc);
    lvc.fmt = LVCFMT_RIGHT; lvc.cx = 100; lvc.pszText = L"Size"; lvc.iSubItem = 1; ListView_InsertColumn(hListView, 1, &lvc);
    lvc.fmt = LVCFMT_LEFT; lvc.cx = 100; lvc.pszText = L"Status"; lvc.iSubItem = 2; ListView_InsertColumn(hListView, 2, &lvc);
    // Add more columns as needed...

    DebugOut("TorrentListPage: ListView columns added (HWND %p).\n", hListView);

    return hListView; // Return handle to the ListView
}

// Destroy page resources
void TorrentListPage_Destroy(HWND hPageContainer)
{
    DebugOut("TorrentListPage: Destroying ListView (HWND %p)\n", hPageContainer);
    if(hPageContainer && IsWindow(hPageContainer))
    { // Check if handle is still a valid window
        DestroyWindow(hPageContainer);
    }
}

// Handle forwarded messages
BOOL TorrentListPage_HandleMessage(HWND hPageContainer, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_NOTIFY:
        {
            LPNMHDR lpnmhdr = (LPNMHDR)lParam;
            if(lpnmhdr && lpnmhdr->hwndFrom == hPageContainer && lpnmhdr->idFrom == IDC_TORRENT_LISTVIEW)
            {
                switch(lpnmhdr->code)
                {
                    case NM_DBLCLK: DebugOut("TorrentListPage: ListView Double Click! Item: %d\n", ((LPNMITEMACTIVATE)lParam)->iItem); return TRUE;
                    case NM_RCLICK: DebugOut("TorrentListPage: ListView Right Click! Item: %d\n", ((LPNMITEMACTIVATE)lParam)->iItem); /* TODO: Show context menu */ return TRUE;
                    case LVN_KEYDOWN: if(((LPNMLVKEYDOWN)lParam)->wVKey == VK_DELETE) { DebugOut(">>> Delete Key on ListView <<<\n"); /* TODO: Delete logic */ return TRUE; } break;
                        // Add LVN_COLUMNCLICK for sorting, LVN_ITEMCHANGED for selection, etc.
                }
            }
            break;
        }
    }
    return FALSE; // Message not handled
}

// Show the page
void TorrentListPage_Activate(HWND hPageContainer)
{
    // DebugOut("TorrentListPage: Activating... (HWND %p)\n", hPageContainer);
    if(hPageContainer) ShowWindow(hPageContainer, SW_SHOWNA); // Show without changing activation
}

// Hide the page
void TorrentListPage_Deactivate(HWND hPageContainer)
{
    // DebugOut("TorrentListPage: Deactivating... (HWND %p)\n", hPageContainer);
    if(hPageContainer) ShowWindow(hPageContainer, SW_HIDE);
}

// Resize the page (the ListView itself)
void TorrentListPage_Resize(HWND hPageContainer, int width, int height)
{
    if(hPageContainer) MoveWindow(hPageContainer, 0, 0, width, height, TRUE); // Position at 0,0 of parent area
}