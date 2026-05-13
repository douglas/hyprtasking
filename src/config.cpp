#include "config.hpp"

#include <cmath>
#include <format>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprlang.hpp>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "build_contract.hpp"
#include "logic/geometry_model.hpp"
#if HT_HYPRLAND_GE_0_55
    #include <hyprland/src/config/values/types/FloatValue.hpp>
    #include <hyprland/src/config/values/types/IntValue.hpp>
#endif

#include "runtime_fail.hpp"

namespace {

HTConfig::RuntimeConfig g_runtime_config;

void log_missing_config_once(const std::string& config) {
    static std::unordered_set<std::string> warned_configs;
    if (warned_configs.contains(config))
        return;
    warned_configs.insert(config);
    Log::logger->log(
        Log::WARN,
        "[Hyprtasking] Missing config key '{}', using fallback",
        HTConfig::Detail::prefixed_key(config)
    );
}

#if HT_HYPRLAND_GE_0_55
template<typename T>
struct OwnedConfigValue {
    std::string name;
    std::string description;
    SP<T> value;
};

std::unordered_map<std::string, OwnedConfigValue<Config::Values::CIntValue>> g_int_config_values;
std::unordered_map<std::string, OwnedConfigValue<Config::Values::CFloatValue>>
    g_float_config_values;
#else
template<typename T>
T legacy_value_or(std::string config, T fallback) {
    const auto* config_value =
        HyprlandAPI::getConfigValue(PHANDLE, HTConfig::Detail::prefixed_key(config));
    if (config_value == nullptr) {
        log_missing_config_once(config);
        return fallback;
    }

    auto* static_ptr = (T* const*)config_value->getDataStaticPtr();
    if (static_ptr == nullptr || *static_ptr == nullptr) {
        log_missing_config_once(config);
        return fallback;
    }

    return **static_ptr;
}
#endif

bool add_int_config(std::string_view key, int default_value, std::string_view description) {
    const std::string name = HTConfig::Detail::prefixed_key(std::string(key));
#if HT_HYPRLAND_GE_0_55
    auto& entry = g_int_config_values[std::string(key)];
    entry.name = name;
    entry.description = std::string(description);
    entry.value = makeShared<Config::Values::CIntValue>(
        entry.name.c_str(),
        entry.description.c_str(),
        static_cast<Config::INTEGER>(default_value)
    );
    if (!HyprlandAPI::addConfigValueV2(PHANDLE, entry.value)) {
        g_int_config_values.erase(std::string(key));
        return false;
    }
    return true;
#else
    return HyprlandAPI::addConfigValue(PHANDLE, name, Hyprlang::INT {default_value});
#endif
}

bool add_float_config(std::string_view key, float default_value, std::string_view description) {
    const std::string name = HTConfig::Detail::prefixed_key(std::string(key));
#if HT_HYPRLAND_GE_0_55
    auto& entry = g_float_config_values[std::string(key)];
    entry.name = name;
    entry.description = std::string(description);
    entry.value = makeShared<Config::Values::CFloatValue>(
        entry.name.c_str(),
        entry.description.c_str(),
        static_cast<Config::FLOAT>(default_value)
    );
    if (!HyprlandAPI::addConfigValueV2(PHANDLE, entry.value)) {
        g_float_config_values.erase(std::string(key));
        return false;
    }
    return true;
#else
    return HyprlandAPI::addConfigValue(PHANDLE, name, Hyprlang::FLOAT {default_value});
#endif
}

int int_config_or(std::string_view key, int fallback) {
#if HT_HYPRLAND_GE_0_55
    const auto it = g_int_config_values.find(std::string(key));
    if (it == g_int_config_values.end() || it->second.value == nullptr) {
        log_missing_config_once(std::string(key));
        return fallback;
    }

    return static_cast<int>(it->second.value->value());
#else
    return legacy_value_or<Hyprlang::INT>(std::string(key), fallback);
#endif
}

float float_config_or(std::string_view key, float fallback) {
#if HT_HYPRLAND_GE_0_55
    const auto it = g_float_config_values.find(std::string(key));
    if (it == g_float_config_values.end() || it->second.value == nullptr) {
        log_missing_config_once(std::string(key));
        return fallback;
    }

    return static_cast<float>(it->second.value->value());
#else
    return legacy_value_or<Hyprlang::FLOAT>(std::string(key), fallback);
#endif
}

std::optional<std::string> validate_runtime_config(const HTConfig::RuntimeConfig& config) {
    if (config.grid_rows <= 0 || config.grid_cols <= 0) {
        return std::format(
            "Invalid grid dimensions rows={} cols={}",
            config.grid_rows,
            config.grid_cols
        );
    }
    if (!HTLogic::gridCellCount(
             config.grid_rows,
             config.grid_cols,
             HTConfig::MAX_GRID_ROWS,
             HTConfig::MAX_GRID_COLS
        )
             .has_value()) {
        return std::format(
            "Grid dimensions exceed supported maximum rows={} cols={} max_rows={} max_cols={}",
            config.grid_rows,
            config.grid_cols,
            HTConfig::MAX_GRID_ROWS,
            HTConfig::MAX_GRID_COLS
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

    success = add_int_config("debug:trace", 0, "Enable Hyprtasking trace logging") && success;
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

bool refresh_runtime_config_or_disable(std::string_view source) {
    RuntimeConfig config;
    config.debug_trace = int_config_or("debug:trace", config.debug_trace);
    config.drag_button = int_config_or("drag_button", config.drag_button);
    config.select_button = int_config_or("select_button", config.select_button);
    config.gestures_enabled =
        int_config_or("gestures:enabled", config.gestures_enabled ? 1 : 0) != 0;
    config.move_fingers = int_config_or("gestures:move_fingers", config.move_fingers);
    config.move_distance = float_config_or("gestures:move_distance", config.move_distance);
    config.open_fingers = int_config_or("gestures:open_fingers", config.open_fingers);
    config.open_distance = float_config_or("gestures:open_distance", config.open_distance);
    config.open_positive =
        int_config_or("gestures:open_positive", config.open_positive ? 1 : 0) != 0;
    config.grid_rows = int_config_or("grid:rows", config.grid_rows);
    config.grid_cols = int_config_or("grid:cols", config.grid_cols);
    config.grid_loop = int_config_or("grid:loop", config.grid_loop ? 1 : 0) != 0;

    if (const auto error = validate_runtime_config(config); error.has_value()) {
        HTRuntimeFail::disable(source, *error);
        return false;
    }

    g_runtime_config = config;
    return true;
}

const RuntimeConfig& runtime_config() {
    return g_runtime_config;
}

void reset_runtime_config() {
    g_runtime_config = RuntimeConfig {};
#if HT_HYPRLAND_GE_0_55
    g_int_config_values.clear();
    g_float_config_values.clear();
#endif
}

} // namespace HTConfig
