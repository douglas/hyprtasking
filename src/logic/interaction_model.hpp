#pragma once

namespace HTLogic {

enum class SelectEndAction {
    Ignore,
    FinalizeSelect,
    CancelSelect,
};

enum class MouseButtonResult {
    Ignore,
    Consume,
};

// Mouse interactions need an open overview that is not closing under the
// cursor, and something to act on.
bool canStartInteraction(bool has_view, bool view_active, bool view_closing, bool has_target);

SelectEndAction decideSelectEnd(
    bool select_pending,
    bool has_view,
    bool view_active,
    bool view_closing,
    bool has_workspace_target
);
}
