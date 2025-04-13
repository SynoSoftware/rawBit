#ifndef ABOUTPAGE_H
#define ABOUTPAGE_H

#include <windows.h>

// Functions implementing the Page interface for the About page
HWND AboutPage_Create(HWND hParent, HINSTANCE hInstance);
void AboutPage_Destroy(HWND hPageContainer);
BOOL AboutPage_HandleMessage(HWND hPageContainer, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AboutPage_Activate(HWND hPageContainer);
void AboutPage_Deactivate(HWND hPageContainer);
void AboutPage_Resize(HWND hPageContainer, int width, int height);

#endif // ABOUTPAGE_H