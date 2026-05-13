#include <exception>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/PluginSystem.hpp>

#include "build_contract.hpp"
#include "compat/profile.hpp"
#include "compat/renderer_compat.hpp"
#include "globals.hpp"
#include "manager.hpp"
#include "plugin/dispatchers.hpp"
#include "plugin/runtime.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    if (ht_manager == nullptr)
        ht_manager = std::make_unique<HTManager>();
    else
        ht_manager->reset();

    HTPlugin::initializeConfig();
    HTPlugin::registerDispatchers();

    const auto compatibility = HTCompat::verify_compatibility();
    if (!compatibility.supported) {
        ht_manager->disable_runtime("compatibility_gate", compatibility.error);
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
    } catch (const std::exception& e) {
        HTPlugin::unregisterCallbacks();
        HTCompat::shutdownRendererHooks();
        if (ht_manager != nullptr)
            ht_manager->disable_runtime("plugin_init", e.what());
    } catch (...) {
        HTPlugin::unregisterCallbacks();
        HTCompat::shutdownRendererHooks();
        if (ht_manager != nullptr)
            ht_manager->disable_runtime("plugin_init", "unknown exception");
    }

    Log::logger->log(LOG, "[Hyprtasking] Plugin initialized");

    return {"Hyprtasking", "A workspace management plugin", "douglas", HT_PROJECT_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    Log::logger->log(LOG, "[Hyprtasking] Plugin exiting");

    HTPlugin::unregisterCallbacks();
    HTCompat::shutdownRendererHooks();
    if (ht_manager != nullptr)
        ht_manager->reset();
}
