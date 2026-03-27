#include "runtime_compat.hpp"

#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/View.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>

SDispatchResult HTCompat::invoke_dispatcher(
    const std::string& dispatch_name,
    const std::string& dispatch_arg
) {
    if (!g_pKeybindManager)
        return {.success = false, .error = "keybind manager unavailable"};

    const auto dispatcher = g_pKeybindManager->m_dispatchers.find(dispatch_name);
    if (dispatcher == g_pKeybindManager->m_dispatchers.end())
        return {.success = false, .error = "invalid dispatcher"};

    return dispatcher->second(dispatch_arg);
}

PHLMONITOR HTCompat::focused_monitor() {
    return Desktop::focusState()->monitor();
}

void HTCompat::focus_monitor(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return;

    Desktop::focusState()->rawMonitorFocus(monitor);
}

void HTCompat::focus_window(PHLWINDOW window) {
    if (window == nullptr)
        return;

    Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_CLICK);
}

bool HTCompat::can_warp_window_cursor(PHLWINDOW window) {
    if (window == nullptr)
        return false;

    const auto focused_surface =
        Desktop::View::CWLSurface::fromResource(g_pSeatManager->m_state.pointerFocus.lock());
    return !focused_surface || focused_surface->view();
}

void HTCompat::warp_window_cursor(PHLWINDOW window, bool force) {
    if (window == nullptr)
        return;

    window->warpCursor(force);
}

void HTCompat::set_mouse_bind_mode(eMouseBindMode mode) {
    if (!g_pKeybindManager)
        return;

    g_pKeybindManager->changeMouseBindMode(mode);
}

SP<Layout::ITarget> HTCompat::drag_controller_target() {
    if (!g_layoutManager)
        return SP<Layout::ITarget> {};

    const auto& drag_controller = g_layoutManager->dragController();
    if (!drag_controller)
        return SP<Layout::ITarget> {};

    return drag_controller->target();
}

bool HTCompat::drag_controller_is_tiled() {
    if (!g_layoutManager)
        return false;

    const auto& drag_controller = g_layoutManager->dragController();
    if (!drag_controller)
        return false;

    return drag_controller->draggingTiled();
}

eMouseBindMode HTCompat::drag_controller_mode() {
    if (!g_layoutManager)
        return MBIND_INVALID;

    const auto& drag_controller = g_layoutManager->dragController();
    if (!drag_controller)
        return MBIND_INVALID;

    return drag_controller->mode();
}

void HTCompat::end_drag_controller() {
    if (!g_layoutManager)
        return;

    const auto& drag_controller = g_layoutManager->dragController();
    if (!drag_controller)
        return;

    drag_controller->dragEnd();
}

void HTCompat::listen_mouse_button(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SButtonEvent, Event::SCallbackInfo&)>& callback
) {
    listener = Event::bus()->m_events.input.mouse.button.listen(callback);
}

void HTCompat::listen_mouse_move(
    CHyprSignalListener& listener,
    const std::function<void(Vector2D, Event::SCallbackInfo&)>& callback
) {
    listener = Event::bus()->m_events.input.mouse.move.listen(callback);
}

void HTCompat::listen_mouse_axis(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SAxisEvent, Event::SCallbackInfo&)>& callback
) {
    listener = Event::bus()->m_events.input.mouse.axis.listen(callback);
}

void HTCompat::listen_touch_down(
    CHyprSignalListener& listener,
    const std::function<void(ITouch::SDownEvent, Event::SCallbackInfo)>& callback
) {
    listener = Event::bus()->m_events.input.touch.down.listen(callback);
}

void HTCompat::listen_touch_up(
    CHyprSignalListener& listener,
    const std::function<void(ITouch::SUpEvent, Event::SCallbackInfo)>& callback
) {
    listener = Event::bus()->m_events.input.touch.up.listen(callback);
}

void HTCompat::listen_touch_motion(
    CHyprSignalListener& listener,
    const std::function<void(ITouch::SMotionEvent, Event::SCallbackInfo)>& callback
) {
    listener = Event::bus()->m_events.input.touch.motion.listen(callback);
}

void HTCompat::listen_swipe_begin(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeBeginEvent, Event::SCallbackInfo&)>& callback
) {
    listener = Event::bus()->m_events.gesture.swipe.begin.listen(callback);
}

void HTCompat::listen_swipe_update(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeUpdateEvent, Event::SCallbackInfo&)>& callback
) {
    listener = Event::bus()->m_events.gesture.swipe.update.listen(callback);
}

void HTCompat::listen_swipe_end(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeEndEvent, Event::SCallbackInfo&)>& callback
) {
    listener = Event::bus()->m_events.gesture.swipe.end.listen(callback);
}

void HTCompat::listen_config_reloaded(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
) {
    listener = Event::bus()->m_events.config.reloaded.listen(callback);
}

void HTCompat::listen_monitor_added(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
) {
    listener = Event::bus()->m_events.monitor.added.listen(callback);
}

void HTCompat::listen_monitor_removed(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
) {
    listener = Event::bus()->m_events.monitor.removed.listen(callback);
}
