#pragma once

#include <string>
#include <string_view>

#include "utils/PatternSource.h"

// ---------------------------------------------------------------------------
// PatternDownloader — HTTPS fetch + atomic write.
// Difference: no global state; the custom mirror is passed as a parameter.
// The OST fallback is opt-in via enableOst (default false): when false, the
// "opensteamtool" registry entry is skipped for every kind because its tables
// lack the cloud patterns.
// ---------------------------------------------------------------------------
namespace smpd::downloader {

bool Download(Kind kind, const std::string& sha, const std::string& outPath,
              const std::string& userMirrorTemplate,
              std::string* outSource = nullptr, int timeoutSec = 12,
              bool enableOst = false);

bool Download(std::string_view kindName, const std::string& sha, const std::string& outPath,
              const std::string& userMirrorTemplate,
              std::string* outSource = nullptr, int timeoutSec = 12,
              bool enableOst = false);

}  // namespace smpd::downloader
