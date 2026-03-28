#include "runtime.hpp"

#include <linux/input-event-codes.h>

#include <format>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprlang.hpp>
#include <hyprutils/signal/Listener.hpp>

#include "../compat/runtime_compat.hpp"
#include "../compat/profile.hpp"
#include "../config.hpp"
#include "../globals.hpp"
#include "../overview.hpp"
#include "../trace.hpp"
#include "guards.hpp"

using Hyprutils::Signal::CHyprSignalListener;

namespace {

CHyprSignalListener g_mouse_move_listener;
CHyprSignalListener g_mouse_axis_listener;
CHyprSignalListener g_touch_down_listener;
CHyprSignalListener g_touch_up_listener;
CHyprSignalListener g_touch_motion_listener;
CHyprSignalListener g_tablet_button_listener;
CHyprSignalListener g_tablet_tip_listener;
CHyprSignalListener g_tablet_proximity_listener;
CHyprSignalListener g_swipe_begin_listener;
CHyprSignalListener g_swipe_update_listener;
CHyprSignalListener g_swipe_end_listener;
CHyprSignalListener g_config_reloaded_listener;
CHyprSignalListener g_monitor_added_listener;
CHyprSignalListener g_monitor_removed_listener;

template <class... Args>
void trace_log(std::format_string<Args...> fmt, Args&&... args) {
    if (!HTTrace::enabled())
        return;

    HTTrace::log(fmt, std::forward<Args>(args)...);
}

void cancelEvent(Event::SCallbackInfo& info);
void shutdownMouseButtonHook();

const char* mouseButtonResultName(HTLogic::MouseButtonResult result) {
    switch (result) {
        case HTLogic::MouseButtonResult::Ignore:
            return "ignore";
        case HTLogic::MouseButtonResult::Consume:
            return "consume";
        case HTLogic::MouseButtonResult::PassThrough:
            return "pass_through";
    }

    return "unknown";
}

void onMouseButton(IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_mouse_button", [&] {
        if (ht_manager == nullptr || !ht_manager->runtime_enabled())
            return;

        const bool pressed = e.state == WL_POINTER_BUTTON_STATE_PRESSED;
        const auto result = ht_manager->handle_mouse_button(e.button, pressed);
        info.cancelled = result == HTLogic::MouseButtonResult::Consume;
        trace_log(
            "[Hyprtasking][trace] mouse button={} pressed={} mouse_device={} result={} cancelled={}",
            e.button,
            pressed,
            e.mouse,
            mouseButtonResultName(result),
            info.cancelled
        );
    });
}

void hookMouseButton(void* thisptr, IPointer::SButtonEvent e) {
    const bool cancelled = HTPlugin::guardedValue("hook_mouse_button", false, [&] {
        Event::SCallbackInfo info;
        onMouseButton(e, info);
        return info.cancelled;
    });
    if (cancelled)
        return;

    HTCompat::invoke_mouse_button_original(thisptr, e);
}

void initializeMouseButtonHook() {
    const auto mouse_button = HTCompat::resolve_hook_address(HTCompat::input_mouse_button_spec());
    if (mouse_button.address == nullptr) {
        fail_exit(
            "No {}: {}",
            HTCompat::input_mouse_button_spec().label,
            mouse_button.error
        );
    }

    input_mouse_button_hook = HyprlandAPI::createFunctionHook(
        PHANDLE,
        mouse_button.address,
        (void*)hookMouseButton
    );
    Log::logger->log(
        LOG,
        "[Hyprtasking] Attempting hook {} via {}",
        mouse_button.signature,
        mouse_button.method
    );

    if (!input_mouse_button_hook->hook()) {
        shutdownMouseButtonHook();
        fail_exit("Failed initializing onMouseButton hook");
    }
}

void shutdownMouseButtonHook() {
    if (input_mouse_button_hook == nullptr)
        return;

    if (!input_mouse_button_hook->unhook())
        Log::logger->log(Log::WARN, "[Hyprtasking] Failed to unhook onMouseButton");

    input_mouse_button_hook = nullptr;
}

void onTouchDown(ITouch::SDownEvent e, Event::SCallbackInfo info) {
    HTPlugin::guardedCallback("on_touch_down", [&] {
        trace_log(
            "[Hyprtasking][trace] touch down id={} pos=({}, {}) cancelled_before={} cursor_view_active={}",
            e.touchID,
            e.pos.x,
            e.pos.y,
            info.cancelled,
            ht_manager != nullptr && ht_manager->cursor_view_active()
        );
        cancelEvent(info);
        trace_log(
            "[Hyprtasking][trace] touch down id={} cancelled_after={}",
            e.touchID,
            info.cancelled
        );
    });
}

void onTouchUp(ITouch::SUpEvent e, Event::SCallbackInfo info) {
    HTPlugin::guardedCallback("on_touch_up", [&] {
        trace_log(
            "[Hyprtasking][trace] touch up id={} cancelled_before={} cursor_view_active={}",
            e.touchID,
            info.cancelled,
            ht_manager != nullptr && ht_manager->cursor_view_active()
        );
        cancelEvent(info);
        trace_log(
            "[Hyprtasking][trace] touch up id={} cancelled_after={}",
            e.touchID,
            info.cancelled
        );
    });
}

void onTouchMotion(ITouch::SMotionEvent e, Event::SCallbackInfo info) {
    HTPlugin::guardedCallback("on_touch_motion", [&] {
        trace_log(
            "[Hyprtasking][trace] touch motion id={} pos=({}, {}) cancelled_before={} cursor_view_active={}",
            e.touchID,
            e.pos.x,
            e.pos.y,
            info.cancelled,
            ht_manager != nullptr && ht_manager->cursor_view_active()
        );
        cancelEvent(info);
        trace_log(
            "[Hyprtasking][trace] touch motion id={} cancelled_after={}",
            e.touchID,
            info.cancelled
        );
    });
}

void onTabletButton(CTablet::SButtonEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_tablet_button", [&] {
        trace_log(
            "[Hyprtasking][trace] tablet button={} down={} cancelled_before={} cursor_view_active={}",
            e.button,
            e.down,
            info.cancelled,
            ht_manager != nullptr && ht_manager->cursor_view_active()
        );
    });
}

void onTabletTip(CTablet::STipEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_tablet_tip", [&] {
        trace_log(
            "[Hyprtasking][trace] tablet tip in={} tip=({}, {}) cancelled_before={} cursor_view_active={}",
            e.in,
            e.tip.x,
            e.tip.y,
            info.cancelled,
            ht_manager != nullptr && ht_manager->cursor_view_active()
        );
    });
}

void onTabletProximity(CTablet::SProximityEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_tablet_proximity", [&] {
        trace_log(
            "[Hyprtasking][trace] tablet proximity in={} proximity=({}, {}) cancelled_before={} cursor_view_active={}",
            e.in,
            e.proximity.x,
            e.proximity.y,
            info.cancelled,
            ht_manager != nullptr && ht_manager->cursor_view_active()
        );
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
        HTTrace::refresh();
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
        "plugin:hyprtasking:debug:trace",
        Hyprlang::INT {0}
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
    HTTrace::refresh();
}

void unregisterCallbacks() {
    shutdownMouseButtonHook();
    g_mouse_move_listener.reset();
    g_mouse_axis_listener.reset();
    g_touch_down_listener.reset();
    g_touch_up_listener.reset();
    g_touch_motion_listener.reset();
    g_tablet_button_listener.reset();
    g_tablet_tip_listener.reset();
    g_tablet_proximity_listener.reset();
    g_swipe_begin_listener.reset();
    g_swipe_update_listener.reset();
    g_swipe_end_listener.reset();
    g_config_reloaded_listener.reset();
    g_monitor_added_listener.reset();
    g_monitor_removed_listener.reset();
}

void registerCallbacks() {
    unregisterCallbacks();
    HTTrace::refresh();
    trace_log(
        "[Hyprtasking][trace] registerCallbacks installing live input listeners"
    );

    initializeMouseButtonHook();
    HTCompat::listen_mouse_move(g_mouse_move_listener, onMouseMove);
    HTCompat::listen_mouse_axis(g_mouse_axis_listener, onMouseAxis);

    HTCompat::listen_touch_down(g_touch_down_listener, onTouchDown);
    HTCompat::listen_touch_up(g_touch_up_listener, onTouchUp);
    HTCompat::listen_touch_motion(g_touch_motion_listener, onTouchMotion);
    HTCompat::listen_tablet_button(g_tablet_button_listener, onTabletButton);
    HTCompat::listen_tablet_tip(g_tablet_tip_listener, onTabletTip);
    HTCompat::listen_tablet_proximity(g_tablet_proximity_listener, onTabletProximity);

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
