#include "runtime_compat.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>

#include "../build_contract.hpp"
#include "../globals.hpp"
#if HT_HYPRLAND_GE_0_55
    #include <hyprland/src/config/shared/animation/AnimationTree.hpp>
#endif
#if HT_HYPRLAND_GE_0_56
    #include <hyprland/src/desktop/state/GlobalWindowController.hpp>
    #include <hyprland/src/desktop/state/ViewState.hpp>
    #include <hyprland/src/state/MonitorState.hpp>
    #include <hyprland/src/state/WorkspaceState.hpp>
#endif
#include <hyprland/src/desktop/view/View.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include "animation_compat.hpp"
#include "cursor_compat.hpp"
#include "monitor_compat.hpp"
#include "pointer_compat.hpp"

SDispatchResult
HTCompat::invoke_dispatcher(const std::string& dispatch_name, const std::string& dispatch_arg) {
    if (!g_pKeybindManager)
        return {.success = false, .error = "keybind manager unavailable"};

    const auto dispatcher = g_pKeybindManager->m_dispatchers.find(dispatch_name);
    if (dispatcher == g_pKeybindManager->m_dispatchers.end())
        return {.success = false, .error = "invalid dispatcher"};

    return dispatcher->second(dispatch_arg);
}

bool HTCompat::enter_submap(const std::string& name) {
    return invoke_dispatcher("submap", name).success;
}

bool HTCompat::exit_submap() {
    return invoke_dispatcher("submap", "reset").success;
}

PHLMONITOR HTCompat::cursor_monitor() {
#if HT_HYPRLAND_GE_0_56
    const auto pointer_manager = HTCompat::pointer_manager();
    if (!State::monitorState() || pointer_manager == nullptr)
        return nullptr;

    return State::monitorState()->query().vec(pointer_manager->position()).run();
#else
    if (!g_pCompositor)
        return nullptr;

    return g_pCompositor->getMonitorFromCursor();
#endif
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
    const auto manager = animation_manager();
    if (manager == nullptr)
        return;

#if HT_HYPRLAND_GE_0_55
    if (!Config::animationTree())
        return;

    const auto config = Config::animationTree()->getAnimationPropertyConfig(config_name);
#else
    if (!g_pConfigManager)
        return;

    const auto config = g_pConfigManager->getAnimationPropertyConfig(config_name);
#endif

    manager->createAnimation(initial_value, animation, config, policy);
}

void HTCompat::create_vector_animation(
    const Vector2D& initial_value,
    PHLANIMVAR<Vector2D>& animation,
    const std::string& config_name,
    eAVarDamagePolicy policy
) {
    const auto manager = animation_manager();
    if (manager == nullptr)
        return;

#if HT_HYPRLAND_GE_0_55
    if (!Config::animationTree())
        return;

    const auto config = Config::animationTree()->getAnimationPropertyConfig(config_name);
#else
    if (!g_pConfigManager)
        return;

    const auto config = g_pConfigManager->getAnimationPropertyConfig(config_name);
#endif

    manager->createAnimation(initial_value, animation, config, policy);
}

std::vector<PHLMONITOR> HTCompat::compositor_monitors() {
#if HT_HYPRLAND_GE_0_56
    if (!State::monitorState())
        return {};

    std::vector<PHLMONITOR> monitors;
    monitors.reserve(State::monitorState()->monitors().size());
    for (const PHLMONITOR& monitor : State::monitorState()->monitors()) {
        if (monitor != nullptr)
            monitors.push_back(monitor);
    }

    return monitors;
#else
    if (!g_pCompositor)
        return {};

    std::vector<PHLMONITOR> monitors;
    monitors.reserve(g_pCompositor->m_monitors.size());
    for (const PHLMONITOR& monitor : g_pCompositor->m_monitors) {
        if (monitor != nullptr)
            monitors.push_back(monitor);
    }

    return monitors;
#endif
}

PHLMONITOR HTCompat::monitor_from_id(MONITORID monitor_id) {
#if HT_HYPRLAND_GE_0_56
    if (!State::monitorState())
        return nullptr;

    return State::monitorState()->query().id(monitor_id).run();
#else
    if (!g_pCompositor)
        return nullptr;

    return g_pCompositor->getMonitorFromID(monitor_id);
#endif
}

std::string HTCompat::monitor_description(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return {};

    return monitor->m_description;
}

PHLWINDOW
HTCompat::window_at(const Vector2D& position, uint16_t properties, PHLWINDOW ignore_window) {
#if HT_HYPRLAND_GE_0_56
    if (!Desktop::viewState())
        return nullptr;

    return Desktop::viewState()->hitTest().windowAt(position, properties, ignore_window);
#else
    if (!g_pCompositor)
        return nullptr;

    return g_pCompositor->vectorToWindowUnified(position, properties, ignore_window);
#endif
}

PHLWINDOW HTCompat::hover_target_window_at(const Vector2D& position, PHLWINDOW ignore_window) {
    return window_at(
        position,
        Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS
            | Desktop::View::ALLOW_FLOATING,
        ignore_window
    );
}

void HTCompat::set_cursor_override_enabled(bool enabled) {
    if (!CursorCompat::overrideController)
        return;

    if (enabled) {
        CursorCompat::overrideController->setOverride(
            "left_ptr",
            CursorCompat::CURSOR_OVERRIDE_UNKNOWN
        );
    } else {
        CursorCompat::overrideController->unsetOverride(CursorCompat::CURSOR_OVERRIDE_UNKNOWN);
    }
}

void HTCompat::damage_monitor(PHLMONITOR monitor) {
    if (monitor == nullptr || !g_pHyprRenderer)
        return;

    g_pHyprRenderer->damageMonitor(monitor);
}

void HTCompat::schedule_frame_for_monitor(PHLMONITOR monitor) {
#if HT_HYPRLAND_GE_0_56
    if (monitor == nullptr)
        return;

    monitor->scheduleFrame();
#else
    if (monitor == nullptr || !g_pCompositor)
        return;

    g_pCompositor->scheduleFrameForMonitor(monitor);
#endif
}

void HTCompat::simulate_mouse_movement() {
    if (!g_pInputManager)
        return;

    g_pInputManager->simulateMouseMovement();
}

bool HTCompat::do_later(const std::function<void()>& callback) {
    if (!callback)
        return false;

    if (!g_pEventLoopManager)
        return false;

    g_pEventLoopManager->doLater(callback);
    return true;
}

void HTCompat::move_window_to_workspace(PHLWINDOW window, PHLWORKSPACE workspace) {
#if HT_HYPRLAND_GE_0_56
    if (window == nullptr || workspace == nullptr || !Desktop::globalWindowController())
        return;

    Desktop::globalWindowController()->moveWindowToWorkspace(window, workspace);
#else
    if (window == nullptr || workspace == nullptr || !g_pCompositor)
        return;

    g_pCompositor->moveWindowToWorkspaceSafe(window, workspace);
#endif
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

#if HT_HYPRLAND_GE_0_55
bool HTCompat::invoke_mouse_button_original(
    void* thisptr,
    IPointer::SButtonEvent event,
    SP<IPointer> mouse
) {
    if (thisptr == nullptr || input_mouse_button_hook == nullptr)
        return false;
    if (input_mouse_button_hook->m_original == nullptr)
        return false;

    using origOnMouseButton = void (*)(void*, IPointer::SButtonEvent, SP<IPointer>);
    ((origOnMouseButton)input_mouse_button_hook->m_original)(thisptr, event, mouse);
    return true;
}
#else
bool HTCompat::invoke_mouse_button_original(void* thisptr, IPointer::SButtonEvent event) {
    if (thisptr == nullptr || input_mouse_button_hook == nullptr)
        return false;
    if (input_mouse_button_hook->m_original == nullptr)
        return false;

    using origOnMouseButton = void (*)(void*, IPointer::SButtonEvent);
    ((origOnMouseButton)input_mouse_button_hook->m_original)(thisptr, event);
    return true;
}
#endif

bool HTCompat::listen_mouse_move(
    CHyprSignalListener& listener,
    const std::function<void(Vector2D, Event::SCallbackInfo&)>& callback
) {
    auto& bus = Event::bus();
    if (!bus || !callback)
        return false;

    listener = bus->m_events.input.mouse.move.listen(callback);
    return true;
}

bool HTCompat::listen_swipe_begin(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeBeginEvent, Event::SCallbackInfo&)>& callback
) {
    auto& bus = Event::bus();
    if (!bus || !callback)
        return false;

    listener = bus->m_events.gesture.swipe.begin.listen(callback);
    return true;
}

bool HTCompat::listen_swipe_update(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeUpdateEvent, Event::SCallbackInfo&)>& callback
) {
    auto& bus = Event::bus();
    if (!bus || !callback)
        return false;

    listener = bus->m_events.gesture.swipe.update.listen(callback);
    return true;
}

bool HTCompat::listen_swipe_end(
    CHyprSignalListener& listener,
    const std::function<void(IPointer::SSwipeEndEvent, Event::SCallbackInfo&)>& callback
) {
    auto& bus = Event::bus();
    if (!bus || !callback)
        return false;

    listener = bus->m_events.gesture.swipe.end.listen(callback);
    return true;
}

bool HTCompat::listen_config_reloaded(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
) {
    auto& bus = Event::bus();
    if (!bus || !callback)
        return false;

    listener = bus->m_events.config.reloaded.listen(callback);
    return true;
}

bool HTCompat::listen_monitor_added(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
) {
    auto& bus = Event::bus();
    if (!bus || !callback)
        return false;

    listener = bus->m_events.monitor.added.listen(callback);
    return true;
}

bool HTCompat::listen_monitor_removed(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
) {
    auto& bus = Event::bus();
    if (!bus || !callback)
        return false;

    listener = bus->m_events.monitor.removed.listen(callback);
    return true;
}
