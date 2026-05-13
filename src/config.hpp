#pragma once

#include <linux/input-event-codes.h>

#include <hyprutils/math/Vector2D.hpp>
#include <string>
#include <string_view>

#include "build_contract.hpp"
#include "globals.hpp"

namespace HTConfig {

inline constexpr int MAX_GRID_ROWS = 8;
inline constexpr int MAX_GRID_COLS = 8;

struct RuntimeConfig {
    int debug_trace = 0;
    int drag_button = BTN_LEFT;
    int select_button = BTN_RIGHT;
    bool gestures_enabled = true;
    int move_fingers = 3;
    float move_distance = 300.F;
    int open_fingers = 4;
    float open_distance = 300.F;
    bool open_positive = true;
    int grid_rows = 3;
    int grid_cols = 3;
    bool grid_loop = false;
};

namespace Detail {

    inline std::string prefixed_key(const std::string& config) {
        return "plugin:hyprtasking:" + config;
    }

} // namespace Detail

bool register_values();
bool refresh_runtime_config_or_disable(std::string_view source);
const RuntimeConfig& runtime_config();
void reset_runtime_config();

} // namespace HTConfig
