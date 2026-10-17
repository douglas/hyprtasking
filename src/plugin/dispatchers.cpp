#include "dispatchers.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <string>

#include "../build_contract.hpp"

// Plugin Lua functions arrived in Hyprland 0.55.
#if HT_HYPRLAND_GE_0_55
extern "C" {
    #include <lauxlib.h>
    #include <lua.h>
}
#endif

#include "../config.hpp"
#include "../globals.hpp"
#include "../logic/navigation_model.hpp"
#include "../overview.hpp"
#include "guards.hpp"

namespace {

using HTPlugin::guardedDispatch;
using HTPlugin::guardedValue;

bool isViewInteractivelyActive(const PHTVIEW& view) {
    return view != nullptr && view->active && !view->closing;
}

SDispatchResult requireRuntimeReady() {
    if (ht_manager == nullptr)
        return {.success = false, .error = "ht_manager is null"};
    if (!ht_manager->runtime_enabled())
        return {.success = false, .error = "runtime disabled"};
    HTConfig::refresh_runtime_config("runtime_config_validation");
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
        if (!HTLogic::isGridDirection(arg))
            return {.success = false, .error = "invalid arg"};

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

#if HT_HYPRLAND_GE_0_55
// lua_error longjmps, which skips C++ destructors. Keep every non-trivial
// object in an inner scope so nothing is alive when the error is raised.
template<typename Fn>
int luaAction(lua_State* state, Fn&& dispatch) {
    bool failed = false;
    {
        const SDispatchResult result = dispatch();
        if (!result.success) {
            lua_pushstring(
                state,
                result.error.empty() ? "hyprtasking action failed" : result.error.c_str()
            );
            failed = true;
        }
    }
    if (failed)
        return lua_error(state);
    return 0;
}

int luaToggle(lua_State* state) {
    const char* arg = luaL_checkstring(state, 1);
    return luaAction(state, [arg] { return dispatchToggleView(arg); });
}

int luaMove(lua_State* state) {
    const char* arg = luaL_checkstring(state, 1);
    return luaAction(state, [arg] { return dispatchMove(arg); });
}

int luaSelect(lua_State* state) {
    const char* arg = luaL_checkstring(state, 1);
    return luaAction(state, [arg] { return dispatchSelect(arg); });
}

int luaCommit(lua_State* state) {
    return luaAction(state, [] { return dispatchCommit({}); });
}

// Returns the health summary without the notification dispatchHealth shows,
// so scripts can poll it.
int luaHealth(lua_State* state) {
    const char* arg = luaL_optstring(state, 1, "");
    {
        const std::string fallback = "{\"runtime_enabled\":false,\"error\":\"health unavailable\"}";
        const auto health = guardedValue("lua_health", fallback, [arg, &fallback] {
            return ht_manager == nullptr
                ? fallback
                : ht_manager->runtime_health_summary(std::string_view {arg} == "json");
        });
        lua_pushstring(state, health.c_str());
    }
    return 1;
}
#endif

} // namespace

namespace HTPlugin {

void registerDispatchers() {
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:toggle", dispatchToggleView);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:move", dispatchMove);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:select", dispatchSelect);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:commit", dispatchCommit);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprtasking:health", dispatchHealth);

#if HT_HYPRLAND_GE_0_55
    const auto addLuaFunction = [](const std::string& name, PLUGIN_LUA_FN function) {
        if (!HyprlandAPI::addLuaFunction(PHANDLE, "hyprtasking", name, function))
            Log::logger->log(Log::ERR, "[Hyprtasking] failed to register hl.plugin.hyprtasking.{}", name);
    };

    addLuaFunction("toggle", luaToggle);
    addLuaFunction("move", luaMove);
    addLuaFunction("select", luaSelect);
    addLuaFunction("commit", luaCommit);
    addLuaFunction("health", luaHealth);
#endif
}

} // namespace HTPlugin
