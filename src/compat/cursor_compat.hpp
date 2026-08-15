#pragma once

#if __has_include(<hyprland/src/pointer/cursor/CursorShapeOverrideController.hpp>)
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
