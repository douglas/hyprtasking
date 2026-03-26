#include "runtime.hpp"

#include <algorithm>
#include <linux/input-event-codes.h>
#include <vector>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/macros.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprlang.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "../compat/renderer_hooks.hpp"
#include "../config.hpp"
#include "../globals.hpp"
#include "../logic/reload_model.hpp"
#include "../overview.hpp"
#include "guards.hpp"

using Hyprutils::Utils::CScopeGuard;

namespace {

using HTPlugin::guardedCallback;

void onMouseButton(IPointer::SButtonEvent e, Event::SCallbackInfo& info) {
    guardedCallback("on_mouse_button", [&]() {
        if (ht_manager == nullptr)
            return;

        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return;

        const bool pressed = e.state == WL_POINTER_BUTTON_STATE_PRESSED;

        const unsigned int drag_button   = HTConfig::value<Hyprlang::INT>("drag_button");
        const unsigned int select_button = HTConfig::value<Hyprlang::INT>("select_button");

        if (pressed && e.button == drag_button) {
            info.cancelled = ht_manager->start_window_drag();
        } else if (!pressed && e.button == drag_button) {
            info.cancelled = ht_manager->end_window_drag();
        } else if (pressed && e.button == select_button) {
            info.cancelled = ht_manager->exit_to_workspace();
        }
    });
}

void onMouseMove([[maybe_unused]] Vector2D cursor, Event::SCallbackInfo& info) {
    guardedCallback("on_mouse_move", [&]() {
        if (ht_manager == nullptr)
            return;
        info.cancelled = ht_manager->on_mouse_move();
    });
}

void onMouseAxis(IPointer::SAxisEvent e, Event::SCallbackInfo& info) {
    guardedCallback("on_mouse_axis", [&]() {
        if (ht_manager == nullptr)
            return;
        info.cancelled = ht_manager->on_mouse_axis(e.delta);
    });
}

void onSwipeBegin(
    [[maybe_unused]] IPointer::SSwipeBeginEvent e,
    [[maybe_unused]] Event::SCallbackInfo& info
) {
    guardedCallback("on_swipe_begin", [&]() {
        if (ht_manager == nullptr)
            return;
        ht_manager->swipe_start();
    });
}

void onSwipeUpdate(IPointer::SSwipeUpdateEvent e, Event::SCallbackInfo& info) {
    guardedCallback("on_swipe_update", [&]() {
        if (ht_manager == nullptr)
            return;
        info.cancelled = ht_manager->swipe_update(e);
    });
}

void onSwipeEnd([[maybe_unused]] IPointer::SSwipeEndEvent e, Event::SCallbackInfo& info) {
    guardedCallback("on_swipe_end", [&]() {
        if (ht_manager == nullptr)
            return;
        info.cancelled = ht_manager->swipe_end();
    });
}

void onRenderStage(eRenderStage stage) {
    guardedCallback("on_render_stage", [&]() {
        if (stage != RENDER_POST_WINDOWS || ht_manager == nullptr || !HTCompat::beginOverviewRender())
            return;

        CScopeGuard reset_rendering_state([] { HTCompat::endOverviewRender(); });

        const PHLMONITOR monitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
        if (monitor == nullptr)
            return;

        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view == nullptr || (!view->navigating && !view->active))
            return;

        view->layout->render();
    });
}

void cancelEvent(Event::SCallbackInfo& info) {
    if (ht_manager == nullptr || !ht_manager->cursor_view_active())
        return;
    info.cancelled = true;
}

void notifyConfigChanges() {
    const int rows = HTConfig::value<Hyprlang::INT>("rows");
    if (rows != -1) {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[Hyprtasking] plugin:hyprtasking:rows has moved to plugin:hyprtasking:grid:rows in the config.",
            CHyprColor {1.0, 0.2, 0.2, 1.0},
            20000
        );
    }

    CVarList exit_behavior {HTConfig::value<Hyprlang::STRING>("exit_behavior"), 0, 's', true};
    if (exit_behavior.size() != 0) {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[Hyprtasking] plugin:hyprtasking:exit_behavior is deprecated. Hyprtasking will always exit to the active workspace, which is changed when interacting with the plugin.",
            CHyprColor {1.0, 0.2, 0.2, 1.0},
            20000
        );
    }
}

std::vector<MONITORID> currentMonitorIDs() {
    std::vector<MONITORID> monitor_ids;
    monitor_ids.reserve(g_pCompositor->m_monitors.size());

    for (const PHLMONITOR& monitor : g_pCompositor->m_monitors) {
        if (monitor == nullptr)
            continue;
        monitor_ids.push_back(monitor->m_id);
    }

    return monitor_ids;
}

std::vector<MONITORID> currentViewIDs() {
    std::vector<MONITORID> view_ids;
    if (ht_manager == nullptr)
        return view_ids;

    view_ids.reserve(ht_manager->views.size());
    for (const PHTVIEW& view : ht_manager->views) {
        if (view == nullptr)
            continue;
        view_ids.push_back(view->monitor_id);
    }

    return view_ids;
}

void syncMonitorViews(bool reinitialize_inactive_views) {
    if (ht_manager == nullptr)
        return;

    const auto monitor_ids    = currentMonitorIDs();
    const auto stale_view_ids = HTLogic::staleMonitorViewIDs(currentViewIDs(), monitor_ids);
    std::erase_if(ht_manager->views, [&](const PHTVIEW& view) {
        if (view == nullptr)
            return true;

        const bool stale =
            std::ranges::find(stale_view_ids, view->monitor_id) != stale_view_ids.end();
        if (stale) {
            Log::logger->log(LOG, "[Hyprtasking] Removing stale view for monitor id {}", view->monitor_id);
        }
        return stale;
    });

    const auto missing_view_ids = HTLogic::missingMonitorViewIDs(currentViewIDs(), monitor_ids);
    for (const PHLMONITOR& monitor : g_pCompositor->m_monitors) {
        if (monitor == nullptr)
            continue;

        const bool missing =
            std::ranges::find(missing_view_ids, monitor->m_id) != missing_view_ids.end();
        if (missing) {
            ht_manager->views.push_back(makeShared<HTView>(monitor->m_id));

            Log::logger->log(
                LOG,
                "[Hyprtasking] Registering view for monitor {} with resolution {}x{}",
                monitor->m_description,
                monitor->m_transformedSize.x,
                monitor->m_transformedSize.y
            );
            continue;
        }

        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view != nullptr && reinitialize_inactive_views && !view->active && !view->closing)
            view->layout->init_position();
    }
}

void onConfigReloaded() {
    notifyConfigChanges();

    if (ht_manager == nullptr)
        return;

    syncMonitorViews(false);

    const Hyprlang::STRING new_layout = HTConfig::value<Hyprlang::STRING>("layout");
    const bool close_overview_on_reload = HTConfig::value<Hyprlang::INT>("close_overview_on_reload");
    for (PHTVIEW& view : ht_manager->views) {
        if (view == nullptr || view->get_monitor() == nullptr)
            continue;

        view->reload_config(new_layout, close_overview_on_reload);
    }
}

}

namespace HTPlugin {

void initializeConfig() {
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:layout", Hyprlang::STRING {"grid"});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:bg_color", Hyprlang::INT {0x000000FF});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:gap_size", Hyprlang::FLOAT {8.f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:border_size", Hyprlang::FLOAT {4.f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:exit_on_hovered", Hyprlang::INT {0});
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:warp_on_move_window",
        Hyprlang::INT {1}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:close_overview_on_reload",
        Hyprlang::INT {1}
    );

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:drag_button", Hyprlang::INT {BTN_LEFT});
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:select_button",
        Hyprlang::INT {BTN_RIGHT}
    );

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:gestures:enabled", Hyprlang::INT {1});
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:gestures:move_fingers",
        Hyprlang::INT {3}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:gestures:move_distance",
        Hyprlang::FLOAT {300.0}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:gestures:open_fingers",
        Hyprlang::INT {4}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:gestures:open_distance",
        Hyprlang::FLOAT {300.0}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:gestures:open_positive",
        Hyprlang::INT {1}
    );

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:rows", Hyprlang::INT {3});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:cols", Hyprlang::INT {3});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:layers", Hyprlang::INT {1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:loop_layers", Hyprlang::INT {1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:grid:loop", Hyprlang::INT {0});
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:grid:gaps_use_aspect_ratio",
        Hyprlang::INT {0}
    );

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:linear:blur", Hyprlang::INT {1});
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:linear:height",
        Hyprlang::FLOAT {300.f}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:hyprtasking:linear:scroll_speed",
        Hyprlang::FLOAT {1.f}
    );
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:linear:top", Hyprlang::INT {0});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:rows", Hyprlang::INT {-1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtasking:exit_behavior", Hyprlang::STRING {""});

    HyprlandAPI::reloadConfig();
}

void registerCallbacks() {
    static auto mouse_button = Event::bus()->m_events.input.mouse.button.listen(onMouseButton);
    static auto mouse_move   = Event::bus()->m_events.input.mouse.move.listen(onMouseMove);
    static auto mouse_axis   = Event::bus()->m_events.input.mouse.axis.listen(onMouseAxis);

    static auto touch_down = Event::bus()->m_events.input.touch.down.listen(
        []([[maybe_unused]] ITouch::SDownEvent e, Event::SCallbackInfo i) { cancelEvent(i); }
    );
    static auto touch_up = Event::bus()->m_events.input.touch.up.listen(
        []([[maybe_unused]] ITouch::SUpEvent e, Event::SCallbackInfo i) { cancelEvent(i); }
    );
    static auto touch_motion = Event::bus()->m_events.input.touch.motion.listen(
        []([[maybe_unused]] ITouch::SMotionEvent e, Event::SCallbackInfo i) { cancelEvent(i); }
    );

    static auto swipe_begin  = Event::bus()->m_events.gesture.swipe.begin.listen(onSwipeBegin);
    static auto swipe_update = Event::bus()->m_events.gesture.swipe.update.listen(onSwipeUpdate);
    static auto swipe_end    = Event::bus()->m_events.gesture.swipe.end.listen(onSwipeEnd);

    static auto config_reloaded = Event::bus()->m_events.config.reloaded.listen(onConfigReloaded);
    static auto monitor_added   = Event::bus()->m_events.monitor.added.listen(registerMonitors);
    static auto render_stage    = Event::bus()->m_events.render.stage.listen(onRenderStage);

    (void)mouse_button;
    (void)mouse_move;
    (void)mouse_axis;
    (void)touch_down;
    (void)touch_up;
    (void)touch_motion;
    (void)swipe_begin;
    (void)swipe_update;
    (void)swipe_end;
    (void)config_reloaded;
    (void)monitor_added;
    (void)render_stage;
}

void registerMonitors() {
    syncMonitorViews(true);
}

}
