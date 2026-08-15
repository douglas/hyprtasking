#pragma once

#if __has_include(<hyprland/src/pointer/PointerManager.hpp>)
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
