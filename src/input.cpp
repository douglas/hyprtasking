#include <cmath>
#include <format>

#include "compat/renderer_compat.hpp"
#include "compat/runtime_compat.hpp"
#include "config.hpp"
#include "logic/controller_state.hpp"
#include "logic/geometry_model.hpp"
#include "logic/gesture_model.hpp"
#include "logic/interaction_model.hpp"
#include "manager.hpp"
#include "overview.hpp"
#include "scope_guard.hpp"
#include "state_guards.hpp"

void HTManager::reset_mouse_button_state() {
    claimed_mouse_button = 0;
    claimed_mouse_interaction = HT_MOUSE_NONE;
    claimed_mouse_view_id = INVALID_VIEW_ID;
    pending_mouse_selection_workspace_id = WORKSPACE_INVALID;
}

HTLogic::MouseButtonResult HTManager::handle_mouse_button(unsigned int button, bool pressed) {
    const HTCursorWorkspaceContext cursor_context = resolve_cursor_workspace();
    const PHTVIEW cursor_view = cursor_context.view;
    const bool has_view = cursor_view != nullptr;
    const bool view_active = cursor_view != nullptr && cursor_view->active;
    const bool view_closing = cursor_view != nullptr && cursor_view->closing;
    const auto& config = HTConfig::runtime_config();
    const unsigned int drag_button = static_cast<unsigned int>(config.drag_button);
    const unsigned int select_button = static_cast<unsigned int>(config.select_button);

    if (pressed) {
        const bool is_drag_button = button == drag_button;
        const bool is_select_button = button == select_button;
        const bool consume_press = HTLogic::canStartInteraction(
            has_view,
            view_active,
            view_closing,
            is_drag_button || is_select_button
        );
        if (!consume_press)
            return HTLogic::MouseButtonResult::Ignore;
        // Another managed button already owns the interaction; re-claiming would
        // orphan its release and leave a drag attached to the cursor. The same
        // button may re-claim, so a lost release cannot block input for good.
        if (claimed_mouse_button != 0 && claimed_mouse_button != button)
            return HTLogic::MouseButtonResult::Consume;

        reset_mouse_button_state();
        claimed_mouse_button = button;
        claimed_mouse_view_id = cursor_view->monitor_id;

        if (cursor_context.workspace == nullptr)
            return HTLogic::MouseButtonResult::Consume;

        if (is_drag_button) {
            if (start_window_drag(cursor_context))
                claimed_mouse_interaction = HT_MOUSE_DRAG;
        } else {
            claimed_mouse_interaction = HT_MOUSE_SELECT;
            pending_mouse_selection_workspace_id = cursor_context.workspace_id;
            cursor_view->hover_workspace(cursor_context.workspace_id);
        }
        return HTLogic::MouseButtonResult::Consume;
    }

    if (claimed_mouse_button == 0 || claimed_mouse_button != button)
        return HTLogic::MouseButtonResult::Ignore;

    if (claimed_mouse_interaction == HT_MOUSE_SELECT) {
        // Commit only if the view the press selected on is still open.
        const PHTVIEW view = get_view_from_id(claimed_mouse_view_id);
        if (view != nullptr && view->active && !view->closing)
            view->commit_mouse_selection(pending_mouse_selection_workspace_id);
    } else if (claimed_mouse_interaction == HT_MOUSE_DRAG) {
        end_window_drag();
    }
    reset_mouse_button_state();
    return HTLogic::MouseButtonResult::Consume;
}

// cursor_context has an open, non-closing view and a workspace under the cursor.
bool HTManager::start_window_drag(const HTCursorWorkspaceContext& cursor_context) {
    clear_dragged_window();

    const PHLMONITOR cursor_monitor = cursor_context.monitor;
    const PHTVIEW cursor_view = cursor_context.view;
    const Vector2D mouse_coords = cursor_context.mouse_coords;
    const WORKSPACEID workspace_id = cursor_context.workspace_id;
    const PHLWORKSPACE cursor_workspace = cursor_context.workspace;

    HTScopedMonitorWorkspace restore_workspace(cursor_monitor);
    if (!HTCompat::activate_monitor_workspace_internal(cursor_monitor, cursor_workspace))
        return false;
    HTScopedWorkspaceVisibility visible_workspace(cursor_workspace, true);

    const auto workspace_coords =
        cursor_view->layout->global_to_workspace_monitor_coords(mouse_coords, workspace_id);
    if (!workspace_coords.has_value())
        return false;
    const PHLWINDOW hovered_window = HTCompat::hover_target_window_at(*workspace_coords);
    if (!HTCompat::warp_pointer(*workspace_coords))
        return false;

    bool reset_mouse_bind_mode = false;
    HTScopeGuard reset_drag_mode([&reset_mouse_bind_mode] {
        if (reset_mouse_bind_mode)
            HTCompat::set_mouse_bind_mode(MBIND_INVALID);
    });

    bool end_drag_on_failure = false;
    HTScopeGuard end_failed_drag([this, &end_drag_on_failure] {
        if (end_drag_on_failure)
            abort_drag_controller();
    });

    HTCompat::set_mouse_bind_mode(MBIND_MOVE);
    reset_mouse_bind_mode = true;
    end_drag_on_failure = true;
    if (HTCompat::drag_controller_target() == nullptr && hovered_window != nullptr)
        HTCompat::begin_drag_window(hovered_window, MBIND_MOVE);

    if (!HTCompat::warp_pointer(mouse_coords))
        return false;
    HTCompat::simulate_mouse_movement();

    SP<Layout::ITarget> target = HTCompat::drag_controller_target();
    if (target == nullptr) {
        if (hovered_window != nullptr && HTCompat::begin_drag_window(hovered_window, MBIND_MOVE)) {
            target = HTCompat::drag_controller_target();
        }
    }
    if (target == nullptr)
        return false;

    const PHLWINDOW target_window = target->window();
    if (target_window != nullptr) {
        if (HTCompat::drag_controller_is_tiled()) {
            const auto inverse_drag_scale =
                HTLogic::inverseDragWindowScale(cursor_view->layout->drag_window_scale());
            if (!inverse_drag_scale.has_value())
                return false;
            const PHLMONITOR dragged_monitor = HTCompat::window_monitor(target_window);
            if (dragged_monitor == nullptr)
                return false;
            const Vector2D dragged_monitor_pos = HTCompat::monitor_position(dragged_monitor);

            const Vector2D pre_pos = cursor_view->layout->local_ws_unscaled_to_global(
                HTCompat::window_real_position(target_window) - dragged_monitor_pos,
                workspace_id
            );
            const Vector2D post_pos = cursor_view->layout->local_ws_unscaled_to_global(
                HTCompat::window_real_position_goal(target_window) - dragged_monitor_pos,
                workspace_id
            );
            const Vector2D mapped_pre_pos =
                (pre_pos - mouse_coords) * *inverse_drag_scale + mouse_coords;
            const Vector2D mapped_post_pos =
                (post_pos - mouse_coords) * *inverse_drag_scale + mouse_coords;

            HTCompat::set_window_real_position(target_window, mapped_pre_pos);
            HTCompat::set_window_real_position_goal(target_window, mapped_post_pos);
        } else {
            HTCompat::simulate_mouse_movement();
        }
    }

    dragged_window = target_window;
    reset_mouse_bind_mode = false;
    end_drag_on_failure = false;
    restore_workspace.dismiss();

    return true;
}

void HTManager::end_window_drag() {
    const HTCursorWorkspaceContext cursor_context = resolve_cursor_workspace();
    const PHLMONITOR cursor_monitor = cursor_context.monitor;
    const PHTVIEW cursor_view = cursor_context.view;
    HTScopeGuard reset_drag_mode([] { HTCompat::set_mouse_bind_mode(MBIND_INVALID); });
    HTScopeGuard clear_dragged_window_guard([this] { clear_dragged_window(); });
    bool drop_completed = false;
    HTScopeGuard end_unfinished_drag([this, &drop_completed] {
        if (!drop_completed)
            abort_drag_controller();
    });
    const SP<Layout::ITarget> target = HTCompat::drag_controller_target();
    const PHLWINDOW dragged_window = target == nullptr ? nullptr : target->window();
    if (cursor_monitor == nullptr || cursor_view == nullptr || dragged_window == nullptr
        || HTCompat::drag_controller_mode() != MBIND_MOVE
        || !HTLogic::canStartInteraction(true, cursor_view->active, cursor_view->closing, true))
        return;

    const Vector2D mouse_coords = cursor_context.mouse_coords;
    Vector2D use_mouse_coords = mouse_coords;
    const WORKSPACEID hovered_workspace_id = cursor_context.workspace_id;
    const PHLWORKSPACE dragged_workspace = HTCompat::window_workspace(dragged_window);
    const std::optional<WORKSPACEID> dragged_workspace_id = dragged_workspace == nullptr
        ? std::nullopt
        : std::optional<WORKSPACEID> {HTCompat::workspace_id(dragged_workspace)};
    const auto drop_decision =
        HTLogic::resolveDropWorkspace(hovered_workspace_id, dragged_workspace_id);
    if (!drop_decision.valid) {
        Log::logger->log(LOG, "[Hyprtasking] tried to drop on null workspace??");
        return;
    }

    PHLWORKSPACE cursor_workspace = HTCompat::resolve_workspace_target(
        cursor_monitor,
        drop_decision.workspace_id,
        drop_decision.create_if_missing
    );

    if (cursor_workspace == nullptr) {
        Log::logger->log(
            LOG,
            "[Hyprtasking] drop target workspace {} could not be resolved",
            drop_decision.workspace_id
        );
        return;
    }

    const WORKSPACEID drop_workspace_id = HTCompat::workspace_id(cursor_workspace);
    if (drop_decision.snap_to_workspace) {
        // Ensure that the mouse coords are snapped to inside the workspace box itself
        use_mouse_coords = cursor_view->layout->get_global_ws_box(drop_workspace_id)
                               .closestPoint(use_mouse_coords);

        Log::logger->log(
            LOG,
            "[Hyprtasking] Dragging to invalid position, snapping to last ws {}",
            drop_workspace_id
        );
    }

    Log::logger->log(LOG, "[Hyprtasking] trying to drop window on ws {}", drop_workspace_id);

    const auto workspace_coords = cursor_view->layout->global_to_workspace_monitor_coords(
        use_mouse_coords,
        drop_workspace_id
    );
    if (!workspace_coords.has_value())
        return;
    const auto drag_scale = HTLogic::dragWindowScale(cursor_view->layout->drag_window_scale());
    if (!drag_scale.has_value())
        return;
    const Vector2D cursor_monitor_pos = HTCompat::monitor_position(cursor_monitor);

    const Vector2D warped_global_pos =
        cursor_view->layout->global_to_local_ws_unscaled(
            (HTCompat::window_real_position(dragged_window) - use_mouse_coords) * *drag_scale
                + use_mouse_coords,
            drop_workspace_id
        )
        + cursor_monitor_pos;
    if (!HTLogic::isFinitePoint(warped_global_pos.x, warped_global_pos.y))
        return;

    HTScopedMonitorWorkspace restore_workspace(cursor_monitor);
    if (!HTCompat::activate_monitor_workspace_internal(cursor_monitor, cursor_workspace))
        return;
    HTScopedWorkspaceVisibility visible_workspace(cursor_workspace, true);

    HTCompat::move_window_to_workspace(dragged_window, cursor_workspace);
    HTCompat::set_window_real_position(dragged_window, warped_global_pos);

    if (!HTCompat::warp_pointer(*workspace_coords))
        return;
    HTCompat::set_mouse_bind_mode(MBIND_INVALID);
    if (!HTCompat::warp_pointer(mouse_coords))
        return;

    // otherwise the window leaves blur (?) artifacts on all
    // workspaces
    HTCompat::reset_window_workspace_move_alpha(dragged_window);
    restore_workspace.dismiss();
    drop_completed = true;
}

void HTManager::on_mouse_move() {
    const HTCursorWorkspaceContext cursor_context = resolve_cursor_workspace();
    const PHTVIEW cursor_view = cursor_context.view;
    // A view implies the cursor monitor was found.
    if (cursor_view == nullptr || !cursor_view->active || cursor_view->closing)
        return;

    if (cursor_context.workspace == nullptr) {
        if (cursor_view->hovered_workspace_id != WORKSPACE_INVALID) {
            cursor_view->clear_hover_workspace();
            HTCompat::redraw_monitor(cursor_context.monitor);
        }
        return;
    }

    // A pending selection keeps the cell it was pressed on.
    if (claimed_mouse_interaction == HT_MOUSE_SELECT)
        return;

    if (cursor_view->hover_suppressed_workspace_id != WORKSPACE_INVALID) {
        if (cursor_context.workspace_id == cursor_view->hover_suppressed_workspace_id)
            return;

        cursor_view->clear_hover_suppression();
    }

    if (cursor_view->hovered_workspace_id != cursor_context.workspace_id) {
        cursor_view->hover_workspace(cursor_context.workspace_id);
        HTCompat::redraw_monitor(cursor_context.monitor);
    }
}

bool HTManager::swipe_update(IPointer::SSwipeUpdateEvent e) {
    const auto& config = HTConfig::runtime_config();
    if (!config.gestures_enabled) {
        reset_swipe_state();
        return false;
    }

    const PHTVIEW swipe_view = swipe_state == HT_SWIPE_NONE
        ? get_view_from_monitor(HTCompat::cursor_monitor())
        : get_swipe_view();
    if (swipe_view == nullptr || swipe_view->closing) {
        reset_swipe_state();
        return false;
    }

    HTConfig::refresh_runtime_config("gesture_config_validation");
    const uint32_t open_fingers = static_cast<uint32_t>(config.open_fingers);
    const uint32_t move_fingers = static_cast<uint32_t>(config.move_fingers);

    if (e.fingers == open_fingers) {
        const bool res = swipe_view->active || swipe_state == HT_SWIPE_OPEN;
        const float deltaY = config.open_positive ? e.delta.y : -e.delta.y;

        if (swipe_state != HT_SWIPE_OPEN) {
            const auto action = HTLogic::resolveOpenSwipeStart(
                std::abs(e.delta.y) > std::abs(e.delta.x),
                swipe_view->active,
                swipe_view->navigating,
                deltaY
            );
            if (action == HTLogic::OpenSwipeStartAction::None)
                return res;

            const bool show = action == HTLogic::OpenSwipeStartAction::ShowOverview;
            if (show)
                swipe_view->show();
            swipe_state = HT_SWIPE_OPEN;
            swipe_started_open = !show;
            swipe_view_id = swipe_view->monitor_id;
            swipe_amt = show ? config.open_distance : 0.F;
        }

        swipe_amt += deltaY;
        swipe_view->layout->close_open_lerp(
            HTLogic::openSwipeProgress(swipe_amt, config.open_distance)
        );
        if (const PHLMONITOR swipe_monitor = swipe_view->get_monitor(); swipe_monitor != nullptr)
            HTCompat::redraw_monitor(swipe_monitor);
        return res;
    }

    if (e.fingers != move_fingers)
        return false;

    const bool res = swipe_state == HT_SWIPE_MOVE;
    if (swipe_state != HT_SWIPE_MOVE) {
        if (!HTLogic::shouldStartMoveSwipe(swipe_view->active, swipe_view->navigating))
            return res;

        swipe_state = HT_SWIPE_MOVE;
        swipe_view_id = swipe_view->monitor_id;
        swipe_view->set_runtime_state(swipe_view->active, swipe_view->closing, true);

        // need to schedule frames for monitor, otherwise the screen doesn't re-render
        if (const PHLMONITOR swipe_monitor = swipe_view->get_monitor(); swipe_monitor != nullptr)
            HTCompat::redraw_monitor(swipe_monitor);
    }

    if (!swipe_view->navigating) {
        reset_swipe_state();
        return res;
    }
    swipe_view->layout->on_move_swipe(e.delta);
    return res;
}

// A cancelled gesture (e.g. a finger lifted mid-swipe) returns to where it started.
bool HTManager::swipe_end(bool cancelled) {
    if (swipe_state == HT_SWIPE_NONE)
        return false;

    const PHTVIEW swipe_view = get_swipe_view();
    if (swipe_view == nullptr || swipe_view->closing) {
        reset_swipe_state();
        return false;
    }

    switch (swipe_state) {
        case HT_SWIPE_OPEN: {
            const bool keep_open = cancelled ? swipe_started_open
                                             : HTLogic::shouldKeepOverviewOpen(
                                                   swipe_amt,
                                                   HTConfig::runtime_config().open_distance
                                               );
            if (keep_open)
                swipe_view->show();
            else
                swipe_view->hide();
            break;
        }
        case HT_SWIPE_MOVE: {
            if (!swipe_view->navigating) {
                reset_swipe_state();
                return false;
            }
            WORKSPACEID ws_id = WORKSPACE_INVALID;
            if (!cancelled)
                ws_id = swipe_view->layout->on_move_swipe_end();
            else if (
                const PHLWORKSPACE active =
                    HTCompat::active_monitor_workspace(swipe_view->get_monitor());
                active != nullptr
            )
                ws_id = HTCompat::workspace_id(active);
            swipe_view->move_id(ws_id);
            break;
        }
        case HT_SWIPE_NONE:
            break;
    }

    reset_swipe_state();
    return true;
}
