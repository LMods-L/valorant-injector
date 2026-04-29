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
    // Find a visible window that belongs to the target process
    if (pid == args->targetPID && IsWindowVisible(hwnd)) {
        args->resultTID = tid;
        return FALSE; // stop enumerating
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

    cout << "[?] enter target process name (e.g. game.exe): ";
    wstring processName;
    std::getline(std::wcin, processName);
    if (processName.empty()) {
        cout << "[-] error: no process name entered" << endl;
        system("pause");
        return EXIT_FAILURE;
    }

    cout << "[+] locating target process..." << endl;
    DWORD pid = GetProcessIdByName(processName);
    if (!pid) {
        cout << "[-] error: target process not found" << endl;
        system("pause");
        return EXIT_FAILURE;
    }

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
    cout << "[>] press any key to unhook" << endl;
    system("pause > nul");

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
