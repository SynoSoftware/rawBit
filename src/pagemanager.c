#include "pagemanager.h"
#include "debug.h"      // Assuming DebugOut exists
#include <stdlib.h>     // For calloc, free
#include <string.h>     // For memset

#define MAX_PAGES 10    // Max number of pages supported

// Internal state for the page manager
static struct
{
    PageRegistration pages[MAX_PAGES]; // Array to store registered pages
    int pageCount;                     // Number of registered pages
    int activePageId;                  // ID of the currently visible/active page
    HWND hParentContainer;             // HWND of the control that hosts the pages
    HINSTANCE hInstance;               // Application instance handle
} g_pageManager = { .pageCount = 0, .activePageId = PAGE_ID_INVALID };


// Find the index of a registered page by its ID
static int FindPageIndex(int pageId)
{
    if(pageId == PAGE_ID_INVALID) return -1; // Don't search for invalid ID
    for(int i = 0; i < g_pageManager.pageCount; ++i)
    {
        if(g_pageManager.pages[i].pageId == pageId)
        {
            return i; // Return the index in the array
        }
    }
    return -1; // Not found
}

// Initialize the page manager system
void PageManager_Initialize(HWND hParentContainer, HINSTANCE hInstance)
{
    DebugOut("PageManager: Initializing...\n");
    if(!hParentContainer || !hInstance)
    {
        DebugOut("PageManager: Error - Invalid arguments to Initialize.\n");
        // Fatal error potential, maybe assert or handle differently?
        return;
    }
    // Clear existing state in case of re-init (though shouldn't happen)
    PageManager_Cleanup(); // Ensure clean slate

    g_pageManager.hParentContainer = hParentContainer;
    g_pageManager.hInstance = hInstance;
    g_pageManager.pageCount = 0;
    g_pageManager.activePageId = PAGE_ID_INVALID;
    memset(g_pageManager.pages, 0, sizeof(g_pageManager.pages)); // Clear registrations
}

// Clean up page manager resources
void PageManager_Cleanup(void)
{
    DebugOut("PageManager: Cleaning up %d registered pages...\n", g_pageManager.pageCount);
    // Destroy the HWND/resources for all registered pages
    for(int i = 0; i < g_pageManager.pageCount; ++i)
    {
        PageRegistration* reg = &g_pageManager.pages[i];
        if(reg->hPageContainer && reg->pfnDestroy)
        {
            DebugOut("PageManager: Destroying page ID %d container (HWND %p)\n", reg->pageId, reg->hPageContainer);
            reg->pfnDestroy(reg->hPageContainer); // Ask the page to clean up its HWND and resources
        }
        // Clear the registration slot
        memset(reg, 0, sizeof(PageRegistration));
    }
    g_pageManager.pageCount = 0;
    g_pageManager.activePageId = PAGE_ID_INVALID;
    g_pageManager.hParentContainer = NULL;
    g_pageManager.hInstance = NULL;
}

// Register a page implementation
BOOL PageManager_RegisterPage(int pageId,
                              PFN_PAGE_CREATE pfnCreate,
                              PFN_PAGE_DESTROY pfnDestroy,
                              PFN_PAGE_HANDLEMESSAGE pfnHandleMessage,
                              PFN_PAGE_ACTIVATE pfnActivate,
                              PFN_PAGE_DEACTIVATE pfnDeactivate,
                              PFN_PAGE_RESIZE pfnResize)
{
    if(g_pageManager.pageCount >= MAX_PAGES)
    {
        DebugOut("PageManager: Error - Max pages (%d) reached. Cannot register page ID %d.\n", MAX_PAGES, pageId);
        return FALSE;
    }
    if(pageId == PAGE_ID_INVALID)
    {
        DebugOut("PageManager: Error - Cannot register PAGE_ID_INVALID.\n");
        return FALSE;
    }
    if(FindPageIndex(pageId) != -1)
    {
        DebugOut("PageManager: Error - Page ID %d already registered.\n", pageId);
        return FALSE; // Prevent duplicate registration
    }
    if(!pfnCreate || !pfnDestroy || !pfnHandleMessage || !pfnActivate || !pfnDeactivate || !pfnResize)
    {
        DebugOut("PageManager: Error - One or more required function pointers are NULL for page ID %d registration.\n", pageId);
        return FALSE; // Require all functions for consistent interface
    }

    DebugOut("PageManager: Registering Page ID %d\n", pageId);
    PageRegistration* reg = &g_pageManager.pages[g_pageManager.pageCount];
    reg->pageId = pageId;
    reg->pfnCreate = pfnCreate;
    reg->pfnDestroy = pfnDestroy;
    reg->pfnHandleMessage = pfnHandleMessage;
    reg->pfnActivate = pfnActivate;
    reg->pfnDeactivate = pfnDeactivate;
    reg->pfnResize = pfnResize;
    reg->hPageContainer = NULL; // Not created yet

    g_pageManager.pageCount++;
    return TRUE;
}

// Switch the currently displayed page
BOOL PageManager_SwitchToPage(int pageId)
{
    int newIndex = FindPageIndex(pageId);
    if(newIndex < 0)
    {
        DebugOut("PageManager: Error - Attempted to switch to unregistered page ID %d.\n", pageId);
        return FALSE;
    }

    int oldIndex = FindPageIndex(g_pageManager.activePageId);

    if(newIndex == oldIndex)
    {
        // DebugOut("PageManager: Already on page ID %d.\n", pageId);
        return TRUE; // No switch needed
    }

    DebugOut("PageManager: Switching from page %d to %d.\n", g_pageManager.activePageId, pageId);

    // --- Deactivate old page ---
    if(oldIndex >= 0)
    {
        PageRegistration* oldReg = &g_pageManager.pages[oldIndex];
        if(oldReg->hPageContainer)
        { // Only deactivate if it was created
            DebugOut("PageManager: Deactivating old page ID %d (HWND %p)\n", oldReg->pageId, oldReg->hPageContainer);
            oldReg->pfnDeactivate(oldReg->hPageContainer); // Call deactivate func
        }
        else
        {
            DebugOut("PageManager: Warning - Old page %d was not created, cannot deactivate.\n", oldReg->pageId);
        }
    }
    else
    {
        DebugOut("PageManager: No previous active page to deactivate.\n");
    }

    // --- Ensure new page UI is created ---
    PageRegistration* newReg = &g_pageManager.pages[newIndex];
    if(!newReg->hPageContainer)
    {
        DebugOut("PageManager: Creating page ID %d controls...\n", pageId);
        newReg->hPageContainer = newReg->pfnCreate(g_pageManager.hParentContainer, g_pageManager.hInstance);
        if(!newReg->hPageContainer)
        {
            DebugOut("PageManager: Error - Page ID %d Create function failed. Cannot switch.\n", pageId);
            // Potential state issue: old page is deactivated, new one failed creation.
            // Revert? For now, just fail the switch. Keep activePageId as the old one or INVALID?
            // Let's keep old one active if creation fails, but it's hidden now. Complex recovery needed?
             // Simple approach: leave activePageId as the old one (even if hidden).
            return FALSE;
        }
        else
        {
            DebugOut("PageManager: Page ID %d created with HWND %p\n", pageId, newReg->hPageContainer);
            // Set the parent relationship (good practice)
            SetParent(newReg->hPageContainer, g_pageManager.hParentContainer);
            // Trigger initial size calculation for the new page right after creation
            RECT rcParent;
            if(GetClientRect(g_pageManager.hParentContainer, &rcParent))
            {
                DebugOut("PageManager: Triggering initial resize for new page ID %d\n", pageId);
                newReg->pfnResize(newReg->hPageContainer, rcParent.right - rcParent.left, rcParent.bottom - rcParent.top);
            }
            else
            {
                DebugOut("PageManager: Warning - Could not get parent client rect for initial resize of page ID %d.\n", pageId);
            }
        }
    }

    // --- Activate new page ---
    DebugOut("PageManager: Activating new page ID %d (HWND %p)\n", pageId, newReg->hPageContainer);
    newReg->pfnActivate(newReg->hPageContainer); // Call activate func (should make it visible)

    // Update active page ID
    g_pageManager.activePageId = pageId;

    // Explicitly redraw the parent container? May help with visual glitches during switch.
    // InvalidateRect(g_pageManager.hParentContainer, NULL, TRUE); // Could cause flicker, maybe avoid.

    return TRUE;
}

// Get the currently active page ID
int PageManager_GetActivePageId(void)
{
    return g_pageManager.activePageId;
}


// Forwards messages to the active page's handler
BOOL PageManager_ForwardMessageToActivePage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int activeIndex = FindPageIndex(g_pageManager.activePageId);
    if(activeIndex >= 0)
    {
        PageRegistration* reg = &g_pageManager.pages[activeIndex];
        if(reg->hPageContainer && reg->pfnHandleMessage)
        {
            // Forward the message TO the page's specific container HWND
            return reg->pfnHandleMessage(reg->hPageContainer, uMsg, wParam, lParam);
        }
    }
    return FALSE; // No active page or handler not set or page not created
}

// Informs the active page that its designated area has been resized
void PageManager_ResizeActivePageArea(int width, int height)
{
    int activeIndex = FindPageIndex(g_pageManager.activePageId);
    if(activeIndex >= 0)
    {
        PageRegistration* reg = &g_pageManager.pages[activeIndex];
        if(reg->hPageContainer && reg->pfnResize)
        {
            // Tell the page implementation to resize ITS container/contents
            reg->pfnResize(reg->hPageContainer, width, height);
        }
    }
}