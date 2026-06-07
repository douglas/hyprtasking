#include "active_grid_model.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace HTLogic {

ActiveGridShape computeActiveGridShape(int visibleCount) {
    if (visibleCount <= 0)
        return {0, 0};

    const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(visibleCount))));
    const int rows = std::max(1, static_cast<int>(std::ceil(static_cast<double>(visibleCount) / cols)));

    return {rows, cols};
}

ActiveGridShape computeActiveGridShape(int visibleCount, double monitorAspect) {
    if (visibleCount <= 0)
        return {0, 0};
    if (!std::isfinite(monitorAspect) || monitorAspect <= 0.0)
        return computeActiveGridShape(visibleCount);

    const double product = static_cast<double>(visibleCount) * monitorAspect;
    if (!std::isfinite(product))
        return computeActiveGridShape(visibleCount);
    int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(product))));
    cols = std::min(cols, visibleCount);
    const int rows = std::max(1, static_cast<int>(std::ceil(static_cast<double>(visibleCount) / cols)));

    return {rows, cols};
}

ActiveGridPlacement placementForIndex(int index, int visibleCount, ActiveGridShape shape) {
    if (index < 0 || index >= visibleCount || shape.cols <= 0 || shape.rows <= 0)
        return {0, 0, 0, 0.0};

    const int row = index / shape.cols;
    const int col = index % shape.cols;
    const int itemsInRow = std::min(shape.cols, visibleCount - row * shape.cols);
    const double rowOffsetCells = (shape.cols - itemsInRow) / 2.0;

    return {row, col, itemsInRow, rowOffsetCells};
}

std::vector<::WorkspaceID> capVisibleWorkspaces(
    std::vector<::WorkspaceID> ids,
    int maxSlots,
    ::WorkspaceID activeId
) {
    if (maxSlots <= 0)
        return {};

    if (static_cast<int>(ids.size()) > maxSlots)
        ids.resize(maxSlots);

    if (activeId != -1 && std::find(ids.begin(), ids.end(), activeId) == ids.end()) {
        if (ids.empty())
            ids.push_back(activeId);
        else
            ids.back() = activeId;
    }

    return ids;
}

} // namespace HTLogic
