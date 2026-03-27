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

    const PHLMONITOR cursor_monitor = g_pCompositor->getMonitorFromCursor();
    const PHTVIEW cursor_view = get_view_from_monitor(cursor_monitor);
    const bool manages_mouse = cursor_view != nullptr && cursor_view->layout->should_manage_mouse();

    const Vector2D mouse_coords = g_pInputManager->getMouseCoordsInternal();
    const WORKSPACEID workspace_id =
        cursor_view == nullptr ? WORKSPACE_INVALID : cursor_view->layout->get_ws_id_from_global(mouse_coords);
    PHLWORKSPACE cursor_workspace = HTCompat::resolve_workspace_target(
        cursor_monitor,
        workspace_id,
        false
    );

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
            return true;
        case HTLogic::DragStartAction::BeginDrag:
            break;
    }

    if (cursor_monitor == nullptr || cursor_view == nullptr) {
        return false;
    }

    if (!manages_mouse) {
        // hide all views if should not manage mouse but active
        hide_all_views();
        return true;
    }

    HTScopedMonitorWorkspace restore_workspace(cursor_monitor, true);
    if (!HTCompat::activate_monitor_workspace(cursor_monitor, cursor_workspace))
        return false;

    const auto workspace_coords =
        cursor_view->layout->global_to_workspace_monitor_coords(mouse_coords, workspace_id);
    if (!workspace_coords.has_value())
        return false;
    if (!HTCompat::warp_pointer(*workspace_coords))
        return false;

    bool reset_mouse_bind_mode = false;
    CScopeGuard reset_drag_mode([&reset_mouse_bind_mode] {
        if (reset_mouse_bind_mode)
            g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
    });

    g_pKeybindManager->changeMouseBindMode(MBIND_MOVE);
    reset_mouse_bind_mode = true;
    if (!HTCompat::warp_pointer(mouse_coords))
        return false;

    const SP<Layout::ITarget> target = g_layoutManager->dragController()->target();
    if (target == nullptr)
        return false;

    const PHLWINDOW dragged_window = target->window();
    if (dragged_window != nullptr) {
        if (g_layoutManager->dragController()->draggingTiled()) {
            const auto inverse_drag_scale =
                HTLogic::inverseDragWindowScale(cursor_view->layout->drag_window_scale());
            if (!inverse_drag_scale.has_value())
                return false;

            const Vector2D pre_pos = cursor_view->layout->local_ws_unscaled_to_global(
                dragged_window->m_realPosition->value() - dragged_window->m_monitor->m_position,
                workspace_id
            );
            const Vector2D post_pos = cursor_view->layout->local_ws_unscaled_to_global(
                dragged_window->m_realPosition->goal() - dragged_window->m_monitor->m_position,
                workspace_id
            );
            const Vector2D mapped_pre_pos =
                (pre_pos - mouse_coords) * *inverse_drag_scale + mouse_coords;
            const Vector2D mapped_post_pos =
                (post_pos - mouse_coords) * *inverse_drag_scale + mouse_coords;

            dragged_window->m_realPosition->setValueAndWarp(mapped_pre_pos);
            *dragged_window->m_realPosition = mapped_post_pos;
        } else {
            g_pInputManager->simulateMouseMovement();
        }
    }

    set_dragged_window(dragged_window);
    reset_mouse_bind_mode = false;
    restore_workspace.dismiss();

    return true;
}

bool HTManager::end_window_drag() {
    const PHLMONITOR cursor_monitor = g_pCompositor->getMonitorFromCursor();
    const PHTVIEW cursor_view = get_view_from_monitor(cursor_monitor);
    CScopeGuard reset_drag_mode([] { g_pKeybindManager->changeMouseBindMode(MBIND_INVALID); });
    CScopeGuard clear_dragged_window_guard([this] { clear_dragged_window(); });
    const bool has_view = cursor_view != nullptr;
    const bool manages_mouse = cursor_view != nullptr && cursor_view->layout->should_manage_mouse();
    const bool has_layout_manager = static_cast<bool>(g_layoutManager);
    const SP<Layout::ITarget> target =
        has_layout_manager ? g_layoutManager->dragController()->target() : SP<Layout::ITarget> {};
    const PHLWINDOW dragged_window = target == nullptr ? nullptr : target->window();
    const bool move_mode =
        has_layout_manager && g_layoutManager->dragController()->mode() == MBIND_MOVE;

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

    const Vector2D mouse_coords = g_pInputManager->getMouseCoordsInternal();
    Vector2D use_mouse_coords = mouse_coords;
    const WORKSPACEID hovered_workspace_id = cursor_view->layout->get_ws_id_from_global(mouse_coords);
    const std::optional<WORKSPACEID> dragged_workspace_id =
        dragged_window->m_workspace == nullptr ? std::nullopt
                                               : std::optional<WORKSPACEID> {dragged_window->m_workspace->m_id};
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
        use_mouse_coords =
            cursor_view->layout->get_global_ws_box(cursor_workspace->m_id).closestPoint(use_mouse_coords);

        Log::logger->log(
            LOG,
            "[Hyprtasking] Dragging to invalid position, snapping to last ws {}",
            cursor_workspace->m_id
        );
    }

    Log::logger->log(LOG, "[Hyprtasking] trying to drop window on ws {}", cursor_workspace->m_id);

    const auto workspace_coords = cursor_view->layout->global_to_workspace_monitor_coords(
        use_mouse_coords,
        cursor_workspace->m_id
    );
    if (!workspace_coords.has_value())
        return false;
    const auto drag_scale = HTLogic::dragWindowScale(cursor_view->layout->drag_window_scale());
    if (!drag_scale.has_value())
        return false;

    const Vector2D warped_global_pos = cursor_view->layout->global_to_local_ws_unscaled(
                                (dragged_window->m_realPosition->value() - use_mouse_coords)
                                        * *drag_scale
                                    + use_mouse_coords,
                                cursor_workspace->m_id
                            ) + cursor_monitor->m_position;
    if (!HTLogic::isFinitePoint(warped_global_pos.x, warped_global_pos.y))
        return false;
    const Vector2D tp_pos = warped_global_pos;

    HTScopedMonitorWorkspace restore_workspace(cursor_monitor, true);
    if (!HTCompat::activate_monitor_workspace(cursor_monitor, cursor_workspace))
        return false;

    g_pCompositor->moveWindowToWorkspaceSafe(dragged_window, cursor_workspace);
    dragged_window->m_realPosition->setValueAndWarp(tp_pos);

    if (!HTCompat::warp_pointer(*workspace_coords))
        return false;
    g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
    if (!HTCompat::warp_pointer(mouse_coords))
        return false;

    // otherwise the window leaves blur (?) artifacts on all
    // workspaces
    dragged_window->m_movingToWorkspaceAlpha->setValueAndWarp(1.0);
    dragged_window->m_movingFromWorkspaceAlpha->setValueAndWarp(1.0);
    restore_workspace.dismiss();

    // Do not return true and cancel the event! Mouse release requires some stuff to be done for
    // floating windows to be unfocused properly
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
    return false;
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
        const PHLMONITOR cursor_monitor = g_pCompositor->getMonitorFromCursor();
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
                    g_pHyprRenderer->damageMonitor(swipe_monitor);
                    g_pCompositor->scheduleFrameForMonitor(swipe_monitor);
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
