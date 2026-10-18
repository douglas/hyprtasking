#pragma once

namespace HTLogic {

enum class MouseButtonResult {
    Ignore,
    Consume,
};

// Mouse interactions need an open overview that is not closing under the
// cursor, and something to act on.
bool canStartInteraction(bool has_view, bool view_active, bool view_closing, bool has_target);
}
