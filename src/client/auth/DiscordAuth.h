#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

// discord sign in + session storage.
//
// the client never sees the discord password, the browser runs the whole
// oauth handshake and hands a bearer token to a small loopback listener.
// the token is only ever stored dpapi encrypted (per windows user, bound to
// the machine), so a copied session file is worthless anywhere else.

constexpr wchar_t kDiscordClientId[] = L"1547724949639135302";
constexpr int kOAuthPort = 8976;

class DiscordAuth {
public:
    struct Session {
        std::string userId;
        std::string username;
        std::string displayName;
        std::string accessToken;
    };

    enum class State {
        Idle,           // waiting for the user to press the sign in button
        Restoring,      // checking the saved session
        BrowserWaiting, // browser is open, loopback listener is running
        Verifying,      // checking a token with discord
        SignedIn,
        Failed,
    };

    static DiscordAuth& get();

    DiscordAuth(DiscordAuth const&) = delete;
    DiscordAuth& operator=(DiscordAuth const&) = delete;

    // check whether a stored session is still good, runs in the background
    void startRestore();
    // open the browser and wait for the oauth callback
    void beginSignIn();
    void cancelSignIn();

    [[nodiscard]] bool isSignedIn() const { return signedIn.load(std::memory_order_acquire); }
    [[nodiscard]] State getState() const { return state.load(std::memory_order_acquire); }
    [[nodiscard]] std::wstring getError() const;
    [[nodiscard]] Session session() const;

private:
    DiscordAuth() = default;

    void listenerLoop(std::string expectedState);
    void verifyToken(std::string const& token, bool fromStorage);
    void finish(bool ok, std::wstring error);

    [[nodiscard]] static std::filesystem::path sessionPath();
    [[nodiscard]] static std::optional<Session> readStoredSession();
    [[nodiscard]] static bool writeSession(Session const& s);

    std::atomic<State> state{State::Idle};
    std::atomic_bool signedIn{false};
    std::atomic_bool listenerStop{false};
    mutable std::mutex sessionMutex;
    Session currentSession;
    std::wstring lastError;
};
