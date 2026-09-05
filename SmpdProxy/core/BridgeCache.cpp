#include "pch.h"
#include "core/BridgeCache.h"

#include <toml++/toml.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

#include "core/Logger.h"
#include "utils/Hasher.h"
#include "utils/PatternDownloader.h"

namespace smpd::bridge {
namespace {

constexpr int kUpgradeProbeTimeoutSec = 5;

size_t CountPatternEntries(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return 0;
        const toml::table parsed = toml::parse(file);
        size_t count = 0;
        for (const auto& [_, val] : parsed) {
            const auto* sub = val.as_table();
            if (!sub) continue;
            if (sub->at_path("name").value_or(std::string{}).empty()) continue;
            if (sub->at_path("rva").value_or(std::string{}).empty()) continue;
            ++count;
        }
        return count;
    } catch (...) {
        return 0;
    }
}

// IPC files carry interface tables with funcHash instead of name/rva.
// Counting them with CountPatternEntries would always return 0, which would
// quarantine a healthy file to .bad on every launch. Dedicated counter.
size_t CountIpcEntries(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return 0;
        const toml::table parsed = toml::parse(file);
        size_t count = 0;
        for (const auto& [_, ifaceNode] : parsed) {
            const auto* ifaceTbl = ifaceNode.as_table();
            if (!ifaceTbl) continue;
            for (const auto& [mKey, mNode] : *ifaceTbl) {
                if (mKey.str() == "interface_id") continue;
                const auto* mTbl = mNode.as_table();
                if (!mTbl) continue;
                if (mTbl->at_path("funcHash").value_or(std::string{}).empty()) continue;
                ++count;
            }
        }
        return count;
    } catch (...) {
        return 0;
    }
}

inline bool IsIpcKind(const std::string& kindName) {
    return kindName == "steamclientipc";
}

// Short display name for a fetch label ("migo3:raw" -> "migo3",
// "opensteamtool:cdn" -> "ost", "mirror" -> "mirror").
std::string ShortSourceName(const std::string& label) {
    std::string id = label;
    const size_t colon = id.find(':');
    if (colon != std::string::npos) id.resize(colon);
    if (id == "opensteamtool") return "ost";
    return id;
}

inline size_t CountEntries(const std::string& kindName, const std::string& path) {
    return IsIpcKind(kindName) ? CountIpcEntries(path) : CountPatternEntries(path);
}

bool FileExists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Provenance sidecar: "<sha>.toml.src" holding the fetch label that produced
// the file (e.g. "migo3:raw"). Lets the next run report transitions such as
// "updated from ost to migo3".
std::string ReadProvenance(const std::string& srcPath) {
    std::ifstream in(srcPath);
    if (!in) return {};
    std::string label;
    std::getline(in, label);
    while (!label.empty() && (label.back() == ' ' || label.back() == '\t' ||
                              label.back() == '\r' || label.back() == '\n')) {
        label.pop_back();
    }
    return label;
}

void WriteProvenance(const std::string& srcPath, const std::string& label) {
    const std::string tmp = srcPath + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return;
        out << label;
    }
    if (!MoveFileExA(tmp.c_str(), srcPath.c_str(), MOVEFILE_REPLACE_EXISTING))
        DeleteFileA(tmp.c_str());
}

bool SameFileContent(const std::string& a, const std::string& b) {
    std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
    if (!fa.is_open() || !fb.is_open()) return false;
    std::ostringstream sa, sb;
    sa << fa.rdbuf();
    sb << fb.rdbuf();
    return sa.str() == sb.str();
}

// Downloads to .tmp and adopts the candidate only when it holds at least as
// many entries as the cache. Returns the change record
// when a file was written, nullopt when the cache was kept or no source
// could serve this build.
std::optional<PatternChange> EnsureOne(const std::string& kindName, const std::string& sha,
                                       const std::string& tomlPath,
                                       const std::string& userMirror, bool enableOst) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(tomlPath).parent_path(), ec);
    const std::string srcPath = tomlPath + ".src";

    if (!FileExists(tomlPath)) {
        log::Info("No cached pattern for %s; downloading...", kindName.c_str());
        std::string src;
        if (!downloader::Download(kindName, sha, tomlPath, userMirror, &src, 12, enableOst))
            return std::nullopt;
        // Never poison the cache with error pages served as HTTP 200:
        // drop the file right away when it holds no valid entries.
        if (CountEntries(kindName, tomlPath) == 0) {
            DeleteFileA(tomlPath.c_str());
            log::Warn("Downloaded %s has 0 valid entries; cache removed.", kindName.c_str());
            return std::nullopt;
        }
        log::Info("Installed %s from '%s'.", kindName.c_str(), src.c_str());
        WriteProvenance(srcPath, src);
        return PatternChange{kindName, sha, ShortSourceName(src), "downloaded", {}};
    }

    const size_t cached = CountEntries(kindName, tomlPath);
    if (cached == 0) {
        // Corrupt cache: quarantine it (.bad) and re-fetch.
        MoveFileExA(tomlPath.c_str(), (tomlPath + ".bad").c_str(), MOVEFILE_REPLACE_EXISTING);
        DeleteFileA(srcPath.c_str());  // stale provenance dies with the file
        log::Warn("Quarantined corrupt %s cache.", kindName.c_str());
        std::string src;
        if (!downloader::Download(kindName, sha, tomlPath, userMirror, &src, 12, enableOst))
            return std::nullopt;
        if (CountEntries(kindName, tomlPath) == 0) {
            DeleteFileA(tomlPath.c_str());
            log::Warn("Downloaded %s has 0 valid entries; cache removed.", kindName.c_str());
            return std::nullopt;
        }
        log::Info("Reinstalled %s from '%s'.", kindName.c_str(), src.c_str());
        WriteProvenance(srcPath, src);
        return PatternChange{kindName, sha, ShortSourceName(src), "downloaded", {}};
    }

    if (!enableOst && ShortSourceName(ReadProvenance(srcPath)) == "ost") {
        // Cache previously served by the OST fallback while it is now
        // disabled: quarantine it (.bad) and re-fetch from the safe sources.
        // OST tables lack the cloud patterns, which can leave Steam and
        // LumaCore disagreeing about saves.
        MoveFileExA(tomlPath.c_str(), (tomlPath + ".bad").c_str(), MOVEFILE_REPLACE_EXISTING);
        DeleteFileA(srcPath.c_str());  // stale provenance dies with the file
        log::Warn("Quarantined OST-sourced %s cache (OST fallback disabled).", kindName.c_str());
        std::string src;
        if (!downloader::Download(kindName, sha, tomlPath, userMirror, &src, 12, enableOst))
            return std::nullopt;
        if (CountEntries(kindName, tomlPath) == 0) {
            DeleteFileA(tomlPath.c_str());
            log::Warn("Downloaded %s has 0 valid entries; cache removed.", kindName.c_str());
            return std::nullopt;
        }
        log::Info("Reinstalled %s from '%s'.", kindName.c_str(), src.c_str());
        WriteProvenance(srcPath, src);
        return PatternChange{kindName, sha, ShortSourceName(src), "downloaded", {"ost"}};
    }

    // Upgrade probe: never overwrite the cache with a smaller table.
    const std::string candidate = tomlPath + ".tmp";
    std::string fetched;
    if (!downloader::Download(kindName, sha, candidate, userMirror, &fetched,
                              kUpgradeProbeTimeoutSec, enableOst)) {
        DeleteFileA(candidate.c_str());
        log::Info("No upstream for %s; keeping cache (%zu entries).",
                  kindName.c_str(), cached);
        return std::nullopt;
    }
    const size_t fresh = CountEntries(kindName, candidate);
    // Byte-identical content means already installed: drop the candidate and
    // stay silent, so plain launches never notify.
    if (SameFileContent(tomlPath, candidate)) {
        DeleteFileA(candidate.c_str());
        log::Info("%s is already up to date (%zu entries).", kindName.c_str(), cached);
        return std::nullopt;
    }
    if (fresh >= cached && fresh > 0) {
        const std::string previous = ShortSourceName(ReadProvenance(srcPath));
        MoveFileExA(candidate.c_str(), tomlPath.c_str(), MOVEFILE_REPLACE_EXISTING);
        WriteProvenance(srcPath, fetched);
        log::Info("Upgraded %s from '%s' (%zu entries).",
                  kindName.c_str(), fetched.c_str(), fresh);
        return PatternChange{kindName, sha, ShortSourceName(fetched), "upgraded", previous};
    }
    DeleteFileA(candidate.c_str());
    log::Info("Upstream '%s' is smaller than the cache; keeping cache.", fetched.c_str());
    return std::nullopt;
}

}  // namespace

std::vector<PatternChange> EnsureCache(const std::string& steamDir,
                                       const std::string& userMirror,
                                       bool enableOst) {
    std::vector<PatternChange> changes;
    const std::string clientDll = steamDir + "\\steamclient64.dll";
    const std::string uiDll = steamDir + "\\steamui.dll";

    const std::string clientSha = hasher::ComputeFileSha256(clientDll);
    const std::string uiSha = hasher::ComputeFileSha256(uiDll);

    log::Info("steamclient SHA: %s", clientSha.empty() ? "<hash-failed>" : clientSha.c_str());
    log::Info("steamui SHA: %s", uiSha.empty() ? "<hash-failed>" : uiSha.c_str());

    const std::string base = steamDir + "\\lumacore\\pattern";

    if (!clientSha.empty()) {
        // steamclient plus the IPC spec (same SHA as LumaCore IpcSpecLoader).
        if (auto c = EnsureOne("steamclient", clientSha, base + "\\" + clientSha + ".toml", userMirror,
                               enableOst))
            changes.push_back(*c);
        if (auto c = EnsureOne("steamclientipc", clientSha,
                               base + "\\steamclientipc\\" + clientSha + ".toml", userMirror, enableOst))
            changes.push_back(*c);
    }
    if (!uiSha.empty() && uiSha != clientSha) {
        if (auto c = EnsureOne("steamui", uiSha, base + "\\" + uiSha + ".toml", userMirror, enableOst))
            changes.push_back(*c);
    }

    for (const auto& c : changes) {
        if (c.source == "ost") {
            log::Warn("Patterns served by the OST fallback do not include "
                      "the cloud patterns; cloud error notifications are expected "
                      "until migo3 publishes the new patterns.");
            break;
        }
    }
    return changes;
}

}  // namespace smpd::bridge
