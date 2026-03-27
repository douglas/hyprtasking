#include "manager.hpp"

#include <algorithm>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/cursor/CursorShapeOverrideController.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include "globals.hpp"
#include "logic/reload_model.hpp"
#include "overview.hpp"
#include "state_guards.hpp"

HTManager::HTManager() {
    reset_swipe_state();
}

PHTVIEW HTManager::get_view_from_monitor(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return nullptr;
    for (PHTVIEW view : views) {
        if (view == nullptr)
            continue;
        if (view->get_monitor() != monitor)
            continue;
        return view;
    }
    return nullptr;
}

PHTVIEW HTManager::get_view_from_cursor() {
    return get_view_from_monitor(g_pCompositor->getMonitorFromCursor());
}

PHTVIEW HTManager::get_view_from_id(VIEWID view_id) {
    for (PHTVIEW view : views) {
        if (view == nullptr)
            continue;
        if (view->monitor_id != view_id)
            continue;
        return view;
    }
    return nullptr;
}

HTCursorWorkspaceContext HTManager::resolve_cursor_workspace(bool create_if_missing) {
    HTCursorWorkspaceContext context;
    context.monitor = g_pCompositor->getMonitorFromCursor();
    if (context.monitor == nullptr)
        return context;

    context.view = get_view_from_monitor(context.monitor);
    context.mouse_coords = g_pInputManager->getMouseCoordsInternal();
    if (context.view == nullptr)
        return context;

    context.workspace_id = context.view->layout->get_ws_id_from_global(context.mouse_coords);
    context.workspace = HTCompat::resolve_workspace_target(
        context.monitor,
        context.workspace_id,
        create_if_missing
    );
    return context;
}

PHLWINDOW HTManager::get_window_from_cursor(bool return_focused) {
    const HTCursorWorkspaceContext cursor_context = resolve_cursor_workspace(false);
    const PHLMONITOR cursor_monitor = cursor_context.monitor;
    if (cursor_monitor == nullptr)
        return nullptr;

    if (return_focused) {
        const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(cursor_monitor);
        if (active_workspace == nullptr)
            return nullptr;
        return active_workspace->getLastFocusedWindow();
    }

    const PHTVIEW cursor_view = cursor_context.view;
    if (cursor_view == nullptr)
        return nullptr;
    const Vector2D mouse_coords = cursor_context.mouse_coords;

    if (!cursor_view->active || !cursor_view->layout->should_manage_mouse()) {
        return g_pCompositor->vectorToWindowUnified(
            mouse_coords,
            Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING
        );
    }
    const WORKSPACEID ws_id = cursor_context.workspace_id;
    const PHLWORKSPACE hovered_workspace = cursor_context.workspace;
    if (hovered_workspace == nullptr)
        return nullptr;

    const auto ws_coords =
        cursor_view->layout->global_to_workspace_monitor_coords(mouse_coords, ws_id);
    if (!ws_coords.has_value())
        return nullptr;

    HTScopedMonitorWorkspace restore_workspace(cursor_monitor, true);
    if (!HTCompat::activate_monitor_workspace(cursor_monitor, hovered_workspace))
        return nullptr;

    const PHLWINDOW hovered_window = g_pCompositor->vectorToWindowUnified(
        *ws_coords,
        Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING
    );

    return hovered_window;
}

void HTManager::show_all_views() {
    for (PHTVIEW view : views) {
        if (view == nullptr)
            continue;
        view->show();
    }
}

void HTManager::hide_all_views() {
    for (PHTVIEW view : views) {
        if (view == nullptr)
            continue;
        view->hide(false);
    }
}

void HTManager::show_cursor_view() {
    const PHTVIEW view = get_view_from_cursor();
    if (view != nullptr)
        view->show();
}

bool HTManager::has_runtime_view() {
    return std::ranges::any_of(views, [](const PHTVIEW& view) {
        return view != nullptr && view->has_runtime_activity();
    });
}

void HTManager::refresh_cursor_override() {
    if (has_runtime_view()) {
        Cursor::overrideController->setOverride("left_ptr", Cursor::CURSOR_OVERRIDE_UNKNOWN);
    } else {
        Cursor::overrideController->unsetOverride(Cursor::CURSOR_OVERRIDE_UNKNOWN);
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

void HTManager::set_dragged_window(PHLWINDOW window) {
    dragged_window = window;
}

void HTManager::clear_dragged_window() {
    dragged_window = {};
}

void HTManager::reset() {
    runtime_disabled = false;
    disabled_reason.clear();
    clear_dragged_window();
    reset_drag_state();
    reset_swipe_state();
    views.clear();
    refresh_cursor_override();
}

bool HTManager::runtime_enabled() const {
    return !runtime_disabled;
}

void HTManager::disable_runtime(std::string_view source, std::string_view reason) {
    if (runtime_disabled)
        return;

    runtime_disabled = true;
    disabled_reason = std::string(reason);

    for (const PHTVIEW& view : views) {
        if (view == nullptr)
            continue;
        view->cancel_runtime_state();
    }

    reset_drag_state();
    reset_swipe_state();
    refresh_cursor_override();

    Log::logger->log(
        Log::ERR,
        "[Hyprtasking] runtime disabled after {}: {}",
        source,
        reason
    );
    HyprlandAPI::addNotification(
        PHANDLE,
        "[Hyprtasking] Disabled for this session after an internal runtime failure.",
        CHyprColor {1.0, 0.2, 0.2, 1.0},
        5000
    );
}

std::string HTManager::runtime_disable_reason() const {
    return disabled_reason;
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
    refresh_cursor_override();
}

void HTManager::reset_drag_state() {
    clear_dragged_window();

    if (!g_layoutManager)
        return;

    const auto& drag_controller = g_layoutManager->dragController();
    if (!drag_controller)
        return;

    const auto target = drag_controller->target();
    if (const PHLWINDOW dragged_window = target == nullptr ? nullptr : target->window();
        dragged_window != nullptr) {
        dragged_window->m_movingToWorkspaceAlpha->setValueAndWarp(1.0);
        dragged_window->m_movingFromWorkspaceAlpha->setValueAndWarp(1.0);
    }

    if (drag_controller->mode() != MBIND_INVALID)
        drag_controller->dragEnd();

    g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
}

void HTManager::sync_monitor_views() {
    std::vector<HTLogic::MonitorID> view_ids;
    view_ids.reserve(views.size());
    for (const PHTVIEW& view : views) {
        if (view == nullptr)
            continue;
        view_ids.push_back(view->monitor_id);
    }

    std::vector<HTLogic::MonitorID> monitor_ids;
    monitor_ids.reserve(g_pCompositor->m_monitors.size());
    for (const PHLMONITOR& monitor : g_pCompositor->m_monitors) {
        if (monitor == nullptr)
            continue;
        monitor_ids.push_back(monitor->m_id);
    }

    const auto stale_ids = HTLogic::staleMonitorViewIDs(view_ids, monitor_ids);
    bool removed_runtime_view = false;
    std::erase_if(views, [&](const PHTVIEW& view) {
        if (view == nullptr)
            return true;

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

    const bool removed_swipe_view =
        swipe_view_id != INVALID_VIEW_ID && std::ranges::find(stale_ids, swipe_view_id) != stale_ids.end();
    if (removed_runtime_view || removed_swipe_view) {
        reset_drag_state();
        reset_swipe_state();
        refresh_cursor_override();
    }

    const auto missing_ids = HTLogic::missingMonitorViewIDs(view_ids, monitor_ids);
    for (const auto missing_id : missing_ids) {
        const PHLMONITOR monitor = g_pCompositor->getMonitorFromID(missing_id);
        if (monitor == nullptr)
            continue;

        views.push_back(makeShared<HTView>(monitor->m_id));
        Log::logger->log(
            LOG,
            "[Hyprtasking] Registering view for monitor {} with resolution {}x{}",
            monitor->m_description,
            monitor->m_transformedSize.x,
            monitor->m_transformedSize.y
        );
    }

    for (const PHLMONITOR& monitor : g_pCompositor->m_monitors) {
        const PHTVIEW view = get_view_from_monitor(monitor);
        if (view == nullptr || view->active)
            continue;
        view->layout->init_position();
    }
}

bool HTManager::has_active_view() {
    if (!runtime_enabled())
        return false;

    for (const auto& view : views) {
        if (view == nullptr)
            continue;
        if (view->active)
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
