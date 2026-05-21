#include "overview.hpp"

#include <hyprutils/math/Box.hpp>
#include <vector>

#include "compat/renderer_compat.hpp"
#include "compat/runtime_compat.hpp"
#include "globals.hpp"
#include "layout/grid.hpp"
#include "logic/controller_state.hpp"
#include "logic/navigation_model.hpp"
#include "runtime_fail.hpp"
#include "trace.hpp"

namespace {

bool commit_selection_target(HTView& view, WORKSPACEID target_workspace_id, bool defer_activation) {
    const PHLMONITOR monitor = view.get_monitor();
    if (monitor == nullptr)
        return false;

    const PHLWORKSPACE workspace =
        HTCompat::resolve_workspace_target(monitor, target_workspace_id, true);
    if (workspace == nullptr)
        return false;

    // Freeze visual selection to the committed target during close animation.
    view.clear_hover_workspace();
    view.clear_hover_suppression();
    view.set_keyboard_workspace(target_workspace_id);

    if (!defer_activation && !HTCompat::activate_monitor_workspace_user(monitor, workspace))
        return false;

    const MONITORID monitor_id = HTCompat::monitor_id(monitor);
    view.set_runtime_state(true, true, false);
    view.layout->on_hide(
        target_workspace_id,
        [&view, monitor_id, workspace_ref = PHLWORKSPACEREF {workspace}, defer_activation](
            auto self
        ) {
            if (defer_activation) {
                const bool scheduled = HTCompat::do_later([monitor_id, workspace_ref] {
                    if (ht_manager == nullptr || !ht_manager->runtime_enabled())
                        return;

                    const PHLMONITOR target_monitor = HTCompat::monitor_from_id(monitor_id);
                    if (target_monitor == nullptr)
                        return;

                    const PHLWORKSPACE target_workspace = workspace_ref.lock();
                    if (target_workspace != nullptr)
                        HTCompat::activate_monitor_workspace_user(target_monitor, target_workspace);
                });
                if (!scheduled) {
                    HTRuntimeFail::disable(
                        "commit_selection_target",
                        "event loop manager unavailable for deferred workspace activation"
                    );
                }
            }

            view.set_runtime_state(false, false, false);
            view.reset_interaction_state();
        }
    );

    HTCompat::damage_monitor(monitor);
    HTCompat::schedule_frame_for_monitor(monitor);
    return true;
}

} // namespace

HTView::HTView(MONITORID in_monitor_id) {
    monitor_id = in_monitor_id;
    reset_interaction_state();
    set_runtime_state(false, false, false);

    layout = makeShared<HTLayoutGrid>(monitor_id);
}

void HTView::show() {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;

    set_keyboard_workspace(HTCompat::workspace_id(active_workspace));
    clear_hover_workspace();
    set_runtime_state(true, false, false);

    layout->on_show();

    HTCompat::damage_monitor(monitor);
    HTCompat::schedule_frame_for_monitor(monitor);
}

void HTView::hide() {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;

    set_runtime_state(true, true, false);

    layout->on_hide(WORKSPACE_INVALID, [this](auto self) {
        set_runtime_state(false, false, false);
        reset_interaction_state();
    });

    HTCompat::damage_monitor(monitor);
    HTCompat::schedule_frame_for_monitor(monitor);
}

void HTView::set_runtime_state(bool new_active, bool new_closing, bool new_navigating) {
    const bool was_interactively_active = active && !closing;

    const bool changed =
        active != new_active || closing != new_closing || navigating != new_navigating;
    active = new_active;
    closing = new_closing;
    navigating = new_navigating;

    const bool is_interactively_active = active && !closing;

    if (changed) {
        Log::logger->log(
            LOG,
            "[Hyprtasking] view {} runtime state -> active={} closing={} navigating={}",
            monitor_id,
            active,
            closing,
            navigating
        );
    }

    if (!was_interactively_active && is_interactively_active) {
        if (!HTCompat::enter_submap("hyprtasking")) {
            HTRuntimeFail::disable("set_runtime_state", "failed entering hyprtasking submap");
        }
    } else if (was_interactively_active && !is_interactively_active) {
        if (ht_manager != nullptr && !ht_manager->has_interactively_active_view()) {
            if (!HTCompat::exit_submap())
                HTRuntimeFail::disable("set_runtime_state", "failed resetting hyprtasking submap");
        }
    }

    if (ht_manager != nullptr)
        ht_manager->refresh_cursor_override();
}

void HTView::reset_interaction_state() {
    keyboard_workspace_id = WORKSPACE_INVALID;
    selected_workspace_id = WORKSPACE_INVALID;
    clear_hover_suppression();
    clear_hover_workspace();
}

void HTView::clear_hover_workspace() {
    hover_active = false;
    hovered_workspace_id = WORKSPACE_INVALID;
}

void HTView::clear_hover_suppression() {
    hover_suppressed = false;
    hover_suppressed_workspace_id = WORKSPACE_INVALID;
}

void HTView::suppress_hover_workspace(WORKSPACEID workspace_id) {
    clear_hover_workspace();
    if (workspace_id == WORKSPACE_INVALID) {
        clear_hover_suppression();
        return;
    }

    hover_suppressed = true;
    hover_suppressed_workspace_id = workspace_id;
}

void HTView::set_hovered_workspace(WORKSPACEID workspace_id) {
    if (workspace_id == WORKSPACE_INVALID) {
        clear_hover_workspace();
        return;
    }

    hover_active = true;
    hovered_workspace_id = workspace_id;
}

void HTView::set_keyboard_workspace(WORKSPACEID workspace_id) {
    keyboard_workspace_id = workspace_id;
    selected_workspace_id = workspace_id;
}

void HTView::set_selected_workspace_id(WORKSPACEID ws_id) {
    selected_workspace_id = ws_id;
}

WORKSPACEID
HTView::keyboard_selection_workspace_id(WORKSPACEID fallback_workspace_id) const {
    return HTLogic::resolveKeyboardSelectionWorkspace(keyboard_workspace_id, fallback_workspace_id);
}

WORKSPACEID HTView::visual_workspace_id(WORKSPACEID fallback_workspace_id) const {
    return HTLogic::resolveVisualWorkspaceID(
        hover_active,
        hovered_workspace_id,
        selected_workspace_id,
        fallback_workspace_id
    );
}

void HTView::cancel_runtime_state() {
    layout->cancel_animation_callbacks();

    set_runtime_state(false, false, false);
    reset_interaction_state();
}

bool HTView::has_runtime_activity() const {
    return active || closing || navigating;
}

void HTView::reload_config() {
    if (has_runtime_activity())
        cancel_runtime_state();

    layout->init_position();

    const PHLMONITOR monitor = get_monitor();
    if (monitor != nullptr) {
        HTCompat::damage_monitor(monitor);
        HTCompat::schedule_frame_for_monitor(monitor);
    }
}

bool HTView::commit_selection() {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return false;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return false;

    const WORKSPACEID hovered_workspace_id =
        hover_active ? this->hovered_workspace_id : WORKSPACE_INVALID;
    const WORKSPACEID target_workspace_id = HTLogic::resolveSelectionWorkspace(
        selected_workspace_id,
        hovered_workspace_id,
        HTCompat::workspace_id(active_workspace)
    );
    return commit_selection_target(*this, target_workspace_id, false);
}

bool HTView::commit_mouse_selection(WORKSPACEID target_workspace_id) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return false;
    return commit_selection_target(*this, target_workspace_id, true);
}

void HTView::move_id(WORKSPACEID ws_id) {
    set_runtime_state(active, closing, false);
    if (closing || ws_id == WORKSPACE_INVALID)
        return;
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;
    const WORKSPACEID active_workspace_id = HTCompat::workspace_id(active_workspace);
    const WORKSPACEID current_workspace_id = keyboard_selection_workspace_id(active_workspace_id);

    const PHLWORKSPACE other_workspace = HTCompat::resolve_workspace_target(monitor, ws_id, true);
    if (other_workspace == nullptr)
        return;

    if (!HTCompat::activate_monitor_workspace_user(monitor, other_workspace))
        return;

    set_keyboard_workspace(ws_id);
    clear_hover_workspace();

    const bool was_active = active;
    const bool was_closing = closing;
    set_runtime_state(was_active, was_closing, true);
    layout->on_move(
        current_workspace_id,
        HTCompat::workspace_id(other_workspace),
        [this, was_active, was_closing](auto self) {
            set_runtime_state(was_active, was_closing, false);
        }
    );
}

void HTView::move(std::string arg) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;
    const WORKSPACEID navigation_workspace_id =
        keyboard_selection_workspace_id(HTCompat::workspace_id(active_workspace));
    const WORKSPACEID source_ws_id = navigation_workspace_id;
    if (source_ws_id == WORKSPACE_INVALID)
        return;

    layout->build_overview_layout(HT_VIEW_CLOSED);
    const auto* ws_layout = layout->find_layout_workspace(source_ws_id);
    if (ws_layout == nullptr)
        return;
    const WORKSPACEID id = layout->get_ws_id_in_direction(ws_layout->x, ws_layout->y, arg);
    if (id == WORKSPACE_INVALID)
        return;

    move_id(id);
}

bool HTView::navigate_selection(const std::string& arg) {
    const auto navigate_arg = HTLogic::parseNavigateArg(arg);
    if (navigate_arg.kind == HTLogic::NavigateArgKind::Invalid)
        return false;

    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return false;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return false;

    const WORKSPACEID active_workspace_id = HTCompat::workspace_id(active_workspace);
    layout->build_overview_layout(HT_VIEW_CLOSED);
    auto select_workspace = [&](WORKSPACEID target_ws_id) {
        set_keyboard_workspace(target_ws_id);
        const HTCursorWorkspaceContext cursor_context = ht_manager == nullptr
            ? HTCursorWorkspaceContext {}
            : ht_manager->resolve_cursor_workspace(false);
        const WORKSPACEID cursor_workspace_id =
            cursor_context.monitor == monitor ? cursor_context.workspace_id : WORKSPACE_INVALID;
        suppress_hover_workspace(cursor_workspace_id);
        HTCompat::damage_monitor(monitor);
        HTCompat::schedule_frame_for_monitor(monitor);
    };

    if (navigate_arg.kind == HTLogic::NavigateArgKind::Direction) {
        const WORKSPACEID source_ws_id =
            HTLogic::resolveKeyboardSelectionWorkspace(keyboard_workspace_id, active_workspace_id);
        HTTrace::log(
            "[Hyprtasking][trace] navigate_selection dir={} active_ws={} keyboard_ws={} selected_ws={} hover_active={} hovered_ws={} source_ws={}",
            navigate_arg.direction,
            active_workspace_id,
            keyboard_workspace_id,
            selected_workspace_id,
            hover_active,
            hovered_workspace_id,
            source_ws_id
        );

        if (source_ws_id == WORKSPACE_INVALID)
            return false;

        const auto* ws_layout = layout->find_layout_workspace(source_ws_id);
        if (ws_layout == nullptr) {
            HTTrace::log(
                "[Hyprtasking][trace] navigate_selection no layout for source_ws={}",
                source_ws_id
            );
            return false;
        }

        std::string direction = navigate_arg.direction;
        const WORKSPACEID target_ws_id =
            layout->get_ws_id_in_direction(ws_layout->x, ws_layout->y, direction);
        HTTrace::log(
            "[Hyprtasking][trace] navigate_selection source=({}, {}) target_ws={}",
            ws_layout->x,
            ws_layout->y,
            target_ws_id
        );
        if (target_ws_id == WORKSPACE_INVALID)
            return true;

        select_workspace(target_ws_id);
        return true;
    }

    if (navigate_arg.kind == HTLogic::NavigateArgKind::Workspace) {
        if (layout->find_layout_workspace(navigate_arg.workspace_id) == nullptr)
            return true;

        select_workspace(navigate_arg.workspace_id);
        return true;
    }

    return false;
}

PHLMONITOR HTView::get_monitor() {
    const PHLMONITOR monitor = HTCompat::monitor_from_id(monitor_id);
    if (monitor == nullptr)
        Log::logger->log(Log::WARN, "[Hyprtasking] Returning null monitor from get_monitor!");
    return monitor;
}
