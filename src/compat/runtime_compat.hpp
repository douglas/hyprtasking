#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopTimer.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/signal/Listener.hpp>
#include <string>
#include <vector>

#include "../build_contract.hpp"

namespace HTCompat {

using Hyprutils::Signal::CHyprSignalListener;

SDispatchResult
invoke_dispatcher(const std::string& dispatch_name, const std::string& dispatch_arg);
bool enter_submap(const std::string& name);
bool exit_submap();
PHLMONITOR cursor_monitor();
Vector2D mouse_coords();
std::vector<PHLMONITOR> compositor_monitors();
PHLMONITOR monitor_from_id(MONITORID monitor_id);
std::string monitor_description(PHLMONITOR monitor);
std::string monitor_name(PHLMONITOR monitor);
bool create_float_animation(
    float initial_value,
    PHLANIMVAR<float>& animation,
    const std::string& config_name,
    eAVarDamagePolicy policy
);
bool create_vector_animation(
    const Vector2D& initial_value,
    PHLANIMVAR<Vector2D>& animation,
    const std::string& config_name,
    eAVarDamagePolicy policy
);
PHLWINDOW hover_target_window_at(const Vector2D& position);
void set_cursor_override_enabled(bool enabled);
void damage_monitor(PHLMONITOR monitor);
void schedule_frame_for_monitor(PHLMONITOR monitor);
// Damage the whole monitor and schedule a frame so the damage gets drawn.
void redraw_monitor(PHLMONITOR monitor);
void simulate_mouse_movement();
bool do_later(const std::function<void()>& callback);
// Drops queued do_later callbacks; their code lives in this plugin's .so.
void cancel_pending_do_later();
void move_window_to_workspace(PHLWINDOW window, PHLWORKSPACE workspace);
void set_mouse_bind_mode(eMouseBindMode mode);
bool begin_drag_window(PHLWINDOW window, eMouseBindMode mode);
SP<Layout::ITarget> drag_controller_target();
bool drag_controller_is_tiled();
eMouseBindMode drag_controller_mode();
void end_drag_controller();
void invoke_mouse_button_original(void* thisptr, IPointer::SButtonEvent event, SP<IPointer> mouse);

bool listen_mouse_move(
    CHyprSignalListener& listener,
    const std::function<void(Vector2D, Event::SCallbackInfo&)>& callback
);
bool listen_swipe_begin(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeBeginEvent, Event::SCallbackInfo&)>& callback
);
bool listen_swipe_update(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeUpdateEvent, Event::SCallbackInfo&)>& callback
);
bool listen_swipe_end(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeEndEvent, Event::SCallbackInfo&)>& callback
);
bool listen_config_reloaded(CHyprSignalListener& listener, const std::function<void()>& callback);
bool listen_monitor_added(CHyprSignalListener& listener, const std::function<void()>& callback);
// 0.55 emits this before dropping the monitor from its list, so pass it on.
bool listen_monitor_removed(
    CHyprSignalListener& listener,
    const std::function<void(PHLMONITOR)>& callback
);
bool listen_monitor_layout_changed(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
);
// Window open, close and move-to-workspace events.
bool listen_window_set_changed(
    std::vector<CHyprSignalListener>& listeners,
    const std::function<void()>& callback
);

const std::vector<PHLWINDOW>& compositor_windows();
// Fires after each commit of the window's main surface; empty if it has none.
CHyprSignalListener
listen_window_surface_commit(PHLWINDOW window, const std::function<void()>& callback);

// Repeating timer on Hyprland's event loop; stop it before the plugin unloads.
SP<CEventLoopTimer>
start_repeating_timer(std::chrono::milliseconds interval, const std::function<void()>& callback);
void stop_timer(SP<CEventLoopTimer>& timer);

} // namespace HTCompat
