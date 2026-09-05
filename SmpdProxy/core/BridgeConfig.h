#pragma once

#include <string>

// ---------------------------------------------------------------------------
// BridgeConfig — general Steam-side configuration for the pattern bridge.
//
// File: <Steam>\smpd_config.txt (optional, key=value, one entry per line):
//   ost=0|1
//   mirror=https://my-server/pattern/{subdir}/{sha}.toml
//
// Rules:
//   - Blank lines and lines starting with '#' or ';' are ignored.
//   - Keys are case-insensitive; surrounding whitespace is trimmed.
//   - "ost" accepts 1/true/yes/on/enable/enabled as true and
//     0/false/no/off/disable/disabled as false. Anything else (or a missing
//     key/file) means false: the OST fallback stays disabled by default
//     because its tables do not include the cloud patterns, which has been
//     linked to corrupted saves when Steam and LumaCore disagree.
//   - "mirror" is an optional URL template with {subdir} and {sha}
//     placeholders. When absent here, the legacy <Steam>\smpd_mirror.txt
//     file is used as a fallback for backward compatibility.
// ---------------------------------------------------------------------------

namespace smpd::bridge {

struct BridgeConfig {
    std::string mirror;  // custom mirror URL template, empty when unset
    bool useOst = false;  // OST fallback disabled by default (opt-in)
};

// Reads the configuration from the Steam directory. Never throws and never
// logs (it runs before the logger is initialized); the caller is expected
// to log the resolved values after Logger::Init.
BridgeConfig ReadBridgeConfig(const std::string& steamDir);

// Creates smpd_config.txt with documented defaults when it does not exist
// yet, so users can discover and edit the options. Never overwrites an
// existing file. Returns true when the file was created. Call after
// Logger::Init so the caller can log the outcome.
bool EnsureDefaultConfigExists(const std::string& steamDir);

}  // namespace smpd::bridge
