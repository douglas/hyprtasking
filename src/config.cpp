#include "config.hpp"

#include <cmath>
#include <format>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "globals.hpp"
#include "logic/logic.hpp"

namespace {

HTConfig::RuntimeConfig g_runtime_config;
std::string g_config_error;

constexpr int MAX_GRID_ROWS = 8;
constexpr int MAX_GRID_COLS = 8;

std::string prefixed_key(std::string_view key) {
    return "plugin:hyprtasking:" + std::string(key);
}

void log_missing_config_once(const std::string& config) {
    static std::unordered_set<std::string> warned_configs;
    if (warned_configs.contains(config))
        return;
    warned_configs.insert(config);
    Log::logger->log(
        Log::WARN,
        "[Hyprtasking] Missing config key '{}', using fallback",
        prefixed_key(config)
    );
}

using IntValue = Config::Values::CIntValue;
using FloatValue = Config::Values::CFloatValue;

template<typename T>
using StoredValue = decltype(std::declval<const T&>().value());

template<typename T>
struct OwnedConfigValue {
    std::string name;
    std::string description;
    SP<T> value;
};

// Registered values by key, one map per value type.
template<typename T>
std::unordered_map<std::string, OwnedConfigValue<T>> g_config_values;

template<typename T>
bool add_config(std::string_view key, StoredValue<T> default_value, std::string_view description) {
    auto& entry = g_config_values<T>[std::string(key)];
    entry.name = prefixed_key(key);
    entry.description = std::string(description);
    entry.value = makeShared<T>(entry.name.c_str(), entry.description.c_str(), default_value);
    if (!HyprlandAPI::addConfigValueV2(PHANDLE, entry.value)) {
        g_config_values<T>.erase(std::string(key));
        return false;
    }
    return true;
}

const auto add_int_config = &add_config<IntValue>;
const auto add_float_config = &add_config<FloatValue>;

template<typename T, typename Fallback>
Fallback value_or(std::string_view key, Fallback fallback) {
    const auto it = g_config_values<T>.find(std::string(key));
    if (it == g_config_values<T>.end()) {
        log_missing_config_once(std::string(key));
        return fallback;
    }

    return static_cast<Fallback>(it->second.value->value());
}

std::optional<std::string> validate_runtime_config(const HTConfig::RuntimeConfig& config) {
    if (config.grid_rows <= 0 || config.grid_cols <= 0) {
        return std::format(
            "Invalid grid dimensions rows={} cols={}",
            config.grid_rows,
            config.grid_cols
        );
    }
    if (config.grid_rows > MAX_GRID_ROWS || config.grid_cols > MAX_GRID_COLS) {
        return std::format(
            "Grid dimensions exceed supported maximum rows={} cols={} max_rows={} max_cols={}",
            config.grid_rows,
            config.grid_cols,
            MAX_GRID_ROWS,
            MAX_GRID_COLS
        );
    }
    if (config.drag_button <= 0 || config.select_button <= 0) {
        return std::format(
            "Invalid mouse button bindings drag_button={} select_button={}",
            config.drag_button,
            config.select_button
        );
    }
    if (config.drag_button == config.select_button) {
        return std::format(
            "Mouse button bindings must differ drag_button={} select_button={}",
            config.drag_button,
            config.select_button
        );
    }
    if (config.move_fingers <= 0 || config.open_fingers <= 0) {
        return std::format(
            "Invalid gesture finger counts move_fingers={} open_fingers={}",
            config.move_fingers,
            config.open_fingers
        );
    }
    if (config.move_fingers == config.open_fingers) {
        return std::format(
            "Gesture finger counts must differ move_fingers={} open_fingers={}",
            config.move_fingers,
            config.open_fingers
        );
    }
    if (!std::isfinite(config.move_distance) || config.move_distance <= 0.F)
        return std::format("Invalid gestures:move_distance {}", config.move_distance);
    if (!std::isfinite(config.open_distance) || config.open_distance <= 0.F)
        return std::format("Invalid gestures:open_distance {}", config.open_distance);

    return std::nullopt;
}

} // namespace

namespace HTConfig {

bool register_values() {
    bool success = true;

    success =
        add_int_config("drag_button", BTN_LEFT, "Mouse button used for overview drag") && success;
    success = add_int_config("select_button", BTN_RIGHT, "Mouse button used for overview selection")
        && success;
    success = add_int_config("gestures:enabled", 1, "Enable Hyprtasking gestures") && success;
    success = add_int_config("gestures:move_fingers", 3, "Finger count for workspace move gestures")
        && success;
    success =
        add_float_config("gestures:move_distance", 300.F, "Distance for workspace move gestures")
        && success;
    success = add_int_config("gestures:open_fingers", 4, "Finger count for overview open gestures")
        && success;
    success =
        add_float_config("gestures:open_distance", 300.F, "Distance for overview open gestures")
        && success;
    success = add_int_config("gestures:open_positive", 1, "Positive swipe direction opens overview")
        && success;
    success = add_int_config("grid:rows", 3, "Overview grid rows") && success;
    success = add_int_config("grid:cols", 3, "Overview grid columns") && success;
    success = add_int_config("grid:loop", 0, "Loop directional grid movement") && success;

    return success;
}

bool refresh_runtime_config(std::string_view source) {
    RuntimeConfig config;
    config.drag_button = value_or<IntValue>("drag_button", config.drag_button);
    config.select_button = value_or<IntValue>("select_button", config.select_button);
    config.gestures_enabled =
        value_or<IntValue>("gestures:enabled", config.gestures_enabled ? 1 : 0) != 0;
    config.move_fingers = value_or<IntValue>("gestures:move_fingers", config.move_fingers);
    config.move_distance = value_or<FloatValue>("gestures:move_distance", config.move_distance);
    config.open_fingers = value_or<IntValue>("gestures:open_fingers", config.open_fingers);
    config.open_distance = value_or<FloatValue>("gestures:open_distance", config.open_distance);
    config.open_positive =
        value_or<IntValue>("gestures:open_positive", config.open_positive ? 1 : 0) != 0;
    config.grid_rows = value_or<IntValue>("grid:rows", config.grid_rows);
    config.grid_cols = value_or<IntValue>("grid:cols", config.grid_cols);
    config.grid_loop = value_or<IntValue>("grid:loop", config.grid_loop ? 1 : 0) != 0;

    if (const auto error = validate_runtime_config(config); error.has_value()) {
        if (*error != g_config_error) {
            g_config_error = *error;
            Log::logger->log(Log::ERR, "[Hyprtasking] {}: {}", source, *error);
            HyprlandAPI::addNotification(
                PHANDLE,
                "[Hyprtasking] Invalid config, keeping previous values: " + *error,
                CHyprColor {1.0, 0.2, 0.2, 1.0},
                7000
            );
        }
        return false;
    }

    g_config_error.clear();
    g_runtime_config = config;
    return true;
}

const RuntimeConfig& runtime_config() {
    return g_runtime_config;
}

const std::string& config_error() {
    return g_config_error;
}

void reset_runtime_config() {
    g_runtime_config = RuntimeConfig {};
    g_config_error.clear();
    g_config_values<IntValue>.clear();
    g_config_values<FloatValue>.clear();
}

} // namespace HTConfig
