#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/PluginSystem.hpp>

#include "compat/renderer_hooks.hpp"
#include "globals.hpp"
#include "manager.hpp"
#include "plugin/dispatchers.hpp"
#include "plugin/runtime.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string compositor_hash = __hyprland_api_get_hash();
    const std::string client_hash     = __hyprland_api_get_client_hash();

    if (compositor_hash != client_hash)
        fail_exit("Mismatched headers! Can't proceed.");

    if (ht_manager == nullptr)
        ht_manager = std::make_unique<HTManager>();
    else
        ht_manager->reset();

    HTPlugin::initializeConfig();
    HTPlugin::registerDispatchers();
    HTPlugin::registerCallbacks();
    HTCompat::initializeRendererHooks();
    HTPlugin::registerMonitors();

    Log::logger->log(LOG, "[Hyprtasking] Plugin initialized");

    return {"Hyprtasking", "A workspace management plugin", "raybbian", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    Log::logger->log(LOG, "[Hyprtasking] Plugin exiting");

    HTCompat::shutdownRendererHooks();
    if (ht_manager != nullptr)
        ht_manager->reset();
}
