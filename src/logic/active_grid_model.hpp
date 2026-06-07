#pragma once

#include <vector>

using WorkspaceID = long;

namespace HTLogic {

struct ActiveGridShape {
    int rows = 0;
    int cols = 0;
};

struct ActiveGridPlacement {
    int row = 0;
    int col = 0;
    int itemsInRow = 0;
    double rowOffsetCells = 0.0;
};

ActiveGridShape computeActiveGridShape(int visibleCount);
ActiveGridShape computeActiveGridShape(int visibleCount, double monitorAspect);

ActiveGridPlacement placementForIndex(int index, int visibleCount, ActiveGridShape shape);

// Pure: cap visible workspace IDs to max slots, ensuring active ID is never dropped.
std::vector<::WorkspaceID> capVisibleWorkspaces(
    std::vector<::WorkspaceID> ids,
    int maxSlots,
    ::WorkspaceID activeId
);

} // namespace HTLogic
