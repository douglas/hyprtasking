#include "dispatchers.hpp"

#include <string>

#include <hyprland/src/plugins/PluginAPI.hpp>

#include "../compat/runtime_compat.hpp"
#include "../globals.hpp"
#include "../overview.hpp"
#include "guards.hpp"

namespace {

using HTPlugin::guardedDispatch;

SDispatchResult dispatchIf(std::string arg, bool is_active) {
    return guardedDispatch("dispatch_if", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.passEvent = true, .success = false, .error = "ht_manager is null"};
        if (!ht_manager->runtime_enabled())
            return {.passEvent = true, .success = false, .error = "runtime disabled"};

        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.passEvent = true, .success = false, .error = "cursor_view is null"};
        if (cursor_view->active != is_active)
            return {.passEvent = true, .success = false, .error = "predicate not met"};

        const auto dispatch_str = arg.substr(0, arg.find_first_of(' '));

        auto dispatch_arg = std::string();
        if ((int)arg.find_first_of(' ') != -1)
            dispatch_arg = arg.substr(arg.find_first_of(' ') + 1);

        SDispatchResult res = HTCompat::invoke_dispatcher(dispatch_str, dispatch_arg);

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
        if (!ht_manager->runtime_enabled())
            return {.success = false, .error = "runtime disabled"};

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
        if (!ht_manager->runtime_enabled())
            return {.success = false, .error = "runtime disabled"};

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
        if (!ht_manager->runtime_enabled())
            return {.success = false, .error = "runtime disabled"};

        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.success = false, .error = "cursor_view is null"};

        cursor_view->move(arg, true);
        return {};
    });
}

SDispatchResult dispatchKillHover([[maybe_unused]] std::string arg) {
    return guardedDispatch("dispatch_kill_hover", [&]() -> SDispatchResult {
        if (ht_manager == nullptr)
            return {.success = false, .error = "ht_manager is null"};
        if (!ht_manager->runtime_enabled())
            return {.success = false, .error = "runtime disabled"};

        const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
        if (cursor_view == nullptr)
            return {.success = false, .error = "cursor_view is null"};

        const PHLWINDOW hovered_window = ht_manager->get_window_from_cursor(!cursor_view->active);
        if (hovered_window == nullptr)
            return {.success = false, .error = "hovered_window is null"};

        HTCompat::close_window(hovered_window);
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
}

}
