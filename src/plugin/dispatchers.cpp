#include "dispatchers.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <string>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "../globals.hpp"
#include "../overview.hpp"
#include "../runtime_validation.hpp"
#include "guards.hpp"

namespace {

using HTPlugin::guardedDispatch;

bool isViewInteractivelyActive(const PHTVIEW& view) {
    return view != nullptr && view->active && !view->closing;
}

SDispatchResult requireRuntimeReady() {
    if (ht_manager == nullptr)
        return {.success = false, .error = "ht_manager is null"};
    if (!HTRuntimeValidation::ensure_grid_gesture_or_disable("runtime_config_validation"))
        return {.success = false, .error = "runtime disabled"};
    if (!ht_manager->runtime_enabled())
        return {.success = false, .error = "runtime disabled"};
    return {};
}

SDispatchResult requireCursorView(PHTVIEW& cursor_view, bool require_interactive = false) {
    const auto runtime_guard = requireRuntimeReady();
    if (!runtime_guard.success)
        return runtime_guard;

    cursor_view = ht_manager->get_view_from_cursor();
    if (require_interactive && !isViewInteractivelyActive(cursor_view)) {
        for (const PHTVIEW& view : ht_manager->views) {
            if (!isViewInteractivelyActive(view))
                continue;

            cursor_view = view;
            break;
        }
    }
    if (cursor_view == nullptr)
        return {.success = false, .error = "cursor_view is null"};
    if (require_interactive && !isViewInteractivelyActive(cursor_view))
        return {.success = false, .error = "selection unavailable"};
    return {};
}

SDispatchResult dispatchToggleView(std::string arg) {
    return guardedDispatch("dispatch_toggle_view", [&]() -> SDispatchResult {
        const auto runtime_guard = requireRuntimeReady();
        if (!runtime_guard.success)
            return runtime_guard;

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
        PHTVIEW cursor_view = nullptr;
        const auto view_guard = requireCursorView(cursor_view);
        if (!view_guard.success)
            return view_guard;

        cursor_view->move(arg);
        return {};
    });
}

SDispatchResult dispatchSelect(std::string arg) {
    return guardedDispatch("dispatch_select", [&]() -> SDispatchResult {
        PHTVIEW cursor_view = nullptr;
        const auto view_guard = requireCursorView(cursor_view, true);
        if (!view_guard.success)
            return view_guard;

        if (!cursor_view->navigate_selection(arg))
            return {.success = false, .error = "invalid arg"};
        return {};
    });
}

SDispatchResult dispatchCommit([[maybe_unused]] std::string arg) {
    return guardedDispatch("dispatch_commit", [&]() -> SDispatchResult {
        PHTVIEW cursor_view = nullptr;
        const auto view_guard = requireCursorView(cursor_view, true);
        if (!view_guard.success)
            return view_guard;

        if (!cursor_view->commit_selection())
            return {.success = false, .error = "selection unavailable"};
        return {};
    });
}

SDispatchResult dispatchHealth(std::string arg) {
    return guardedDispatch("dispatch_health", [&]() -> SDispatchResult {
        const bool json = arg == "json";
        std::string health;
        if (ht_manager == nullptr) {
            health = json ? "{\"runtime_enabled\":false,\"error\":\"ht_manager is null\"}"
                          : "runtime_enabled=false disable_reason=\"ht_manager is null\"";
        } else {
            health = ht_manager->runtime_health_summary(json);
        }

        Log::logger->log(LOG, "[Hyprtasking] health {}", health);
        HyprlandAPI::addNotification(
            PHANDLE,
            "[Hyprtasking] " + health,
            CHyprColor {0.2, 0.8, 0.2, 1.0},
            5000
        );
        return {};
    });
}

int luaResult(lua_State* state, const SDispatchResult& result) {
    if (result.success)
        return 0;

    lua_pushstring(state, result.error.empty() ? "hyprtasking action failed" : result.error.c_str());
    return lua_error(state);
}

int luaToggle(lua_State* state) {
    return luaResult(state, dispatchToggleView(luaL_checkstring(state, 1)));
}

int luaMove(lua_State* state) {
    return luaResult(state, dispatchMove(luaL_checkstring(state, 1)));
}

int luaSelect(lua_State* state) {
    return luaResult(state, dispatchSelect(luaL_checkstring(state, 1)));
}

int luaCommit(lua_State* state) {
    return luaResult(state, dispatchCommit({}));
}

int luaHealth(lua_State* state) {
    const auto arg = luaL_optstring(state, 1, "");
    const auto result = dispatchHealth(arg);
    if (!result.success)
        return luaResult(state, result);

    const auto health = ht_manager == nullptr
        ? std::string {"{\"runtime_enabled\":false,\"error\":\"ht_manager is null\"}"}
        : ht_manager->runtime_health_summary(std::string {arg} == "json");
    lua_pushstring(state, health.c_str());
    return 1;
}

} // namespace

namespace HTPlugin {

void registerDispatchers() {
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:toggle", dispatchToggleView);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:move", dispatchMove);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:select", dispatchSelect);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:commit", dispatchCommit);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:health", dispatchHealth);

    const auto addLuaFunction = [](const std::string& name, PLUGIN_LUA_FN function) {
        if (!HyprlandAPI::addLuaFunction(PHANDLE, "hyprtasking", name, function))
            Log::logger->log(Log::ERR, "[Hyprtasking] failed to register hl.plugin.hyprtasking.{}", name);
    };

    addLuaFunction("toggle", luaToggle);
    addLuaFunction("move", luaMove);
    addLuaFunction("select", luaSelect);
    addLuaFunction("commit", luaCommit);
    addLuaFunction("health", luaHealth);
}

} // namespace HTPlugin
