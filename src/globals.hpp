#pragma once

#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/View.hpp>

#include "manager.hpp"

inline constexpr auto LOG = Hyprutils::CLI::LOG_DEBUG;

inline HANDLE PHANDLE = nullptr;

inline CFunctionHook* should_render_window_hook = nullptr;
inline void* render_workspace = nullptr;
inline void* render_window = nullptr;
inline bool rendering_overview = false;

inline std::unique_ptr<HTManager> ht_manager;

template<typename... Args>
inline void fail_exit(const std::format_string<Args...>& fmt, Args... args) {
    std::string err_string =
        "[Hyprtasking] " + std::vformat(fmt.get(), std::make_format_args(args...));

    HyprlandAPI::addNotification(PHANDLE, err_string, CHyprColor {1.0, 0.2, 0.2, 1.0}, 5000);
    throw std::runtime_error(err_string);
}
