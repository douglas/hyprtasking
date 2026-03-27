#include "runtime_compat.hpp"

#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>

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
