#include "runtime.hpp"

#include <linux/input-event-codes.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprlang.hpp>
#include <hyprutils/signal/Listener.hpp>

#include "../compat/runtime_compat.hpp"
#include "../config.hpp"
#include "../globals.hpp"
#include "../overview.hpp"
#include "guards.hpp"

using Hyprutils::Signal::CHyprSignalListener;

namespace {

CHyprSignalListener g_mouse_button_listener;
CHyprSignalListener g_mouse_move_listener;
CHyprSignalListener g_mouse_axis_listener;
CHyprSignalListener g_touch_down_listener;
CHyprSignalListener g_touch_up_listener;
CHyprSignalListener g_touch_motion_listener;
CHyprSignalListener g_swipe_begin_listener;
CHyprSignalListener g_swipe_update_listener;
CHyprSignalListener g_swipe_end_listener;
CHyprSignalListener g_config_reloaded_listener;
CHyprSignalListener g_monitor_added_listener;
CHyprSignalListener g_monitor_removed_listener;

void onMouseButton(IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_mouse_button", [&] {
        if (ht_manager == nullptr || !ht_manager->runtime_enabled())
            return;

        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return;

        const bool pressed = e.state == WL_POINTER_BUTTON_STATE_PRESSED;
        const unsigned int drag_button = HTConfig::value<Hyprlang::INT>("drag_button");
        const unsigned int select_button = HTConfig::value<Hyprlang::INT>("select_button");

        if (pressed && e.button == drag_button) {
            info.cancelled = ht_manager->start_window_drag();
        } else if (!pressed && e.button == drag_button) {
            info.cancelled = ht_manager->end_window_drag();
        } else if (pressed && e.button == select_button) {
            info.cancelled = ht_manager->exit_to_workspace();
        }
    });
}

void onMouseMove([[maybe_unused]] Vector2D cursor, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_mouse_move", [&] {
        if (ht_manager == nullptr || !ht_manager->runtime_enabled())
            return;

        info.cancelled = ht_manager->on_mouse_move();
    });
}

void onMouseAxis(IPointer::SAxisEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_mouse_axis", [&] {
        if (ht_manager == nullptr || !ht_manager->runtime_enabled())
            return;

        info.cancelled = ht_manager->on_mouse_axis(e.delta);
    });
}

void onSwipeBegin([[maybe_unused]] IPointer::SSwipeBeginEvent e, [[maybe_unused]] Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_swipe_begin", [&] {
        if (ht_manager == nullptr || !ht_manager->runtime_enabled())
            return;

        ht_manager->swipe_start();
    });
}

void onSwipeUpdate(IPointer::SSwipeUpdateEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_swipe_update", [&] {
        if (ht_manager == nullptr || !ht_manager->runtime_enabled())
            return;

        info.cancelled = ht_manager->swipe_update(e);
    });
}

void onSwipeEnd([[maybe_unused]] IPointer::SSwipeEndEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_swipe_end", [&] {
        if (ht_manager == nullptr || !ht_manager->runtime_enabled())
            return;

        info.cancelled = ht_manager->swipe_end();
    });
}

void cancelEvent(Event::SCallbackInfo& info) {
    if (ht_manager == nullptr || !ht_manager->runtime_enabled() || !ht_manager->cursor_view_active())
        return;

    info.cancelled = true;
}

void notifyConfigChanges() {
    const int rows = HTConfig::value<Hyprlang::INT>("rows");
    if (rows != -1) {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[Hyprtasking] plugin:hyprtasking:rows has moved to plugin:hyprtasking:grid:rows in the config.",
            CHyprColor {1.0, 0.2, 0.2, 1.0},
            20000
        );
    }

    CVarList exit_behavior {HTConfig::value<Hyprlang::STRING>("exit_behavior"), 0, 's', true};
    if (exit_behavior.size() != 0) {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[Hyprtasking] plugin:hyprtasking:exit_behavior is deprecated. Hyprtasking will always exit to the active workspace, which is changed when interacting with the plugin.",
            CHyprColor {1.0, 0.2, 0.2, 1.0},
            20000
        );
    }
}

void onConfigReloaded() {
    HTPlugin::guardedCallback("on_config_reloaded", [] {
        notifyConfigChanges();

        if (ht_manager == nullptr)
            return;
        if (!ht_manager->runtime_enabled())
            return;

        ht_manager->sync_monitor_views();

        const Hyprlang::STRING new_layout = HTConfig::value<Hyprlang::STRING>("layout");
        const bool close_overview_on_reload =
            HTConfig::value<Hyprlang::INT>("close_overview_on_reload");
        for (PHTVIEW& view : ht_manager->views) {
            if (view == nullptr)
                continue;

            view->reload_config(close_overview_on_reload, new_layout);
        }
    });
}

}

namespace HTPlugin {

void initializeConfig() {
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:layout", Hyprlang::STRING {"grid"});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:bg_color", Hyprlang::INT {0x000000FF});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:gap_size", Hyprlang::FLOAT {8.f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:border_size", Hyprlang::FLOAT {4.f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:exit_on_hovered", Hyprlang::INT {0});
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:warp_on_move_window",
        Hyprlang::INT {1}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:close_overview_on_reload",
        Hyprlang::INT {1}
    );

    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:drag_button",
        Hyprlang::INT {BTN_LEFT}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:select_button",
        Hyprlang::INT {BTN_RIGHT}
    );

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:gestures:enabled", Hyprlang::INT {1});
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:gestures:move_fingers",
        Hyprlang::INT {3}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:gestures:move_distance",
        Hyprlang::FLOAT {300.0}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:gestures:open_fingers",
        Hyprlang::INT {4}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:gestures:open_distance",
        Hyprlang::FLOAT {300.0}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:gestures:open_positive",
        Hyprlang::INT {1}
    );

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:rows", Hyprlang::INT {3});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:cols", Hyprlang::INT {3});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:loop", Hyprlang::INT {0});
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:grid:gaps_use_aspect_ratio",
        Hyprlang::INT {0}
    );

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:linear:blur", Hyprlang::INT {1});
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:linear:height",
        Hyprlang::FLOAT {300.f}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:linear:scroll_speed",
        Hyprlang::FLOAT {1.f}
    );
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:linear:top", Hyprlang::INT {0});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:rows", Hyprlang::INT {-1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:exit_behavior", Hyprlang::STRING {""});

    HyprlandAPI::reloadConfig();
}

void unregisterCallbacks() {
    g_mouse_button_listener.reset();
    g_mouse_move_listener.reset();
    g_mouse_axis_listener.reset();
    g_touch_down_listener.reset();
    g_touch_up_listener.reset();
    g_touch_motion_listener.reset();
    g_swipe_begin_listener.reset();
    g_swipe_update_listener.reset();
    g_swipe_end_listener.reset();
    g_config_reloaded_listener.reset();
    g_monitor_added_listener.reset();
    g_monitor_removed_listener.reset();
}

void registerCallbacks() {
    unregisterCallbacks();

    HTCompat::listen_mouse_button(g_mouse_button_listener, onMouseButton);
    HTCompat::listen_mouse_move(g_mouse_move_listener, onMouseMove);
    HTCompat::listen_mouse_axis(g_mouse_axis_listener, onMouseAxis);

    HTCompat::listen_touch_down(
        g_touch_down_listener,
        []([[maybe_unused]] ITouch::SDownEvent e, Event::SCallbackInfo i) { cancelEvent(i); }
    );
    HTCompat::listen_touch_up(
        g_touch_up_listener,
        []([[maybe_unused]] ITouch::SUpEvent e, Event::SCallbackInfo i) { cancelEvent(i); }
    );
    HTCompat::listen_touch_motion(
        g_touch_motion_listener,
        []([[maybe_unused]] ITouch::SMotionEvent e, Event::SCallbackInfo i) { cancelEvent(i); }
    );

    HTCompat::listen_swipe_begin(g_swipe_begin_listener, onSwipeBegin);
    HTCompat::listen_swipe_update(g_swipe_update_listener, onSwipeUpdate);
    HTCompat::listen_swipe_end(g_swipe_end_listener, onSwipeEnd);

    HTCompat::listen_config_reloaded(g_config_reloaded_listener, onConfigReloaded);
    HTCompat::listen_monitor_added(g_monitor_added_listener, registerMonitors);
    HTCompat::listen_monitor_removed(g_monitor_removed_listener, registerMonitors);
}

void registerMonitors() {
    HTPlugin::guardedCallback("register_monitors", [] {
        if (ht_manager == nullptr)
            return;
        if (!ht_manager->runtime_enabled())
            return;

        ht_manager->sync_monitor_views();
    });
}

}
