#include "pch.h"
#include "DiscordAuth.h"

#include "client/Envy.h"
#include "util/Logger.h"
#include "util/Util.h"

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Storage.Streams.h>
#include <nlohmann/json.hpp>

#include <wincrypt.h>
#include <shellapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cctype>
#include <iterator>
#include <random>
#include <sstream>

using namespace winrt;
using namespace winrt::Windows::Web::Http;

namespace {
    std::string randomHex(size_t length) {
        static std::mt19937_64 rng{std::random_device{}()};
        std::ostringstream out;
        while (out.str().size() < length) {
            out << std::hex << (rng() & 0xffffffffLL);
        }
        return out.str().substr(0, length);
    }

    std::string urlEncode(std::string const& value) {
        std::string out;
        char buf[4];
        for (unsigned char c : value) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                out += (char)c;
            } else {
                snprintf(buf, sizeof(buf), "%%%02X", c);
                out += buf;
            }
        }
        return out;
    }

    std::string queryParam(std::string const& query, std::string const& key) {
        size_t start = 0;
        while (start <= query.size()) {
            size_t amp = query.find('&', start);
            std::string pair =
                query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
            size_t eq = pair.find('=');
            if (eq != std::string::npos && pair.substr(0, eq) == key) {
                return pair.substr(eq + 1);
            }
            if (amp == std::string::npos) break;
            start = amp + 1;
        }
        return {};
    }

    bool ensureWinsock() {
        static bool ok = [] {
            WSADATA data{};
            return WSAStartup(MAKEWORD(2, 2), &data) == 0;
        }();
        return ok;
    }

    void sendHttpResponse(SOCKET client, std::string const& body) {
        std::string response =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n" + body;
        send(client, response.c_str(), (int)response.size(), 0);
        shutdown(client, SD_SEND);
    }

    char const* callbackPageStyle =
        "<html><head><meta charset='utf-8'></head><body style='background:#1e1f22;color:#dbdee1;"
        "font-family:'Segoe UI',sans-serif;display:flex;align-items:center;justify-content:center;"
        "height:100vh;margin:0'><div style='text-align:center'>";

    char const* callbackPageEnd = "</div></body></html>";
} // namespace

DiscordAuth& DiscordAuth::get() {
    static DiscordAuth instance;
    return instance;
}

void DiscordAuth::startRestore() {
    if (signedIn.load(std::memory_order_acquire)) return;

    auto stored = readStoredSession();
    if (!stored) {
        state.store(State::Idle, std::memory_order_release);
        return;
    }

    state.store(State::Restoring, std::memory_order_release);
    std::thread([this, s = std::move(*stored)] { verifyToken(s.accessToken, true); }).detach();
}

void DiscordAuth::beginSignIn() {
    if (signedIn.load(std::memory_order_acquire)) return;

    std::wstring clientId = kDiscordClientId;
    if (clientId == L"REPLACE_WITH_CLIENT_ID") {
        finish(false, L"Discord sign in is not set up in this build yet (missing client id).");
        return;
    }

    cancelSignIn();
    listenerStop.store(false, std::memory_order_release);

    std::string oauthState = randomHex(16);
    state.store(State::BrowserWaiting, std::memory_order_release);
    std::thread([this, st = oauthState] { listenerLoop(st); }).detach();

    std::wstring url =
        L"https://discord.com/oauth2/authorize?client_id=" + clientId + L"&response_type=token&redirect_uri=" +
        util::StrToWStr(urlEncode("http://127.0.0.1:" + std::to_string(kOAuthPort) + "/callback")) +
        L"&scope=identify&state=" + util::StrToWStr(oauthState);
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void DiscordAuth::cancelSignIn() {
    listenerStop.store(true, std::memory_order_release);
    if (state.load(std::memory_order_acquire) == State::BrowserWaiting) {
        state.store(State::Idle, std::memory_order_release);
    }
}

void DiscordAuth::listenerLoop(std::string expectedState) {
    if (!ensureWinsock()) {
        finish(false, L"Networking failed to start.");
        return;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        finish(false, L"Could not create the local listener.");
        return;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons((u_short)kOAuthPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server, (sockaddr const*)&address, sizeof(address)) == SOCKET_ERROR ||
        listen(server, 1) == SOCKET_ERROR) {
        closesocket(server);
        finish(false, L"Local sign in port is busy, try again.");
        return;
    }

    auto deadline = std::chrono::steady_clock::now() + 5min;

    while (!listenerStop.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        timeval wait{0, 400 * 1000};
        if (select(0, &fds, nullptr, nullptr, &wait) <= 0) continue;

        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        char raw[8192]{};
        int received = recv(client, raw, sizeof(raw) - 1, 0);
        if (received <= 0) {
            closesocket(client);
            continue;
        }
        std::string request(raw, received);

        bool hasToken = false;
        std::string token;

        if (request.rfind("GET /token?", 0) == 0) {
            // the callback page relays the url fragment back to us, fragments
            // are stripped by the browser and never reach a server directly
            size_t lineEnd = request.find(' ', 11);
            std::string query =
                request.substr(11, lineEnd == std::string::npos ? std::string::npos : lineEnd - 11);
            token = queryParam(query, "access_token");
            std::string returnedState = queryParam(query, "state");
            hasToken = !token.empty() && returnedState == expectedState;

            std::string page = callbackPageStyle;
            page += "<h2>";
            page += hasToken ? "Signed in." : "Sign in failed, start again from the game.";
            page += "</h2><p>You can close this tab.</p>";
            page += callbackPageEnd;
            sendHttpResponse(client, page);
        } else {
            // first hop, discord redirected here with the token in the fragment
            std::string page = callbackPageStyle;
            page += "<h2>Signing you in...</h2>";
            page += "<script>fetch('/token?' + location.hash.slice(1));</script>";
            page += callbackPageEnd;
            sendHttpResponse(client, page);
        }

        closesocket(client);

        if (hasToken) {
            listenerStop.store(true, std::memory_order_release);
            closesocket(server);
            verifyToken(token, false);
            return;
        }
    }

    closesocket(server);
    if (!listenerStop.load(std::memory_order_acquire) &&
        state.load(std::memory_order_acquire) == State::BrowserWaiting) {
        finish(false, L"Sign in timed out, try again.");
    }
}

void DiscordAuth::verifyToken(std::string const& token, bool fromStorage) {
    state.store(State::Verifying, std::memory_order_release);

    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {
        // already initialized on this thread with a different mode, fine
    }

    try {
        HttpRequestMessage request(
            HttpMethod::Get(), winrt::Windows::Foundation::Uri(L"https://discord.com/api/v10/users/@me"));
        request.Headers().Authorization(
            winrt::Windows::Web::Http::Headers::HttpCredentialsHeaderValue(L"Bearer", util::StrToWStr(token)));

        HttpClient http;
        auto response = http.SendRequestAsync(request).get();
        int status = (int)response.StatusCode();

        if (status == 401) {
            // token is dead, a stored one just falls back to the sign in box
            if (fromStorage) {
                std::filesystem::remove(sessionPath());
                state.store(State::Idle, std::memory_order_release);
            } else {
                finish(false, L"Discord rejected the sign in, try again.");
            }
            return;
        }
        if (status != 200) {
            finish(false, L"Discord returned an error (HTTP " + util::StrToWStr(std::to_string(status)) + L").");
            return;
        }

        auto buffer = response.Content().ReadAsBufferAsync().get();
        std::string body(reinterpret_cast<char const*>(buffer.data()), buffer.Length());
        auto json = nlohmann::json::parse(body);

        Session s;
        s.accessToken = token;
        s.userId = json.value("id", "");
        s.username = json.value("username", "");
        s.displayName = json.value("global_name", "");
        if (s.displayName.empty()) s.displayName = s.username;

        if (s.userId.empty() || s.username.empty()) {
            finish(false, L"Unexpected response from Discord.");
            return;
        }

        {
            std::lock_guard lock(sessionMutex);
            currentSession = s;
        }
        if (!fromStorage) writeSession(s);

        Logger::Info("Signed in as {} ({})", s.username, s.userId);
        finish(true, {});
    } catch (std::exception const& err) {
        if (fromStorage) {
            Logger::Warn("Could not validate the stored session: {}", err.what());
            state.store(State::Idle, std::memory_order_release);
        } else {
            finish(false, util::StrToWStr(std::string("Discord request failed: ") + err.what()));
        }
    }
}

void DiscordAuth::finish(bool ok, std::wstring error) {
    if (ok) {
        signedIn.store(true, std::memory_order_release);
        state.store(State::SignedIn, std::memory_order_release);
    } else {
        {
            std::lock_guard lock(sessionMutex);
            lastError = std::move(error);
        }
        state.store(State::Failed, std::memory_order_release);
    }
}

std::wstring DiscordAuth::getError() const {
    std::lock_guard lock(sessionMutex);
    return lastError;
}

DiscordAuth::Session DiscordAuth::session() const {
    std::lock_guard lock(sessionMutex);
    return currentSession;
}

std::filesystem::path DiscordAuth::sessionPath() {
    return util::GetEnvyPath() / L"session.bin";
}

bool DiscordAuth::writeSession(Session const& s) {
    std::string blob = s.userId + "\n" + s.username + "\n" + s.displayName + "\n" + s.accessToken;

    DATA_BLOB in{};
    in.pbData = (BYTE*)blob.data();
    in.cbData = (DWORD)blob.size();
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"Envy session", nullptr, 0, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        Logger::Warn("CryptProtectData failed, error {}", (unsigned long)GetLastError());
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(sessionPath().parent_path(), ec);
    std::ofstream file(sessionPath(), std::ios::binary);
    file.write((char const*)out.pbData, out.cbData);
    LocalFree(out.pbData);
    return file.good();
}

std::optional<DiscordAuth::Session> DiscordAuth::readStoredSession() {
    std::ifstream file(sessionPath(), std::ios::binary);
    if (!file) return std::nullopt;

    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.empty()) return std::nullopt;

    DATA_BLOB in{};
    in.pbData = (BYTE*)data.data();
    in.cbData = (DWORD)data.size();
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, 0, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        // encrypted for a different user or machine, as good as no session
        return std::nullopt;
    }
    std::string blob((char const*)out.pbData, out.cbData);
    LocalFree(out.pbData);

    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t nl = blob.find('\n', start);
        parts.push_back(blob.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    if (parts.size() != 4 || parts[0].empty() || parts[3].empty()) return std::nullopt;

    return Session{parts[0], parts[1], parts[2], parts[3]};
}
