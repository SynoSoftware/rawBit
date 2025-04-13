#include "aboutpage.h"
#include "debug.h"

// Create the About Page controls
HWND AboutPage_Create(HWND hParent, HINSTANCE hInstance) {
     DebugOut("AboutPage: Creating...\n");
     HWND hContainer = CreateWindowExW(0, L"STATIC", L"ABOUT rawBit v0.1\nMinimal Torrent Client\n\n(Pure C for Win11)",
                                      WS_CHILD | WS_VISIBLE | SS_CENTER | SS_ETCHEDFRAME, // Center text, visible frame
                                      0, 0, 100, 100, // Positioned/Sized in Resize
                                      hParent, NULL, hInstance, NULL);
    if (!hContainer) {
        DebugOut("AboutPage: Failed to create container STATIC control. Error: %lu\n", GetLastError());
    }
     return hContainer;
}

void AboutPage_Destroy(HWND hPageContainer) {
     DebugOut("AboutPage: Destroying... (HWND %p)\n", hPageContainer);
     if (hPageContainer) DestroyWindow(hPageContainer);
}

BOOL AboutPage_HandleMessage(HWND hPageContainer, UINT uMsg, WPARAM wParam, LPARAM lParam) {
     return FALSE; /* Not handled */
}

void AboutPage_Activate(HWND hPageContainer) {
    DebugOut("AboutPage: Activating... (HWND %p)\n", hPageContainer);
     if (hPageContainer) ShowWindow(hPageContainer, SW_SHOW);
}

void AboutPage_Deactivate(HWND hPageContainer) {
    DebugOut("AboutPage: Deactivating... (HWND %p)\n", hPageContainer);
     if (hPageContainer) ShowWindow(hPageContainer, SW_HIDE);
}

void AboutPage_Resize(HWND hPageContainer, int width, int height) {
     // DebugOut("AboutPage: Resizing... (HWND %p) to %d x %d\n", hPageContainer, width, height);
     if (hPageContainer) MoveWindow(hPageContainer, 0, 0, width, height, TRUE);
}