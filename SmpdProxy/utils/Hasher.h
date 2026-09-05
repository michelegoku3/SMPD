#pragma once

#include <string>

// ---------------------------------------------------------------------------
// Hasher — file SHA-256 via BCrypt.
// ---------------------------------------------------------------------------

namespace smpd::hasher {

std::string ComputeFileSha256(const std::string& filePath);

}  // namespace smpd::hasher
