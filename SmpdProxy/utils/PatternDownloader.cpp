#include "pch.h"
#include "utils/PatternDownloader.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include "core/Logger.h"
#include "network/RuntimeHttp.h"

namespace smpd::downloader {
namespace {

constexpr size_t kMaxPatternResponseBytes = 8u * 1024u * 1024u;

struct FetchResult {
    bool ok = false;
    bool definitiveMissing = false;
    std::string body;
};

FetchResult Fetch(const std::string& url, int timeoutSec) {
    FetchResult result;
    http::Response resp = http::GetUnchecked(url, timeoutSec);
    if (resp.networkError) {
        log::Warn("GET failed (network) for %s.", url.c_str());
        return result;
    }
    if (resp.status == 404) {
        log::Info("GET 404 for %s.", url.c_str());
        result.definitiveMissing = true;
        return result;
    }
    if (resp.status != 200) {
        log::Warn("GET failed for %s (status=%d).", url.c_str(), resp.status);
        return result;
    }
    if (resp.body.size() > kMaxPatternResponseBytes) {
        log::Warn("Response exceeds size cap; aborting (%s).", url.c_str());
        return result;
    }
    result.ok = true;
    result.body = std::move(resp.body);
    return result;
}

void EnsureParentDir(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
}

bool SaveAtomic(const std::string& path, const std::string& content) {
    EnsureParentDir(path);
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(content.data(), (std::streamsize)content.size());
    }
    if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp.c_str());
        return false;
    }
    return true;
}

std::string ApplyTemplate(std::string tmpl, std::string_view subdir, const std::string& sha) {
    auto replace = [&](const std::string& token, const std::string& value) {
        for (size_t p = tmpl.find(token); p != std::string::npos; p = tmpl.find(token, p)) {
            tmpl.replace(p, token.size(), value);
            p += value.size();
        }
    };
    replace("{subdir}", std::string(subdir));
    replace("{sha}", sha);
    return tmpl;
}

struct Candidate {
    std::string label;
    std::string url;
};

struct Level {
    std::vector<Candidate> endpoints;
};

// Same policy as Aether: custom mirror = Level 0, then the registry in order.
std::vector<Level> BuildPlan(Kind kind, const std::string& sha,
                             const std::string& userMirror) {
    std::vector<Level> plan;
    if (!userMirror.empty()) {
        Level level;
        level.endpoints.push_back(
            Candidate{"mirror", ApplyTemplate(userMirror, KindName(kind), sha)});
        plan.push_back(std::move(level));
    }
    for (const Source& src : DefaultSources()) {
        const auto mirrors = src.UrlsFor(kind, sha);
        if (mirrors.empty()) continue;
        Level level;
        for (const MirrorUrl& m : mirrors) {
            level.endpoints.push_back(
                Candidate{std::string(src.id) + ":" + m.label, m.url});
        }
        plan.push_back(std::move(level));
    }
    return plan;
}

std::string TryLevel(const Level& level, std::string& outBody, int timeoutSec) {
    for (const Candidate& ep : level.endpoints) {
        FetchResult res = Fetch(ep.url, timeoutSec);
        if (res.ok) {
            outBody = std::move(res.body);
            return ep.label;
        }
        if (res.definitiveMissing) {
            log::Info("Endpoint '%s' has no pattern yet (404); source skipped.", ep.label.c_str());
            return {};
        }
        log::Info("Endpoint '%s' unreachable; trying next transport.", ep.label.c_str());
    }
    return {};
}

}  // namespace

bool Download(Kind kind, const std::string& sha, const std::string& outPath,
              const std::string& userMirrorTemplate,
              std::string* outSource, int timeoutSec) {
    log::Info("Resolving '%s/%s'.", KindName(kind), sha.c_str());

    const std::vector<Level> plan = BuildPlan(kind, sha, userMirrorTemplate);
    if (plan.empty()) {
        log::Warn("No sources configured for '%s/%s'.", KindName(kind), sha.c_str());
        return false;
    }

    for (const Level& level : plan) {
        std::string body;
        const std::string label = TryLevel(level, body, timeoutSec);
        if (label.empty()) {
            log::Info("Source level could not serve '%s/%s'; trying next.", KindName(kind), sha.c_str());
            continue;
        }
        if (!SaveAtomic(outPath, body)) {
            log::Warn("Failed to write %s; trying next source.", outPath.c_str());
            continue;
        }
        if (outSource) *outSource = label;
        log::Info("Saved pattern from '%s': %s", label.c_str(), outPath.c_str());
        return true;
    }

    log::Warn("All sources failed for '%s/%s'.", KindName(kind), sha.c_str());
    return false;
}

bool Download(std::string_view kindName, const std::string& sha, const std::string& outPath,
              const std::string& userMirrorTemplate,
              std::string* outSource, int timeoutSec) {
    return Download(KindFromName(kindName), sha, outPath, userMirrorTemplate, outSource, timeoutSec);
}

}  // namespace smpd::downloader
