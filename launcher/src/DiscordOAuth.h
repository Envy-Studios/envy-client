#pragma once

#include <optional>
#include <string>

#include "AuthStore.h"

namespace oauth {
    constexpr wchar_t kDiscordClientId[] = L"1547724949639135302";
    constexpr int kOAuthPort = 8976;

    // check a bearer token with discord, fills error on failure
    std::optional<DiscordSession> VerifyToken(std::string const& token, std::wstring& error);

    // opens the browser and waits for the loopback callback; posts
    // WM_ENVY_AUTH_OK / WM_ENVY_AUTH_FAIL to hwnd when done
    void BeginSignIn(HWND hwnd);
}
