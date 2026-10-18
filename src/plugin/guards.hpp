#pragma once

#include <exception>
#include <string_view>

#include "../globals.hpp"

namespace HTPlugin {

// Message of the exception being handled; call only from a catch block. The
// pointer stays valid until that catch block exits.
inline const char* current_exception_what() noexcept {
    try {
        throw;
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "unknown exception";
    }
}

// Disables the runtime for the exception being handled; call only from a
// catch block.
inline void disable_runtime_for_current_exception(std::string_view name) noexcept {
    const char* reason = current_exception_what();
    try {
        if (ht_manager != nullptr)
            ht_manager->disable_runtime(name, reason);
        Log::logger->log(Log::ERR, "[Hyprtasking] {} failed: {}", name, reason);
    } catch (...) {
    } // NOLINT(bugprone-empty-catch): disabling or logging failed; nothing left to do
}

template<typename Fn>
SDispatchResult guardedDispatch(std::string_view name, Fn&& fn) {
    try {
        return fn();
    } catch (...) {
        disable_runtime_for_current_exception(name);
        return {.success = false, .error = "internal error"};
    }
}

template<typename Fn>
void guardedCallback(std::string_view name, Fn&& fn) {
    try {
        fn();
    } catch (...) {
        disable_runtime_for_current_exception(name);
    }
}

template<typename T, typename Fn>
T guardedValue(std::string_view name, T fallback, Fn&& fn) {
    try {
        return fn();
    } catch (...) {
        disable_runtime_for_current_exception(name);
        return fallback;
    }
}

} // namespace HTPlugin
