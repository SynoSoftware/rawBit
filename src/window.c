
#include "window.h"
#include "debug.h"
#include "win11ui.h"
#include "tray.h"
#include <stdio.h>
#include "resources.h"
#include <windows.h>
#include <windowsx.h>



LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg)
	{
		case WM_DESTROY:
		tray_icon_remove(hwnd);
		PostQuitMessage(0);
		return 0;

		case WM_SIZE:
		{
			int width = LOWORD(lparam);
			int height = HIWORD(lparam);
			InvalidateRect(hwnd, NULL, TRUE);
			// Example: Move status bar or repaint something
			// resize_child_controls(window_handle, width, height);

			return 0;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			RECT rect;
			GetClientRect(hwnd, &rect);

			HBRUSH brush = CreateSolidBrush(RGB(20, 20, 20)); // Dark background
			FillRect(hdc, &rect, brush);
			DeleteObject(brush);

			EndPaint(hwnd, &ps);
			return 0;
		}

		case RAWBIT_TRAY_MESSAGE:
		{
			if(lparam == WM_LBUTTONDBLCLK)
			{
				ShowWindow(hwnd, SW_RESTORE);
				SetForegroundWindow(hwnd);
			}
			return 0;
		}

		case WM_CLOSE:
		{
			DestroyWindow(hwnd); // Clean exit
			return 0;
		}

		case WM_KEYDOWN:
		{
			if(wparam == VK_ESCAPE)
			{
				PostMessageA(hwnd, WM_CLOSE, 0, 0);
			}
			return 0;
		}
		case WM_ACTIVATE:
		//if (LOWORD(wparam) == WA_INACTIVE)
		//{
		//	ShowWindow(hwnd, SW_HIDE);
		//}
		return 0;

		case WM_SETCURSOR:
		{
			if(LOWORD(lparam) == HTLEFT || LOWORD(lparam) == HTRIGHT)
			{
				SetCursor(LoadCursorW(NULL, IDC_SIZEWE)); // Left-Right resize
				return TRUE;
			}
			if(LOWORD(lparam) == HTTOP || LOWORD(lparam) == HTBOTTOM)
			{
				SetCursor(LoadCursorW(NULL, IDC_SIZENS)); // Up-Down resize
				return TRUE;
			}

			// For corners (optional, you can skip if you want)
			if(LOWORD(lparam) == HTTOPLEFT || LOWORD(lparam) == HTBOTTOMRIGHT)
			{
				SetCursor(LoadCursorW(NULL, IDC_SIZENWSE)); // Diagonal resize
				return TRUE;
			}
			if(LOWORD(lparam) == HTTOPRIGHT || LOWORD(lparam) == HTBOTTOMLEFT)
			{
				SetCursor(LoadCursorW(NULL, IDC_SIZENESW)); // Diagonal resize
				return TRUE;
			}

			// Default cursor
			SetCursor(LoadCursorW(NULL, IDC_ARROW));
			return TRUE;
		}



		case WM_NCHITTEST:
		{
			POINT pt;
			pt.x = GET_X_LPARAM(lparam);
			pt.y = GET_Y_LPARAM(lparam);

			ScreenToClient(hwnd, &pt);  // Works fine for hit test

			RECT rect;
			GetClientRect(hwnd, &rect);

			int border = 8;

			// Top-Left Corner
			if(pt.x < rect.left + border && pt.y < rect.top + border)
			{
				//DebugOut("Mouse is over top-left corner (resize)\n");
				return HTTOPLEFT;
			}

			// Top-Right Corner
			if(pt.x > rect.right - border && pt.y < rect.top + border)
			{
				//DebugOut("Mouse is over top-right corner (resize)\n");
				return HTTOPRIGHT;
			}

			// Bottom-Left Corner
			if(pt.x < rect.left + border && pt.y > rect.bottom - border)
			{
				//DebugOut("Mouse is over bottom-left corner (resize)\n");
				return HTBOTTOMLEFT;
			}

			// Bottom-Right Corner
			if(pt.x > rect.right - border && pt.y > rect.bottom - border)
			{
				//DebugOut("Mouse is over bottom-right corner (resize)\n");
				return HTBOTTOMRIGHT;
			}

			// Edges
			if(pt.x < rect.left + border)
				return HTLEFT;

			if(pt.x > rect.right - border)
				return HTRIGHT;

			if(pt.y < rect.top + border)
				return HTTOP;

			if(pt.y > rect.bottom - border)
				return HTBOTTOM;

			//DebugOut("Mouse is inside the client area (no resize)\n");
			return HTCLIENT;
		}

		// WM_SIZING — Handle both horizontal and vertical resizing
		case WM_SIZING:
		{
			RECT* rect = (RECT*)lparam;

			// Log the current window size during resizing
			//char buffer[256];
			//snprintf(buffer, sizeof(buffer), "Resizing window: left=%d, top=%d, right=%d, bottom=%d\n", rect->left, rect->top, rect->right, rect->bottom);
			//DebugOut(buffer);

			int borderThickness = 8; // Define the border thickness

			// Enforce minimum width and height
			if(rect->right - rect->left < 400)
			{
				rect->right = rect->left + 400;  // Prevent shrinking too much in width
			}
			if(rect->bottom - rect->top < 200)
			{
				rect->bottom = rect->top + 200;  // Prevent shrinking too much in height
			}

			// Log after enforcing minimum size
			//snprintf(buffer, sizeof(buffer), "Window size after enforcing min size: left=%d, top=%d, right=%d, bottom=%d\n", rect->left, rect->top, rect->right, rect->bottom);
			//DebugOut(buffer);

			return 0;
		}


		case WM_LBUTTONDOWN:
		{
			POINT pt;
			GetCursorPos(&pt);  // Always correct after move

			//DebugOut("Real Screen Cursor Pos: x=%d, y=%d\n", pt.x, pt.y);

			RECT windowRect;
			GetWindowRect(hwnd, &windowRect);

			pt.x -= windowRect.left;
			pt.y -= windowRect.top;

			//DebugOut("Adjusted Coords Relative to Window: x=%d, y=%d\n", pt.x, pt.y);

			RECT rect;
			GetClientRect(hwnd, &rect);
			//DebugOut("Client Area: left=%d, top=%d, right=%d, bottom=%d\n", rect.left, rect.top, rect.right, rect.bottom);

			int borderThickness = 8;

			if(pt.x >= rect.left + borderThickness && pt.x <= rect.right - borderThickness &&
			   pt.y >= rect.top + borderThickness && pt.y <= rect.bottom - borderThickness)
			{
				//DebugOut("Inside content area: initiating window move\n");
				ReleaseCapture();
				SendMessageA(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
			}
			else
			{
				//DebugOut("Click in border region: treated as resize\n");
			}

			return 0;
		}

	}

	return DefWindowProcA(hwnd, msg, wparam, lparam);
}
