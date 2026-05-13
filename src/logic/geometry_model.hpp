#pragma once

#include <optional>

namespace HTLogic {

bool isPositiveFinite(float value);
bool isFinitePoint(float x, float y);
std::optional<int> gridCellCount(int rows, int cols, int max_rows, int max_cols);
std::optional<float> workspaceWidthScale(float workspace_width, float monitor_width);
std::optional<float> dragWindowScale(float drag_scale);
std::optional<float> inverseDragWindowScale(float drag_scale);
std::optional<float>
windowRenderScale(float box_width, float box_height, float window_width, float window_height);

} // namespace HTLogic
