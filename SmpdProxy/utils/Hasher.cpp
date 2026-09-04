#include "pch.h"
#include "utils/Hasher.h"

#include <bcrypt.h>

#include <array>
#include <cstdio>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace smpd::hasher {

std::string ComputeFileSha256(const std::string& filePath) {
    FILE* raw = nullptr;
    fopen_s(&raw, filePath.c_str(), "rb");
    if (!raw) return {};

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::string out;

    do {
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) break;
        if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) < 0) break;

        std::vector<uint8_t> buf(1u << 20);
        bool ok = true;
        for (;;) {
            size_t got = fread(buf.data(), 1, buf.size(), raw);
            if (got == 0) break;
            if (BCryptHashData(hash, buf.data(), (ULONG)got, 0) < 0) { ok = false; break; }
        }
        if (!ok) break;

        std::array<uint8_t, 32> digest{};
        if (BCryptFinishHash(hash, digest.data(), (ULONG)digest.size(), 0) < 0) break;

        static constexpr char kHex[] = "0123456789abcdef";
        out.reserve(64);
        for (uint8_t b : digest) {
            out.push_back(kHex[b >> 4]);
            out.push_back(kHex[b & 0xF]);
        }
    } while (false);

    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    fclose(raw);
    return out;
}

}  // namespace smpd::hasher
