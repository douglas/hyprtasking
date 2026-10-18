#include "interaction_model.hpp"

namespace HTLogic {

bool canStartInteraction(bool has_view, bool view_active, bool view_closing, bool has_target) {
    return has_view && view_active && !view_closing && has_target;
}

SelectEndAction decideSelectEnd(
    bool select_pending,
    bool has_view,
    bool view_active,
    bool view_closing,
    bool has_workspace_target
) {
    if (!select_pending)
        return SelectEndAction::Ignore;
    if (!canStartInteraction(has_view, view_active, view_closing, has_workspace_target))
        return SelectEndAction::CancelSelect;

    return SelectEndAction::FinalizeSelect;
}
}
