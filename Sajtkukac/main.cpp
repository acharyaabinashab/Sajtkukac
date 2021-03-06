/* Sajtkukac - customizes the position of icons on your Windows taskbar
 * Copyright (C) 2019 friendlyanon <skype1234@waifu.club>
 * main.cpp is part of Sajtkukac.
 *
 * Sajtkukac is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Sajtkukac is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Sajtkukac. If not, see <https://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "main.h"
#include "Updater.h"
#include "IniFile.h"
#include "Easing.h"

#pragma comment (lib, "Version.lib")
#pragma comment (linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define MAX_LOADSTRING (100)
#define WM_USER_SHELLICON (WM_USER + 1)

// Global Variables:
HINSTANCE hInst;                       // current instance
NOTIFYICONDATA nidApp;                 // tray icon
HMENU hPopMenu;                        // right click context menu
WCHAR szTitle[MAX_LOADSTRING];         // the title bar text
WCHAR szWindowClass[MAX_LOADSTRING];   // the main window class name
UINT uPercentage;                      // position of the icons
UINT uRefreshRate;                     // refresh rate in milliseconds
UINT uEasingIdx;                       // index of the easing function
WORKER_DETAILS *wdDetails = nullptr;   // struct to communicate with worker
UINT uMajor = 0, uMinor = 4;           // current version

// TODO: fall back to system icons until someone makes one :)
#ifndef IDI_SAJTKUKAC
#define SHELL_ICON_ID (3)
HINSTANCE hShell32 = ::LoadLibraryA("shell32.dll");
HICON hShellIcon = ::LoadIcon(hShell32, MAKEINTRESOURCE(SHELL_ICON_ID));
HICON hSmallShellIcon = ::LoadIcon(hShell32, MAKEINTRESOURCE(SHELL_ICON_ID));
#endif

// Forward declarations of functions included in this code module:
ATOM             RegisterSajtkukacClass(HINSTANCE hInstance);
BOOL             InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Settings(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
VOID             ShowContextMenu(HWND);
VOID             RestartExplorer(VOID);
VOID             StartProc(LPCWSTR, UINT);
VOID             TerminateApplication(HWND, BOOL);
BOOL             InitWorker(VOID);
VOID             ShowSuccessBalloon(VOID);

int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(hPrevInstance);

	// Detect already running instance
	HANDLE hMutex = ::CreateMutex(0, TRUE, L"Sajtkukac");
	if (GetLastError() == ERROR_ALREADY_EXISTS) return 1;
	SCOPE_EXIT{ ::CloseHandle(hMutex); };

	// Initialize globals
	::LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	::LoadString(hInstance, IDC_SAJTKUKAC, szWindowClass, MAX_LOADSTRING);
	::RegisterSajtkukacClass(hInstance);
	::ReadIni(uPercentage, uRefreshRate, uEasingIdx);
	if (uPercentage > 100) uPercentage = 50;
	if (uRefreshRate < 50 || 10000 < uRefreshRate) uRefreshRate = 500;
	if (uEasingIdx >= easingFunctions.size()) uEasingIdx = 25;

	// Perform application initialization
	if (!::InitInstance(hInstance, nCmdShow) || !::InitWorker())
	{
		return 1;
	}

	::WriteIni(uPercentage, uRefreshRate, uEasingIdx);

	HACCEL hAccelTable = ::LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SAJTKUKAC));
	MSG msg;

	// Main message loop
	while (::GetMessage(&msg, nullptr, 0, 0))
	{
		if (!::TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

ATOM RegisterSajtkukacClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
#ifdef IDI_SAJTKUKAC
	wcex.hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SAJTKUKAC));
	wcex.hIconSm = ::LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
#else
	wcex.hIcon = hShellIcon;
	wcex.hIconSm = hSmallShellIcon;
#endif
	wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SAJTKUKAC);
	wcex.lpszClassName = szWindowClass;

	return ::RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	UNREFERENCED_PARAMETER(nCmdShow);

	// Store instance handle in our global variable
	hInst = hInstance;

	HWND hWnd = ::CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	nidApp.cbSize = sizeof(NOTIFYICONDATA);

#ifdef IDI_SAJTKUKAC
	HICON hMainIcon = ::LoadIcon(hInstance, (LPCTSTR)MAKEINTRESOURCE(IDI_SAJTKUKAC));
	nidApp.uID = IDI_SAJTKUKAC;
	nidApp.hIcon = hMainIcon;
#else
	nidApp.uID = SHELL_ICON_ID;
	nidApp.hIcon = hShellIcon;
#endif

	nidApp.hWnd = hWnd;
	nidApp.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nidApp.uCallbackMessage = WM_USER_SHELLICON;
	::LoadString(hInstance, IDS_APP_TITLE, nidApp.szTip, MAX_LOADSTRING);
	::Shell_NotifyIcon(NIM_ADD, &nidApp);

	return TRUE;
}

BOOL InitWorker(VOID)
{
	return ::Init(nidApp.hWnd, &uPercentage, &uRefreshRate,
		&uEasingIdx, &wdDetails);
}

#define LOADSTRING(name, id) \
	WCHAR name[MAX_LOADSTRING]; \
	::LoadString(hInst, id, name, MAX_LOADSTRING)

VOID ShowContextMenu(HWND hWnd)
{
	hPopMenu = ::CreatePopupMenu();

	constexpr auto makeMenuItem = [&](UINT nameId, UINT flags, UINT menuId)
	{
		LOADSTRING(m, nameId);
		BSTR m_bstr = SysAllocString(m);
		::InsertMenu(hPopMenu, -1, flags, menuId, m_bstr);
		::SysFreeString(m_bstr);
	};

	makeMenuItem(IDS_TRAY_SETTINGS,  MF_BYPOSITION | MF_STRING, IDM_SETTINGS);
	makeMenuItem(IDS_TRAY_RELOAD,    MF_BYPOSITION | MF_STRING, IDM_RELOAD);
	makeMenuItem(IDS_TRAY_SEPARATOR, MF_SEPARATOR,              IDM_SEP);
	makeMenuItem(IDS_TRAY_UPDATE,    MF_BYPOSITION | MF_STRING, IDM_UPDATE);
	makeMenuItem(IDS_TRAY_SEPARATOR, MF_SEPARATOR,              IDM_SEP);
	makeMenuItem(IDS_TRAY_ABOUT,     MF_BYPOSITION | MF_STRING, IDM_ABOUT);
	makeMenuItem(IDS_TRAY_SEPARATOR, MF_SEPARATOR,              IDM_SEP);
	makeMenuItem(IDS_TRAY_EXIT,      MF_BYPOSITION | MF_STRING, IDM_EXIT);

	::SetForegroundWindow(hWnd);

	POINT lpClickPoint;
	::GetCursorPos(&lpClickPoint);
	::TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN,
		lpClickPoint.x, lpClickPoint.y, 0, hWnd, nullptr);
}

VOID StartProc(LPCWSTR program, UINT args)
{
	STARTUPINFO si;
	::ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	::ZeroMemory(&pi, sizeof(pi));
	LOADSTRING(command, args);
	::CreateProcess(program, command, nullptr, nullptr,
		FALSE, 0, nullptr, nullptr, &si, &pi);
}

VOID RestartExplorer(VOID)
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (::Process32First(snapshot, &entry) == TRUE)
	{
		while (::Process32Next(snapshot, &entry) == TRUE)
		{
			if (::wcscmp(L"explorer.exe", entry.szExeFile) != 0) continue;
			HANDLE hProcess = ::OpenProcess(
				PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
			::TerminateProcess(hProcess, 1);
			::CloseHandle(hProcess);
		}
	}

	::CloseHandle(snapshot);

	::StartProc(nullptr, IDS_EXPLORER_COMMAND);
}

VOID TerminateApplication(HWND hWnd, BOOL startUpdater)
{
	if (startUpdater) ::StartProc(L"cscript.exe", IDS_UPDATER_COMMAND);
	else ::RestartExplorer();

	::Shell_NotifyIcon(NIM_DELETE, &nidApp);
	::DestroyWindow(hWnd);
}

VOID ShowSuccessBalloon(VOID)
{
	NOTIFYICONDATA data;

	data.cbSize = sizeof(data);

	data.hWnd = nidApp.hWnd;
#ifdef IDI_SAJTKUKAC
	data.uID = IDI_SAJTKUKAC;
#else
	data.uID = SHELL_ICON_ID;
#endif
	data.uFlags = NIF_INFO;
	data.dwInfoFlags = NIIF_INFO;
	data.uTimeout = 10000;

	::LoadString(hInst, IDS_APP_TITLE, data.szInfoTitle, MAX_LOADSTRING);
	::LoadString(hInst, IDS_WORKER_RELOADED, data.szInfo, MAX_LOADSTRING);

	::Shell_NotifyIcon(NIM_MODIFY, &data);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_USER_SHELLICON:
	{
		switch (LOWORD(lParam))
		{
		case WM_RBUTTONDOWN:
			::ShowContextMenu(hWnd);
			return TRUE;
		}
	}
	break;
	case WM_USER_WORKER:
	{
		switch (wParam)
		{
		case WM_USER_WORKER_FAILURE:
		{
			LOADSTRING(format, IDS_WORKER_FAIL_FORMAT);
			WCHAR buffer[256];
			::StringCbPrintf(buffer, sizeof(buffer),
				format, (HRESULT)lParam);
			LOADSTRING(caption, IDS_WORKER_FAIL_CAPTION);
			::MessageBox(hWnd, buffer, caption,
				MB_ICONSTOP | MB_SYSTEMMODAL);
			::TerminateApplication(hWnd, FALSE);
		}
		break;
		case WM_USER_WORKER_RELOAD:
			if (::InitWorker())
			{
				::ShowSuccessBalloon();
			}
			else
			{
				LOADSTRING(message, IDS_WORKER_FAIL_MSG);
				LOADSTRING(caption, IDS_WORKER_FAIL_CAPTION);
				::MessageBox(hWnd, message, caption,
					MB_ICONSTOP | MB_SYSTEMMODAL);
				::TerminateApplication(hWnd, FALSE);
			}
			break;
		}
	}
	break;
	case WM_USER_UPDATER:
	{
		switch (wParam)
		{
		case WM_USER_UPDATER_FAIL:
		{
			LOADSTRING(format, IDS_UPDATER_FAIL);
			WCHAR buffer[256];
			::StringCbPrintf(buffer, sizeof(buffer),
				format, (DWORD)lParam);
			LOADSTRING(caption, IDS_APP_TITLE);
			::MessageBox(hWnd, buffer, caption,
				MB_ICONWARNING | MB_SYSTEMMODAL);
		}
		break;
		case WM_USER_UPDATER_NOT_FOUND:
		{
			LOADSTRING(message, IDS_UPDATER_NOT_FOUND);
			LOADSTRING(caption, IDS_APP_TITLE);
			::MessageBox(hWnd, message, caption,
				MB_ICONASTERISK | MB_SYSTEMMODAL);
		}
		break;
		case WM_USER_UPDATER_FOUND:
		{
			LOADSTRING(format, IDS_UPDATER_FOUND);
			WCHAR buffer[256];
			::StringCbPrintf(buffer, sizeof(buffer),
				format, LOWORD(lParam), HIWORD(lParam));
			LOADSTRING(caption, IDS_APP_TITLE);
			INT button = ::MessageBox(hWnd, buffer, caption,
				MB_ICONASTERISK | MB_YESNO | MB_SYSTEMMODAL);
			if (button == IDYES)
				::Updater(nidApp.hWnd, lParam, FALSE);
		}
		break;
		case WM_USER_UPDATER_DONE:
		{
			LOADSTRING(message, IDS_UPDATER_DONE);
			LOADSTRING(caption, IDS_APP_TITLE);
			::MessageBox(hWnd, message, caption,
				MB_ICONASTERISK | MB_SYSTEMMODAL);
			::TerminateApplication(nidApp.hWnd, TRUE);
		}
		break;
		}
	}
	break;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections
		switch (wmId)
		{
		case IDM_SETTINGS:
			::DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGSBOX), hWnd, Settings);
			break;
		case IDM_RELOAD:
			if (wdDetails->reload) break;
			wdDetails->reload = TRUE;
			break;
		case IDM_UPDATE:
			::Updater(nidApp.hWnd, MAKELPARAM(uMajor, uMinor), TRUE);
			break;
		case IDM_ABOUT:
			::DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			::TerminateApplication(hWnd, FALSE);
			break;
		default:
			return ::DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
	default:
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}
	return FALSE;
}

INT_PTR CALLBACK Settings(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	static BOOL bInitialized = FALSE;
	if (!bInitialized && message != WM_INITDIALOG) return (INT_PTR)FALSE;
	switch (message)
	{
	case WM_INITDIALOG:
		bInitialized = TRUE;
		::SendDlgItemMessage(hDlg, IDC_PERCENTAGE_SPIN,
			UDM_SETRANGE, 0, MAKELPARAM(100, 0));
		::SendDlgItemMessage(hDlg, IDC_REFRESH_SPIN,
			UDM_SETRANGE, 0, MAKELPARAM(10000, 50));
		::SendDlgItemMessage(hDlg, IDC_PERCENTAGE_SLIDER,
			TBM_SETRANGE, 0, MAKELONG(0, 100));
		::SendDlgItemMessage(hDlg, IDC_PERCENTAGE_SLIDER,
			TBM_SETPAGESIZE, 0, 1);
		::SendDlgItemMessage(hDlg, IDC_PERCENTAGE_SLIDER,
			TBM_SETPOS, TRUE, uPercentage);
		for (const auto& pair : easingFunctions) {
			::SendDlgItemMessage(hDlg, IDC_EASING_COMBO,
				CB_ADDSTRING, 0,
				reinterpret_cast<LPARAM>(pair.first));
		}
		::SendDlgItemMessage(hDlg, IDC_EASING_COMBO,
			CB_SETCURSEL, uEasingIdx, 0);
		::SetDlgItemInt(hDlg, IDC_PERCENTAGE_EDIT,
			uPercentage, TRUE);
		::SetDlgItemInt(hDlg, IDC_REFRESH_EDIT,
			uRefreshRate, TRUE);
		return (INT_PTR)TRUE;
	case WM_NOTIFY:
	{
		LPNMHDR header = (LPNMHDR)lParam;
		switch (header->code)
		{
		case NM_CLICK:
		case NM_RETURN:
		{
			PNMLINK pNMLink = (PNMLINK)lParam;
			LITEM item = pNMLink->item;
			if (header->idFrom == IDC_SYSLINK1 && item.iLink == 0)
			{
				::ShellExecute(hDlg, nullptr, item.szUrl,
					nullptr, nullptr, SW_SHOW);
			}
		}
		break;
		}
	}
	break;
	case WM_COMMAND:
	{
		WORD wNotifyCode = HIWORD(wParam);
		WORD wID = LOWORD(wParam);
		HWND hwndCtl = (HWND)lParam;
		switch (wID)
		{
		case IDC_PERCENTAGE_EDIT:
		{
			if (wNotifyCode != EN_CHANGE) break;
			BOOL success;
			WORD percentage = ::GetDlgItemInt(hDlg, wID, &success, FALSE);
			if (success && 0 <= percentage && percentage <= 100)
			{
				::SendDlgItemMessage(hDlg, IDC_PERCENTAGE_SLIDER,
					TBM_SETPOS, TRUE, uPercentage = percentage);
			}
			break;
		}
		break;
		case IDC_REFRESH_EDIT:
		{
			if (wNotifyCode != EN_CHANGE) break;
			BOOL success;
			WORD refreshRate = ::GetDlgItemInt(hDlg, wID, &success, FALSE);
			if (success && 50 <= refreshRate && refreshRate <= 10000)
			{
				uRefreshRate = refreshRate;
			}
			break;
		}
		break;
		case IDC_EASING_COMBO:
			if (wNotifyCode != CBN_SELCHANGE) break;
			uEasingIdx = ::SendDlgItemMessage(hDlg,
				IDC_EASING_COMBO, CB_GETCURSEL, 0, 0);
		break;
		case IDOK:
		case IDCANCEL:
			::WriteIni(uPercentage, uRefreshRate, uEasingIdx);
			::EndDialog(hDlg, 0);
		}
	}
	break;
	case WM_HSCROLL:
	{
		if (::GetDlgItem(hDlg, IDC_PERCENTAGE_SLIDER) != (HWND)lParam)
			break;
		::SetDlgItemInt(hDlg, IDC_PERCENTAGE_EDIT, uPercentage =
			::SendDlgItemMessage(hDlg, IDC_PERCENTAGE_SLIDER,
				TBM_GETPOS, 0, 0) & 0xFF, FALSE);
	}
	break;
	case WM_DESTROY:
		bInitialized = FALSE;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
	{
		WORD wNotifyCode = HIWORD(wParam);
		WORD wID = LOWORD(wParam);
		HWND hwndCtl = (HWND)lParam;
		switch (wID)
		{
		case IDC_BUTTON1:
			::ShellExecute(hDlg, nullptr,
				L"https://github.com/friendlyanon/Sajtkukac#donation",
				nullptr, nullptr, SW_SHOW);
			break;
		case IDC_BUTTON2:
			::ShellExecute(hDlg, nullptr,
				L"https://github.com/friendlyanon/Sajtkukac",
				nullptr, nullptr, SW_SHOW);
			break;
		case IDOK:
		case IDCANCEL:
			::EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
	}
	break;
	}
	return (INT_PTR)FALSE;
}
