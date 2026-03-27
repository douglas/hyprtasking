#include "overview.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/macros.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/cursor/CursorShapeOverrideController.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprutils/math/Box.hpp>

#include "config.hpp"
#include "compat/renderer_compat.hpp"
#include "compat/runtime_compat.hpp"
#include "globals.hpp"
#include "layout/grid.hpp"
#include "layout/linear.hpp"
#include "logic/controller_state.hpp"
#include "logic/reload_model.hpp"

HTView::HTView(MONITORID in_monitor_id) {
    monitor_id = in_monitor_id;
    set_runtime_state(false, false, false);

    change_layout(HTConfig::value<Hyprlang::STRING>("layout"));
}

void HTView::change_layout(const std::string& layout_name) {
    if (layout != nullptr && layout->layout_name() == layout_name) {
        layout->init_position();
        return;
    }

    if (layout_name == "grid") {
        layout = makeShared<HTLayoutGrid>(monitor_id);
    } else if (layout_name == "linear") {
        layout = makeShared<HTLayoutLinear>(monitor_id);
    } else {
        fail_exit(
            "Bad overview layout name {}, supported ones are 'grid' and 'linear'",
            layout_name
        );
    }
}

void HTView::do_exit_behavior(bool exit_on_mouse) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr) //???
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;

    const HTCursorWorkspaceContext cursor_context =
        ht_manager == nullptr ? HTCursorWorkspaceContext {} : ht_manager->resolve_cursor_workspace(false);
    const WORKSPACEID hovered_workspace_id =
        cursor_context.monitor == monitor ? cursor_context.workspace_id : WORKSPACE_INVALID;

    const int EXIT_ON_HOVERED = HTConfig::value<Hyprlang::INT>("exit_on_hovered");
    const WORKSPACEID ws_id = HTLogic::resolveExitWorkspaceID(
        exit_on_mouse || EXIT_ON_HOVERED,
        hovered_workspace_id,
        HTCompat::workspace_id(active_workspace)
    );
    const PHLWORKSPACE workspace =
        HTCompat::resolve_workspace_target(monitor, ws_id, true);
    if (workspace == nullptr)
        return;

    HTCompat::activate_monitor_workspace(monitor, workspace);
}

void HTView::show() {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;

    set_runtime_state(true, false, false);

    layout->on_show();

    HTCompat::damage_monitor(monitor);
    HTCompat::schedule_frame_for_monitor(monitor);
}

void HTView::hide(bool exit_on_mouse) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;

    do_exit_behavior(exit_on_mouse);

    set_runtime_state(true, true, false);

    layout->on_hide([this](auto self) {
        set_runtime_state(false, false, false);
    });

    HTCompat::damage_monitor(monitor);
    HTCompat::schedule_frame_for_monitor(monitor);
}

void HTView::set_runtime_state(bool new_active, bool new_closing, bool new_navigating) {
    active = new_active;
    closing = new_closing;
    navigating = new_navigating;

    if (ht_manager != nullptr)
        ht_manager->refresh_cursor_override();
}

void HTView::cancel_runtime_state() {
    if (layout != nullptr)
        layout->cancel_animation_callbacks();

    set_runtime_state(false, false, false);
}

bool HTView::has_runtime_activity() const {
    return active || closing || navigating;
}

void HTView::reload_config(bool close_overview_on_reload, const std::string& new_layout) {
    const bool layout_changed = layout == nullptr || layout->layout_name() != new_layout;
    const auto decision = HTLogic::decideViewReload(
        close_overview_on_reload,
        layout_changed,
        active,
        closing,
        navigating
    );

    if (decision.cancel_runtime_state) {
        cancel_runtime_state();
    }

    if (decision.change_layout_now)
        change_layout(new_layout);

    if (decision.reinitialize_position && layout != nullptr)
        layout->init_position();

    const PHLMONITOR monitor = get_monitor();
    if (monitor != nullptr) {
        HTCompat::damage_monitor(monitor);
        HTCompat::schedule_frame_for_monitor(monitor);
    }
}

void HTView::warp_window(Hyprlang::INT warp, PHLWINDOW window) {
    if (warp > 0 && HTCompat::can_warp_window_cursor(window))
        HTCompat::warp_window_cursor(window, warp == 2);
}

void HTView::move_id(WORKSPACEID ws_id, bool move_window) {
    set_runtime_state(active, closing, false);
    if (closing || ws_id == WORKSPACE_INVALID)
        return;
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;

    const PHLWINDOW hovered_window = move_window ? ht_manager->get_window_from_cursor() : nullptr;
    const bool has_hovered_window = hovered_window != nullptr;
    const auto move_execution = HTLogic::resolveMoveExecution(move_window, has_hovered_window);
    if (!move_execution.valid)
        return;

    const PHLWORKSPACE other_workspace =
        HTCompat::resolve_workspace_target(monitor, ws_id, true);
    if (other_workspace == nullptr)
        return;

    if (move_execution.move_hovered_window) {
        HTCompat::move_window_to_workspace(hovered_window, other_workspace);
    }

    Hyprlang::INT warp;

    if (!HTCompat::activate_monitor_workspace(monitor, other_workspace))
        return;
    if (move_execution.focus_moved_window) {
        HTCompat::focus_window(hovered_window);
    }

    if (move_execution.use_move_window_warp) {
        warp = *CConfigValue<Hyprlang::INT>("plugin:hyprtasking:warp_on_move_window");
    } else {
        warp = *CConfigValue<Hyprlang::INT>("cursor:warp_on_change_workspace");
    }
    warp_window(warp, hovered_window);

    const bool was_active = active;
    const bool was_closing = closing;
    set_runtime_state(was_active, was_closing, true);
    layout->on_move(
        HTCompat::workspace_id(active_workspace),
        HTCompat::workspace_id(other_workspace),
        [this, was_active, was_closing](auto self) {
            set_runtime_state(was_active, was_closing, false);
        }
    );
}

void HTView::move(std::string arg, bool move_window) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;
    const PHLWINDOW hovered_window = move_window ? ht_manager->get_window_from_cursor() : nullptr;
    const std::optional<WORKSPACEID> hovered_workspace_id =
        hovered_window == nullptr ? std::nullopt : std::optional<WORKSPACEID> {hovered_window->workspaceID()};

    // if moving a window, the up/down/left/right should be relative to the window (and cursor) and not necessarily the active workspace
    const WORKSPACEID source_ws_id =
        HTLogic::resolveMoveSourceWorkspace(
            move_window,
            HTCompat::workspace_id(active_workspace),
            hovered_workspace_id
        );
    if (source_ws_id == WORKSPACE_INVALID)
        return;

    layout->build_overview_layout(HT_VIEW_CLOSED);
    const auto* ws_layout = layout->find_layout_workspace(source_ws_id);
    if (ws_layout == nullptr)
        return;
    const WORKSPACEID id = layout->get_ws_id_in_direction(ws_layout->x, ws_layout->y, arg);
    if (id == WORKSPACE_INVALID)
        return;

    move_id(id, move_window);
}

PHLMONITOR HTView::get_monitor() {
    const PHLMONITOR monitor = HTCompat::monitor_from_id(monitor_id);
    if (monitor == nullptr)
        Log::logger->log(Log::WARN, "[Hyprtasking] Returning null monitor from get_monitor!");
    return monitor;
}
