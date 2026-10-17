#pragma once

#include "../build_contract.hpp"

// Gate on the Hyprland version being built against, not on which headers
// happen to be on the include path.
#if HT_HYPRLAND_GE_0_56
    #include <hyprland/src/pointer/cursor/CursorShapeOverrideController.hpp>
namespace HTCompat {
namespace CursorCompat = Pointer::Cursor;
}
#else
#include <hyprland/src/managers/cursor/CursorShapeOverrideController.hpp>
namespace HTCompat {
namespace CursorCompat = Cursor;
}
#endif
