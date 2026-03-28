#include "trace.hpp"

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string_view>

#include "config.hpp"

namespace HTTrace {

namespace {

std::atomic<bool>& trace_enabled() {
    static std::atomic<bool> enabled = false;
    return enabled;
}

std::mutex& trace_mutex() {
    static std::mutex mutex;
    return mutex;
}

bool env_trace_enabled() {
    const char* raw_value = std::getenv("HYPRTASKING_TRACE");
    if (raw_value == nullptr)
        return false;

    const std::string_view value {raw_value};
    return value == "1" || value == "true" || value == "TRUE" || value == "on"
        || value == "ON" || value == "yes" || value == "YES";
}

}

bool enabled() {
    return trace_enabled().load(std::memory_order_relaxed);
}

void refresh() {
    const bool config_enabled =
        PHANDLE != nullptr && HTConfig::value<Hyprlang::INT>("debug:trace") != 0;
    trace_enabled().store(
        env_trace_enabled() || config_enabled,
        std::memory_order_relaxed
    );
}

void write(std::string_view message) {
    if (!enabled())
        return;

    std::lock_guard lock(trace_mutex());

    std::ofstream trace_file("/tmp/hyprtasking-trace.log", std::ios::app);
    if (!trace_file.is_open())
        return;

    trace_file << message << '\n';
}

} // namespace HTTrace
