#include "DiscordOAuth.h"

#include "Flow.h"
#include "Http.h"

#include <nlohmann/json.hpp>

#include <shellapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <chrono>
#include <cctype>
#include <random>
#include <sstream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

namespace {
    std::atomic_bool g_listenerStop{false};
    std::atomic<SOCKET> g_listenerSocket{INVALID_SOCKET};

    // stops a pending listener and closes its socket right away, so a new sign
    // in attempt can bind the same port immediately
    void cancelAndWait() {
        g_listenerStop.store(true, std::memory_order_release);
        SOCKET old = g_listenerSocket.exchange(INVALID_SOCKET, std::memory_order_acq_rel);
        if (old != INVALID_SOCKET) closesocket(old);
    }

    void PostAuthOk(HWND hwnd, std::wstring name) {
        auto* heap = new std::wstring(std::move(name));
        if (!PostMessageW(hwnd, WM_ENVY_AUTH_OK, 0, (LPARAM)heap)) delete heap;
    }

    void PostAuthFail(HWND hwnd, std::wstring error) {
        auto* heap = new std::wstring(std::move(error));
        if (!PostMessageW(hwnd, WM_ENVY_AUTH_FAIL, 0, (LPARAM)heap)) delete heap;
    }

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

    void listenerLoop(HWND hwnd, std::string expectedState) {
        if (!ensureWinsock()) {
            PostAuthFail(hwnd, L"Networking failed to start.");
            return;
        }

        SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server == INVALID_SOCKET) {
            PostAuthFail(hwnd, L"Could not create the local listener.");
            return;
        }
        g_listenerSocket.store(server, std::memory_order_release);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons((u_short)oauth::kOAuthPort);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (bind(server, (sockaddr const*)&address, sizeof(address)) == SOCKET_ERROR ||
            listen(server, 1) == SOCKET_ERROR) {
            g_listenerSocket.exchange(INVALID_SOCKET);
            closesocket(server);
            PostAuthFail(hwnd, L"The local sign in port is busy, try again.");
            return;
        }

        auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);

        while (!g_listenerStop.load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() < deadline) {
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
                page += hasToken ? "Signed in." : "Sign in failed, start again from the launcher.";
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
                g_listenerStop.store(true, std::memory_order_release);
                if (g_listenerSocket.exchange(INVALID_SOCKET) == server) closesocket(server);

                std::wstring error;
                auto session = oauth::VerifyToken(token, error);
                if (session) {
                    authstore::SaveDiscordSession(*session);
                    std::wstring name = Widen(session->displayName.empty() ? session->username
                                                                           : session->displayName);
                    PostAuthOk(hwnd, std::move(name));
                } else {
                    PostAuthFail(hwnd, error.empty() ? std::wstring(L"Discord rejected the sign in.")
                                                     : std::move(error));
                }
                return;
            }
        }

        if (g_listenerSocket.exchange(INVALID_SOCKET) == server) closesocket(server);
        if (!g_listenerStop.load(std::memory_order_acquire)) {
            PostAuthFail(hwnd, L"Sign in timed out, try again.");
        }
    }
} // namespace

namespace oauth {

    std::optional<DiscordSession> VerifyToken(std::string const& token, std::wstring& error) {
        std::wstring wtoken(token.begin(), token.end());
        HttpResult http = HttpSend(L"https://discord.com/api/v10/users/@me", L"GET",
                                   L"Authorization: Bearer " + wtoken + L"\r\n", "");
        if (!http.ok) {
            error = http.error;
            return std::nullopt;
        }
        if (http.status == 401) {
            error = L"token dead";
            return std::nullopt;
        }
        if (http.status != 200) {
            error = L"Discord returned an error (HTTP " + std::to_wstring(http.status) + L").";
            return std::nullopt;
        }

        try {
            auto json = nlohmann::json::parse(http.body);
            DiscordSession s;
            s.accessToken = token;
            s.userId = json.value("id", "");
            s.username = json.value("username", "");
            s.displayName = json.value("global_name", "");
            if (s.displayName.empty()) s.displayName = s.username;
            if (s.userId.empty() || s.username.empty()) {
                error = L"Unexpected response from Discord.";
                return std::nullopt;
            }
            return s;
        } catch (...) {
            error = L"Unexpected response from Discord.";
            return std::nullopt;
        }
    }

    void BeginSignIn(HWND hwnd) {
        cancelAndWait();

        std::string oauthState = randomHex(16);
        std::thread([hwnd, st = oauthState] { listenerLoop(hwnd, st); }).detach();

        std::wstring url =
            L"https://discord.com/oauth2/authorize?client_id=" + std::wstring(kDiscordClientId) +
            L"&response_type=token&redirect_uri=" +
            Widen(urlEncode("http://127.0.0.1:" + std::to_string(kOAuthPort) + "/callback")) +
            L"&scope=identify&state=" + Widen(oauthState);
        ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}
