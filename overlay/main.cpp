#include <Windows.h>
#include <map>

using namespace std;

map<HWND, WNDPROC> oWndProcMap;

void inverse_paint(HWND hWnd) {
    PAINTSTRUCT ps;
    BeginPaint(hWnd, &ps);
    BitBlt(
        ps.hdc,
        ps.rcPaint.left,
        ps.rcPaint.top,
        ps.rcPaint.right - ps.rcPaint.left,
        ps.rcPaint.bottom - ps.rcPaint.top,
        ps.hdc,
        ps.rcPaint.left,
        ps.rcPaint.top,
        NOTSRCCOPY
    );
    EndPaint(hWnd, &ps);
}

LRESULT CALLBACK hWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg) {
    case WM_PAINT:
        inverse_paint(hwnd);
        break;
    }
    return CallWindowProc(
        oWndProcMap[hwnd],
        hwnd, uMsg, wParam, lParam
    );
    //oWndProcMap[hwnd](hwnd, uMsg, wParam, lParam);
}

void hook_wndproc(HWND hWnd) {
    auto oWndProc =
        (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)hWndProc);
    oWndProcMap[hWnd] = oWndProc;
    SetWindowPos(
        hWnd,
        NULL,
        0, 0, 0, 0,
        SWP_ASYNCWINDOWPOS |
        SWP_NOCOPYBITS |
        SWP_NOMOVE |
        SWP_NOSIZE |
        SWP_NOZORDER
    );
}

BOOL CALLBACK EnumFunc(HWND hWnd, LPARAM curPid) {
    DWORD wndPid = 0;
    GetWindowThreadProcessId(hWnd, &wndPid);
    if(wndPid == curPid) {
        hook_wndproc(hWnd);
    }
    return TRUE; // continue enumeration
}

DWORD WINAPI init(LPVOID param) {
    EnumWindows(EnumFunc, GetCurrentProcessId());
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    HANDLE hInitThread;
    switch(fdwReason) {
    case DLL_PROCESS_ATTACH:
        hInitThread = CreateThread(NULL, 0, init, NULL, 0, NULL);
        if(!hInitThread)
            return FALSE;
        CloseHandle(hInitThread);
        return TRUE;
        break;
    }
    return 0; //ignored when not DLL_PROCESS_ATTACH
}
