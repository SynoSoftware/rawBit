#include <windows.h>
#include <stdio.h>
#include "win11ui.h"
#include "resources.h"
#include "debug.h"
#include "window.h"
#include <windowsx.h> 



int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
{
	
	// Load Icons from separate .ico files
	
	HICON icon_small = LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_ICON_SMALL));
	HICON icon_large = LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_ICON_LARGE));
	if(!icon_small)
	{
		MessageBoxA(NULL, "Failed to load small icon", "Error", MB_OK | MB_ICONERROR);
		return 1;
	}
	if(!icon_large)
	{
		MessageBoxA(NULL, "Failed to load large icon", "Error", MB_OK | MB_ICONERROR);
		return 1;
	}

	// Register window class
	WNDCLASSEXA wc = { 0 };

	wc.cbSize = sizeof(WNDCLASSEXA);  // Make sure the size is set properly
	wc.lpfnWndProc = window_proc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "rawbit_window_class";
	wc.hbrBackground = NULL;  // Since you handle painting
	wc.hIconSm = icon_small;
	wc.hIcon = icon_large;

	RegisterClassExA(&wc);
	// Create window
	HWND hwnd = CreateWindowExA(
		WS_EX_APPWINDOW,
		"rawbit_window_class",
		"rawBit",
		WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 300,
		NULL, NULL, hInstance, NULL
	);
	if(!hwnd)
	{
		MessageBoxA(NULL, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
		return 1;
	}
	win11ui_enable_dark_mode();
	win11ui_apply_rounded_corners(hwnd);
	win11ui_apply_blur(hwnd);


	ShowWindow(hwnd, nCmdShow);
	tray_icon_add(hwnd, hInstance);

	MSG msg;
	while(GetMessageA(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	return 0;
}


