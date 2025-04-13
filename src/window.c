// --- Set minimum Windows target version BEFORE including windows.h ---
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

// --- Standard & Windows Headers ---
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>    // Message crackers (GET_X_LPARAM, etc.)
#include <CommCtrl.h>    // Common Controls APIs
#include <WinUser.h>     // User interface functions

// --- Project Headers ---
#include "window.h"        // Declares window_proc
#include "debug.h"         // Debug logging functions
#include "win11ui.h"       // Optional Win11 helpers
#include "tray.h"          // Tray icon functions
#include "resources.h"     // Resource IDs
#include "pagemanager.h"   // Page Manager API
#include "page.h"          // Page IDs

// --- Link Common Controls Library ---
#pragma comment(lib, "Comctl32.lib")

// --- Control IDs ---
#define IDC_HAMBURGER_BUTTON 1000
#define IDC_SEARCH_EDIT      1001

// --- Static HWNDs for main frame controls ---
static HWND hHamburgerButton = NULL;
static HWND hSearchEdit = NULL;

// Shared GDI Resources
static HBRUSH hDarkBackgroundBrush = NULL;

// --- Constants ---
#define BORDER_WIDTH 8       // Pixel thickness of the resize border
#define TITLE_BAR_HEIGHT 30  // Height of the draggable area + controls

// Helper Function (optional for debugging)
const char* HitTestToString(LRESULT ht)
{
    switch(ht)
    {
        case HTNOWHERE: return "HTNOWHERE"; case HTCLIENT: return "HTCLIENT"; case HTCAPTION: return "HTCAPTION"; case HTSYSMENU: return "HTSYSMENU"; case HTGROWBOX: return "HTGROWBOX"; case HTMENU: return "HTMENU";
        case HTHSCROLL: return "HTHSCROLL"; case HTVSCROLL: return "HTVSCROLL"; case HTMINBUTTON: return "HTMINBUTTON"; case HTMAXBUTTON: return "HTMAXBUTTON"; case HTLEFT: return "HTLEFT"; case HTRIGHT: return "HTRIGHT";
        case HTTOP: return "HTTOP"; case HTTOPLEFT: return "HTTOPLEFT"; case HTTOPRIGHT: return "HTTOPRIGHT"; case HTBOTTOM: return "HTBOTTOM"; case HTBOTTOMLEFT: return "HTBOTTOMLEFT"; case HTBOTTOMRIGHT: return "HTBOTTOMRIGHT";
        case HTBORDER: return "HTBORDER"; case HTCLOSE: return "HTCLOSE"; case HTHELP: return "HTHELP"; default: return "Unknown";
    }
}


// --- Main Window Procedure ---
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch(msg)
    {
        // --- Standard Messages (Keep Latest Corrected Versions) ---
        case WM_CREATE:
        {
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE); if(!hInst)return -1;
            hDarkBackgroundBrush = CreateSolidBrush(RGB(20, 20, 20)); if(!hDarkBackgroundBrush)return -1;
            hHamburgerButton = CreateWindowExW(0, L"BUTTON", L"\x2630", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)IDC_HAMBURGER_BUTTON, hInst, NULL);
            hSearchEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, (HMENU)IDC_SEARCH_EDIT, hInst, NULL);
            if(!hHamburgerButton || !hSearchEdit) { if(hHamburgerButton)DestroyWindow(hHamburgerButton); if(hSearchEdit)DestroyWindow(hSearchEdit); DeleteObject(hDarkBackgroundBrush); return -1; }
            SendMessage(hHamburgerButton, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            return 0;
        }
        case WM_DESTROY: { PageManager_Cleanup(); tray_icon_remove(hwnd); PostQuitMessage(0); if(hDarkBackgroundBrush) DeleteObject(hDarkBackgroundBrush); hDarkBackgroundBrush = NULL; hHamburgerButton = NULL; hSearchEdit = NULL; return 0; }
        case WM_SIZE:
        {
            if(wparam == SIZE_MINIMIZED) return 0; int w = LOWORD(lparam), h = HIWORD(lparam); int btnW = 30, pad = 5, topEffH = TITLE_BAR_HEIGHT - (2 * pad), currY = pad;
            if(hHamburgerButton) MoveWindow(hHamburgerButton, pad, currY, btnW, topEffH, TRUE); // Use TRUE
            if(hSearchEdit) { int sL = pad + btnW + pad, sW = w - sL - pad; if(sW < 50)sW = 50; MoveWindow(hSearchEdit, sL, currY, sW, topEffH, TRUE); } // Use TRUE
            int pageTop = TITLE_BAR_HEIGHT, pageW = w, pageH = h - pageTop; if(pageH < 0)pageH = 0;
            PageManager_ResizeActivePageArea(pageW, pageH); // Page func should use TRUE
            // InvalidateRect(hwnd, NULL, TRUE); // Possibly removed again if children repaint OK now
            return 0;
        }
        case WM_PAINT: { PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps); RECT rcCli; GetClientRect(hwnd, &rcCli); RECT rcTop = { rcCli.left, rcCli.top, rcCli.right, rcCli.top + TITLE_BAR_HEIGHT }; if(hDarkBackgroundBrush) FillRect(hdc, &rcTop, hDarkBackgroundBrush); EndPaint(hwnd, &ps); return 0; }
        case WM_CTLCOLORSTATIC: { HDC hdcS = (HDC)wparam; SetTextColor(hdcS, RGB(220, 220, 220)); SetBkColor(hdcS, RGB(20, 20, 20)); return (LRESULT)(hDarkBackgroundBrush ? hDarkBackgroundBrush : GetStockObject(BLACK_BRUSH)); }
        case WM_COMMAND: { WORD id = LOWORD(wparam), nCode = HIWORD(wparam); if(id == IDC_HAMBURGER_BUTTON && nCode == BN_CLICKED) { /* Flyout... */ return 0; } if(PageManager_ForwardMessageToActivePage(msg, wparam, lparam))return 0; break; }
        case WM_NOTIFY: { if(PageManager_ForwardMessageToActivePage(msg, wparam, lparam))return 0; break; }
        case WM_CLOSE: { DestroyWindow(hwnd); return 0; }
        case WM_KEYDOWN: { if(wparam == VK_ESCAPE) PostMessage(hwnd, WM_CLOSE, 0, 0); return 0; }
        case WM_ACTIVATE: { return 0; }
        case WM_NCCALCSIZE: { if(wparam == TRUE) return 0; break; }


                          // --- RESTORED/ADAPTED FRAME LOGIC (Based on your working version) ---

        case WM_SETCURSOR: // <<<< USING YOUR ORIGINAL WORKING LOGIC >>>
        {
            // This uses the hit-test result embedded in LOWORD(lparam) by Windows BEFORE calling WM_SETCURSOR.
            // It DOES NOT call SendMessage(WM_NCHITTEST) again here.
            LRESULT hitTestResult = LOWORD(lparam);
            // DebugOut("WM_SETCURSOR: Received Hit Code = %s\n", HitTestToString(hitTestResult));

            if(hitTestResult == HTLEFT || hitTestResult == HTRIGHT)
            {
                SetCursor(LoadCursorW(NULL, IDC_SIZEWE)); return TRUE;
            }
            if(hitTestResult == HTTOP || hitTestResult == HTBOTTOM)
            {
                SetCursor(LoadCursorW(NULL, IDC_SIZENS)); return TRUE;
            }
            if(hitTestResult == HTTOPLEFT || hitTestResult == HTBOTTOMRIGHT)
            {
                SetCursor(LoadCursorW(NULL, IDC_SIZENWSE)); return TRUE;
            }
            if(hitTestResult == HTTOPRIGHT || hitTestResult == HTBOTTOMLEFT)
            {
                SetCursor(LoadCursorW(NULL, IDC_SIZENESW)); return TRUE;
            }
            // Important: Only set default cursor explicitly if you NEED to override child controls
            // SetCursor(LoadCursorW(NULL, IDC_ARROW)); // Usually REMOVE this
            return FALSE; // Let DefWindowProc handle other cursors (like HTCLIENT giving arrow/ibeam)
        }


        case WM_NCHITTEST: // <<<< USING YOUR ORIGINAL WORKING LOGIC (CLIENT COORDS) + TITLE BAR ADAPTATION >>>>
        {
            POINT pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }; // Screen coordinates
            ScreenToClient(hwnd, &pt); // Convert pt to client coordinates

            RECT rcClient; GetClientRect(hwnd, &rcClient);
            int border = BORDER_WIDTH;

            // 1. Check borders (using client coordinates now)
            // These conditions assume rcClient.left and rcClient.top are typically 0.
            if(pt.y < rcClient.top + border)
            { // Top border zone
                if(pt.x < rcClient.left + border) return HTTOPLEFT;
                if(pt.x > rcClient.right - border) return HTTOPRIGHT;
                return HTTOP;
            }
            if(pt.y >= rcClient.bottom - border)
            { // Bottom border zone (use >= for bottom/right edges)
                if(pt.x < rcClient.left + border) return HTBOTTOMLEFT;
                if(pt.x >= rcClient.right - border) return HTBOTTOMRIGHT; // use >=
                return HTBOTTOM;
            }
            if(pt.x < rcClient.left + border) return HTLEFT; // Left border zone (excl corners)
            if(pt.x >= rcClient.right - border) return HTRIGHT;// Right border zone (excl corners, use >=)


            // 2. If NOT a border, check if in the Title Bar Area (for dragging)
            if(pt.y < TITLE_BAR_HEIGHT)
            {
                // We don't check *explicitly* for child controls here in this simpler version.
                // If DefWindowProc receives the subsequent WM_LBUTTONDOWN message
                // when the hit test is HTCAPTION, but the actual click hits a child,
                // Windows *should* route the click correctly to the child.
                return HTCAPTION;
            }

            // 3. Otherwise, it's the main client area (pages etc.)
            return HTCLIENT;
        }

        case WM_SIZING: // <<<< USING YOUR ORIGINAL WORKING VERSION + TRUE return + Invalidate >>>>
        {
            RECT* sizingRect = (RECT*)lparam;
            int minWidth = 400; int minHeight = 200;
            if(sizingRect->right - sizingRect->left < minWidth) { /* Adjust */ if(wparam == WMSZ_LEFT || wparam == WMSZ_TOPLEFT || wparam == WMSZ_BOTTOMLEFT) sizingRect->left = sizingRect->right - minWidth; else sizingRect->right = sizingRect->left + minWidth; }
            if(sizingRect->bottom - sizingRect->top < minHeight) { /* Adjust */ if(wparam == WMSZ_TOP || wparam == WMSZ_TOPLEFT || wparam == WMSZ_TOPRIGHT) sizingRect->top = sizingRect->bottom - minHeight; else sizingRect->bottom = sizingRect->top + minHeight; }
            // Debugging redraw during resize might be needed again
            InvalidateRect(hwnd, NULL, FALSE); // Try FALSE first to avoid erasing, might reduce flicker
            return TRUE; // Return TRUE when handling WM_SIZING
        }


        case WM_LBUTTONDOWN: // <<<< REVERTING CLOSE TO ORIGINAL, but using HTCAPTION CHECK >>>>
        {
            // Check what *current* NCHITTEST result is for this click position
            POINT ptScreen = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }; // Needed for SendMessage LParam
            LRESULT hit = SendMessage(hwnd, WM_NCHITTEST, 0, MAKELPARAM(ptScreen.x, ptScreen.y));

            //DebugOut("WM_LBUTTONDOWN: Hit Test Result = %s\n", HitTestToString(hit));

            if(hit == HTCAPTION)
            {
                // Click was in the area we designated as draggable title bar
                ReleaseCapture();
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lparam); // Initiate move
                return 0; // We handled it
            }
            // If it was HTLEFT, HTRIGHT, HTCLIENT etc., let DefWindowProc handle it.
            // DefWindowProc initiates resize on border hits and forwards client clicks.
            break;
        }


        case RAWBIT_TRAY_MESSAGE: { /* Keep original */ return 0; }

    } // End switch(msg)

    // Use the default window procedure for unhandled messages
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}