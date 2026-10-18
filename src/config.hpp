#pragma once

#include <linux/input-event-codes.h>

#include <string>
#include <string_view>

namespace HTConfig {

struct RuntimeConfig {
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

bool register_values();
// Invalid values keep the last good config and report the error; the
// runtime stays enabled so a corrected reload takes effect.
bool refresh_runtime_config(std::string_view source);
const RuntimeConfig& runtime_config();
// Last rejected config error, empty when the current config is valid.
const std::string& config_error();
void reset_runtime_config();

} // namespace HTConfig
