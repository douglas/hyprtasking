#pragma once

#include <string_view>

#include "globals.hpp"

namespace HTRuntimeFail {

inline void disable(std::string_view source, std::string_view reason) {
    Log::logger->log(Log::ERR, "[Hyprtasking] {}: {}", source, reason);
    if (ht_manager != nullptr)
        ht_manager->disable_runtime(source, reason);
}

template<typename T>
inline T disable_and_return(std::string_view source, std::string_view reason, T fallback) {
    disable(source, reason);
    return fallback;
}

} // namespace HTRuntimeFail
