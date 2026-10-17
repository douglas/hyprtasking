#include "trace.hpp"

#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <unistd.h>

#include "config.hpp"

namespace HTTrace {

namespace {

bool& trace_enabled() {
    static bool enabled = false;
    return enabled;
}

int& trace_fd() {
    static int fd = -1;
    return fd;
}

// $XDG_RUNTIME_DIR is private to the user. The /tmp fallback is per-user and
// opened with O_NOFOLLOW so a planted symlink cannot redirect writes.
std::string trace_path() {
    if (const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
        runtime_dir != nullptr && *runtime_dir != '\0')
        return std::string {runtime_dir} + "/hyprtasking-trace.log";

    return "/tmp/hyprtasking-trace-" + std::to_string(getuid()) + ".log";
}

void close_trace_file() {
    if (trace_fd() < 0)
        return;

    close(trace_fd());
    trace_fd() = -1;
}

void open_trace_file() {
    if (trace_fd() >= 0)
        return;

    trace_fd() =
        open(trace_path().c_str(), O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
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
    if (trace_enabled())
        open_trace_file();
    else
        close_trace_file();
}

void write(std::string_view message) {
    if (!enabled() || trace_fd() < 0)
        return;

    std::string line {message};
    line += '\n';
    [[maybe_unused]] const auto written = ::write(trace_fd(), line.data(), line.size());
}

void shutdown() {
    trace_enabled() = false;
    close_trace_file();
}

} // namespace HTTrace
