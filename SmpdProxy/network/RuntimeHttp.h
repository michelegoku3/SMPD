#pragma once

#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// RuntimeHttp — GET minimale via WinHTTP (estratto da
// AetherDLL/AetherCore/network/RuntimeHttp.cpp, GetUnchecked only).
// No allowlist: the bridge must be able to reach custom mirrors.
// ---------------------------------------------------------------------------

namespace smpd::http {

struct Response {
    bool networkError = true;
    int status = 0;
    std::string body;
};

Response GetUnchecked(std::string_view url, int timeoutSec = 12);

}  // namespace smpd::http
