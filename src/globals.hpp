#pragma once

#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include "manager.hpp"

inline constexpr auto LOG = Hyprutils::CLI::LOG_DEBUG;

inline HANDLE PHANDLE = nullptr;

using origIsSolitaryBlocked = uint32_t (*)(void*, bool);

// Function hooks installed into Hyprland, plus the resolved renderWindow symbol.
struct HTHookRegistry {
    CFunctionHook* input_mouse_button = nullptr;
    CFunctionHook* render_workspace = nullptr;
    CFunctionHook* render_texture = nullptr;
    CFunctionHook* render_border = nullptr;
    CFunctionHook* render_border_lerp = nullptr;
    CFunctionHook* blur_optimizations = nullptr;
    CFunctionHook* should_render_window = nullptr;
    CFunctionHook* is_solitary_blocked = nullptr;
    void* render_window = nullptr;
};

inline HTHookRegistry g_hooks;

inline std::unique_ptr<HTManager> ht_manager;
