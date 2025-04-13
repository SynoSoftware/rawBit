#ifndef PAGEMANAGER_H
#define PAGEMANAGER_H

#include <windows.h>
#include "page.h" // Include page definitions and IDs

// --- Page Manager API ---

// Initialize the page manager system. Must be called once at startup.
// hParentContainer: HWND of the window area where pages will be displayed.
// hInstance: The application instance handle.
void PageManager_Initialize(HWND hParentContainer, HINSTANCE hInstance);

// Clean up page manager resources. Call before application exit.
void PageManager_Cleanup(void);

// Register a page implementation with the manager. Must be called after Initialize.
BOOL PageManager_RegisterPage(int pageId,
                              PFN_PAGE_CREATE pfnCreate,
                              PFN_PAGE_DESTROY pfnDestroy,
                              PFN_PAGE_HANDLEMESSAGE pfnHandleMessage,
                              PFN_PAGE_ACTIVATE pfnActivate,
                              PFN_PAGE_DEACTIVATE pfnDeactivate,
                              PFN_PAGE_RESIZE pfnResize);

// Switch the currently displayed page. Creates page UI if not already created.
BOOL PageManager_SwitchToPage(int pageId);

// Get the currently active page ID.
int PageManager_GetActivePageId(void);

// --- Functions to forward messages from the main window procedure ---

// Forwards messages like WM_COMMAND, WM_NOTIFY to the active page's handler.
// Returns TRUE if the active page handled the message.
BOOL PageManager_ForwardMessageToActivePage(UINT uMsg, WPARAM wParam, LPARAM lParam);

// Informs the active page that its designated area has been resized.
void PageManager_ResizeActivePageArea(int width, int height);


#endif // PAGEMANAGER_H