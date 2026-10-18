#include "interaction_model.hpp"

namespace HTLogic {

bool canStartInteraction(bool has_view, bool view_active, bool view_closing, bool has_target) {
    return has_view && view_active && !view_closing && has_target;
}
}
