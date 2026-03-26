#include "dispatchers.hpp"

#include <string>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>

#include "../config.hpp"
#include "../globals.hpp"
#include "../logic/dispatch_args.hpp"
#include "../overview.hpp"
#include "guards.hpp"

namespace {

using HTPlugin::guardedDispatch;

SDispatchResult dispatchIf(std::string arg, bool is_active) {
    return guardedDispatch("dispatch_if", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.passEvent = true, .success = false, .error = "ht_manager is null"};
        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.passEvent = true, .success = false, .error = "cursor_view is null"};
        if (cursor_view->active != is_active)
            return {.passEvent = true, .success = false, .error = "predicate not met"};

        const auto dispatch_str = arg.substr(0, arg.find_first_of(' '));

        auto dispatch_arg = std::string();
        if ((int)arg.find_first_of(' ') != -1)
            dispatch_arg = arg.substr(arg.find_first_of(' ') + 1);

        const auto dispatcher = g_pKeybindManager->m_dispatchers.find(dispatch_str);
        if (dispatcher == g_pKeybindManager->m_dispatchers.end())
            return {.success = false, .error = "invalid dispatcher"};

        SDispatchResult res = dispatcher->second(dispatch_arg);

        Log::logger->log(
            LOG,
            "[Hyprtasking] passthrough dispatch: {} : {}{}",
            dispatch_str,
            dispatch_arg,
            res.success ? "" : " -> " + res.error
        );

        return res;
    });
}

SDispatchResult dispatchIfNotActive(std::string arg) {
    return dispatchIf(arg, false);
}

SDispatchResult dispatchIfActive(std::string arg) {
    return dispatchIf(arg, true);
}

SDispatchResult dispatchToggleView(std::string arg) {
    return guardedDispatch("dispatch_toggle_view", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.success = false, .error = "ht_manager is null"};

        if (arg == "all") {
            if (ht_manager->has_active_view())
                ht_manager->hide_all_views();
            else
                ht_manager->show_all_views();
        } else if (arg == "cursor") {
            if (ht_manager->cursor_view_active())
                ht_manager->hide_all_views();
            else
                ht_manager->show_cursor_view();
        } else {
            return {.success = false, .error = "invalid arg"};
        }

        return {};
    });
}

SDispatchResult dispatchMove(std::string arg) {
    return guardedDispatch("dispatch_move", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.success = false, .error = "ht_manager is null"};
        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.success = false, .error = "cursor_view is null"};
        cursor_view->move(arg, false);
        return {};
    });
}

SDispatchResult dispatchMoveWindow(std::string arg) {
    return guardedDispatch("dispatch_move_window", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.success = false, .error = "ht_manager is null"};
        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.success = false, .error = "cursor_view is null"};
        cursor_view->move(arg, true);
        return {};
    });
}

void setOffset(PHTVIEW view, int new_offset) {
    if (view == nullptr || view->closing)
        return;

    Log::logger->log(
        LOG,
        "[Hyprtasking] View \"{}\", previous offset: {}, new: {}",
        view->get_monitor()->m_name,
        view->layout->first_ws_offset,
        new_offset
    );
    view->layout->first_ws_offset = new_offset;
}

SDispatchResult dispatchSetOffset(std::string arg) {
    return guardedDispatch("dispatch_setoffset", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.success = false, .error = "ht_manager is null"};
        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.success = false, .error = "cursor_view is null"};

        const int  original_offset = cursor_view->layout->first_ws_offset;
        const auto parsed_arg      = HTLogic::parseOffsetArg(arg, original_offset);
        if (!parsed_arg.ok)
            return {.success = false, .error = parsed_arg.error};

        setOffset(cursor_view, parsed_arg.value);

        const PHLMONITOR monitor = cursor_view->get_monitor();
        if (monitor == nullptr)
            return {.success = false, .error = "monitor is null"};
        const PHLWORKSPACE active_workspace = monitor->m_activeWorkspace;
        if (active_workspace == nullptr)
            return {.success = false, .error = "active_workspace is null"};

        const int offset_delta = parsed_arg.value - original_offset;

        Log::logger->log(
            LOG,
            "[Hyprtasking] Setting offset from workspace \"{}\", from offset: {}",
            active_workspace->m_id,
            original_offset
        );

        cursor_view->move_id(active_workspace->m_id + offset_delta, false);
        return {};
    });
}

SDispatchResult changeLayer(std::string arg, bool move_window) {
    if (ht_manager == nullptr)
        return {.success = false, .error = "ht_manager is null"};

    const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
    if (cursor_view == nullptr)
        return {.success = false, .error = "cursor_view is null"};

    if (cursor_view->layout->layout_name() != "grid")
        return {.success = false, .error = "only grid layout is supported"};

    const int rows            = HTConfig::value<Hyprlang::INT>("grid:rows");
    const int cols            = HTConfig::value<Hyprlang::INT>("grid:cols");
    const int layers          = HTConfig::value<Hyprlang::INT>("grid:layers");
    const int loop_layers     = HTConfig::value<Hyprlang::INT>("grid:loop_layers");
    const int original_offset = cursor_view->layout->first_ws_offset;
    const auto parsed_arg =
        HTLogic::parseLayerOffsetArg(arg, original_offset, rows, cols, layers);
    if (!parsed_arg.ok)
        return {.success = false, .error = parsed_arg.error};
    int resulting_offset = parsed_arg.requested_offset;

    const PHLMONITOR monitor = cursor_view->get_monitor();
    if (monitor == nullptr)
        return {.success = false, .error = "monitor is null"};
    const PHLWORKSPACE active_workspace = monitor->m_activeWorkspace;
    if (active_workspace == nullptr)
        return {.success = false, .error = "active_workspace is null"};
    const WORKSPACEID source_ws_id = active_workspace->m_id;

    const int   offset_delta = resulting_offset - original_offset;
    WORKSPACEID target_ws_id = source_ws_id + offset_delta;

    if (resulting_offset > parsed_arg.max_offset || resulting_offset < 0) {
        if (!loop_layers)
            return {};

        target_ws_id = source_ws_id - original_offset;
        if (resulting_offset < 0) {
            target_ws_id += parsed_arg.max_offset;
            resulting_offset = parsed_arg.max_offset;
        } else if (resulting_offset > parsed_arg.max_offset) {
            resulting_offset = 0;
        }
    }

    setOffset(cursor_view, resulting_offset);
    cursor_view->move_id(target_ws_id, move_window);
    return {};
}

SDispatchResult dispatchSetLayer(std::string arg) {
    return guardedDispatch("dispatch_setlayer", [&]() { return changeLayer(arg, false); });
}

SDispatchResult dispatchSetLayerWindow(std::string arg) {
    return guardedDispatch("dispatch_setlayerwindow", [&]() { return changeLayer(arg, true); });
}

SDispatchResult dispatchKillHover([[maybe_unused]] std::string arg) {
    return guardedDispatch("dispatch_kill_hover", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.success = false, .error = "ht_manager is null"};

        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.success = false, .error = "cursor_view is null"};
        const PHLWINDOW hovered_window = ht_manager->get_window_from_cursor(!cursor_view->active);
        if (hovered_window == nullptr)
            return {.success = false, .error = "hovered_window is null"};

        g_pCompositor->closeWindow(hovered_window);
        return {};
    });
}

}

namespace HTPlugin {

void registerDispatchers() {
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:if_not_active", dispatchIfNotActive);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:if_active", dispatchIfActive);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:toggle", dispatchToggleView);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:move", dispatchMove);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:movewindow", dispatchMoveWindow);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:killhovered", dispatchKillHover);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:setoffset", dispatchSetOffset);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:setlayer", dispatchSetLayer);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:setlayerwindow", dispatchSetLayerWindow);
}

}
