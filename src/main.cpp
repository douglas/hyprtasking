#include <charconv>
#include <linux/input-event-codes.h>
#include <limits>
#include <optional>
#include <string_view>

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
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprlang.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "config.hpp"
#include "globals.hpp"
#include "overview.hpp"
#include "types.hpp"

using Hyprutils::Utils::CScopeGuard;

static std::string_view trim_arg(const std::string& arg) {
    const auto start = arg.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return {};

    const auto end = arg.find_last_not_of(" \t\n\r");
    return std::string_view(arg).substr(start, end - start + 1);
}

static std::optional<int> parse_int_arg(std::string_view arg) {
    if (arg.empty())
        return std::nullopt;

    int value = 0;
    const auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), value);
    if (ec != std::errc {} || ptr != arg.data() + arg.size())
        return std::nullopt;

    return value;
}

static std::optional<int> checked_int(int64_t value) {
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        return std::nullopt;

    return static_cast<int>(value);
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static SDispatchResult dispatch_if(std::string arg, bool is_active) {
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
}

static SDispatchResult dispatch_if_not_active(std::string arg) {
    return dispatch_if(arg, false);
}

static SDispatchResult dispatch_if_active(std::string arg) {
    return dispatch_if(arg, true);
}

static SDispatchResult dispatch_toggle_view(std::string arg) {
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
}

static SDispatchResult dispatch_move(std::string arg) {
    if (ht_manager == nullptr)
        return {.success = false, .error = "ht_manager is null"};
    const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
    if (cursor_view == nullptr)
        return {.success = false, .error = "cursor_view is null"};
    cursor_view->move(arg, false);
    return {};
}

static SDispatchResult dispatch_move_window(std::string arg) {
    if (ht_manager == nullptr)
        return {.success = false, .error = "ht_manager is null"};
    const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
    if (cursor_view == nullptr)
        return {.success = false, .error = "cursor_view is null"};
    cursor_view->move(arg, true);
    return {};
}

static void set_offset(PHTVIEW view, int new_offset) {
    if (view == nullptr)
        return;

    // HACK: Prevent no focus when closing the view
    // Makes layers less responsive and less buggy
    // Ideally we would wait for it to close and then update
    // Or update the destination as the offset is changing
    // If you wanna fix this, then test it
    // on a multimonitor setup with this command:
    //   hyprctl dispatch --batch 'dispatch hyprtasking:setlayer -1;
    //   dispatch hyprtasking:move left;
    //   dispatch hyprtasking:toggle cursor;
    //   dispatch hyprtasking:setlayer -1;
    //   dispatch hyprtasking:toggle cursor;
    //   dispatch hyprtasking:toggle cursor;
    //   dispatch hyprtasking:move down;
    //   dispatch hyprtasking:setlayer +1;
    //   dispatch hyprtasking:toggle cursor'
    if (view->closing)
        return;
    Log::logger->log(
        LOG,
        "[Hyprtasking] View \"{}\", previous offset: {}, new: {}",
        view->get_monitor()->m_name,
        view->layout->first_ws_offset,
        new_offset
    );
    view->layout->first_ws_offset = new_offset;
}

static SDispatchResult dispatch_setoffset(std::string arg) {
    if (ht_manager == nullptr)
        return {.success = false, .error = "ht_manager is null"};
    const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
    if (cursor_view == nullptr)
        return {.success = false, .error = "cursor_view is null"};

    const int original_offset = cursor_view->layout->first_ws_offset;
    const auto trimmed_arg = trim_arg(arg);
    if (trimmed_arg.empty())
        return {.success = false, .error = "missing arg"};

    const auto parsed_arg = parse_int_arg(trimmed_arg);
    if (!parsed_arg.has_value())
        return {.success = false, .error = "invalid numeric arg"};

    const bool relative = trimmed_arg.front() == '+' || trimmed_arg.front() == '-';
    const int64_t raw_offset =
        relative ? static_cast<int64_t>(original_offset) + *parsed_arg : *parsed_arg;
    if (raw_offset < 0)
        return {.success = false, .error = "offset cannot be negative"};

    const auto new_offset = checked_int(raw_offset);
    if (!new_offset.has_value())
        return {.success = false, .error = "offset out of range"};

    set_offset(cursor_view, *new_offset);

    const PHLMONITOR monitor = cursor_view->get_monitor();
    if (monitor == nullptr)
        return {.success = false, .error = "monitor is null"};
    const PHLWORKSPACE active_workspace = monitor->m_activeWorkspace;
    if (active_workspace == nullptr)
        return {.success = false, .error = "active_workspace is null"};
    const WORKSPACEID source_ws_id = active_workspace->m_id;

    const int offset_delta = *new_offset - original_offset;

    Log::logger->log(
        LOG,
        "[Hyprtasking] Setting offset from workspace \"{}\", from offset: {}",
        active_workspace->m_id,
        original_offset
    );

    cursor_view->move_id(source_ws_id + offset_delta, false);
    return {};
}

static SDispatchResult change_layer(std::string arg, bool move_window) {
    if (ht_manager == nullptr)
        return {.success = false, .error = "ht_manager is null"};

    const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
    if (cursor_view == nullptr)
        return {.success = false, .error = "cursor_view is null"};

    if (cursor_view->layout->layout_name() != "grid")
        return {.success = false, .error = "only grid layout is supported"};

    const int ROWS = HTConfig::value<Hyprlang::INT>("grid:rows");
    const int COLS = HTConfig::value<Hyprlang::INT>("grid:cols");
    const int LAYERS = HTConfig::value<Hyprlang::INT>("grid:layers");
    const int LOOP_LAYERS = HTConfig::value<Hyprlang::INT>("grid:loop_layers");
    if (ROWS <= 0 || COLS <= 0 || LAYERS <= 0)
        return {.success = false, .error = "invalid grid dimensions"};

    const int64_t ws_per_layer = static_cast<int64_t>(ROWS) * COLS;
    const int64_t max_offset_raw = static_cast<int64_t>(LAYERS - 1) * ws_per_layer;
    const auto max_offset = checked_int(max_offset_raw);
    if (!max_offset.has_value())
        return {.success = false, .error = "grid dimensions out of range"};

    const int original_offset = cursor_view->layout->first_ws_offset;
    const auto trimmed_arg = trim_arg(arg);

    int step = 1;
    bool relative = true;
    if (!trimmed_arg.empty()) {
        relative = trimmed_arg.front() == '+' || trimmed_arg.front() == '-';
        const auto parsed_arg = parse_int_arg(trimmed_arg);
        if (!parsed_arg.has_value())
            return {.success = false, .error = "invalid numeric arg"};
        step = *parsed_arg;
    }

    int64_t resulting_offset_raw = original_offset;
    if (relative) {
        // relative jump
        resulting_offset_raw += ws_per_layer * step;
    } else {
        // absolute jump
        resulting_offset_raw = ws_per_layer * step;
    }

    const auto resulting_offset_checked = checked_int(resulting_offset_raw);
    if (!resulting_offset_checked.has_value())
        return {.success = false, .error = "layer offset out of range"};
    int resulting_offset = *resulting_offset_checked;

    const PHLMONITOR monitor = cursor_view->get_monitor();
    if (monitor == nullptr)
        return {.success = false, .error = "monitor is null"};
    const PHLWORKSPACE active_workspace = monitor->m_activeWorkspace;
    if (active_workspace == nullptr)
        return {.success = false, .error = "active_workspace is null"};
    const WORKSPACEID source_ws_id = active_workspace->m_id;

    const int offset_delta = resulting_offset - original_offset;
    WORKSPACEID target_ws_id = source_ws_id + offset_delta;

    // if resulting offset doesn't fit in boundaries
    if (resulting_offset > *max_offset || resulting_offset < 0) {
        // Don't do anything if next is invalid and grid:loop_layers is disabled
        if (!LOOP_LAYERS) {
            return {};
        }

        target_ws_id = source_ws_id - original_offset;
        if (resulting_offset < 0) {
            target_ws_id += *max_offset;
            resulting_offset = *max_offset;
        } else if (resulting_offset > *max_offset) {
            resulting_offset = 0;
        }
    }

    set_offset(cursor_view, resulting_offset);

    cursor_view->move_id(target_ws_id, move_window);
    return {};
}

static SDispatchResult dispatch_setlayer(std::string arg) {
    return change_layer(arg, false);
}

static SDispatchResult dispatch_setlayerwindow(std::string arg) {
    return change_layer(arg, true);
}

static SDispatchResult dispatch_kill_hover(std::string arg) {
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
}

static SFunctionMatch find_function_match(
    const std::string& label,
    const std::string& query,
    const std::string& signature = ""
) {
    const auto matches = HyprlandAPI::findFunctionsByName(PHANDLE, query);
    if (matches.empty())
        fail_exit("No {} for query {}", label, query);

    if (signature.empty())
        return matches[0];

    for (const auto& match : matches) {
        if (match.signature == signature)
            return match;
    }

    Log::logger->log(
        LOG,
        "[Hyprtasking] No exact {} match for {}. {} candidate(s) returned for query {}",
        label,
        signature,
        matches.size(),
        query
    );
    for (const auto& match : matches) {
        Log::logger->log(
            LOG,
            "[Hyprtasking] Candidate {} hook signature: {}",
            label,
            match.signature
        );
    }

    fail_exit("No exact {} match for {}", label, signature);
    __builtin_unreachable();
}

static bool hook_should_render_window(void* thisptr, PHLWINDOW window, PHLMONITOR monitor) {
    const bool ori_result = ((should_render_window_t)(should_render_window_hook->m_original))(
        thisptr,
        window,
        monitor
    );
    if (ht_manager == nullptr || !ht_manager->has_active_view())
        return ori_result;

    const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
    if (view == nullptr)
        return ori_result;

    return view->layout->should_render_window(window);
}

static void on_mouse_button(IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
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
}

static void on_mouse_move(Vector2D c, Event::SCallbackInfo& info) {
    if (ht_manager == nullptr)
        return;
    info.cancelled = ht_manager->on_mouse_move();
}

static void on_mouse_axis(IPointer::SAxisEvent e, Event::SCallbackInfo& info) {
    if (ht_manager == nullptr)
        return;
    info.cancelled = ht_manager->on_mouse_axis(e.delta);
}

static void on_swipe_begin(IPointer::SSwipeBeginEvent e, Event::SCallbackInfo& info) {
    if (ht_manager == nullptr)
        return;
    ht_manager->swipe_start();
}

static void on_swipe_update(IPointer::SSwipeUpdateEvent e, Event::SCallbackInfo& info) {
    if (ht_manager == nullptr)
        return;
    info.cancelled = ht_manager->swipe_update(e);
}

static void on_swipe_end(IPointer::SSwipeEndEvent e, Event::SCallbackInfo& info) {
    if (ht_manager == nullptr)
        return;
    info.cancelled = ht_manager->swipe_end();
}

static void on_render_stage(eRenderStage stage) {
    if (stage != RENDER_POST_WINDOWS || ht_manager == nullptr || rendering_overview)
        return;

    const PHLMONITOR monitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
    if (monitor == nullptr)
        return;

    const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
    if (view == nullptr || (!view->navigating && !view->active))
        return;

    rendering_overview = true;
    CScopeGuard reset_rendering_state([] { rendering_overview = false; });
    view->layout->render();
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
    if (ht_manager == nullptr)
        return;
    for (const PHLMONITOR& monitor : g_pCompositor->m_monitors) {
        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view != nullptr) {
            if (!view->active)
                view->layout->init_position();
            continue;
        }
        ht_manager->views.push_back(makeShared<HTView>(monitor->m_id));

        Log::logger->log(
            LOG,
            "[Hyprtasking] Registering view for monitor {} with resolution {}x{}",
            monitor->m_description,
            monitor->m_transformedSize.x,
            monitor->m_transformedSize.y
        );
    }
}

static void on_config_reloaded() {
    notify_config_changes();

    if (ht_manager == nullptr)
        return;

    // re-init scale and offset for inactive views, change layout if changed
    for (PHTVIEW& view : ht_manager->views) {
        if (view == nullptr)
            continue;
        const Hyprlang::STRING new_layout = HTConfig::value<Hyprlang::STRING>("layout");
        if (HTConfig::value<Hyprlang::INT>("close_overview_on_reload")
            || view->layout->layout_name() != new_layout) {
            Log::logger->log(LOG, "[Hyprtasking] Closing overview on config reload");
            view->hide(false);
            view->change_layout(new_layout);
        }
    }
}

static void init_functions() {
    static const auto RENDER_WORKSPACE_MATCH = find_function_match(
        "renderWorkspace",
        "renderWorkspace",
        "_ZN13CHyprRenderer15renderWorkspaceEN9Hyprutils6Memory14CSharedPointerI8CMon"
        "itorEENS2_I10CWorkspaceEERKNSt6chrono10time_pointINS7_3_V212steady_clockENS7"
        "_8durationIlSt5ratioILl1ELl1000000000EEEEEERKNS0_4Math4CBoxE"
    );
    render_workspace = RENDER_WORKSPACE_MATCH.address;

    static const auto SHOULD_RENDER_WINDOW_MATCH = find_function_match(
        "shouldRenderWindow",
        "shouldRenderWindow",
        "_ZN13CHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CSharedPointerIN7"
        "Desktop4View7CWindowEEENS2_I8CMonitorEE"
    );
    should_render_window_hook = HyprlandAPI::createFunctionHook(
        PHANDLE,
        SHOULD_RENDER_WINDOW_MATCH.address,
        (void*)hook_should_render_window
    );
    Log::logger->log(LOG, "[Hyprtasking] Attempting hook {}", SHOULD_RENDER_WINDOW_MATCH.signature);
    if (!should_render_window_hook->hook())
        fail_exit("Failed initializing shouldRenderWindow hook");

    static const auto RENDER_WINDOW_MATCH = find_function_match(
        "renderWindow",
        "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
        "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
        "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
        "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb",
        "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
        "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
        "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
        "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb"
    );
    render_window = RENDER_WINDOW_MATCH.address;
}

static void register_callbacks() {
    static auto P1 = Event::bus()->m_events.input.mouse.button.listen(on_mouse_button);
    static auto P2 = Event::bus()->m_events.input.mouse.move.listen(on_mouse_move);
    static auto P3 = Event::bus()->m_events.input.mouse.axis.listen(on_mouse_axis);

    // TODO: support touch
    static auto P4 = Event::bus()->m_events.input.touch.down.listen([&] (ITouch::SDownEvent e, Event::SCallbackInfo i) { cancel_event(i); } );
    static auto P5 = Event::bus()->m_events.input.touch.up.listen([&] (ITouch::SUpEvent e, Event::SCallbackInfo i) { cancel_event(i); } );
    static auto P6 = Event::bus()->m_events.input.touch.motion.listen([&] (ITouch::SMotionEvent e, Event::SCallbackInfo i) { cancel_event(i); } );
    // static auto P7 = Event::bus()->m_events.input.touch.cancel.listen([&] (ITouch::SCancelEvent e, Event::SCallbackInfo i) { cancel_event(i); } );


    static auto P7 = Event::bus()->m_events.gesture.swipe.begin.listen(on_swipe_begin);
    static auto P8 = Event::bus()->m_events.gesture.swipe.update.listen(on_swipe_update);
    static auto P9 = Event::bus()->m_events.gesture.swipe.end.listen(on_swipe_end);

    static auto P10 = Event::bus()->m_events.config.reloaded.listen(on_config_reloaded);
    static auto P11 = Event::bus()->m_events.monitor.added.listen(register_monitors);
    static auto P12 = Event::bus()->m_events.render.stage.listen(on_render_stage);
}

static void add_dispatchers() {
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:if_not_active", dispatch_if_not_active);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:if_active", dispatch_if_active);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:toggle", dispatch_toggle_view);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:move", dispatch_move);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:movewindow", dispatch_move_window);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:killhovered", dispatch_kill_hover);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:setoffset", dispatch_setoffset);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:setlayer", dispatch_setlayer);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:setlayerwindow", dispatch_setlayerwindow);
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
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:layers", Hyprlang::INT {1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:loop_layers", Hyprlang::INT {1});
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

    ht_manager->reset();
}
