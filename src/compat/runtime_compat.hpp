#pragma once

#include <functional>
#include <string>

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/signal/Listener.hpp>

namespace HTCompat {

using Hyprutils::Signal::CHyprSignalListener;

SDispatchResult invoke_dispatcher(const std::string& dispatch_name, const std::string& dispatch_arg);
PHLMONITOR focused_monitor();
void focus_monitor(PHLMONITOR monitor);
void focus_window(PHLWINDOW window);
bool can_warp_window_cursor(PHLWINDOW window);
void warp_window_cursor(PHLWINDOW window, bool force);
void set_mouse_bind_mode(eMouseBindMode mode);
SP<Layout::ITarget> drag_controller_target();
bool drag_controller_is_tiled();
eMouseBindMode drag_controller_mode();
void end_drag_controller();

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
