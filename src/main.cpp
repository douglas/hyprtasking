#include <hyprland/src/plugins/PluginAPI.hpp>
#include <string_view>

#include "build_contract.hpp"
#include "compat/profile.hpp"
#include "compat/renderer_compat.hpp"
#include "compat/runtime_compat.hpp"
#include "globals.hpp"
#include "manager.hpp"
#include "plugin/dispatchers.hpp"
#include "plugin/guards.hpp"
#include "plugin/runtime.hpp"

namespace {

template<typename Fn>
void cleanup_step(std::string_view name, Fn&& fn) noexcept {
    try {
        fn();
    } catch (...) {
        const char* reason = HTPlugin::current_exception_what();
        try {
            Log::logger->log(Log::ERR, "[Hyprtasking] {} cleanup failed: {}", name, reason);
        } catch (...) {
        }
    }
}

} // namespace

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    ht_manager = std::make_unique<HTManager>();

    const auto compatibility = HTCompat::verify_compatibility();
    if (!compatibility.supported) {
        ht_manager->block_runtime("compatibility_gate", compatibility.error);
        // Plugin API registration stays available so health reports the reason
        // and config keys do not error, but no HTCompat state is touched.
        HTPlugin::guardedCallback("plugin_init", [] {
            HTPlugin::initializeConfig();
            HTPlugin::registerDispatchers();
        });
        Log::logger->log(
            Log::ERR,
            "[Hyprtasking] Compatibility gate blocked runtime activation: {}",
            compatibility.error
        );
        HyprlandAPI::addNotification(
            PHANDLE,
            "[Hyprtasking] Disabled: " + compatibility.error,
            CHyprColor {1.0, 0.2, 0.2, 1.0},
            7000
        );
        return {"Hyprtasking", "A workspace management plugin", "douglas", HT_PROJECT_VERSION};
    }

    try {
        HTPlugin::initializeConfig();
        HTPlugin::registerDispatchers();
        const bool callbacks_ready = HTPlugin::registerCallbacks();
        const bool renderer_hooks_ready = HTCompat::initializeRendererHooks();
        if (callbacks_ready && renderer_hooks_ready && ht_manager->runtime_enabled()) {
            HTPlugin::registerMonitors();
        } else {
            HTPlugin::unregisterCallbacks();
            HTCompat::shutdownRendererHooks();
            if (ht_manager != nullptr && ht_manager->runtime_enabled()) {
                ht_manager->disable_runtime(
                    "plugin_init",
                    "runtime startup did not complete"
                );
            }
        }
    } catch (...) {
        HTPlugin::unregisterCallbacks();
        HTCompat::shutdownRendererHooks();
        HTPlugin::disable_runtime_for_current_exception("plugin_init");
    }

    Log::logger->log(LOG, "[Hyprtasking] Plugin initialized");

    return {"Hyprtasking", "A workspace management plugin", "douglas", HT_PROJECT_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    Log::logger->log(LOG, "[Hyprtasking] Plugin exiting");

    cleanup_step("manager_unload", [] {
        if (ht_manager != nullptr)
            ht_manager->prepare_for_unload();
    });
    cleanup_step("callbacks", [] { HTPlugin::unregisterCallbacks(); });
    cleanup_step("deferred_callbacks", [] { HTCompat::cancel_pending_do_later(); });
    cleanup_step("renderer_hooks", [] { HTCompat::shutdownRendererHooks(); });
    cleanup_step("manager_destroy", [] { ht_manager.reset(); });
}
