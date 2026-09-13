#pragma once

#include <windows.h>

#include <string>

// worker threads post these back to the window. every pointer payload is a
// heap std::wstring the ui thread owns (and deletes) once received.

constexpr UINT WM_ENVY_STATUS      = WM_APP + 1; // lp = std::wstring*  neutral step text
constexpr UINT WM_ENVY_AUTH_OK     = WM_APP + 2; // lp = std::wstring*  display name
constexpr UINT WM_ENVY_AUTH_FAIL   = WM_APP + 3; // lp = std::wstring*  error ("" = just show login)
constexpr UINT WM_ENVY_LAUNCH_OK   = WM_APP + 4;
constexpr UINT WM_ENVY_LAUNCH_FAIL = WM_APP + 5; // lp = std::wstring*  error

namespace flow {
    // check the saved discord session / product key on startup
    void StartRestore(HWND hwnd);
    // open the browser for the discord oauth handshake
    void StartDiscordSignIn(HWND hwnd);
    // ask the key server whether a product key may in
    void StartKeyCheck(HWND hwnd, std::wstring key);
    // start minecraft if needed and inject the client
    void StartLaunch(HWND hwnd);
}
