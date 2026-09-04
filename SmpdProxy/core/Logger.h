#pragma once

#include <string>
#include <string_view>

namespace smpd::log {

void Init(const std::string& steamDir);
void Info(const char* fmt, ...);
void Warn(const char* fmt, ...);

}  // namespace smpd::log
