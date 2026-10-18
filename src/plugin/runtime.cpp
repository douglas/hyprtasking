#include "runtime.hpp"

#include <format>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/signal/Listener.hpp>
#include <string_view>

#include "../compat/profile.hpp"
#include "../compat/renderer_compat.hpp"
#include "../compat/runtime_compat.hpp"
#include "../config.hpp"
#include "../globals.hpp"
#include "../overview.hpp"
#include "../runtime_fail.hpp"
#include "guards.hpp"

using Hyprutils::Signal::CHyprSignalListener;

namespace {

CHyprSignalListener g_mouse_move_listener;
CHyprSignalListener g_swipe_begin_listener;
CHyprSignalListener g_swipe_update_listener;
CHyprSignalListener g_swipe_end_listener;
CHyprSignalListener g_config_reloaded_listener;
CHyprSignalListener g_monitor_added_listener;
CHyprSignalListener g_monitor_removed_listener;
CHyprSignalListener g_monitor_layout_changed_listener;
std::vector<CHyprSignalListener> g_window_set_listeners;

bool disable_runtime_on_setup_failure(std::string_view source, std::string_view reason) {
    HTRuntimeFail::disable(source, reason);
    return false;
}

bool mark_listener_ready(bool installed, std::string_view label) {
    if (installed)
        return true;

    return disable_runtime_on_setup_failure(
        "register_callbacks",
        std::format("failed installing {} Event::bus listener", label)
    );
}

void hookMouseButton(void* thisptr, IPointer::SButtonEvent e, SP<IPointer> mouse) {
    const bool consumed = HTPlugin::guardedValue("hook_mouse_button", false, [&] {
        return ht_manager->runtime_enabled()
            && ht_manager->handle_mouse_button(e.button, e.state == WL_POINTER_BUTTON_STATE_PRESSED)
            == HTLogic::MouseButtonResult::Consume;
    });
    if (!consumed)
        HTCompat::invoke_mouse_button_original(thisptr, e, std::move(mouse));
}

bool initializeMouseButtonHook() {
    const auto mouse_button = HTCompat::install_function_hook(
        g_hooks.input_mouse_button,
        HTCompat::input_mouse_button_spec(),
        HTCompat::hook_destination(hookMouseButton)
    );
    if (!mouse_button.installed)
        return disable_runtime_on_setup_failure("initialize_mouse_button_hook", mouse_button.error);

    return true;
}

void shutdownMouseButtonHook() {
    HTCompat::remove_function_hook(g_hooks.input_mouse_button, "onMouseButton");
}

void onMouseMove([[maybe_unused]] Vector2D cursor, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_mouse_move", [&] {
        if (!ht_manager->runtime_enabled())
            return;

        info.cancelled = ht_manager->on_mouse_move();
    });
}

void onSwipeBegin(
    [[maybe_unused]] IPointer::SSwipeBeginEvent e,
    [[maybe_unused]] Event::SCallbackInfo& info
) {
    HTPlugin::guardedCallback("on_swipe_begin", [&] {
        if (!ht_manager->runtime_enabled())
            return;

        ht_manager->reset_swipe_state();
    });
}

void onSwipeUpdate(IPointer::SSwipeUpdateEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_swipe_update", [&] {
        if (!ht_manager->runtime_enabled())
            return;

        info.cancelled = ht_manager->swipe_update(e);
    });
}

void onSwipeEnd(IPointer::SSwipeEndEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_swipe_end", [&] {
        if (!ht_manager->runtime_enabled())
            return;

        info.cancelled = ht_manager->swipe_end(e.cancelled);
    });
}

void onWindowSetChanged() {
    HTPlugin::guardedCallback("on_window_set_changed", [] {
        if (ht_manager->runtime_enabled())
            ht_manager->on_window_set_changed();
    });
}

void onConfigReloaded() {
    HTPlugin::guardedCallback("on_config_reloaded", [] {
        HTConfig::refresh_runtime_config("on_config_reloaded");

        if (!ht_manager->runtime_enabled())
            return;

        ht_manager->sync_monitor_views();

        for (PHTVIEW& view : ht_manager->views) {
            if (view == nullptr)
                continue;

            view->reload_config();
        }
    });
}

} // namespace

namespace HTPlugin {

void initializeConfig() {
    HTConfig::reset_runtime_config();
    if (!HTConfig::register_values()) {
        disable_runtime_on_setup_failure(
            "initialize_config",
            "failed registering one or more Hyprtasking config values"
        );
    }

    HyprlandAPI::reloadConfig();
    HTConfig::refresh_runtime_config("initialize_config");
}

void unregisterCallbacks() {
    shutdownMouseButtonHook();
    g_mouse_move_listener.reset();
    g_swipe_begin_listener.reset();
    g_swipe_update_listener.reset();
    g_swipe_end_listener.reset();
    g_config_reloaded_listener.reset();
    g_monitor_added_listener.reset();
    g_monitor_removed_listener.reset();
    g_monitor_layout_changed_listener.reset();
    g_window_set_listeners.clear();
}

bool registerCallbacks() {
    if (!initializeMouseButtonHook())
        return false;

    if (!mark_listener_ready(
            HTCompat::listen_mouse_move(g_mouse_move_listener, onMouseMove),
            "mouse.move"
        ))
        return false;

    if (!mark_listener_ready(
            HTCompat::listen_swipe_begin(g_swipe_begin_listener, onSwipeBegin),
            "gesture.swipe.begin"
        ))
        return false;
    if (!mark_listener_ready(
            HTCompat::listen_swipe_update(g_swipe_update_listener, onSwipeUpdate),
            "gesture.swipe.update"
        ))
        return false;
    if (!mark_listener_ready(
            HTCompat::listen_swipe_end(g_swipe_end_listener, onSwipeEnd),
            "gesture.swipe.end"
        ))
        return false;

    if (!mark_listener_ready(
            HTCompat::listen_config_reloaded(g_config_reloaded_listener, onConfigReloaded),
            "config.reloaded"
        ))
        return false;
    if (!mark_listener_ready(
            HTCompat::listen_monitor_added(g_monitor_added_listener, [] { registerMonitors(); }),
            "monitor.added"
        ))
        return false;
    if (!mark_listener_ready(
            HTCompat::listen_monitor_removed(
                g_monitor_removed_listener,
                [](PHLMONITOR monitor) { registerMonitors(HTCompat::monitor_id(monitor)); }
            ),
            "monitor.removed"
        ))
        return false;
    if (!mark_listener_ready(
            HTCompat::listen_monitor_layout_changed(
                g_monitor_layout_changed_listener,
                [] { registerMonitors(); }
            ),
            "monitor.layoutChanged"
        ))
        return false;
    if (!mark_listener_ready(
            HTCompat::listen_window_set_changed(g_window_set_listeners, onWindowSetChanged),
            "window.open/close/moveToWorkspace"
        ))
        return false;

    return true;
}

void registerMonitors(MONITORID removed_monitor_id) {
    HTPlugin::guardedCallback("register_monitors", [removed_monitor_id] {
        if (!ht_manager->runtime_enabled())
            return;

        ht_manager->sync_monitor_views(removed_monitor_id);
    });
}

} // namespace HTPlugin
