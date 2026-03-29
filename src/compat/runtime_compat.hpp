#pragma once

#include <functional>
#include <string>
#include <vector>

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/devices/Tablet.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/signal/Listener.hpp>

namespace HTCompat {

using Hyprutils::Signal::CHyprSignalListener;

SDispatchResult invoke_dispatcher(const std::string& dispatch_name, const std::string& dispatch_arg);
void enter_submap(const std::string& name);
void exit_submap();
PHLMONITOR cursor_monitor();
Vector2D mouse_coords();
PHLMONITOR focused_monitor();
std::vector<PHLMONITOR> compositor_monitors();
PHLMONITOR monitor_from_id(MONITORID monitor_id);
std::string monitor_description(PHLMONITOR monitor);
void create_float_animation(
    float initial_value,
    PHLANIMVAR<float>& animation,
    const std::string& config_name,
    eAVarDamagePolicy policy
);
void create_vector_animation(
    const Vector2D& initial_value,
    PHLANIMVAR<Vector2D>& animation,
    const std::string& config_name,
    eAVarDamagePolicy policy
);
PHLWINDOW window_at(const Vector2D& position, uint8_t properties, PHLWINDOW ignore_window = nullptr);
PHLWINDOW hover_target_window_at(const Vector2D& position, PHLWINDOW ignore_window = nullptr);
void focus_monitor(PHLMONITOR monitor);
void focus_window(PHLWINDOW window);
bool can_warp_window_cursor(PHLWINDOW window);
void warp_window_cursor(PHLWINDOW window, bool force);
void set_cursor_override_enabled(bool enabled);
void damage_monitor(PHLMONITOR monitor);
void schedule_frame_for_monitor(PHLMONITOR monitor);
void simulate_mouse_movement();
void do_later(const std::function<void()>& callback);
void close_window(PHLWINDOW window);
void move_window_to_workspace(PHLWINDOW window, PHLWORKSPACE workspace);
void set_mouse_bind_mode(eMouseBindMode mode);
bool begin_drag_window(PHLWINDOW window, eMouseBindMode mode);
SP<Layout::ITarget> drag_controller_target();
bool drag_controller_is_tiled();
eMouseBindMode drag_controller_mode();
void end_drag_controller();
void invoke_mouse_button_original(void* thisptr, IPointer::SButtonEvent event);

void listen_mouse_button(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SButtonEvent, Event::SCallbackInfo&)>& callback
);
void listen_mouse_move(
    CHyprSignalListener& listener,
    const std::function<void(Vector2D, Event::SCallbackInfo&)>& callback
);
void listen_mouse_axis(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SAxisEvent, Event::SCallbackInfo&)>& callback
);
void listen_touch_down(
    CHyprSignalListener& listener,
    const std::function<void(ITouch::SDownEvent, Event::SCallbackInfo)>& callback
);
void listen_touch_up(
    CHyprSignalListener& listener,
    const std::function<void(ITouch::SUpEvent, Event::SCallbackInfo)>& callback
);
void listen_touch_motion(
    CHyprSignalListener& listener,
    const std::function<void(ITouch::SMotionEvent, Event::SCallbackInfo)>& callback
);
void listen_tablet_button(
    CHyprSignalListener& listener,
    const std::function<void(CTablet::SButtonEvent, Event::SCallbackInfo&)>& callback
);
void listen_tablet_tip(
    CHyprSignalListener& listener,
    const std::function<void(CTablet::STipEvent, Event::SCallbackInfo&)>& callback
);
void listen_tablet_proximity(
    CHyprSignalListener& listener,
    const std::function<void(CTablet::SProximityEvent, Event::SCallbackInfo&)>& callback
);
void listen_swipe_begin(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeBeginEvent, Event::SCallbackInfo&)>& callback
);
void listen_swipe_update(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeUpdateEvent, Event::SCallbackInfo&)>& callback
);
void listen_swipe_end(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeEndEvent, Event::SCallbackInfo&)>& callback
);
void listen_config_reloaded(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
);
void listen_monitor_added(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
);
void listen_monitor_removed(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
);

} // namespace HTCompat
