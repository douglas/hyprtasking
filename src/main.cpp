#include <linux/input-event-codes.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/macros.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprutils/signal/Listener.hpp>
#include <hyprlang.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include "config.hpp"
#include "compat/renderer_compat.hpp"
#include "globals.hpp"
#include "overview.hpp"
#include "plugin/guards.hpp"
#include "types.hpp"

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

uint32_t solitary_blocked_original(void* thisptr, bool full) {
    return (*(origIsSolitaryBlocked)is_solitary_blocked_hook->m_original)(thisptr, full);
}

void cleanup_hooks() {
    auto unhook = [](CFunctionHook*& hook, const char* name) {
        if (hook == nullptr)
            return;
        if (!hook->unhook())
            Log::logger->log(Log::WARN, "[Hyprtasking] Failed to unhook {}", name);
        hook = nullptr;
    };

    unhook(render_workspace_hook, "renderWorkspace");
    unhook(should_render_window_hook, "shouldRenderWindow");
    unhook(is_solitary_blocked_hook, "isSolitaryBlocked");
    render_window = nullptr;
}

void unregister_callbacks() {
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

} // namespace

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static SDispatchResult dispatch_if(std::string arg, bool is_active) {
    return HTPlugin::guardedDispatch("dispatch_if", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.passEvent = true, .success = false, .error = "ht_manager is null"};
        PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.passEvent = true, .success = false, .error = "cursor_view is null"};
        if (cursor_view->active != is_active)
            return {.passEvent = true, .success = false, .error = "predicate not met"};

        const auto DISPATCHSTR = arg.substr(0, arg.find_first_of(' '));

        auto DISPATCHARG = std::string();
        if ((int)arg.find_first_of(' ') != -1)
            DISPATCHARG = arg.substr(arg.find_first_of(' ') + 1);

        const auto DISPATCHER = g_pKeybindManager->m_dispatchers.find(DISPATCHSTR);
        if (DISPATCHER == g_pKeybindManager->m_dispatchers.end())
            return {.success = false, .error = "invalid dispatcher"};

        SDispatchResult res = DISPATCHER->second(DISPATCHARG);

        Log::logger->log(
            LOG,
            "[Hyprtasking] passthrough dispatch: {} : {}{}",
            DISPATCHSTR,
            DISPATCHARG,
            res.success ? "" : " -> " + res.error
        );

        return res;
    });
}

static SDispatchResult dispatch_if_not_active(std::string arg) {
    return dispatch_if(arg, false);
}

static SDispatchResult dispatch_if_active(std::string arg) {
    return dispatch_if(arg, true);
}

static SDispatchResult dispatch_toggle_view(std::string arg) {
    return HTPlugin::guardedDispatch("dispatch_toggle_view", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.success = false, .error = "ht_manager is null"};

        if (arg == "all") {
            if (ht_manager->has_active_view())
                ht_manager->hide_all_views();
            else
                ht_manager->show_all_views();
        } else if (arg == "cursor") {
            if (ht_manager->cursor_view_active())
                ht_manager->hide_all_views();
            else
                ht_manager->show_cursor_view();
        } else {
            return {.success = false, .error = "invalid arg"};
        }
        return {};
    });
}

static SDispatchResult dispatch_move(std::string arg) {
    return HTPlugin::guardedDispatch("dispatch_move", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.success = false, .error = "ht_manager is null"};
        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.success = false, .error = "cursor_view is null"};
        cursor_view->move(arg, false);
        return {};
    });
}

static SDispatchResult dispatch_move_window(std::string arg) {
    return HTPlugin::guardedDispatch("dispatch_move_window", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.success = false, .error = "ht_manager is null"};
        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.success = false, .error = "cursor_view is null"};
        cursor_view->move(arg, true);
        return {};
    });
}

static SDispatchResult dispatch_kill_hover(std::string arg) {
    return HTPlugin::guardedDispatch("dispatch_kill_hover", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.success = false, .error = "ht_manager is null"};

        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.success = false, .error = "cursor_view is null"};
        // Only use actually hovered window when overview is active
        // Use focused otherwise
        const PHLWINDOW hovered_window = ht_manager->get_window_from_cursor(!cursor_view->active);
        if (hovered_window == nullptr)
            return {.success = false, .error = "hovered_window is null"};
        g_pCompositor->closeWindow(hovered_window);
        return {};
    });
}

static void hook_render_workspace(
    void* thisptr,
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
   const Time::steady_tp& now,
    const CBox& geometry
) {
    try {
        if (ht_manager == nullptr) {
            HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry);
            return;
        }

        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view != nullptr && (view->navigating || ht_manager->has_active_view())) {
            view->layout->render();
            return;
        }

        HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry);
    } catch (const std::exception& e) {
        Log::logger->log(Log::ERR, "[Hyprtasking] hook_render_workspace failed: {}", e.what());
        HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry);
    } catch (...) {
        Log::logger->log(Log::ERR, "[Hyprtasking] hook_render_workspace failed with unknown exception");
        HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry);
    }
}

static bool hook_should_render_window(void* thisptr, PHLWINDOW window, PHLMONITOR monitor) {
    const bool ori_result = HTCompat::should_render_window_original(thisptr, window, monitor);
    return HTPlugin::guardedValue("hook_should_render_window", ori_result, [&] {
        if (ht_manager == nullptr || !ht_manager->has_active_view())
            return ori_result;
        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view == nullptr)
            return ori_result;
        return view->layout->should_render_window(window);
    });
}

static uint32_t hook_is_solitary_blocked(void* thisptr, bool full) {
    return HTPlugin::guardedValue("hook_is_solitary_blocked", solitary_blocked_original(thisptr, full), [&] {
        if (ht_manager == nullptr)
            return solitary_blocked_original(thisptr, full);

        const PHTVIEW view = ht_manager->get_view_from_cursor();
        if (view == nullptr)
            return solitary_blocked_original(thisptr, full);

        if (view->active || view->navigating)
            return static_cast<uint32_t>(CMonitor::SC_UNKNOWN);
        return solitary_blocked_original(thisptr, full);
    });
}

static void on_mouse_button(IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_mouse_button", [&] {
        if (ht_manager == nullptr)
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

static void on_mouse_move(Vector2D c, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_mouse_move", [&] {
        if (ht_manager == nullptr)
            return;
        info.cancelled = ht_manager->on_mouse_move();
    });
}

static void on_mouse_axis(IPointer::SAxisEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_mouse_axis", [&] {
        if (ht_manager == nullptr)
            return;
        info.cancelled = ht_manager->on_mouse_axis(e.delta);
    });
}

static void on_swipe_begin(IPointer::SSwipeBeginEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_swipe_begin", [&] {
        if (ht_manager == nullptr)
            return;
        ht_manager->swipe_start();
    });
}

static void on_swipe_update(IPointer::SSwipeUpdateEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_swipe_update", [&] {
        if (ht_manager == nullptr)
            return;
        info.cancelled = ht_manager->swipe_update(e);
    });
}

static void on_swipe_end(IPointer::SSwipeEndEvent e, Event::SCallbackInfo& info) {
    HTPlugin::guardedCallback("on_swipe_end", [&] {
        if (ht_manager == nullptr)
            return;
        info.cancelled = ht_manager->swipe_end();
    });
}

static void cancel_event(Event::SCallbackInfo& info) {
    if (ht_manager == nullptr || !ht_manager->cursor_view_active())
        return;
    info.cancelled = true;
}

static void notify_config_changes() {
    const int ROWS = HTConfig::value<Hyprlang::INT>("rows");
    if (ROWS != -1) {
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

static void register_monitors() {
    HTPlugin::guardedCallback("register_monitors", [] {
        if (ht_manager == nullptr)
            return;
        ht_manager->sync_monitor_views();
    });
}

static void on_config_reloaded() {
    HTPlugin::guardedCallback("on_config_reloaded", [] {
        notify_config_changes();

        if (ht_manager == nullptr)
            return;

        ht_manager->sync_monitor_views();

        // re-init scale and offset for inactive views, change layout if changed
        for (PHTVIEW& view : ht_manager->views) {
            if (view == nullptr)
                continue;
            const Hyprlang::STRING new_layout = HTConfig::value<Hyprlang::STRING>("layout");
            view->reload_config(HTConfig::value<Hyprlang::INT>("close_overview_on_reload"), new_layout);
        }
    });
}

static void init_functions() {
    bool success = true;

    static auto FNS1 = HyprlandAPI::findFunctionsByName(PHANDLE, "renderWorkspace");
    if (FNS1.empty())
        fail_exit("No renderWorkspace!");
    render_workspace_hook =
        HyprlandAPI::createFunctionHook(PHANDLE, FNS1[0].address, (void*)hook_render_workspace);
    Log::logger->log(LOG, "[Hyprtasking] Attempting hook {}", FNS1[0].signature);
    success = render_workspace_hook->hook();

    static auto FNS2 = HyprlandAPI::findFunctionsByName(
        PHANDLE,
        "_ZN13CHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CS"
        "haredPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEE"
    );
    if (FNS2.empty())
        fail_exit("No shouldRenderWindow");
    should_render_window_hook =
        HyprlandAPI::createFunctionHook(PHANDLE, FNS2[0].address, (void*)hook_should_render_window);
    Log::logger->log(LOG, "[Hyprtasking] Attempting hook {}", FNS2[0].signature);
    success = should_render_window_hook->hook() && success;

    // Right now (in v0.54.0) there are several "renderWindow" functions
    // This is needed so it won't break on update that adds/removes a
    // function with this name
    // This, however, requires checking for signautre changes
    static auto FNS3 = HyprlandAPI::findFunctionsByName(
        PHANDLE,
        "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
        "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
        "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
        "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb"
    );
    if (FNS3.empty())
        fail_exit("No renderWindow");
    render_window = FNS3[0].address;

    static auto FNS4 = HyprlandAPI::findFunctionsByName(PHANDLE, "isSolitaryBlocked");
    if (FNS4.empty())
        fail_exit("No isSolitaryBlocked");

    is_solitary_blocked_hook =
        HyprlandAPI::createFunctionHook(PHANDLE, FNS4[0].address, (void*)hook_is_solitary_blocked);
    Log::logger->log(LOG, "[Hyprtasking] Attempting hook {}", FNS4[0].signature);
    success = is_solitary_blocked_hook->hook() && success;

    if (!success) {
        cleanup_hooks();
        fail_exit("Failed initializing hooks");
    }
}

static void register_callbacks() {
    unregister_callbacks();

    g_mouse_button_listener = Event::bus()->m_events.input.mouse.button.listen(on_mouse_button);
    g_mouse_move_listener = Event::bus()->m_events.input.mouse.move.listen(on_mouse_move);
    g_mouse_axis_listener = Event::bus()->m_events.input.mouse.axis.listen(on_mouse_axis);

    // TODO: support touch
    g_touch_down_listener = Event::bus()->m_events.input.touch.down.listen([&] (ITouch::SDownEvent e, Event::SCallbackInfo i) { cancel_event(i); } );
    g_touch_up_listener = Event::bus()->m_events.input.touch.up.listen([&] (ITouch::SUpEvent e, Event::SCallbackInfo i) { cancel_event(i); } );
    g_touch_motion_listener = Event::bus()->m_events.input.touch.motion.listen([&] (ITouch::SMotionEvent e, Event::SCallbackInfo i) { cancel_event(i); } );
    // static auto P7 = Event::bus()->m_events.input.touch.cancel.listen([&] (ITouch::SCancelEvent e, Event::SCallbackInfo i) { cancel_event(i); } );


    g_swipe_begin_listener = Event::bus()->m_events.gesture.swipe.begin.listen(on_swipe_begin);
    g_swipe_update_listener = Event::bus()->m_events.gesture.swipe.update.listen(on_swipe_update);
    g_swipe_end_listener = Event::bus()->m_events.gesture.swipe.end.listen(on_swipe_end);

    g_config_reloaded_listener = Event::bus()->m_events.config.reloaded.listen(on_config_reloaded);
    g_monitor_added_listener = Event::bus()->m_events.monitor.added.listen(register_monitors);
    g_monitor_removed_listener = Event::bus()->m_events.monitor.removed.listen(register_monitors);
}

static void add_dispatchers() {
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:if_not_active", dispatch_if_not_active);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:if_active", dispatch_if_active);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:toggle", dispatch_toggle_view);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:move", dispatch_move);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:movewindow", dispatch_move_window);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:killhovered", dispatch_kill_hover);
}

static void init_config() {
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:layout", Hyprlang::STRING {"grid"});

    // general
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

    // swipe
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

    // grid specific
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:rows", Hyprlang::INT {3});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:cols", Hyprlang::INT {3});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:loop", Hyprlang::INT {0});
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:grid:gaps_use_aspect_ratio",
        Hyprlang::INT {0}
    );

    //linear specifig
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

    // Old config value, warning about updates
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:rows", Hyprlang::INT {-1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:exit_behavior", Hyprlang::STRING {""});

    HyprlandAPI::reloadConfig();
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string COMPOSITOR_HASH = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (COMPOSITOR_HASH != CLIENT_HASH)
        fail_exit("Mismatched headers! Can't proceed.");

    if (ht_manager == nullptr)
        ht_manager = std::make_unique<HTManager>();
    else
        ht_manager->reset();

    init_config();
    add_dispatchers();
    register_callbacks();
    init_functions();
    register_monitors();

    Log::logger->log(LOG, "[Hyprtasking] Plugin initialized");

    return {"Hyprtasking", "A workspace management plugin", "raybbian", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    Log::logger->log(LOG, "[Hyprtasking] Plugin exiting");

    unregister_callbacks();
    cleanup_hooks();
    if (ht_manager != nullptr)
        ht_manager->reset();
}
