#include "runtime_compat.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/View.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/cursor/CursorShapeOverrideController.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>

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

PHLMONITOR HTCompat::cursor_monitor() {
    if (!g_pCompositor)
        return nullptr;

    return g_pCompositor->getMonitorFromCursor();
}

Vector2D HTCompat::mouse_coords() {
    if (!g_pInputManager)
        return {};

    return g_pInputManager->getMouseCoordsInternal();
}

void HTCompat::create_float_animation(
    float initial_value,
    PHLANIMVAR<float>& animation,
    const std::string& config_name,
    eAVarDamagePolicy policy
) {
    if (!g_pAnimationManager || !g_pConfigManager)
        return;

    g_pAnimationManager->createAnimation(
        initial_value,
        animation,
        g_pConfigManager->getAnimationPropertyConfig(config_name),
        policy
    );
}

void HTCompat::create_vector_animation(
    const Vector2D& initial_value,
    PHLANIMVAR<Vector2D>& animation,
    const std::string& config_name,
    eAVarDamagePolicy policy
) {
    if (!g_pAnimationManager || !g_pConfigManager)
        return;

    g_pAnimationManager->createAnimation(
        initial_value,
        animation,
        g_pConfigManager->getAnimationPropertyConfig(config_name),
        policy
    );
}

PHLMONITOR HTCompat::focused_monitor() {
    return Desktop::focusState()->monitor();
}

std::vector<PHLMONITOR> HTCompat::compositor_monitors() {
    if (!g_pCompositor)
        return {};

    std::vector<PHLMONITOR> monitors;
    monitors.reserve(g_pCompositor->m_monitors.size());
    for (const PHLMONITOR& monitor : g_pCompositor->m_monitors) {
        if (monitor != nullptr)
            monitors.push_back(monitor);
    }

    return monitors;
}

PHLMONITOR HTCompat::monitor_from_id(MONITORID monitor_id) {
    if (!g_pCompositor)
        return nullptr;

    return g_pCompositor->getMonitorFromID(monitor_id);
}

std::string HTCompat::monitor_description(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return {};

    return monitor->m_description;
}

PHLWINDOW HTCompat::window_at(const Vector2D& position, uint8_t properties, PHLWINDOW ignore_window) {
    if (!g_pCompositor)
        return nullptr;

    return g_pCompositor->vectorToWindowUnified(position, properties, ignore_window);
}

PHLWINDOW HTCompat::hover_target_window_at(const Vector2D& position, PHLWINDOW ignore_window) {
    return window_at(
        position,
        Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS
            | Desktop::View::ALLOW_FLOATING,
        ignore_window
    );
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

void HTCompat::set_cursor_override_enabled(bool enabled) {
    if (!Cursor::overrideController)
        return;

    if (enabled) {
        Cursor::overrideController->setOverride("left_ptr", Cursor::CURSOR_OVERRIDE_UNKNOWN);
    } else {
        Cursor::overrideController->unsetOverride(Cursor::CURSOR_OVERRIDE_UNKNOWN);
    }
}

void HTCompat::damage_monitor(PHLMONITOR monitor) {
    if (monitor == nullptr || !g_pHyprRenderer)
        return;

    g_pHyprRenderer->damageMonitor(monitor);
}

void HTCompat::schedule_frame_for_monitor(PHLMONITOR monitor) {
    if (monitor == nullptr || !g_pCompositor)
        return;

    g_pCompositor->scheduleFrameForMonitor(monitor);
}

void HTCompat::simulate_mouse_movement() {
    if (!g_pInputManager)
        return;

    g_pInputManager->simulateMouseMovement();
}

void HTCompat::close_window(PHLWINDOW window) {
    if (window == nullptr || !g_pCompositor)
        return;

    g_pCompositor->closeWindow(window);
}

void HTCompat::move_window_to_workspace(PHLWINDOW window, PHLWORKSPACE workspace) {
    if (window == nullptr || workspace == nullptr || !g_pCompositor)
        return;

    g_pCompositor->moveWindowToWorkspaceSafe(window, workspace);
}

void HTCompat::set_mouse_bind_mode(eMouseBindMode mode) {
    if (!g_pKeybindManager)
        return;

    g_pKeybindManager->changeMouseBindMode(mode);
}

bool HTCompat::begin_drag_window(PHLWINDOW window, eMouseBindMode mode) {
    if (window == nullptr || !g_layoutManager)
        return false;

    g_layoutManager->beginDragTarget(window->layoutTarget(), mode);
    return drag_controller_target() != nullptr;
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
