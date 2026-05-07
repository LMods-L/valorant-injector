/*
 * This code is based on the original work from:
 * https://github.com/DrNseven/SetWindowsHookEx-Injector
 *
 * Significant modifications and improvements include:
 * - Complete recoding of the original implementation
 * - Added automatic export function extraction
 * - Various optimizations and code refinements
 *
 * Modified and enhanced by: celebred
 * UC Thread: https://www.unknowncheats.me/forum/valorant/702198-hookloader-fork.html
 */

#include "utils.hpp"
#include <TlHelp32.h>

DWORD GetProcessIdByName(const std::wstring& processName) {
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (processName == pe32.szExeFile) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return 0;
}

struct EnumWindowsCallbackArgs {
    DWORD targetPID;
    DWORD resultTID;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumWindowsCallbackArgs* args = (EnumWindowsCallbackArgs*)lParam;
    DWORD pid = 0;
    DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
    
    // Find a visible window that belongs to the target process and has a title
    if (pid == args->targetPID && IsWindowVisible(hwnd)) {
        char windowTitle[256];
        GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
        
        if (strlen(windowTitle) > 0) {
            args->resultTID = tid;
            std::cout << "[+] found game window: " << windowTitle << " (TID: " << tid << ")" << std::endl;
            return FALSE; // stop enumerating
        }
    }
    return TRUE; // continue enumerating
}

DWORD GetThreadIdByProcessId(DWORD pid) {
    EnumWindowsCallbackArgs args = { pid, 0 };
    EnumWindows(EnumWindowsProc, (LPARAM)&args);
    
    // If no visible window found, try enumerating again without IsWindowVisible check
    if (args.resultTID == 0) {
        auto fallbackProc = [](HWND hwnd, LPARAM lParam) -> BOOL {
            EnumWindowsCallbackArgs* a = (EnumWindowsCallbackArgs*)lParam;
            DWORD p = 0;
            DWORD t = GetWindowThreadProcessId(hwnd, &p);
            if (p == a->targetPID) {
                a->resultTID = t;
                return FALSE;
            }
            return TRUE;
        };
        EnumWindows(fallbackProc, (LPARAM)&args);
    }
    
    return args.resultTID;
}

int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
    CustomizeConsoleWindow();

    cout << "[+] please select ur dll..." << endl;
    wstring dllPath = SelectDLL();
    if (dllPath.empty()) {
        cout << "[-] error: no dll selected" << endl;
        system("pause");
        return EXIT_FAILURE;
    }

    cout << "[+] searching for exported functions..." << endl;
    wstring functionName = GetExportedFunctionName(dllPath);
    if (functionName.empty()) {
        cout << "[-] error: no valid export function found" << endl;
        system("pause");
        return EXIT_FAILURE;
    }
    cout << "[+] found export: ";
    wcout << functionName << endl;

    cout << "[?] enter target process name [HTGame.exe]: ";
    wstring processName;
    std::getline(std::wcin, processName);
    if (processName.empty()) {
        processName = L"HTGame.exe";
    }

    wcout << L"[+] waiting for " << processName << L" to be launched..." << endl;
    DWORD pid = 0;
    while (!(pid = GetProcessIdByName(processName))) {
        Sleep(1000);
    }
    cout << "[+] process found (PID: " << pid << ")" << endl;

    cout << "[+] locating target thread..." << endl;
    DWORD tid = GetThreadIdByProcessId(pid);
    if (!tid) {
        cout << "[-] error: failed to get thread id" << endl;
        system("pause");
        return EXIT_FAILURE;
    }

    if (!SetupHook(dllPath, functionName, tid)) {
        system("pause");
        return EXIT_FAILURE;
    }

    cout << "[+] hook active (function: ";
    wcout << functionName;
    cout << ")" << endl;
    cout << "[>] press END key to unhook" << endl;
    
    while (true) {
        // Cek jika tombol END (VK_END) ditekan
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            break;
        }
        Sleep(100); // Cegah CPU 100% usage
    }

    cout << "[+] removing hook..." << endl;
    if (!RemoveHook()) {
        cout << "[-] error: failed to remove hook" << endl;
        system("pause");
        return EXIT_FAILURE;
    }

    cout << "[+] operation completed" << endl;
    cout << "[>] press any key to exit" << endl;
    system("pause > nul");
    return EXIT_SUCCESS;
}
