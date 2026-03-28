#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace HTTrace {

bool enabled();
void refresh();
void write(std::string_view message);

template <class... Args>
void log(std::format_string<Args...> fmt, Args&&... args) {
    write(std::format(fmt, std::forward<Args>(args)...));
}

} // namespace HTTrace
