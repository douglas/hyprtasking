#pragma once

#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <string>

#include "manager.hpp"

inline constexpr auto LOG = Hyprutils::CLI::LOG_DEBUG;

inline HANDLE PHANDLE = nullptr;

using origIsSolitaryBlocked = uint32_t (*)(void*, bool);

struct HTHookInfo {
    std::string signature;
    std::string method;
};

struct HTListenerInfo {
    bool installed = false;
};

struct HTInstalledHook {
    CFunctionHook* hook = nullptr;
    HTHookInfo info;
};

// Function hooks installed into Hyprland, plus the resolved renderWindow symbol.
struct HTHookRegistry {
    HTInstalledHook input_mouse_button;
    HTInstalledHook render_workspace;
    HTInstalledHook render_texture;
    HTInstalledHook render_border;
    HTInstalledHook render_border_lerp;
    HTInstalledHook blur_optimizations;
    HTInstalledHook should_render_window;
    HTInstalledHook is_solitary_blocked;
    void* render_window = nullptr;
    HTHookInfo render_window_info;
};

// Event bus listener install state, reported by the health dispatcher.
struct HTListenerRegistry {
    HTListenerInfo mouse_move;
    HTListenerInfo swipe_begin;
    HTListenerInfo swipe_update;
    HTListenerInfo swipe_end;
    HTListenerInfo config_reloaded;
    HTListenerInfo monitor_added;
    HTListenerInfo monitor_removed;
    HTListenerInfo monitor_layout_changed;
    HTListenerInfo window_set_changed;
};

inline HTHookRegistry g_hooks;
inline HTListenerRegistry g_listeners;

inline std::unique_ptr<HTManager> ht_manager;
