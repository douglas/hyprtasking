#include "runtime_compat.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <memory>

#include "../build_contract.hpp"
#include "../globals.hpp"
#include "../plugin/guards.hpp"
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
#include <hyprland/src/protocols/core/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include "animation_compat.hpp"
#include "cursor_compat.hpp"
#include "monitor_compat.hpp"
#include "pointer_compat.hpp"
#include "profile.hpp"

SDispatchResult
HTCompat::invoke_dispatcher(const std::string& dispatch_name, const std::string& dispatch_arg) {
    if (!g_pKeybindManager)
        return {.success = false, .error = "keybind manager unavailable"};

    const auto dispatcher = g_pKeybindManager->m_dispatchers.find(dispatch_name);
    if (dispatcher == g_pKeybindManager->m_dispatchers.end())
        return {.success = false, .error = "invalid dispatcher"};

    return dispatcher->second(dispatch_arg);
}

// Only reset a submap Hyprtasking entered, so cleanup never kicks the user out
// of a submap of their own.
static bool entered_submap = false;

bool HTCompat::enter_submap(const std::string& name) {
    if (!invoke_dispatcher("submap", name).success)
        return false;
    entered_submap = true;
    return true;
}

bool HTCompat::exit_submap() {
    if (!entered_submap)
        return true;
    if (!invoke_dispatcher("submap", "reset").success)
        return false;
    entered_submap = false;
    return true;
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

bool HTCompat::create_float_animation(
    float initial_value,
    PHLANIMVAR<float>& animation,
    const std::string& config_name,
    eAVarDamagePolicy policy
) {
    const auto manager = animation_manager();
    if (manager == nullptr)
        return false;

#if HT_HYPRLAND_GE_0_55
    if (!Config::animationTree())
        return false;

    const auto config = Config::animationTree()->getAnimationPropertyConfig(config_name);
#else
    if (!g_pConfigManager)
        return false;

    const auto config = g_pConfigManager->getAnimationPropertyConfig(config_name);
#endif

    manager->createAnimation(initial_value, animation, config, policy);
    return static_cast<bool>(animation);
}

bool HTCompat::create_vector_animation(
    const Vector2D& initial_value,
    PHLANIMVAR<Vector2D>& animation,
    const std::string& config_name,
    eAVarDamagePolicy policy
) {
    const auto manager = animation_manager();
    if (manager == nullptr)
        return false;

#if HT_HYPRLAND_GE_0_55
    if (!Config::animationTree())
        return false;

    const auto config = Config::animationTree()->getAnimationPropertyConfig(config_name);
#else
    if (!g_pConfigManager)
        return false;

    const auto config = g_pConfigManager->getAnimationPropertyConfig(config_name);
#endif

    manager->createAnimation(initial_value, animation, config, policy);
    return static_cast<bool>(animation);
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

std::string HTCompat::monitor_name(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return {};

    return monitor->m_name;
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

namespace {

std::vector<uint64_t>& pending_do_later() {
    static std::vector<uint64_t> pending;
    return pending;
}

} // namespace

bool HTCompat::do_later(const std::function<void()>& callback) {
    if (!callback)
        return false;

    if (!g_pEventLoopManager)
        return false;

#if HT_HYPRLAND_GE_0_55
    const auto seq = std::make_shared<uint64_t>(0);
    *seq = g_pEventLoopManager->doLater([deferred = callback, seq] {
        std::erase(pending_do_later(), *seq);
        HTPlugin::guardedCallback("do_later", deferred);
    });
    pending_do_later().push_back(*seq);
#else
    // 0.54 has no removeDoLater, so queued callbacks cannot be cancelled.
    g_pEventLoopManager->doLater([callback] { HTPlugin::guardedCallback("do_later", callback); });
#endif
    return true;
}

void HTCompat::cancel_pending_do_later() {
#if HT_HYPRLAND_GE_0_55
    if (g_pEventLoopManager) {
        for (const uint64_t seq : pending_do_later())
            g_pEventLoopManager->removeDoLater(seq);
    }
#endif
    pending_do_later().clear();
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
    using origOnMouseButton = void (*)(void*, IPointer::SButtonEvent, SP<IPointer>);
    const auto original = hook_original<origOnMouseButton>(g_hooks.input_mouse_button.hook);
    if (thisptr == nullptr || original == nullptr)
        return false;

    original(thisptr, event, mouse);
    return true;
}
#else
bool HTCompat::invoke_mouse_button_original(void* thisptr, IPointer::SButtonEvent event) {
    using origOnMouseButton = void (*)(void*, IPointer::SButtonEvent);
    const auto original = hook_original<origOnMouseButton>(g_hooks.input_mouse_button.hook);
    if (thisptr == nullptr || original == nullptr)
        return false;

    original(thisptr, event);
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

// Fired when mirroring or other layout changes edit the monitor list without
// an added/removed event.
bool HTCompat::listen_monitor_layout_changed(
    CHyprSignalListener& listener,
    const std::function<void()>& callback
) {
    auto& bus = Event::bus();
    if (!bus || !callback)
        return false;

    listener = bus->m_events.monitor.layoutChanged.listen(callback);
    return true;
}

bool HTCompat::listen_window_set_changed(
    std::vector<CHyprSignalListener>& listeners,
    const std::function<void()>& callback
) {
    auto& bus = Event::bus();
    if (!bus || !callback)
        return false;

    listeners.clear();
    listeners.push_back(bus->m_events.window.open.listen(callback));
    listeners.push_back(bus->m_events.window.close.listen(callback));
    listeners.push_back(bus->m_events.window.moveToWorkspace.listen(callback));
    return true;
}

std::vector<PHLWINDOW> HTCompat::compositor_windows() {
#if HT_HYPRLAND_GE_0_56
    if (!Desktop::viewState())
        return {};

    return Desktop::viewState()->windows();
#else
    if (!g_pCompositor)
        return {};

    return g_pCompositor->m_windows;
#endif
}

CHyprSignalListener
HTCompat::listen_window_surface_commit(PHLWINDOW window, const std::function<void()>& callback) {
    if (window == nullptr || !callback)
        return {};

    const auto surface = window->resource();
    if (surface == nullptr)
        return {};

    return surface->m_events.commit.listen(callback);
}

SP<CEventLoopTimer> HTCompat::start_repeating_timer(
    std::chrono::milliseconds interval,
    const std::function<void()>& callback
) {
    if (!g_pEventLoopManager || !callback)
        return nullptr;

    auto timer = makeShared<CEventLoopTimer>(
        interval,
        [interval, on_fire = callback](SP<CEventLoopTimer> self, void*) {
            on_fire();
            self->updateTimeout(interval);
        },
        nullptr
    );
    g_pEventLoopManager->addTimer(timer);
    return timer;
}

void HTCompat::stop_timer(SP<CEventLoopTimer>& timer) {
    if (timer == nullptr)
        return;

    timer->cancel();
    if (g_pEventLoopManager)
        g_pEventLoopManager->removeTimer(timer);
    timer.reset();
}
