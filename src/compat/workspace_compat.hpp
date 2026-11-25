#pragma once

#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/macros.hpp>

#include "../build_contract.hpp"

// Hyprland 0.57 replaced integer workspace IDs with a numbered/special variant
// and dropped WORKSPACEID and WORKSPACE_INVALID. Hyprtasking only lays out
// numbered workspaces, so it keeps integer IDs everywhere and treats named and
// special workspaces as WORKSPACE_INVALID. The conversions live in
// renderer_compat.cpp.
#if HT_HYPRLAND_GE_0_57
    #include <cstdint>

using WORKSPACEID = int64_t;
inline constexpr WORKSPACEID WORKSPACE_INVALID = -1;
#endif
