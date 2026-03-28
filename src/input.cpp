#include <linux/input-event-codes.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/macros.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "config.hpp"
#include "compat/renderer_compat.hpp"
#include "compat/runtime_compat.hpp"
#include "logic/controller_state.hpp"
#include "logic/gesture_model.hpp"
#include "logic/geometry_model.hpp"
#include "logic/interaction_model.hpp"
#include "manager.hpp"
#include "overview.hpp"
#include "state_guards.hpp"

using Hyprutils::Utils::CScopeGuard;

bool HTManager::start_window_drag() {
    clear_dragged_window();

    const HTCursorWorkspaceContext cursor_context = resolve_cursor_workspace(false);
    const PHLMONITOR cursor_monitor = cursor_context.monitor;
    const PHTVIEW cursor_view = cursor_context.view;
    const bool manages_mouse = cursor_view != nullptr && cursor_view->layout->should_manage_mouse();
    const Vector2D mouse_coords = cursor_context.mouse_coords;
    const WORKSPACEID workspace_id = cursor_context.workspace_id;
    PHLWORKSPACE cursor_workspace = cursor_context.workspace;

    switch (HTLogic::decideDragStart(
        cursor_view != nullptr,
        cursor_view != nullptr && cursor_view->active,
        cursor_view != nullptr && cursor_view->closing,
        manages_mouse,
        cursor_workspace != nullptr
    )) {
        case HTLogic::DragStartAction::Ignore:
            return false;
        case HTLogic::DragStartAction::HideViews:
            // hide all views if should not manage mouse but active
            hide_all_views();
            return false;
        case HTLogic::DragStartAction::BeginDrag:
            break;
    }

    if (cursor_monitor == nullptr || cursor_view == nullptr) {
        return false;
    }

    if (!manages_mouse) {
        // hide all views if should not manage mouse but active
        hide_all_views();
        return false;
    }

    HTScopedMonitorWorkspace restore_workspace(cursor_monitor, true);
    if (!HTCompat::activate_monitor_workspace_internal(cursor_monitor, cursor_workspace))
        return true;
    HTScopedWorkspaceVisibility visible_workspace(cursor_workspace, true);

    const auto workspace_coords =
        cursor_view->layout->global_to_workspace_monitor_coords(mouse_coords, workspace_id);
    if (!workspace_coords.has_value())
        return true;
    const PHLWINDOW hovered_window = get_window_from_cursor(false);
    if (hovered_window == nullptr)
        return true;
    if (!HTCompat::warp_pointer(*workspace_coords))
        return true;

    bool reset_mouse_bind_mode = false;
    CScopeGuard reset_drag_mode([&reset_mouse_bind_mode] {
        if (reset_mouse_bind_mode)
            HTCompat::set_mouse_bind_mode(MBIND_INVALID);
    });

    HTCompat::set_mouse_bind_mode(MBIND_MOVE);
    reset_mouse_bind_mode = true;

    if (HTCompat::drag_controller_target() == nullptr
        && !HTCompat::begin_drag_window(hovered_window, MBIND_MOVE))
        return true;

    if (!HTCompat::warp_pointer(mouse_coords))
        return true;

    const SP<Layout::ITarget> target = HTCompat::drag_controller_target();
    if (target == nullptr)
        return true;

    const PHLWINDOW dragged_window = target->window();
    if (dragged_window != nullptr) {
        if (HTCompat::drag_controller_is_tiled()) {
            const auto inverse_drag_scale =
                HTLogic::inverseDragWindowScale(cursor_view->layout->drag_window_scale());
            if (!inverse_drag_scale.has_value())
                return true;
            const PHLMONITOR dragged_monitor = HTCompat::window_monitor(dragged_window);
            if (dragged_monitor == nullptr)
                return true;
            const Vector2D dragged_monitor_pos = HTCompat::monitor_position(dragged_monitor);

            const Vector2D pre_pos = cursor_view->layout->local_ws_unscaled_to_global(
                HTCompat::window_real_position(dragged_window) - dragged_monitor_pos,
                workspace_id
            );
            const Vector2D post_pos = cursor_view->layout->local_ws_unscaled_to_global(
                HTCompat::window_real_position_goal(dragged_window) - dragged_monitor_pos,
                workspace_id
            );
            const Vector2D mapped_pre_pos =
                (pre_pos - mouse_coords) * *inverse_drag_scale + mouse_coords;
            const Vector2D mapped_post_pos =
                (post_pos - mouse_coords) * *inverse_drag_scale + mouse_coords;

            HTCompat::set_window_real_position(dragged_window, mapped_pre_pos);
            HTCompat::set_window_real_position_goal(dragged_window, mapped_post_pos);
        } else {
            HTCompat::simulate_mouse_movement();
        }
    }

    set_dragged_window(dragged_window);
    reset_mouse_bind_mode = false;
    restore_workspace.dismiss();

    return true;
}

bool HTManager::end_window_drag() {
    const HTCursorWorkspaceContext cursor_context = resolve_cursor_workspace(false);
    const PHLMONITOR cursor_monitor = cursor_context.monitor;
    const PHTVIEW cursor_view = cursor_context.view;
    CScopeGuard reset_drag_mode([] { HTCompat::set_mouse_bind_mode(MBIND_INVALID); });
    CScopeGuard clear_dragged_window_guard([this] { clear_dragged_window(); });
    const bool has_view = cursor_view != nullptr;
    const bool manages_mouse = cursor_view != nullptr && cursor_view->layout->should_manage_mouse();
    const SP<Layout::ITarget> target = HTCompat::drag_controller_target();
    const PHLWINDOW dragged_window = target == nullptr ? nullptr : target->window();
    const bool move_mode = HTCompat::drag_controller_mode() == MBIND_MOVE;

    if (HTLogic::decideDragEnd(
            has_view,
            cursor_view != nullptr && cursor_view->active,
            cursor_view != nullptr && cursor_view->closing,
            manages_mouse,
            target != nullptr,
            dragged_window != nullptr,
            move_mode
        )
        != HTLogic::DragEndAction::FinalizeDrop) {
        return false;
    }

    if (cursor_monitor == nullptr || cursor_view == nullptr || dragged_window == nullptr)
        return false;

    const Vector2D mouse_coords = cursor_context.mouse_coords;
    Vector2D use_mouse_coords = mouse_coords;
    const WORKSPACEID hovered_workspace_id = cursor_context.workspace_id;
    const PHLWORKSPACE dragged_workspace = HTCompat::window_workspace(dragged_window);
    const std::optional<WORKSPACEID> dragged_workspace_id =
        dragged_workspace == nullptr
            ? std::nullopt
            : std::optional<WORKSPACEID> {HTCompat::workspace_id(dragged_workspace)};
    const auto drop_decision =
        HTLogic::resolveDropWorkspace(hovered_workspace_id, dragged_workspace_id);
    if (!drop_decision.valid) {
        Log::logger->log(LOG, "[Hyprtasking] tried to drop on null workspace??");
        return false;
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
        return false;
    }

    if (drop_decision.snap_to_workspace) {
        // Ensure that the mouse coords are snapped to inside the workspace box itself
        use_mouse_coords = cursor_view->layout
                               ->get_global_ws_box(HTCompat::workspace_id(cursor_workspace))
                               .closestPoint(use_mouse_coords);

        Log::logger->log(
            LOG,
            "[Hyprtasking] Dragging to invalid position, snapping to last ws {}",
            HTCompat::workspace_id(cursor_workspace)
        );
    }

    Log::logger->log(
        LOG,
        "[Hyprtasking] trying to drop window on ws {}",
        HTCompat::workspace_id(cursor_workspace)
    );

    const auto workspace_coords = cursor_view->layout->global_to_workspace_monitor_coords(
        use_mouse_coords,
        HTCompat::workspace_id(cursor_workspace)
    );
    if (!workspace_coords.has_value())
        return false;
    const auto drag_scale = HTLogic::dragWindowScale(cursor_view->layout->drag_window_scale());
    if (!drag_scale.has_value())
        return false;
    const Vector2D cursor_monitor_pos = HTCompat::monitor_position(cursor_monitor);

    const Vector2D warped_global_pos = cursor_view->layout->global_to_local_ws_unscaled(
                                (HTCompat::window_real_position(dragged_window) - use_mouse_coords)
                                        * *drag_scale
                                    + use_mouse_coords,
                                HTCompat::workspace_id(cursor_workspace)
                            ) + cursor_monitor_pos;
    if (!HTLogic::isFinitePoint(warped_global_pos.x, warped_global_pos.y))
        return false;
    const Vector2D tp_pos = warped_global_pos;

    HTScopedMonitorWorkspace restore_workspace(cursor_monitor, true);
    if (!HTCompat::activate_monitor_workspace_internal(cursor_monitor, cursor_workspace))
        return false;
    HTScopedWorkspaceVisibility visible_workspace(cursor_workspace, true);

    HTCompat::move_window_to_workspace(dragged_window, cursor_workspace);
    HTCompat::set_window_real_position(dragged_window, tp_pos);

    if (!HTCompat::warp_pointer(*workspace_coords))
        return false;
    HTCompat::set_mouse_bind_mode(MBIND_INVALID);
    if (!HTCompat::warp_pointer(mouse_coords))
        return false;

    // otherwise the window leaves blur (?) artifacts on all
    // workspaces
    HTCompat::reset_window_workspace_move_alpha(dragged_window);
    restore_workspace.dismiss();

    // Do not return true and cancel the event! Mouse release requires some stuff to be done for
    // floating windows to be unfocused properly
    return false;
}

bool HTManager::begin_workspace_select() {
    const HTCursorWorkspaceContext cursor_context = resolve_cursor_workspace(false);
    const PHTVIEW cursor_view = cursor_context.view;
    const bool manages_mouse = cursor_view != nullptr && cursor_view->layout->should_manage_mouse();

    selection_pending = false;

    if (HTLogic::decideSelectStart(
            cursor_view != nullptr,
            cursor_view != nullptr && cursor_view->active,
            cursor_view != nullptr && cursor_view->closing,
            manages_mouse,
            cursor_context.workspace != nullptr
        )
        != HTLogic::SelectStartAction::BeginSelect) {
        return false;
    }

    selection_pending = true;
    return true;
}

bool HTManager::end_workspace_select() {
    const HTCursorWorkspaceContext cursor_context = resolve_cursor_workspace(false);
    const PHTVIEW cursor_view = cursor_context.view;
    const bool manages_mouse = cursor_view != nullptr && cursor_view->layout->should_manage_mouse();
    const auto action = HTLogic::decideSelectEnd(
        selection_pending,
        cursor_view != nullptr,
        cursor_view != nullptr && cursor_view->active,
        cursor_view != nullptr && cursor_view->closing,
        manages_mouse,
        cursor_context.workspace != nullptr
    );

    selection_pending = false;

    switch (action) {
        case HTLogic::SelectEndAction::Ignore:
            return false;
        case HTLogic::SelectEndAction::CancelSelect:
            return true;
        case HTLogic::SelectEndAction::FinalizeSelect:
            return exit_to_workspace();
    }

    return false;
}

bool HTManager::exit_to_workspace() {
    const PHTVIEW cursor_view = get_view_from_cursor();
    if (cursor_view == nullptr)
        return false;

    if (!cursor_view->active || !cursor_view->layout->should_manage_mouse())
        return false;

    for (PHTVIEW view : views) {
        if (view == nullptr)
            continue;
        view->hide(true);
    }
    return true;
}

bool HTManager::on_mouse_move() {
    const HTCursorWorkspaceContext cursor_context = resolve_cursor_workspace(false);
    const PHLMONITOR cursor_monitor = cursor_context.monitor;
    const PHTVIEW cursor_view = cursor_context.view;
    const bool manages_mouse = cursor_view != nullptr && cursor_view->layout->should_manage_mouse();

    if (cursor_monitor == nullptr || cursor_view == nullptr)
        return false;
    if (!cursor_view->active || cursor_view->closing || !manages_mouse)
        return false;

    if (cursor_context.workspace == nullptr) {
        if (cursor_view->hover_active) {
            cursor_view->clear_hover_workspace();
            HTCompat::damage_monitor(cursor_monitor);
            HTCompat::schedule_frame_for_monitor(cursor_monitor);
        }
        return true;
    }

    if (!cursor_view->hover_active
        || cursor_view->hovered_workspace_id != cursor_context.workspace_id) {
        cursor_view->set_hovered_workspace(cursor_context.workspace_id);
        HTCompat::damage_monitor(cursor_monitor);
        HTCompat::schedule_frame_for_monitor(cursor_monitor);
    }

    return true;
}

bool HTManager::on_mouse_axis(double delta) {
    const PHTVIEW cursor_view = get_view_from_cursor();
    if (cursor_view == nullptr)
        return false;

    return cursor_view->layout->on_mouse_axis(delta);
}

void HTManager::swipe_start() {
    reset_swipe_state();
}

bool HTManager::swipe_update(IPointer::SSwipeUpdateEvent e) {
    const int ENABLED = HTConfig::value<Hyprlang::INT>("gestures:enabled");
    if (!ENABLED) {
        reset_swipe_state();
        return false;
    }

    PHTVIEW swipe_view = nullptr;
    if (swipe_state == HT_SWIPE_NONE) {
        const PHLMONITOR cursor_monitor = HTCompat::cursor_monitor();
        swipe_view = get_view_from_monitor(cursor_monitor);
    } else {
        swipe_view = get_swipe_view();
    }
    if (swipe_view == nullptr) {
        reset_swipe_state();
        return false;
    }
    if (swipe_view->layout == nullptr || swipe_view->closing) {
        reset_swipe_state();
        return false;
    }

    const unsigned int MOVE_FINGERS = HTConfig::value<Hyprlang::INT>("gestures:move_fingers");
    const float OPEN_DISTANCE = HTConfig::value<Hyprlang::FLOAT>("gestures:open_distance");
    const unsigned int OPEN_FINGERS = HTConfig::value<Hyprlang::INT>("gestures:open_fingers");
    const int OPEN_POSITIVE = HTConfig::value<Hyprlang::INT>("gestures:open_positive");

    bool res = false;
    const auto swipe_direction = HTLogic::detectSwipeDirection(e.delta.x, e.delta.y);

    if (e.fingers == OPEN_FINGERS) {
        if (HTLogic::shouldConsumeOpenSwipe(swipe_view->active, swipe_state == HT_SWIPE_OPEN))
            res = true;

        const float deltaY = HTLogic::normalizedOpenDelta(e.delta.y, OPEN_POSITIVE);

        if (swipe_state != HT_SWIPE_OPEN) {
            const auto action = HTLogic::resolveOpenSwipeStart(
                swipe_direction,
                swipe_view->closing,
                swipe_view->active,
                swipe_view->navigating,
                deltaY
            );
            if (action == HTLogic::OpenSwipeStartAction::None) {
                return res;
            } else if (action == HTLogic::OpenSwipeStartAction::ShowOverview) {
                swipe_view->show();
                swipe_state = HT_SWIPE_OPEN;
                swipe_view_id = swipe_view->monitor_id;
                swipe_amt = OPEN_DISTANCE;
            } else if (action == HTLogic::OpenSwipeStartAction::HideOverview) {
                swipe_view->hide(false);
                swipe_state = HT_SWIPE_OPEN;
                swipe_view_id = swipe_view->monitor_id;
                swipe_amt = 0.0;
            }
        }

        if (swipe_state == HT_SWIPE_OPEN) {
            const auto next_amount = HTLogic::nextSwipeAmount(swipe_amt, deltaY, OPEN_DISTANCE);
            if (!next_amount.has_value()) {
                reset_swipe_state();
                return res;
            }

            swipe_amt = *next_amount;
            const auto swipe_perc = HTLogic::openSwipeProgress(swipe_amt, OPEN_DISTANCE);
            if (!swipe_perc.has_value()) {
                reset_swipe_state();
                return res;
            }

            swipe_view->layout->close_open_lerp(*swipe_perc);
        }
    } else if (e.fingers == MOVE_FINGERS) {
        if (HTLogic::shouldConsumeMoveSwipe(swipe_state == HT_SWIPE_MOVE))
            res = true;

        if (swipe_state != HT_SWIPE_MOVE) {
            if (!HTLogic::shouldStartMoveSwipe(
                    swipe_view->active,
                    swipe_view->closing,
                    swipe_view->navigating
                )) {
                return res;
            } else {
                swipe_state = HT_SWIPE_MOVE;
                swipe_view_id = swipe_view->monitor_id;
                swipe_view->set_runtime_state(swipe_view->active, swipe_view->closing, true);

                // need to schedule frames for monitor, otherwise the screen doesn't re-render
                const PHLMONITOR swipe_monitor = swipe_view->get_monitor();
                if (swipe_monitor != nullptr) {
                    HTCompat::damage_monitor(swipe_monitor);
                    HTCompat::schedule_frame_for_monitor(swipe_monitor);
                }
            }
        }

        if (swipe_state == HT_SWIPE_MOVE) {
            if (!swipe_view->navigating) {
                reset_swipe_state();
                return res;
            }
            swipe_view->layout->on_move_swipe(e.delta);
        }
    }
    return res;
}

bool HTManager::swipe_end() {
    if (swipe_state == HT_SWIPE_NONE)
        return false;

    const PHTVIEW swipe_view = get_swipe_view();
    if (swipe_view == nullptr) {
        reset_swipe_state();
        return false;
    }
    if (swipe_view->layout == nullptr || swipe_view->closing) {
        reset_swipe_state();
        return false;
    }

    switch (swipe_state) {
        case HT_SWIPE_OPEN: {
            const float OPEN_DISTANCE = HTConfig::value<Hyprlang::FLOAT>("gestures:open_distance");
            const auto keep_open = HTLogic::shouldKeepOverviewOpen(swipe_amt, OPEN_DISTANCE);
            if (!keep_open.has_value()) {
                reset_swipe_state();
                return false;
            }

            if (*keep_open) {
                swipe_view->show();
            } else {
                swipe_view->hide(false);
            }
            break;
        }
        case HT_SWIPE_MOVE: {
            if (!swipe_view->navigating) {
                reset_swipe_state();
                return false;
            }
            const WORKSPACEID ws_id = swipe_view->layout->on_move_swipe_end();
            swipe_view->move_id(ws_id, false);
            break;
        }
        case HT_SWIPE_NONE:
            break;
    }

    reset_swipe_state();
    return true;
}
