#pragma once

#include <exception>
#include <string_view>

#include "../globals.hpp"

namespace HTPlugin {

template<typename Fn>
SDispatchResult guardedDispatch(std::string_view name, Fn&& fn) {
    try {
        return fn();
    } catch (const std::exception& e) {
        Log::logger->log(Log::ERR, "[Hyprtasking] {} failed: {}", name, e.what());
        return {.success = false, .error = "internal error"};
    } catch (...) {
        Log::logger->log(Log::ERR, "[Hyprtasking] {} failed with unknown exception", name);
        return {.success = false, .error = "internal error"};
    }
}

template<typename Fn>
void guardedCallback(std::string_view name, Fn&& fn) {
    try {
        fn();
    } catch (const std::exception& e) {
        Log::logger->log(Log::ERR, "[Hyprtasking] {} failed: {}", name, e.what());
    } catch (...) {
        Log::logger->log(Log::ERR, "[Hyprtasking] {} failed with unknown exception", name);
    }
}

template<typename T, typename Fn>
T guardedValue(std::string_view name, T fallback, Fn&& fn) {
    try {
        return fn();
    } catch (const std::exception& e) {
        Log::logger->log(Log::ERR, "[Hyprtasking] {} failed: {}", name, e.what());
        return fallback;
    } catch (...) {
        Log::logger->log(Log::ERR, "[Hyprtasking] {} failed with unknown exception", name);
        return fallback;
    }
}

} // namespace HTPlugin
