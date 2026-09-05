#include "pch.h"
#include "core/BridgeConfig.h"

#include <cstdio>
#include <fstream>

namespace smpd::bridge {
namespace {

constexpr size_t kMaxConfigBytes = 8192;

void TrimInPlace(std::string& s) {
    size_t begin = 0;
    while (begin < s.size() && (s[begin] == ' ' || s[begin] == '\t' ||
                                s[begin] == '\r' || s[begin] == '\n'))
        ++begin;
    size_t end = s.size();
    while (end > begin && (s[end - 1] == ' ' || s[end - 1] == '\t' ||
                           s[end - 1] == '\r' || s[end - 1] == '\n'))
        --end;
    if (begin > 0 || end < s.size()) s = s.substr(begin, end - begin);
}

void ToLowerInPlace(std::string& s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
}

std::string ReadWholeFile(const std::string& path) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return {};
    std::string out;
    char buf[1024];
    size_t total = 0;
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (total + n > kMaxConfigBytes) {
            out.append(buf, kMaxConfigBytes - total);
            break;
        }
        out.append(buf, n);
        total += n;
    }
    fclose(f);
    return out;
}

// Legacy mirror file: a single URL template on the first line.
std::string ReadLegacyMirrorFile(const std::string& dir) {
    std::string content = ReadWholeFile(dir + "\\smpd_mirror.txt");
    // Keep only the first line for backward compatibility.
    const size_t nl = content.find_first_of("\r\n");
    if (nl != std::string::npos) content.resize(nl);
    TrimInPlace(content);
    return content;
}

bool ParseOstValue(const std::string& raw) {
    std::string v = raw;
    TrimInPlace(v);
    ToLowerInPlace(v);
    return v == "1" || v == "true" || v == "yes" || v == "on" ||
           v == "enable" || v == "enabled";
}

}  // namespace

BridgeConfig ReadBridgeConfig(const std::string& steamDir) {
    BridgeConfig config;
    const std::string content = ReadWholeFile(steamDir + "\\smpd_config.txt");

    size_t pos = 0;
    while (pos <= content.size()) {
        size_t end = content.find_first_of("\r\n", pos);
        const bool last = (end == std::string::npos);
        std::string line = content.substr(pos, last ? std::string::npos : end - pos);
        pos = last ? content.size() + 1 : end + 1;

        TrimInPlace(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        TrimInPlace(key);
        TrimInPlace(value);
        ToLowerInPlace(key);
        if (key.empty()) continue;

        if (key == "ost") {
            config.useOst = ParseOstValue(value);
        } else if (key == "mirror") {
            config.mirror = value;
        }
    }

    if (config.mirror.empty()) config.mirror = ReadLegacyMirrorFile(steamDir);
    return config;
}

bool EnsureDefaultConfigExists(const std::string& steamDir) {
    const std::string path = steamDir + "\\smpd_config.txt";
    if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) return false;  // user file wins

    static constexpr const char* kDefaultConfig =
        "# SMPD Pattern Bridge configuration (auto-created on first launch).\n"
        "# Lines starting with '#' or ';' are ignored; keys are case-insensitive.\n"
        "#\n"
        "# OST fallback (OpenSteam001/steam-monitor). DISABLED by default because\n"
        "# its tables do not include the cloud patterns, which can leave Steam\n"
        "# and LumaCore disagreeing about saves. Set ost=1 only if you accept\n"
        "# that risk.\n"
        "ost=0\n"
        "#\n"
        "# Optional custom mirror (takes precedence over smpd_mirror.txt):\n"
        "# mirror=https://my-server/pattern/{subdir}/{sha}.toml\n";

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out << kDefaultConfig;
    out.close();
    return out.good();
}

}  // namespace smpd::bridge
