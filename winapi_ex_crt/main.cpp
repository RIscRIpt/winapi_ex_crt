#include <Windows.h>
#include <TlHelp32.h>
#include <tchar.h>

#include <iostream>

#if defined(UNICODE) || defined(_UNICODE)
#define tcout std::wcout
#else
#define tcout std::cout
#endif

using namespace std;

enum ARG {
    PROGRAM_NAME,
    PROCESS_NAME,
    DLL_NAME,
    COUNT,
};

enum ERR {
    USER_DEFINED = 0x20000000,

    NOT_ENOUGH_ARGUMENTS,

    FAIL_OpenProcess,
    FAIL_VirtualAllocEx,
    FAIL_WriteProcessMemory,
    FAIL_CreateRemoteThread,
    FAIL_WaitForSingleObject,
};

void print_error(DWORD error) {
    if(error >= ERR::USER_DEFINED)
        return;

    TCHAR *pBuffer = NULL;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        0,
        (LPTSTR)&pBuffer,
        0,
        NULL
    );

    tcout << _T("ERROR: ")
        << error
        << " :: "
        << pBuffer
        << endl;

    LocalFree(pBuffer);
}

void abort_with_last_error() {
    DWORD error = GetLastError();
    print_error(error);
    exit(error);
}

DWORD get_pid_by_name(LPCTSTR process_name) {
    HANDLE hSnapshot =
        CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(hSnapshot == INVALID_HANDLE_VALUE)
        return 0;

    DWORD pid = 0;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if(Process32First(hSnapshot, &pe)) {
        do {
            if(_tcsstr(pe.szExeFile, process_name) != NULL) {
                pid = pe.th32ProcessID;
                break;
            }
        } while(Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return pid;
}

DWORD inject(DWORD pid, LPCTSTR dll_name) {
    DWORD error;

    HANDLE hProcess =
        OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if(!hProcess) {
        return ERR::FAIL_OpenProcess;
    }

    auto addr_load_lib =
        (LPTHREAD_START_ROUTINE)LoadLibrary;

    auto dll_name_size = (_tcslen(dll_name) + 1) * sizeof(TCHAR);

    auto addr_dll_name = VirtualAllocEx(
        hProcess,                 /* hProcess         */
        NULL,                     /* lpAddress        */
        dll_name_size,            /* dwSize           */
        MEM_COMMIT | MEM_RESERVE, /* flAllocationType */
        PAGE_READWRITE            /* flProtect        */
    );
    if(!addr_dll_name) {
        error = ERR::FAIL_VirtualAllocEx;
        goto close_process;
    }

    DWORD written;
    BOOL wpm = WriteProcessMemory(
        hProcess,      /* hProcess                */
        addr_dll_name, /* lpBaseAddress           */
        dll_name,      /* lpBuffer                */
        dll_name_size, /* nSize                   */
        &written       /* *lpNumberOfBytesWritten */
    );
    if(!wpm || written != dll_name_size) {
        error = ERR::FAIL_WriteProcessMemory;
        goto free_memory;
    }

    HANDLE hThread = CreateRemoteThread(
        hProcess,      /* hProcess           */
        NULL,          /* lpThreadAttributes */
        0,             /* dwStackSize        */
        addr_load_lib, /* lpStartAddress     */
        addr_dll_name, /* lpParameter        */
        0,             /* dwCreationFlags    */
        0              /* lpThreadId         */
    );
    if(!hThread) {
        error = ERR::FAIL_CreateRemoteThread;
        goto free_memory;
    }

    DWORD wso = WaitForSingleObject(hThread, INFINITE);
    if(wso != WAIT_OBJECT_0) {
        error = ERR::FAIL_WaitForSingleObject;
        goto close_thread;
    }

    error = 0;

close_thread:
    CloseHandle(hThread);
free_memory:
    VirtualFreeEx(hProcess, addr_dll_name, 0, MEM_DECOMMIT);
close_process:
    CloseHandle(hProcess);

    return error;
}

int _tmain(int argc, TCHAR *argv[]) {
    if(argc != ARG::COUNT) {
        tcout << _T("Usage: ")
            << argv[0]
            << _T(" process.exe library.dll")
            << endl;
        return ERR::NOT_ENOUGH_ARGUMENTS;
    }

    auto process_name = argv[ARG::PROCESS_NAME];
    auto dll_name = argv[ARG::DLL_NAME];

    auto pid = get_pid_by_name(process_name);
    if(pid == 0)
        abort_with_last_error();

    auto error = inject(pid, dll_name);
    if(error)
        abort_with_last_error();

    return 0;
}
