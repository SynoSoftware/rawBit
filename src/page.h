#ifndef PAGE_H
#define PAGE_H

#include <windows.h>

// --- Define unique IDs for each page ---
#define PAGE_ID_INVALID      0 // Represents no active page or an error
#define PAGE_ID_TORRENTLIST  1
#define PAGE_ID_SETTINGS     2
#define PAGE_ID_ABOUT        3
// Add more page IDs here...


// --- Function Pointer Typedefs for Page Implementations ---
// Define the "interface" each page module must provide.

// Creates the page's UI. Returns the main container HWND for the page's controls.
// Parent is the HWND the page should draw/place controls within.
typedef HWND(*PFN_PAGE_CREATE)(HWND hParent, HINSTANCE hInstance);

// Destroys the page's UI controls and frees any resources.
// Takes the page's container HWND created by _Create as argument.
typedef void (*PFN_PAGE_DESTROY)(HWND hPageContainer);

// Handles messages forwarded from the main window proc.
// Target HWND might be the page's container or a child within it.
// Should return TRUE if message was handled, FALSE otherwise.
typedef BOOL(*PFN_PAGE_HANDLEMESSAGE)(HWND hPageContainer, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Called when the page is about to be shown.
typedef void (*PFN_PAGE_ACTIVATE)(HWND hPageContainer);

// Called when the page is about to be hidden.
typedef void (*PFN_PAGE_DEACTIVATE)(HWND hPageContainer);

// Called when the parent container area reserved for the page is resized.
typedef void (*PFN_PAGE_RESIZE)(HWND hPageContainer, int width, int height);

// Structure to hold function pointers for a specific page's implementation
typedef struct
{
    int                     pageId;          // Unique ID for this page
    PFN_PAGE_CREATE         pfnCreate;       // Pointer to the Create function
    PFN_PAGE_DESTROY        pfnDestroy;      // Pointer to the Destroy function
    PFN_PAGE_HANDLEMESSAGE  pfnHandleMessage;// Pointer to the message handler
    PFN_PAGE_ACTIVATE       pfnActivate;     // Pointer to the activation function
    PFN_PAGE_DEACTIVATE     pfnDeactivate;   // Pointer to the deactivation function
    PFN_PAGE_RESIZE         pfnResize;       // Pointer to the resize function
    HWND                    hPageContainer;  // HWND created by pfnCreate (managed internally)
} PageRegistration;

#endif // PAGE_H