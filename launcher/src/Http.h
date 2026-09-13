#pragma once

#include <string>

// one synchronous http request over winhttp. good enough for the couple of
// calls the launcher makes, keeps winrt out of the exe entirely.

struct HttpResult {
    bool ok = false;        // we got an http response back
    long status = 0;        // http status code
    std::string body;
    std::wstring error;     // only set when ok == false
};

HttpResult HttpSend(std::wstring const& url,
                    std::wstring const& method,
                    std::wstring const& headers,
                    std::string const& body);
