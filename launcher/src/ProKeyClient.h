#pragma once

#include <string>

struct ProKeyResult {
    enum Outcome {
        Allowed,      // the server recognized the key
        Denied,       // the server knows the key and said no
        NetworkError, // never got an answer, nothing was decided
    } outcome = Denied;

    std::wstring error; // human readable, only for Denied / NetworkError
};

// asks the key server at 73.152.37.82 whether the typed product key may in
ProKeyResult CheckProductKey(std::wstring const& rawKey);
