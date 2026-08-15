#pragma once

#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <string>

#include "manager.hpp"

inline constexpr auto LOG = Hyprutils::CLI::LOG_DEBUG;

inline HANDLE PHANDLE = nullptr;

inline CFunctionHook* input_mouse_button_hook = nullptr;
inline CFunctionHook* render_workspace_hook = nullptr;
inline CFunctionHook* render_texture_hook = nullptr;
inline CFunctionHook* render_border_hook = nullptr;
inline CFunctionHook* render_border_lerp_hook = nullptr;
inline CFunctionHook* blur_optimizations_hook = nullptr;
inline CFunctionHook* should_render_window_hook = nullptr;
inline CFunctionHook* is_solitary_blocked_hook = nullptr;
typedef uint32_t (*origIsSolitaryBlocked)(void*, bool);
inline void* render_window = nullptr;

struct HTHookInfo {
    std::string signature;
    std::string method;
};

struct HTListenerInfo {
    bool installed = false;
};

inline HTHookInfo input_mouse_button_hook_info;
inline HTHookInfo render_workspace_hook_info;
inline HTHookInfo render_texture_hook_info;
inline HTHookInfo render_border_hook_info;
inline HTHookInfo render_border_lerp_hook_info;
inline HTHookInfo blur_optimizations_hook_info;
inline HTHookInfo should_render_window_hook_info;
inline HTHookInfo render_window_symbol_info;
inline HTHookInfo is_solitary_blocked_hook_info;

inline HTListenerInfo mouse_move_listener_info;
inline HTListenerInfo swipe_begin_listener_info;
inline HTListenerInfo swipe_update_listener_info;
inline HTListenerInfo swipe_end_listener_info;
inline HTListenerInfo config_reloaded_listener_info;
inline HTListenerInfo monitor_added_listener_info;
inline HTListenerInfo monitor_removed_listener_info;

inline std::unique_ptr<HTManager> ht_manager;
