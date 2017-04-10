#include <Windows.h>
#include <map>

#include <pointer.h>

using namespace std;
using namespace mmgr;

typedef BOOL(WINAPI *tEndPaint)(HWND, const PAINTSTRUCT*);

tEndPaint oEndPaint;
tEndPaint *pEndPaint;

BOOL WINAPI hEndPaint(HWND hWnd, const PAINTSTRUCT *lpPaint) {
    const PAINTSTRUCT &ps = *lpPaint;
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
    return oEndPaint(hWnd, lpPaint);
}

DWORD WINAPI init(LPVOID param) {
    DWORD old_prot;

    pointer p = EndPaint;
    p += 2;
    p = *p;

    pEndPaint = p;
    p.protect(sizeof(tEndPaint), PAGE_READWRITE, &old_prot);
    oEndPaint = *pEndPaint;
    *pEndPaint = hEndPaint;
    p.protect(sizeof(tEndPaint), old_prot);
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
