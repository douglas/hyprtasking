#include "geometry_model.hpp"

#include <cmath>

namespace HTLogic {

bool isPositiveFinite(float value) {
    return std::isfinite(value) && value > 0.f;
}

std::optional<float> workspaceWidthScale(float workspace_width, float monitor_width) {
    if (!isPositiveFinite(workspace_width) || !isPositiveFinite(monitor_width))
        return std::nullopt;

    return workspace_width / monitor_width;
}

std::optional<float> windowRenderScale(
    float box_width,
    float box_height,
    float window_width,
    float window_height
) {
    if (!isPositiveFinite(box_width) || !isPositiveFinite(box_height)
        || !isPositiveFinite(window_width) || !isPositiveFinite(window_height))
        return std::nullopt;

    const float scale = box_width / window_width;
    if (!isPositiveFinite(scale))
        return std::nullopt;

    return scale;
}

} // namespace HTLogic
