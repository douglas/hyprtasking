#pragma once

#include "../build_contract.hpp"

// Gate on the Hyprland version being built against, not on which headers
// happen to be on the include path.
#if HT_HYPRLAND_GE_0_56
    #include <hyprland/src/pointer/PointerManager.hpp>
namespace HTCompat {
using PointerManager = Pointer::CPointerManager;

inline PointerManager* pointer_manager() {
    return Pointer::mgr().get();
}
}
#else
#include <hyprland/src/managers/PointerManager.hpp>
namespace HTCompat {
using PointerManager = CPointerManager;

inline PointerManager* pointer_manager() {
    return g_pPointerManager.get();
}
}
#endif
