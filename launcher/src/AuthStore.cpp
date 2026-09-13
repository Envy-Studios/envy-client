#include "AuthStore.h"

#include <windows.h>
#include <wincrypt.h>
#include <shlobj.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

namespace {
    fs::path BaseDir() {
        PWSTR raw = nullptr;
        if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw) != S_OK) {
            return {};
        }
        fs::path dir(raw);
        CoTaskMemFree(raw);
        return dir / L"Envy";
    }

    fs::path SessionPath() { return BaseDir() / L"session.bin"; }
    fs::path KeyPath() { return BaseDir() / L"key.bin"; }

    bool Protect(std::string const& plain, std::string& blobOut) {
        DATA_BLOB in{};
        in.pbData = (BYTE*)plain.data();
        in.cbData = (DWORD)plain.size();
        DATA_BLOB out{};
        if (!CryptProtectData(&in, L"Envy launcher session", nullptr, 0, nullptr,
                              CRYPTPROTECT_UI_FORBIDDEN, &out)) {
            return false;
        }
        blobOut.assign((char const*)out.pbData, out.cbData);
        LocalFree(out.pbData);
        return true;
    }

    bool Unprotect(std::string const& blob, std::string& plainOut) {
        DATA_BLOB in{};
        in.pbData = (BYTE*)blob.data();
        in.cbData = (DWORD)blob.size();
        DATA_BLOB out{};
        if (!CryptUnprotectData(&in, nullptr, nullptr, 0, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
            // encrypted for a different user or machine, as good as nothing
            return false;
        }
        plainOut.assign((char const*)out.pbData, out.cbData);
        LocalFree(out.pbData);
        return true;
    }

    bool WriteFileBytes(fs::path const& p, std::string const& data) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(data.data(), (std::streamsize)data.size());
        f.close();
        return f.good();
    }

    std::optional<std::string> ReadFileBytes(fs::path const& p) {
        std::ifstream f(p, std::ios::binary);
        if (!f) return std::nullopt;
        std::ostringstream ss;
        ss << f.rdbuf();
        std::string s = ss.str();
        if (s.empty()) return std::nullopt;
        return s;
    }

    std::vector<std::string> SplitLines(std::string const& blob) {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t nl = blob.find('\n', start);
            parts.push_back(blob.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        return parts;
    }
} // namespace

std::wstring authstore::EnvyDir() {
    fs::path dir = BaseDir();
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.wstring();
}

bool authstore::SaveDiscordSession(DiscordSession const& s) {
    std::string blob = s.userId + "\n" + s.username + "\n" + s.displayName + "\n" + s.accessToken;
    std::string encrypted;
    if (!Protect(blob, encrypted)) return false;
    return WriteFileBytes(SessionPath(), encrypted);
}

std::optional<DiscordSession> authstore::LoadDiscordSession() {
    auto data = ReadFileBytes(SessionPath());
    if (!data) return std::nullopt;

    std::string plain;
    if (!Unprotect(*data, plain)) return std::nullopt;

    auto parts = SplitLines(plain);
    if (parts.size() != 4 || parts[0].empty() || parts[3].empty()) return std::nullopt;
    return DiscordSession{parts[0], parts[1], parts[2], parts[3]};
}

bool authstore::SaveProductKey(std::string const& key) {
    std::string encrypted;
    if (!Protect("prokey\n" + key, encrypted)) return false;
    return WriteFileBytes(KeyPath(), encrypted);
}

std::optional<std::string> authstore::LoadProductKey() {
    auto data = ReadFileBytes(KeyPath());
    if (!data) return std::nullopt;

    std::string plain;
    if (!Unprotect(*data, plain)) return std::nullopt;

    auto parts = SplitLines(plain);
    if (parts.size() != 2 || parts[0] != "prokey" || parts[1].empty()) return std::nullopt;
    return parts[1];
}

void authstore::ClearAll() {
    std::error_code ec;
    fs::remove(SessionPath(), ec);
    fs::remove(KeyPath(), ec);
}
