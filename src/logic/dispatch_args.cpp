#include "dispatch_args.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <optional>

namespace {

std::string_view trim_arg(std::string_view arg) {
    const auto start = arg.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos)
        return {};

    const auto end = arg.find_last_not_of(" \t\n\r");
    return arg.substr(start, end - start + 1);
}

std::optional<int> parse_int_arg(std::string_view arg) {
    if (arg.empty())
        return std::nullopt;

    if (arg.front() == '+')
        arg.remove_prefix(1);

    if (arg.empty())
        return std::nullopt;

    int value = 0;
    const auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), value);
    if (ec != std::errc {} || ptr != arg.data() + arg.size())
        return std::nullopt;

    return value;
}

std::optional<int> checked_int(int64_t value) {
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        return std::nullopt;

    return static_cast<int>(value);
}

} // namespace

HTLogic::IntParseResult HTLogic::parseOffsetArg(std::string_view arg, int original_offset) {
    const auto trimmed_arg = trim_arg(arg);
    if (trimmed_arg.empty())
        return {.error = "missing arg"};

    const auto parsed_arg = parse_int_arg(trimmed_arg);
    if (!parsed_arg.has_value())
        return {.error = "invalid numeric arg"};

    const bool relative = trimmed_arg.front() == '+' || trimmed_arg.front() == '-';
    const int64_t raw_offset =
        relative ? static_cast<int64_t>(original_offset) + *parsed_arg : *parsed_arg;
    if (raw_offset < 0)
        return {.error = "offset cannot be negative"};

    const auto checked_offset = checked_int(raw_offset);
    if (!checked_offset.has_value())
        return {.error = "offset out of range"};

    return {.ok = true, .value = *checked_offset};
}

HTLogic::LayerOffsetParseResult
HTLogic::parseLayerOffsetArg(std::string_view arg, int original_offset, int rows, int cols, int layers) {
    if (rows <= 0 || cols <= 0 || layers <= 0)
        return {.error = "invalid grid dimensions"};

    const int64_t ws_per_layer = static_cast<int64_t>(rows) * cols;
    const int64_t max_offset_raw = static_cast<int64_t>(layers - 1) * ws_per_layer;
    const auto max_offset = checked_int(max_offset_raw);
    if (!max_offset.has_value())
        return {.error = "grid dimensions out of range"};

    const auto trimmed_arg = trim_arg(arg);

    int step = 1;
    bool relative = true;
    if (!trimmed_arg.empty()) {
        relative = trimmed_arg.front() == '+' || trimmed_arg.front() == '-';
        const auto parsed_arg = parse_int_arg(trimmed_arg);
        if (!parsed_arg.has_value())
            return {.error = "invalid numeric arg"};
        step = *parsed_arg;
    }

    int64_t resulting_offset_raw = original_offset;
    if (relative) {
        resulting_offset_raw += ws_per_layer * step;
    } else {
        resulting_offset_raw = ws_per_layer * step;
    }

    const auto resulting_offset = checked_int(resulting_offset_raw);
    if (!resulting_offset.has_value())
        return {.error = "layer offset out of range"};

    return {
        .ok = true,
        .requested_offset = *resulting_offset,
        .max_offset = *max_offset,
    };
}

long HTLogic::nextLinearDummyWorkspaceID(const std::vector<long>& monitor_workspaces, int first_ws_offset) {
    long candidate =
        monitor_workspaces.empty() ? std::max<long>(first_ws_offset, 1) : monitor_workspaces.back();

    while (std::binary_search(monitor_workspaces.begin(), monitor_workspaces.end(), candidate))
        candidate++;

    return candidate;
}
