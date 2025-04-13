#include "settingspage.h"
#include "debug.h"

// Create the Settings Page controls
HWND SettingsPage_Create(HWND hParent, HINSTANCE hInstance) {
     DebugOut("SettingsPage: Creating...\n");
     // Using a STATIC control as the main container/placeholder for this page
     HWND hContainer = CreateWindowExW(0, L"STATIC", L"SETTINGS PAGE PLACEHOLDER",
                                      WS_CHILD | WS_VISIBLE | SS_CENTER | SS_ETCHEDFRAME, // Visible frame
                                      0, 0, 100, 100, // Positioned/Sized in Resize
                                      hParent, NULL, hInstance, NULL);
    // TODO: Create actual settings controls (edit boxes, checkboxes) as *children* of hContainer
    if (!hContainer) {
        DebugOut("SettingsPage: Failed to create container STATIC control. Error: %lu\n", GetLastError());
    }
     return hContainer; // Return the static control as the page container
}

void SettingsPage_Destroy(HWND hPageContainer) {
     DebugOut("SettingsPage: Destroying... (HWND %p)\n", hPageContainer);
     if (hPageContainer) DestroyWindow(hPageContainer);
}

BOOL SettingsPage_HandleMessage(HWND hPageContainer, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // TODO: Handle commands/notifications from settings controls
    return FALSE; /* Not handled */
}

void SettingsPage_Activate(HWND hPageContainer) {
    DebugOut("SettingsPage: Activating... (HWND %p)\n", hPageContainer);
    if (hPageContainer) ShowWindow(hPageContainer, SW_SHOW);
}

void SettingsPage_Deactivate(HWND hPageContainer) {
    DebugOut("SettingsPage: Deactivating... (HWND %p)\n", hPageContainer);
     if (hPageContainer) ShowWindow(hPageContainer, SW_HIDE);
}

void SettingsPage_Resize(HWND hPageContainer, int width, int height) {
     // DebugOut("SettingsPage: Resizing... (HWND %p) to %d x %d\n", hPageContainer, width, height);
     if (hPageContainer) MoveWindow(hPageContainer, 0, 0, width, height, TRUE);
}