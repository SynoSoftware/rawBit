#ifndef TORRENTLISTPAGE_H
#define TORRENTLISTPAGE_H

#include <windows.h>

// Functions implementing the Page interface for the Torrent List page
HWND TorrentListPage_Create(HWND hParent, HINSTANCE hInstance);
void TorrentListPage_Destroy(HWND hPageContainer);
BOOL TorrentListPage_HandleMessage(HWND hPageContainer, UINT uMsg, WPARAM wParam, LPARAM lParam);
void TorrentListPage_Activate(HWND hPageContainer);
void TorrentListPage_Deactivate(HWND hPageContainer);
void TorrentListPage_Resize(HWND hPageContainer, int width, int height);

#endif // TORRENTLISTPAGE_H