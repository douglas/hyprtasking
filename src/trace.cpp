#include "trace.hpp"

#include <cstdlib>
#include <fstream>
#include <string_view>

#include "config.hpp"

namespace HTTrace {

namespace {

bool& trace_enabled() {
    static bool enabled = false;
    return enabled;
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
    return trace_enabled();
}

void refresh() {
    const bool config_enabled =
        PHANDLE != nullptr && HTConfig::runtime_config().debug_trace != 0;
    trace_enabled() = env_trace_enabled() || config_enabled;
}

void write(std::string_view message) {
    if (!enabled())
        return;

    std::ofstream trace_file("/tmp/hyprtasking-trace.log", std::ios::app);
    if (!trace_file.is_open())
        return;

    trace_file << message << '\n';
}

} // namespace HTTrace
