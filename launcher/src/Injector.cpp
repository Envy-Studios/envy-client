#include "Injector.h"

#include "AuthStore.h"

#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <filesystem>
#include <fstream>
#include <cwchar>

// the dll rides inside the exe as a raw binary blob, linked in with the same
// ld.exe trick the asset embedder uses. ld names the symbols after the file.
extern "C" char _binary_Envy_dll_start;
extern "C" char _binary_Envy_dll_end;

#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

namespace {

    constexpr wchar_t const* kMcProcessNames[] = {
        L"Minecraft.Windows.exe",
        L"MinecraftPreview.Windows.exe",
    };

    // stable store edition; the preview app is only tried as a fallback target
    constexpr wchar_t kMcAumid[] = L"shell:appsFolder\\Microsoft.MinecraftUWP_8wekyb3d8bbwe!App";
    constexpr wchar_t kMcProtocol[] = L"minecraft:";

    bool WriteEmbeddedDll(fs::path const& target) {
        char const* begin = &_binary_Envy_dll_start;
        size_t size = (size_t)(&_binary_Envy_dll_end - &_binary_Envy_dll_start);

        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);

        std::ofstream f(target, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(begin, (std::streamsize)size);
        f.close();
        return f.good();
    }

    DWORD FindProcess(wchar_t const* const* names, size_t count) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        DWORD pid = 0;
        if (Process32FirstW(snap, &pe)) {
            do {
                for (size_t i = 0; i < count && pid == 0; i++) {
                    if (_wcsicmp(pe.szExeFile, names[i]) == 0) pid = pe.th32ProcessID;
                }
            } while (pid == 0 && Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return pid;
    }

    void StartMinecraft() {
        HINSTANCE result = ShellExecuteW(nullptr, L"open", kMcAumid, nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32) {
            // fall back to the protocol handler if the aumid is rejected
            ShellExecuteW(nullptr, L"open", kMcProtocol, nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    struct WindowSeeker {
        DWORD pid = 0;
        HWND window = nullptr;
    };

    BOOL CALLBACK FindMainWindowProc(HWND hwnd, LPARAM lp) {
        auto* seek = (WindowSeeker*)lp;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != seek->pid || !IsWindowVisible(hwnd)) return TRUE;
        wchar_t title[64]{};
        GetWindowTextW(hwnd, title, 64);
        if (!title[0]) return TRUE;
        seek->window = hwnd;
        return FALSE;
    }

    // polls until the process owns a visible titled window, which means it is
    // past the fragile early boot where injecting would deadlock it
    bool WaitForGameWindow(DWORD pid) {
        for (int i = 0; i < 120; i++) {
            WindowSeeker seek{pid, nullptr};
            EnumWindows(FindMainWindowProc, (LPARAM)&seek);
            if (seek.window) return true;
            Sleep(500);
        }
        return false;
    }

    bool RivaTunerRunning() {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        bool found = false;
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"RTSS.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"MSIAfterburner.exe") == 0) {
                    found = true;
                    break;
                }
            } while (!found && Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return found;
    }

    void EnableDebugPrivilege() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) return;

        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid)) {
            AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        }
        CloseHandle(token);
    }

    // tri-state: 1 = loaded, 0 = not loaded, -1 = module list unavailable
    int CheckLoaded(DWORD pid) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (snap == INVALID_HANDLE_VALUE) return -1;

        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        int state = 0;
        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, L"Envy.dll") == 0) {
                    state = 1;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return state;
    }

    bool Inject(DWORD pid, fs::path const& dllPath, std::wstring& error) {
        EnableDebugPrivilege();

        HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                  FALSE, pid);
        if (!proc) {
            error = L"Couldn't open Minecraft's process, try running Envy as administrator.";
            return false;
        }

        // already loaded? LoadLibrary would just return the same module, but
        // skipping the dance keeps things quiet
        if (CheckLoaded(pid) == 1) {
            CloseHandle(proc);
            return true;
        }

        std::wstring path = dllPath.wstring();
        SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);

        LPVOID remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remote) {
            CloseHandle(proc);
            error = L"Couldn't allocate memory inside Minecraft.";
            return false;
        }

        if (!WriteProcessMemory(proc, remote, path.c_str(), bytes, nullptr)) {
            VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
            CloseHandle(proc);
            error = L"Couldn't write into Minecraft's memory.";
            return false;
        }

        auto loadLibraryW =
            (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
        if (!loadLibraryW) {
            VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
            CloseHandle(proc);
            error = L"Couldn't find LoadLibraryW, this windows build is too old.";
            return false;
        }

        HANDLE thread = CreateRemoteThread(proc, nullptr, 0, loadLibraryW, remote, 0, nullptr);
        if (!thread) {
            VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
            CloseHandle(proc);
            error = L"Couldn't start the injection thread, try running Envy as administrator.";
            return false;
        }

        WaitForSingleObject(thread, 20000);
        CloseHandle(thread);
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);

        // definitive check: is Envy.dll sitting in the target's module list now
        // (when the module list can't be read, trust the finished thread)
        if (CheckLoaded(pid) == 0) {
            error = L"Minecraft started but never loaded the client.";
            return false;
        }
        return true;
    }
} // namespace

LaunchOutcome ExtractAndInject(std::function<void(std::wstring const&)> const& status) {
    LaunchOutcome outcome;

    status(L"Preparing the client...");
    fs::path dllPath = fs::path(authstore::EnvyDir()) / L"Envy.dll";
    if (!WriteEmbeddedDll(dllPath) && !fs::exists(dllPath)) {
        outcome.error = L"Couldn't write the client dll to disk.";
        return outcome;
    }

    status(L"Looking for Minecraft...");
    DWORD pid = FindProcess(kMcProcessNames, 2);
    if (!pid) {
        status(L"Starting Minecraft...");
        StartMinecraft();

        // the store app can take a while to even show a splash, keep polling
        for (int i = 0; i < 240 && !pid; i++) {
            Sleep(500);
            pid = FindProcess(kMcProcessNames, 2);
            if (!pid && i % 20 == 19) {
                status(L"Still waiting for Minecraft to start...");
            }
        }
        if (!pid) {
            outcome.error = L"Minecraft never showed up, is Bedrock installed?";
            return outcome;
        }
    }

    // going in while the game is still booting deadlocks it on the loader
    // lock, so hold off until its window is up and boot has settled a little
    status(L"Waiting for Minecraft to come up...");
    bool ready = WaitForGameWindow(pid);
    Sleep(ready ? 3000 : 5000);

    status(L"Injecting Envy...");
    if (!Inject(pid, dllPath, outcome.error)) {
        return outcome;
    }

    outcome.ok = true;
    return outcome;
}
