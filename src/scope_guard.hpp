#pragma once

#include <exception>
#include <hyprland/src/debug/log/Logger.hpp>
#include <utility>

// Cleanup runs from destructors, which are noexcept: an exception escaping one
// would terminate Hyprland, so log it instead.
template<typename Fn>
void run_cleanup(Fn&& fn) noexcept {
    try {
        fn();
    } catch (const std::exception& e) {
        try {
            Log::logger->log(Log::ERR, "[Hyprtasking] cleanup failed: {}", e.what());
        } catch (...) {}
    } catch (...) {
        try {
            Log::logger->log(Log::ERR, "[Hyprtasking] cleanup failed with unknown exception");
        } catch (...) {}
    }
}

// Hyprutils' CScopeGuard, but with the cleanup run through run_cleanup.
template<typename Fn>
class [[nodiscard]] HTScopeGuard {
  public:
    explicit HTScopeGuard(Fn fn) : fn(std::move(fn)) {}

    HTScopeGuard(const HTScopeGuard&) = delete;
    HTScopeGuard& operator=(const HTScopeGuard&) = delete;

    ~HTScopeGuard() {
        run_cleanup(fn);
    }

  private:
    Fn fn;
};
