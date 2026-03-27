#pragma once

#include <optional>

namespace HTLogic {

bool isPositiveFinite(float value);
std::optional<float> workspaceWidthScale(float workspace_width, float monitor_width);
std::optional<float> windowRenderScale(
    float box_width,
    float box_height,
    float window_width,
    float window_height
);

} // namespace HTLogic
