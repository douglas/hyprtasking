#pragma once

#include <format>
#include <hyprland/src/debug/log/Logger.hpp>
#include <utility>

#include "../build_contract.hpp"

namespace HTCompat {

// Hyprland 0.57 changed CLogger::log's format overload to take a call-site
// name before the format string, so a plain (level, fmt, args...) call no
// longer resolves. Format here and use the (level, message) overload instead.
template<typename... Args>
void log(Hyprutils::CLI::eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
#if HT_HYPRLAND_GE_0_57
    Log::logger->log(level, std::format(fmt, std::forward<Args>(args)...));
#else
    Log::logger->log(level, fmt, std::forward<Args>(args)...);
#endif
}

} // namespace HTCompat
