#pragma once

#include <optional>
#include <string>

// sign in storage for the launcher. everything is dpapi encrypted and kept in
// %APPDATA%/Envy, so a copied file is worthless on any other machine or user.
// the discord session file keeps the same layout the client used to write, so
// an existing sign in from before the launcher carries over.

struct DiscordSession {
    std::string userId;
    std::string username;
    std::string displayName;
    std::string accessToken;
};

inline std::wstring Widen(std::string const& s) { return std::wstring(s.begin(), s.end()); }
inline std::string Narrow(std::wstring const& s) { return std::string(s.begin(), s.end()); }

namespace authstore {
    // %APPDATA%/Envy, created on demand
    std::wstring EnvyDir();

    bool SaveDiscordSession(DiscordSession const& s);
    std::optional<DiscordSession> LoadDiscordSession();

    bool SaveProductKey(std::string const& key);
    std::optional<std::string> LoadProductKey();

    // wipes the saved sign in, used when the server says a key is dead
    void ClearAll();
}
