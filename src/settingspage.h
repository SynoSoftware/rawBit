#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <windows.h>

// Functions implementing the Page interface for the Settings page
HWND SettingsPage_Create(HWND hParent, HINSTANCE hInstance);
void SettingsPage_Destroy(HWND hPageContainer);
BOOL SettingsPage_HandleMessage(HWND hPageContainer, UINT uMsg, WPARAM wParam, LPARAM lParam);
void SettingsPage_Activate(HWND hPageContainer);
void SettingsPage_Deactivate(HWND hPageContainer);
void SettingsPage_Resize(HWND hPageContainer, int width, int height);

#endif // SETTINGSPAGE_H