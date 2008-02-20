// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// crash_generation_app.cpp : Defines the entry point for the application.
//

#include "precompile.h"

#define MAX_LOADSTRING 100

const wchar_t kPipeName[] = L"\\\\.\\pipe\\GoogleCrashServices";

// Global Variables:
HINSTANCE hInst;                        // current instance
TCHAR szTitle[MAX_LOADSTRING];          // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];    // the main window class name

// Forward declarations of functions included in this code module:
ATOM        MyRegisterClass(HINSTANCE hInstance);
BOOL        InitInstance(HINSTANCE, int);
LRESULT CALLBACK  WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK  About(HWND, UINT, WPARAM, LPARAM);

google_breakpad::CrashGenerationServer *gCrashGenerator;

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  // TODO: Place code here.
  _CrtSetReportMode(_CRT_ASSERT, 0);
  google_breakpad::ExceptionHandler *ex =
    new google_breakpad::ExceptionHandler(
      L"C:\\dumps\\",
      NULL,
      NULL,
      NULL,
      google_breakpad::ExceptionHandler::HANDLER_ALL,
      kPipeName);

  MSG msg;
  HACCEL hAccelTable;

  // Initialize global strings
  LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadString(hInstance,
             IDC_CRASHGENERATIONAPP,
             szWindowClass,
             MAX_LOADSTRING);
  MyRegisterClass(hInstance);

  // Perform application initialization:
  if (!InitInstance (hInstance, nCmdShow)) {
    return FALSE;
  }

  hAccelTable = LoadAccelerators(hInstance,
                                 MAKEINTRESOURCE(IDC_CRASHGENERATIONAPP));

  // Main message loop:
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return (int) msg.wParam;
}

// Registers the window class.
//
// This function and its usage are only necessary if you want this code
// to be compatible with Win32 systems prior to the 'RegisterClassEx'
// function that was added to Windows 95. It is important to call this
// function so that the application will get 'well formed' small icons
// associated with it.
ATOM MyRegisterClass(HINSTANCE hInstance) {
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInstance;
  wcex.hIcon = LoadIcon(hInstance,
                        MAKEINTRESOURCE(IDI_CRASHGENERATIONAPP));
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  wcex.lpszMenuName = MAKEINTRESOURCE(IDC_CRASHGENERATIONAPP);
  wcex.lpszClassName = szWindowClass;
  wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

  return RegisterClassEx(&wcex);
}

// Saves instance handle and creates main window
//
// In this function, we save the instance handle in a global variable and
//   create and display the main program window.
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
  HWND hWnd;

  hInst = hInstance; // Store instance handle in our global variable

  hWnd = CreateWindow(szWindowClass,
                      szTitle,
                      WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT,
                      0,
                      CW_USEDEFAULT,
                      0,
                      NULL,
                      NULL,
                      hInstance,
                      NULL);

  if (!hWnd) {
    return FALSE;
  }

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  return TRUE;
}

static void _cdecl ShowClientConnected(
  void* context,
  const google_breakpad::ClientInfo& client_info) {
  ::MessageBoxW(NULL,
                L"Client connected", 
                L"Crash Generation Server App", MB_OK);
}

static void _cdecl ShowClientCrashed(
  void* context,
  const google_breakpad::ClientInfo& client_info) {
  ::MessageBoxW(NULL,
                L"Client crashed", 
                L"Crash Generation Server App", MB_OK);
}

void CrashServerStart() {
  wstring dumpPath = L"C:\\Dumps\\";
  gCrashGenerator = new google_breakpad::CrashGenerationServer(
    kPipeName,
    ShowClientConnected,
    NULL,
    ShowClientCrashed,
    NULL,
    true,
    &dumpPath);

  if (!gCrashGenerator->Initialize()) {
    ::MessageBoxW(NULL,
                  L"Unable to start server", 
                  L"Dumper", MB_OK);
  }
}

void DerefZeroCrash() {
  int* x = 0;
  *x = 1;
}

void InvalidParamCrash() {
  printf(NULL);
}

void PureCallCrash() {
  Derived derived;
}

// Processes messages for the main window.
//
// WM_COMMAND	- process the application menu
// WM_PAINT	- Paint the main window
// WM_DESTROY	- post a quit message and return
LRESULT CALLBACK WndProc(HWND hWnd,
                         UINT message,
                         WPARAM wParam,
                         LPARAM lParam) {
  int wmId, wmEvent;
  PAINTSTRUCT ps;
  HDC hdc;

  switch (message) {
  case WM_COMMAND:
    wmId    = LOWORD(wParam);
    wmEvent = HIWORD(wParam);
    // Parse the menu selections:
    switch (wmId) {
    case IDM_ABOUT:
      DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
      break;
    case IDM_EXIT:
      DestroyWindow(hWnd);
      break;
    case ID_SERVER_START:
      CrashServerStart();
      break;
    case ID_CLIENT_DEREFZERO:
      DerefZeroCrash();
      break;
    case ID_CLIENT_INVALIDPARAM:
      InvalidParamCrash();
      break;
    case ID_CLIENT_PURECALL:
      PureCallCrash();
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
    }
    break;
  case WM_PAINT:
    hdc = BeginPaint(hWnd, &ps);
    // TODO: Add any drawing code here...
    EndPaint(hWnd, &ps);
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg,
                       UINT message,
                       WPARAM wParam,
                       LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
  case WM_INITDIALOG:
    return (INT_PTR)TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}
