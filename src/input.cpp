#include <linux/input-event-codes.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/macros.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "config.hpp"
#include "logic/controller_state.hpp"
#include "logic/gesture_model.hpp"
#include "manager.hpp"
#include "overview.hpp"
#include "state_guards.hpp"

using Hyprutils::Utils::CScopeGuard;

bool HTManager::start_window_drag() {
    const PHLMONITOR cursor_monitor = g_pCompositor->getMonitorFromCursor();
    const PHTVIEW cursor_view = get_view_from_monitor(cursor_monitor);
    if (cursor_monitor == nullptr || cursor_view == nullptr || !cursor_view->active
        || cursor_view->closing)
        return false;

    if (!cursor_view->layout->should_manage_mouse()) {
        // hide all views if should not manage mouse but active
        hide_all_views();
        return true;
    }

    const Vector2D mouse_coords = g_pInputManager->getMouseCoordsInternal();
    const WORKSPACEID workspace_id = cursor_view->layout->get_ws_id_from_global(mouse_coords);
    PHLWORKSPACE cursor_workspace = g_pCompositor->getWorkspaceByID(workspace_id);

    // If left click on non-workspace workspace, do nothing
    if (cursor_workspace == nullptr)
        return false;

    HTScopedMonitorWorkspace restore_workspace(cursor_monitor, true);
    cursor_monitor->changeWorkspace(cursor_workspace, true);

    const Vector2D workspace_coords =
        cursor_view->layout->global_to_local_ws_unscaled(mouse_coords, workspace_id)
        + cursor_monitor->m_position;

    bool reset_mouse_bind_mode = false;
    CScopeGuard reset_drag_mode([&reset_mouse_bind_mode] {
        if (reset_mouse_bind_mode)
            g_pKeybindManager->changeMouseBindMode(MBIND_INVALID);
    });

    g_pPointerManager->warpTo(workspace_coords);
    g_pKeybindManager->changeMouseBindMode(MBIND_MOVE);
    reset_mouse_bind_mode = true;
    g_pPointerManager->warpTo(mouse_coords);

    const SP<Layout::ITarget> target = g_layoutManager->dragController()->target();
    if (target == nullptr)
        return false;

    reset_mouse_bind_mode = false;
    restore_workspace.dismiss();

    const PHLWINDOW dragged_window = target->window();
    if (dragged_window != nullptr) {
        if (g_layoutManager->dragController()->draggingTiled()) {
            const Vector2D pre_pos = cursor_view->layout->local_ws_unscaled_to_global(
                dragged_window->m_realPosition->value() - dragged_window->m_monitor->m_position,
                workspace_id
            );
            const Vector2D post_pos = cursor_view->layout->local_ws_unscaled_to_global(
                dragged_window->m_realPosition->goal() - dragged_window->m_monitor->m_position,
                workspace_id
            );
            const Vector2D mapped_pre_pos =
                (pre_pos - mouse_coords) / cursor_view->layout->drag_window_scale() + mouse_coords;
            const Vector2D mapped_post_pos =
                (post_pos - mouse_coords) / cursor_view->layout->drag_window_scale() + mouse_coords;

            dragged_window->m_realPosition->setValueAndWarp(mapped_pre_pos);
            *dragged_window->m_realPosition = mapped_post_pos;
        } else {
            g_pInputManager->simulateMouseMovement();
        }
    }

    // if (o_workspace != nullptr)
    //     cursor_monitor->changeWorkspace(o_workspace.lock(), true);

    return true;
}

bool HTManager::end_window_drag() {
    const PHLMONITOR cursor_monitor = g_pCompositor->getMonitorFromCursor();
    const PHTVIEW cursor_view = get_view_from_monitor(cursor_monitor);
    CScopeGuard reset_drag_mode([] { g_pKeybindManager->changeMouseBindMode(MBIND_INVALID); });
    if (cursor_monitor == nullptr || cursor_view == nullptr) {
        return false;
    }

    // Required if dragging and dropping from active to inactive
    if (!cursor_view->active || cursor_view->closing) {
        return false;
    }

    // For linear layout: if dropping on big workspace, just pass on
    if (!cursor_view->layout->should_manage_mouse()) {
        return false;
    }

    const SP<Layout::ITarget> target = g_layoutManager->dragController()->target();
    if (target == nullptr)
        return false;

    // If not dragging window or drag is not move, then we just let go (supposed to prevent it
    // from messing up resize on border, but it should be good because above?)
    const PHLWINDOW dragged_window = target->window();
    if (dragged_window == nullptr || g_layoutManager->dragController()->mode() != MBIND_MOVE) {
        return false;
    }

    const Vector2D mouse_coords = g_pInputManager->getMouseCoordsInternal();
    Vector2D use_mouse_coords = mouse_coords;
    const WORKSPACEID workspace_id = cursor_view->layout->get_ws_id_from_global(mouse_coords);
    const auto drop_workspace = HTLogic::resolveDropWorkspace(
        workspace_id,
        dragged_window->m_workspace == nullptr ? std::nullopt
                                               : std::optional<WORKSPACEID> {dragged_window->m_workspace->m_id}
    );
    if (!drop_workspace.valid) {
        Log::logger->log(LOG, "[Hyprtasking] tried to drop window without a valid workspace target");
        return false;
    }

    PHLWORKSPACE cursor_workspace = g_pCompositor->getWorkspaceByID(drop_workspace.workspace_id);

    // Release on empty dummy workspace, so create and switch to it
    if (cursor_workspace == nullptr && drop_workspace.create_if_missing) {
        cursor_workspace =
            g_pCompositor->createNewWorkspace(drop_workspace.workspace_id, cursor_monitor->m_id);
    } else if (drop_workspace.snap_to_workspace && cursor_workspace != nullptr) {
        // Ensure that the mouse coords are snapped to inside the workspace box itself
        use_mouse_coords = cursor_view->layout->get_global_ws_box(cursor_workspace->m_id)
                               .closestPoint(use_mouse_coords);

        Log::logger->log(
            LOG,
            "[Hyprtasking] Dragging to invalid position, snapping to last ws {}",
            cursor_workspace->m_id
        );
    }

    if (cursor_workspace == nullptr) {
        Log::logger->log(LOG, "[Hyprtasking] tried to drop on null workspace??");
        return false;
    }

    Log::logger->log(LOG, "[Hyprtasking] trying to drop window on ws {}", cursor_workspace->m_id);

    HTScopedMonitorWorkspace restore_workspace(cursor_monitor, true);
    cursor_monitor->changeWorkspace(cursor_workspace, true);

    g_pCompositor->moveWindowToWorkspaceSafe(dragged_window, cursor_workspace);

    const Vector2D workspace_coords =
        cursor_view->layout->global_to_local_ws_unscaled(use_mouse_coords, cursor_workspace->m_id)
        + cursor_monitor->m_position;

    const Vector2D tp_pos = cursor_view->layout->global_to_local_ws_unscaled(
                                (dragged_window->m_realPosition->value() - use_mouse_coords)
                                        * cursor_view->layout->drag_window_scale()
                                    + use_mouse_coords,
                                cursor_workspace->m_id
                            )
        + cursor_monitor->m_position;
    dragged_window->m_realPosition->setValueAndWarp(tp_pos);

    g_pPointerManager->warpTo(workspace_coords);
    g_pPointerManager->warpTo(mouse_coords);

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
    if (!ENABLED)
        return false;

    const PHLMONITOR cursor_monitor = g_pCompositor->getMonitorFromCursor();
    PHTVIEW cursor_view = swipe_state == HT_SWIPE_NONE ? get_view_from_monitor(cursor_monitor) : get_swipe_view();
    if (cursor_view == nullptr)
        return false;
    if (cursor_monitor == nullptr && swipe_state == HT_SWIPE_NONE)
        return false;

    const unsigned int MOVE_FINGERS = HTConfig::value<Hyprlang::INT>("gestures:move_fingers");
    const float OPEN_DISTANCE = HTConfig::value<Hyprlang::FLOAT>("gestures:open_distance");
    const unsigned int OPEN_FINGERS = HTConfig::value<Hyprlang::INT>("gestures:open_fingers");
    const int OPEN_POSITIVE = HTConfig::value<Hyprlang::INT>("gestures:open_positive");

    bool res = false;
    const auto swipe_direction = HTLogic::detectSwipeDirection(e.delta.x, e.delta.y);

    if (e.fingers == OPEN_FINGERS) {
        res = HTLogic::shouldConsumeOpenSwipe(cursor_view->active, swipe_state == HT_SWIPE_OPEN);

        const float deltaY = HTLogic::normalizedOpenDelta(e.delta.y, OPEN_POSITIVE);

        if (swipe_state != HT_SWIPE_OPEN) {
            const auto start_action = HTLogic::resolveOpenSwipeStart(
                swipe_direction,
                cursor_view->closing,
                cursor_view->active,
                deltaY
            );
            if (start_action == HTLogic::OpenSwipeStartAction::None) {
                return res;
            } else if (start_action == HTLogic::OpenSwipeStartAction::ShowOverview) {
                cursor_view->show();
                swipe_state = HT_SWIPE_OPEN;
                swipe_amt = OPEN_DISTANCE;
                swipe_view_id = cursor_view->monitor_id;
            } else if (start_action == HTLogic::OpenSwipeStartAction::HideOverview) {
                cursor_view->hide(false);
                swipe_state = HT_SWIPE_OPEN;
                swipe_amt = 0.0;
                swipe_view_id = cursor_view->monitor_id;
            }
        }

        if (swipe_state == HT_SWIPE_OPEN) {
            const auto next_swipe_amt = HTLogic::nextSwipeAmount(swipe_amt, deltaY, OPEN_DISTANCE);
            if (!next_swipe_amt.has_value())
                return res;

            swipe_amt = *next_swipe_amt;
            const auto swipe_perc = HTLogic::openSwipeProgress(swipe_amt, OPEN_DISTANCE);
            if (swipe_perc.has_value())
                cursor_view->layout->close_open_lerp(*swipe_perc);
        }
    } else if (e.fingers == MOVE_FINGERS) {
        res = HTLogic::shouldConsumeMoveSwipe(swipe_state == HT_SWIPE_MOVE);

        if (swipe_state != HT_SWIPE_MOVE) {
            if (!HTLogic::shouldStartMoveSwipe(cursor_view->active)) {
                return res;
            } else {
                swipe_state = HT_SWIPE_MOVE;
                swipe_view_id = cursor_view->monitor_id;
                cursor_view->navigating = true;

                // need to schedule frames for monitor, otherwise the screen doesn't re-render
                const PHLMONITOR swipe_monitor = cursor_view->get_monitor();
                if (swipe_monitor != nullptr) {
                    g_pHyprRenderer->damageMonitor(swipe_monitor);
                    g_pCompositor->scheduleFrameForMonitor(swipe_monitor);
                }
            }
        }

        if (swipe_state == HT_SWIPE_MOVE) {
            cursor_view->layout->on_move_swipe(e.delta);
        }
    }
    return res;
}

bool HTManager::swipe_end() {
    if (swipe_state == HT_SWIPE_NONE)
        return false;
    CScopeGuard reset_swipe([this] { reset_swipe_state(); });

    const PHTVIEW cursor_view = get_swipe_view();
    if (cursor_view == nullptr) {
        clear_navigating_views();
        return false;
    }

    switch (swipe_state) {
        case HT_SWIPE_OPEN: {
            const float OPEN_DISTANCE = HTConfig::value<Hyprlang::FLOAT>("gestures:open_distance");
            const auto keep_open = HTLogic::shouldKeepOverviewOpen(swipe_amt, OPEN_DISTANCE);
            if (!keep_open.has_value()) {
                cursor_view->hide(false);
            } else if (*keep_open) {
                cursor_view->show(false);
            } else {
                cursor_view->hide(false);
            }
            break;
        }
        case HT_SWIPE_MOVE: {
            const WORKSPACEID ws_id = cursor_view->layout->on_move_swipe_end();
            cursor_view->move_id(ws_id, false);
            break;
        }
        case HT_SWIPE_NONE:
            break;
    }

    return true;
}
