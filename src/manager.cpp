#include "manager.hpp"

#include <algorithm>
#include <format>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include "compat/profile.hpp"
#include "compat/renderer_compat.hpp"
#include "compat/runtime_compat.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "logic/view_sync_model.hpp"
#include "overview.hpp"
#include "plugin/guards.hpp"

// The timer callback and listeners run code from this plugin's .so.
HTManager::~HTManager() {
    surface_commit_listeners.clear();
    HTCompat::stop_timer(idle_redraw_timer);
}

PHTVIEW HTManager::get_view_from_monitor(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return nullptr;
    for (const PHTVIEW& view : views) {
        if (view->get_monitor() != monitor)
            continue;
        return view;
    }
    return nullptr;
}

PHTVIEW HTManager::get_view_from_cursor() {
    return get_view_from_monitor(HTCompat::cursor_monitor());
}

PHTVIEW HTManager::get_view_from_id(VIEWID view_id) {
    for (const PHTVIEW& view : views) {
        if (view->monitor_id != view_id)
            continue;
        return view;
    }
    return nullptr;
}

HTCursorWorkspaceContext HTManager::resolve_cursor_workspace() {
    HTCursorWorkspaceContext context;
    context.monitor = HTCompat::cursor_monitor();
    if (context.monitor == nullptr)
        return context;

    context.view = get_view_from_monitor(context.monitor);
    context.mouse_coords = HTCompat::mouse_coords();
    if (context.view == nullptr)
        return context;

    context.workspace_id = context.view->layout->get_ws_id_from_global(context.mouse_coords);
    context.workspace =
        HTCompat::resolve_workspace_target(context.monitor, context.workspace_id, false);
    return context;
}

void HTManager::show_all_views() {
    for (const PHTVIEW& view : views) {
        view->show();
    }
}

void HTManager::hide_all_views() {
    for (const PHTVIEW& view : views) {
        view->hide();
    }
}

void HTManager::show_cursor_view() {
    const PHTVIEW view = get_view_from_cursor();
    if (view != nullptr)
        view->show();
}

bool HTManager::has_runtime_view() {
    return std::ranges::any_of(views, [](const PHTVIEW& view) {
        return view->has_runtime_activity();
    });
}

// Called whenever a view's active/closing/navigating state changes.
void HTManager::refresh_runtime_activity() {
    if (compat_blocked)
        return;

    HTCompat::set_cursor_override_enabled(has_runtime_view());
    sync_redraw_sources(false);
}

// While any overview is up, redraw it when a window surface commits, and on
// an idle timer that catches what commits miss (subsurfaces, layer surfaces).
void HTManager::sync_redraw_sources(bool rebuild) {
    if (!has_runtime_view()) {
        surface_commit_listeners.clear();
        HTCompat::stop_timer(idle_redraw_timer);
        redraw_sources_active = false;
        return;
    }

    if (redraw_sources_active && !rebuild)
        return;
    redraw_sources_active = true;

    surface_commit_listeners.clear();
    for (const PHLWINDOW& window : HTCompat::compositor_windows()) {
        auto listener = HTCompat::listen_window_surface_commit(
            window,
            [this, window_ref = PHLWINDOWREF {window}] {
                HTPlugin::guardedCallback("surface_commit_redraw", [this, &window_ref] {
                    if (const PHLWINDOW committed = window_ref.lock(); committed != nullptr)
                        request_redraw(HTCompat::window_monitor(committed));
                });
            }
        );
        if (listener)
            surface_commit_listeners.push_back(std::move(listener));
    }

    if (idle_redraw_timer == nullptr) {
        idle_redraw_timer = HTCompat::start_repeating_timer(IDLE_REDRAW_INTERVAL, [this] {
            HTPlugin::guardedCallback("idle_redraw", [this] { request_redraw(nullptr); });
        });
    }
}

void HTManager::on_window_set_changed() {
    if (!redraw_sources_active)
        return;

    sync_redraw_sources(true);
    request_redraw(nullptr);
}

// Redraws overviews with runtime activity, on one monitor or all of them.
void HTManager::request_redraw(PHLMONITOR monitor) {
    for (const PHTVIEW& view : views) {
        if (!view->has_runtime_activity())
            continue;
        if (monitor != nullptr && view->get_monitor() != monitor)
            continue;
        view->layout->request_redraw();
    }
}

PHLWINDOW HTManager::get_dragged_window() {
    return dragged_window.lock();
}

bool HTManager::is_dragged_window(PHLWINDOW window) {
    if (window == nullptr)
        return false;

    return get_dragged_window() == window;
}

void HTManager::clear_dragged_window() {
    dragged_window = {};
}

void HTManager::prepare_for_unload() {
    reset_runtime_controls();
    runtime_disabled = true;
    disabled_source = "plugin_exit";
    disabled_reason = "plugin unloading";
    views.clear();
    refresh_runtime_activity();
}

bool HTManager::runtime_enabled() const {
    return !runtime_disabled;
}

void HTManager::disable_runtime(std::string_view source, std::string_view reason) {
    if (runtime_disabled)
        return;

    runtime_disabled = true;
    disabled_reason = std::string(reason);
    disabled_source = std::string(source);
    reset_runtime_controls();

    Log::logger->log(Log::ERR, "[Hyprtasking] runtime disabled after {}: {}", source, reason);
    HyprlandAPI::addNotification(
        PHANDLE,
        "[Hyprtasking] Disabled for this session after an internal runtime failure. Run hyprctl dispatch hyprtasking:health for details.",
        CHyprColor {1.0, 0.2, 0.2, 1.0},
        5000
    );
}

// Used when the compatibility gate fails: Hyprland internals cannot be trusted,
// so only record the reason and never touch compositor state afterwards.
void HTManager::block_runtime(std::string_view source, std::string_view reason) {
    compat_blocked = true;
    runtime_disabled = true;
    disabled_reason = std::string(reason);
    disabled_source = std::string(source);
    views.clear();
}

std::string HTManager::runtime_health_summary() const {
    const auto active_views =
        std::ranges::count_if(views, [](const PHTVIEW& view) { return view->active; });
    return std::format(
        "runtime_enabled={} disable_reason=\"{}\" failure_source=\"{}\" config_error=\"{}\" views={} active_views={}",
        runtime_enabled(),
        disabled_reason,
        disabled_source,
        HTConfig::config_error(),
        views.size(),
        active_views
    );
}

void HTManager::reset_runtime_controls() {
    if (compat_blocked)
        return;

    // Hand input back first: if a later step throws, the keyboard and cursor
    // must not stay captured by a disabled runtime.
    HTCompat::exit_submap();
    HTCompat::set_cursor_override_enabled(false);

    reset_mouse_button_state();

    for (const PHTVIEW& view : views) {
        view->cancel_runtime_state();
    }

    reset_drag_state();
    reset_swipe_state();
}

PHTVIEW HTManager::get_swipe_view() {
    if (swipe_view_id == INVALID_VIEW_ID)
        return nullptr;

    return get_view_from_id(swipe_view_id);
}

void HTManager::reset_swipe_state() {
    if (const PHTVIEW swipe_view = get_swipe_view(); swipe_view != nullptr)
        swipe_view->set_runtime_state(swipe_view->active, swipe_view->closing, false);

    swipe_state = HT_SWIPE_NONE;
    swipe_amt = 0.0f;
    swipe_view_id = INVALID_VIEW_ID;
    refresh_runtime_activity();
}

void HTManager::abort_drag_controller() {
    const auto target = HTCompat::drag_controller_target();
    if (const PHLWINDOW target_window = target == nullptr ? nullptr : target->window();
        target_window != nullptr) {
        HTCompat::reset_window_workspace_move_alpha(target_window);
    }

    if (HTCompat::drag_controller_mode() != MBIND_INVALID)
        HTCompat::end_drag_controller();
}

void HTManager::reset_drag_state() {
    clear_dragged_window();
    abort_drag_controller();

    HTCompat::set_mouse_bind_mode(MBIND_INVALID);
    if (claimed_mouse_interaction == HT_MOUSE_DRAG)
        reset_mouse_button_state();
}

// Closing the overview or reloading config mid-drag must hand the window back
// to Hyprland; the release that would normally end the drag gets ignored.
void HTManager::cancel_window_drag() {
    if (claimed_mouse_interaction != HT_MOUSE_DRAG && get_dragged_window() == nullptr)
        return;

    reset_drag_state();
}

void HTManager::sync_monitor_views(MONITORID removed_monitor_id) {
    std::vector<HTLogic::MonitorID> view_ids;
    view_ids.reserve(views.size());
    for (const PHTVIEW& view : views) {
        view->refresh_monitor();
        view_ids.push_back(view->monitor_id);
    }

    std::vector<HTLogic::MonitorID> monitor_ids;
    const std::vector<PHLMONITOR> monitors = HTCompat::compositor_monitors();
    monitor_ids.reserve(monitors.size());
    for (const PHLMONITOR& monitor : monitors) {
        if (const MONITORID id = HTCompat::monitor_id(monitor); id != removed_monitor_id)
            monitor_ids.push_back(id);
    }

    const auto stale_ids = HTLogic::staleMonitorViewIDs(view_ids, monitor_ids);
    bool removed_runtime_view = false;
    std::erase_if(views, [&](const PHTVIEW& view) {
        if (std::ranges::find(stale_ids, view->monitor_id) == stale_ids.end())
            return false;

        removed_runtime_view = removed_runtime_view || view->has_runtime_activity();
        view->cancel_runtime_state();
        Log::logger->log(
            LOG,
            "[Hyprtasking] Removing stale view for detached monitor id {}",
            view->monitor_id
        );
        return true;
    });

    const bool removed_swipe_view = swipe_view_id != INVALID_VIEW_ID
        && std::ranges::find(stale_ids, swipe_view_id) != stale_ids.end();
    if (removed_runtime_view || removed_swipe_view) {
        reset_runtime_controls();
    }

    const auto missing_ids = HTLogic::missingMonitorViewIDs(view_ids, monitor_ids);
    for (const auto missing_id : missing_ids) {
        const PHLMONITOR monitor = HTCompat::monitor_from_id(missing_id);
        if (monitor == nullptr)
            continue;

        const Vector2D monitor_size = HTCompat::monitor_transformed_size(monitor);
        views.push_back(makeShared<HTView>(HTCompat::monitor_id(monitor)));
        Log::logger->log(
            LOG,
            "[Hyprtasking] Registering view for monitor {} with resolution {}x{}",
            HTCompat::monitor_description(monitor),
            monitor_size.x,
            monitor_size.y
        );
    }

    std::vector<std::string> monitor_names;
    monitor_names.reserve(monitors.size());
    for (const PHLMONITOR& monitor : monitors)
        monitor_names.push_back(HTCompat::monitor_name(monitor));
    monitor_blocks = HTLogic::assignMonitorBlocks(std::move(monitor_blocks), monitor_names);

    for (const PHLMONITOR& monitor : monitors) {
        const PHTVIEW view = get_view_from_monitor(monitor);
        if (view == nullptr)
            continue;
        const auto block =
            HTLogic::monitorBlockIndex(monitor_blocks, HTCompat::monitor_name(monitor));
        view->layout->set_grid_block(block.value_or(-1));
        view->layout->place_workspaces();
        if (!view->active)
            view->layout->init_position();
    }
}

bool HTManager::has_active_view() {
    if (!runtime_enabled())
        return false;

    for (const auto& view : views) {
        if (view->active)
            return true;
    }
    return false;
}

bool HTManager::has_interactively_active_view() {
    if (!runtime_enabled())
        return false;

    for (const auto& view : views) {
        if (view->active && !view->closing)
            return true;
    }
    return false;
}

bool HTManager::cursor_view_active() {
    if (!runtime_enabled())
        return false;

    const PHTVIEW view = get_view_from_cursor();
    if (view == nullptr)
        return false;
    return view->active;
}
