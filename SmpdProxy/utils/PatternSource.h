#pragma once

// ---------------------------------------------------------------------------
// PatternSource.h — ordered model of remote pattern sources.
// Mirrors AetherDLL/AetherCore/utils/PatternSource.h (smpd namespace).
// Order = priority: index 0 is tried first.
// To change/weigh your source: edit DefaultSources() in the .cpp.
// ---------------------------------------------------------------------------

#include <string>
#include <string_view>
#include <vector>

namespace smpd::downloader {

enum class Kind {
    SteamClient,     // pattern steamclient64.dll
    SteamUi,         // pattern steamui.dll
    SteamClientIpc,  // spec IPC steamclient64.dll
};

constexpr const char* KindName(Kind kind) {
    switch (kind) {
        case Kind::SteamClient:    return "steamclient";
        case Kind::SteamUi:        return "steamui";
        case Kind::SteamClientIpc: return "steamclientipc";
    }
    return "unknown";
}

inline Kind KindFromName(std::string_view name) {
    if (name == "steamui")        return Kind::SteamUi;
    if (name == "steamclientipc") return Kind::SteamClientIpc;
    return Kind::SteamClient;
}

struct MirrorUrl {
    const char* label;
    std::string url;
};

struct Loc {
    const char* branch;
    const char* folder;
};

struct Source {
    const char* id;
    const char* display;
    const char* owner;
    const char* repo;
    Loc steamClient;
    Loc steamUi;
    Loc steamClientIpc;

    const Loc* LocFor(Kind kind) const {
        switch (kind) {
            case Kind::SteamClient:    return &steamClient;
            case Kind::SteamUi:        return &steamUi;
            case Kind::SteamClientIpc: return &steamClientIpc;
        }
        return nullptr;
    }

    std::vector<MirrorUrl> UrlsFor(Kind kind, const std::string& sha) const;
};

const std::vector<Source>& DefaultSources();

}  // namespace smpd::downloader
