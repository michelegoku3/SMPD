#pragma once

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// BridgeCache — fills <Steam>\lumacore\pattern\, writing the layout
// LumaCore expects (cache-first):
//   <Steam>\lumacore\pattern\<sha>.toml                (steamclient/ui)
//   <Steam>\lumacore\pattern\steamclientipc\<sha>.toml (IPC spec)
// Upgrade-only policy: an existing cache file is replaced only when the
// upstream candidate differs from it and holds at least as many entries
// (never downgrades). Byte-identical files are left untouched and produce
// no change record, so plain launches stay silent.
// Each written file gets a "<sha>.toml.src" sidecar holding the fetch label
// (e.g. "migo3:raw"), so the next run knows which source served the cache.
// ---------------------------------------------------------------------------

namespace smpd::bridge {

// One pattern table written to disk during this run. Source names are short
// display names ("migo3", "ost", "koriapolis", "mirror").
struct PatternChange {
    std::string kind;            // "steamclient", "steamui" or "steamclientipc"
    std::string sha;             // full 64-char lowercase hex SHA-256 of the DLL
    std::string source;          // short name of the source that served this run
    std::string action;          // "downloaded" (no cache existed) or "upgraded" (cache replaced)
    std::string previousSource;  // short name of the source behind the replaced
                                 // cache; empty when unknown (no sidecar yet)
};

// Ensures all tables. Returns the tables written during this run;
// empty when everything was already installed and up to date.
std::vector<PatternChange> EnsureCache(const std::string& steamDir,
                                       const std::string& userMirror);

}  // namespace smpd::bridge
