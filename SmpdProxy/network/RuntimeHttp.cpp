#include "pch.h"
#include "network/RuntimeHttp.h"

#include <winhttp.h>

#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace smpd::http {
namespace {

constexpr std::size_t kMaxBodyBytes = 8u * 1024u * 1024u;

struct Handle {
    HINTERNET h = nullptr;
    explicit Handle(HINTERNET v = nullptr) : h(v) {}
    ~Handle() { if (h) WinHttpCloseHandle(h); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return h != nullptr; }
};

std::wstring Widen(std::string_view s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

}  // namespace

Response GetUnchecked(std::string_view url, int timeoutSec) {
    Response out;
    DWORD ms = timeoutSec > 0 ? (DWORD)timeoutSec * 1000u : 12000u;

    std::string_view rest = url;
    bool https = true;
    if (rest.size() >= 8 && _strnicmp(rest.data(), "https://", 8) == 0) {
        rest.remove_prefix(8);
        https = true;
    } else if (rest.size() >= 7 && _strnicmp(rest.data(), "http://", 7) == 0) {
        rest.remove_prefix(7);
        https = false;
    } else {
        return out;
    }
    size_t slash = rest.find('/');
    std::string_view host = (slash == std::string_view::npos) ? rest : rest.substr(0, slash);
    std::string_view path = (slash == std::string_view::npos) ? "/" : rest.substr(slash);
    if (host.empty()) return out;

    std::wstring wHost = Widen(host);
    std::wstring wPath = Widen(path);
    INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

    Handle session(WinHttpOpen(L"SMPD-Bridge/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) return out;
    WinHttpSetTimeouts(session.h, ms, ms, ms, ms);

    Handle conn(WinHttpConnect(session.h, wHost.c_str(), port, 0));
    if (!conn) return out;

    Handle req(WinHttpOpenRequest(conn.h, L"GET", wPath.c_str(), nullptr, WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  https ? WINHTTP_FLAG_SECURE : 0));
    if (!req) return out;

    if (!WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(req.h, nullptr)) {
        return out;
    }

    DWORD status = 0, sz = sizeof(status);
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);

    std::string body;
    while (true) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req.h, &avail) || avail == 0) break;
        if (body.size() + avail > kMaxBodyBytes) return out;
        std::vector<char> chunk(avail);
        DWORD read = 0;
        if (!WinHttpReadData(req.h, chunk.data(), avail, &read) || read == 0) break;
        body.append(chunk.data(), read);
    }

    out.networkError = false;
    out.status = (int)status;
    out.body = std::move(body);
    return out;
}

}  // namespace smpd::http
