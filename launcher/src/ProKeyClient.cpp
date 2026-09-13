#include "ProKeyClient.h"

#include "AuthStore.h"
#include "Http.h"

#include <nlohmann/json.hpp>

#include <cwctype>

namespace {
    constexpr wchar_t kKeyServerUrl[] = L"http://73.152.37.82:5000/api/prokey";

    // keys travel normalized: alphanumerics only, upper case. the server
    // normalizes the same way, so users can paste with or without dashes.
    std::wstring NormalizeKey(std::wstring const& raw) {
        std::wstring out;
        for (wchar_t c : raw) {
            if (std::iswalnum(c)) out += (wchar_t)std::towupper(c);
        }
        return out;
    }
} // namespace

ProKeyResult CheckProductKey(std::wstring const& rawKey) {
    ProKeyResult result;

    std::wstring key = NormalizeKey(rawKey);
    if (key.empty()) {
        result.outcome = ProKeyResult::Denied;
        result.error = L"Type a product key first.";
        return result;
    }

    nlohmann::json body;
    body["key"] = Narrow(key);

    HttpResult http = HttpSend(kKeyServerUrl, L"POST", L"Content-Type: application/json\r\n", body.dump());
    if (!http.ok) {
        result.outcome = ProKeyResult::NetworkError;
        result.error = http.error;
        return result;
    }

    if (http.status == 200) {
        try {
            auto json = nlohmann::json::parse(http.body);
            if (json.value("allowed", false)) {
                result.outcome = ProKeyResult::Allowed;
                return result;
            }
            result.outcome = ProKeyResult::Denied;
            result.error = L"That product key is not active.";
            return result;
        } catch (...) {
            result.outcome = ProKeyResult::NetworkError;
            result.error = L"The key server sent a broken answer.";
            return result;
        }
    }

    result.outcome = ProKeyResult::Denied;
    result.error = L"That product key is not active.";
    return result;
}
