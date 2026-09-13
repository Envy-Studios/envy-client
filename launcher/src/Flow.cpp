#include "Flow.h"

#include "AuthStore.h"
#include "DiscordOAuth.h"
#include "Injector.h"
#include "ProKeyClient.h"

#include <thread>

namespace {
    void PostText(HWND hwnd, UINT msg, std::wstring text) {
        auto* heap = new std::wstring(std::move(text));
        if (!PostMessageW(hwnd, msg, 0, (LPARAM)heap)) delete heap;
    }

    void PostStatus(HWND hwnd, std::wstring text) { PostText(hwnd, WM_ENVY_STATUS, std::move(text)); }
} // namespace

void flow::StartRestore(HWND hwnd) {
    std::thread([hwnd] {
        // discord session first (the old in-game sign in wrote the same file)
        if (auto saved = authstore::LoadDiscordSession()) {
            std::wstring error;
            if (auto session = oauth::VerifyToken(saved->accessToken, error)) {
                PostText(hwnd, WM_ENVY_AUTH_OK,
                         Widen(session->displayName.empty() ? session->username : session->displayName));
                return;
            }
            // dead token, forget it and fall through to the product key
            authstore::ClearAll();
        }

        if (auto savedKey = authstore::LoadProductKey()) {
            ProKeyResult r = CheckProductKey(Widen(*savedKey));
            if (r.outcome == ProKeyResult::Allowed) {
                PostText(hwnd, WM_ENVY_AUTH_OK, L"product key");
                return;
            }
            if (r.outcome == ProKeyResult::Denied) {
                authstore::ClearAll();
                PostText(hwnd, WM_ENVY_AUTH_FAIL, L"Your product key is no longer active.");
                return;
            }
            PostText(hwnd, WM_ENVY_AUTH_FAIL, L"Couldn't reach the key server, try again.");
            return;
        }

        // nothing saved yet, just show the sign in view
        PostText(hwnd, WM_ENVY_AUTH_FAIL, L"");
    }).detach();
}

void flow::StartDiscordSignIn(HWND hwnd) {
    oauth::BeginSignIn(hwnd);
}

void flow::StartKeyCheck(HWND hwnd, std::wstring key) {
    std::thread([hwnd, key = std::move(key)] {
        PostStatus(hwnd, L"Checking that key...");
        ProKeyResult r = CheckProductKey(key);
        if (r.outcome == ProKeyResult::Allowed) {
            authstore::SaveProductKey(Narrow(key));
            PostText(hwnd, WM_ENVY_AUTH_OK, L"product key");
        } else {
            PostText(hwnd, WM_ENVY_AUTH_FAIL, r.error.empty() ? std::wstring(L"That key is not active.")
                                                              : std::move(r.error));
        }
    }).detach();
}

void flow::StartLaunch(HWND hwnd) {
    std::thread([hwnd] {
        LaunchOutcome out = ExtractAndInject([hwnd](std::wstring const& s) { PostStatus(hwnd, s); });
        if (out.ok) {
            PostMessageW(hwnd, WM_ENVY_LAUNCH_OK, 0, 0);
        } else {
            PostText(hwnd, WM_ENVY_LAUNCH_FAIL, std::move(out.error));
        }
    }).detach();
}
