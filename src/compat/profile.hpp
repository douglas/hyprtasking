#pragma once

#include <hyprland/src/plugins/HookSystem.hpp>
#include <span>
#include <string>
#include <string_view>

namespace HTCompat {

struct HookSpec {
    std::string_view label;
    std::string_view query;
    std::string_view signature;
};

struct HookLookupResult {
    void* address = nullptr;
    std::string signature;
    std::string method;
    std::string error;
};

struct HookInstallResult {
    bool installed = false;
    std::string error;
};

struct CompatibilityResult {
    bool supported = false;
    std::string error;
};

const HookSpec& input_mouse_button_spec();
const HookSpec& render_workspace_spec();
const HookSpec& render_texture_spec();
const HookSpec& render_border_spec();
const HookSpec& render_border_lerp_spec();
const HookSpec& blur_optimizations_spec();
const HookSpec& should_render_window_spec();
const HookSpec& render_window_spec();
const HookSpec& solitary_blocked_spec();
HookLookupResult resolve_hook_address(const HookSpec& hook);
HookInstallResult
install_function_hook(CFunctionHook*& hook, const HookSpec& spec, void* destination);
bool function_hook_original_ready(CFunctionHook* hook);
bool remove_function_hook(CFunctionHook*& hook, std::string_view label);

// Typed call-through for an installed hook; nullptr when it is not ready.
template<typename Fn>
Fn hook_original(CFunctionHook* hook) {
    if (!function_hook_original_ready(hook))
        return nullptr;

    return reinterpret_cast<Fn>(hook->m_original);
}

template<typename Fn>
void* hook_destination(Fn* function) {
    return reinterpret_cast<void*>(function);
}
CompatibilityResult verify_compatibility();
} // namespace HTCompat
