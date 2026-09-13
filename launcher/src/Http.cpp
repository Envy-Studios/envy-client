#include "Http.h"

#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

HttpResult HttpSend(std::wstring const& url, std::wstring const& method,
                    std::wstring const& headers, std::string const& body) {
    HttpResult result;

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256]{};
    wchar_t path[2048]{};
    wchar_t extra[1024]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 255;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2047;
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = 1023;

    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &parts)) {
        result.error = L"Could not parse the server url.";
        return result;
    }

    HINTERNET session = WinHttpOpen(L"Envy/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        result.error = L"Could not start networking.";
        return result;
    }
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 30000);

    HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        result.error = L"Could not reach the server.";
        return result;
    }

    std::wstring obj = path;
    obj += extra;
    DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, method.c_str(), obj.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        result.error = L"Could not build the request.";
        return result;
    }

    std::wstring allHeaders = headers;
    if (!body.empty() && allHeaders.find(L"Content-Type") == std::wstring::npos) {
        if (!allHeaders.empty() && allHeaders.back() != L'\n') allHeaders += L"\r\n";
        allHeaders += L"Content-Type: application/json\r\n";
    }

    BOOL sent = WinHttpSendRequest(request,
                                   allHeaders.empty() ? nullptr : allHeaders.c_str(),
                                   (DWORD)allHeaders.size(),
                                   body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
                                   (DWORD)body.size(),
                                   (DWORD)body.size(),
                                   nullptr);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        result.error = L"Connection failed, check your internet.";
        return result;
    }

    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    result.status = (long)status;

    std::string data;
    char buf[16384];
    DWORD read = 0;
    while (WinHttpReadData(request, buf, sizeof(buf), &read) && read > 0) {
        data.append(buf, read);
    }
    result.body = std::move(data);
    result.ok = true;

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
}
