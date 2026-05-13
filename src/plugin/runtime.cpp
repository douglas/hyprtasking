#include "runtime.hpp"

#include <linux/input-event-codes.h>

#include <cmath>
#include <format>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprlang.hpp>
#include <hyprutils/signal/Listener.hpp>
#include <string_view>

#include "../build_contract.hpp"
#include "../compat/profile.hpp"
#include "../compat/runtime_compat.hpp"
#include "../config.hpp"
#include "../globals.hpp"
#include "../overview.hpp"
#include "../runtime_fail.hpp"
#include "../runtime_validation.hpp"
#include "../trace.hpp"
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

template<class... Args>
void trace_log(std::format_string<Args...> fmt, Args&&... args) {
    if (!HTTrace::enabled())
        return;

    HTTrace::log(fmt, std::forward<Args>(args)...);
}

bool disable_runtime_on_setup_failure(std::string_view source, std::string_view reason) {
    HTRuntimeFail::disable(source, reason);
    return false;
}

void resetListenerHealth() {
    mouse_move_listener_info = {};
    swipe_begin_listener_info = {};
    swipe_update_listener_info = {};
    swipe_end_listener_info = {};
    config_reloaded_listener_info = {};
    monitor_added_listener_info = {};
    monitor_removed_listener_info = {};
}

bool mark_listener_ready(HTListenerInfo& info, bool installed, std::string_view label) {
    info.installed = installed;
    if (installed)
        return true;

    return disable_runtime_on_setup_failure(
        "register_callbacks",
        std::format("failed installing {} Event::bus listener", label)
    );
}

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

#if HT_HYPRLAND_GE_0_55
void hookMouseButton(void* thisptr, IPointer::SButtonEvent e, SP<IPointer> mouse) {
    const bool cancelled = HTPlugin::guardedValue("hook_mouse_button", false, [&] {
        Event::SCallbackInfo info;
        onMouseButton(e, info);
        return info.cancelled;
    });
    if (cancelled)
        return;

    if (!HTCompat::invoke_mouse_button_original(thisptr, e, mouse))
        HTRuntimeFail::disable("hook_mouse_button", "missing onMouseButton original call-through");
}
#else
void hookMouseButton(void* thisptr, IPointer::SButtonEvent e) {
    const bool cancelled = HTPlugin::guardedValue("hook_mouse_button", false, [&] {
        Event::SCallbackInfo info;
        onMouseButton(e, info);
        return info.cancelled;
    });
    if (cancelled)
        return;

    if (!HTCompat::invoke_mouse_button_original(thisptr, e))
        HTRuntimeFail::disable("hook_mouse_button", "missing onMouseButton original call-through");
}
#endif

bool initializeMouseButtonHook() {
    const auto mouse_button = HTCompat::install_function_hook(
        input_mouse_button_hook,
        HTCompat::input_mouse_button_spec(),
        (void*)hookMouseButton
    );
    if (!mouse_button.installed) {
        return disable_runtime_on_setup_failure("initialize_mouse_button_hook", mouse_button.error);
    }
    input_mouse_button_hook_info = {
        .signature = mouse_button.signature,
        .method = mouse_button.method,
    };

    return true;
}

void shutdownMouseButtonHook() {
    HTCompat::remove_function_hook(input_mouse_button_hook, "onMouseButton");
    input_mouse_button_hook_info = {};
}

void onMouseMove([[maybe_unused]] Vector2D cursor, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_mouse_move", [&] {
        if (ht_manager == nullptr || !ht_manager->runtime_enabled())
            return;

        info.cancelled = ht_manager->on_mouse_move();
    });
}

void onSwipeBegin(
    [[maybe_unused]] IPointer::SSwipeBeginEvent e,
    [[maybe_unused]] Event::SCallbackInfo& info
) {
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

void onConfigReloaded() {
    HTPlugin::guardedCallback("on_config_reloaded", [] {
        if (!HTConfig::refresh_runtime_config_or_disable("on_config_reloaded"))
            return;
        HTTrace::refresh();

        if (ht_manager == nullptr)
            return;
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
    HTConfig::refresh_runtime_config_or_disable("initialize_config");
    HTTrace::refresh();
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
    resetListenerHealth();
}

bool registerCallbacks() {
    unregisterCallbacks();
    HTTrace::refresh();
    trace_log("[Hyprtasking][trace] registerCallbacks installing live input listeners");
    if (!HTRuntimeValidation::ensure_grid_gesture_or_disable("register_callbacks"))
        return false;

    if (!initializeMouseButtonHook())
        return false;

    if (!mark_listener_ready(
            mouse_move_listener_info,
            HTCompat::listen_mouse_move(g_mouse_move_listener, onMouseMove),
            "mouse.move"
        ))
        return false;

    if (!mark_listener_ready(
            swipe_begin_listener_info,
            HTCompat::listen_swipe_begin(g_swipe_begin_listener, onSwipeBegin),
            "gesture.swipe.begin"
        ))
        return false;
    if (!mark_listener_ready(
            swipe_update_listener_info,
            HTCompat::listen_swipe_update(g_swipe_update_listener, onSwipeUpdate),
            "gesture.swipe.update"
        ))
        return false;
    if (!mark_listener_ready(
            swipe_end_listener_info,
            HTCompat::listen_swipe_end(g_swipe_end_listener, onSwipeEnd),
            "gesture.swipe.end"
        ))
        return false;

    if (!mark_listener_ready(
            config_reloaded_listener_info,
            HTCompat::listen_config_reloaded(g_config_reloaded_listener, onConfigReloaded),
            "config.reloaded"
        ))
        return false;
    if (!mark_listener_ready(
            monitor_added_listener_info,
            HTCompat::listen_monitor_added(g_monitor_added_listener, registerMonitors),
            "monitor.added"
        ))
        return false;
    if (!mark_listener_ready(
            monitor_removed_listener_info,
            HTCompat::listen_monitor_removed(g_monitor_removed_listener, registerMonitors),
            "monitor.removed"
        ))
        return false;

    return true;
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

} // namespace HTPlugin
