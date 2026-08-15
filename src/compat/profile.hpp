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
    std::string signature;
    std::string method;
    std::string error;
};

struct CompatibilityResult {
    bool supported = false;
    std::string error;
};

std::span<const std::string_view> supported_hyprland_versions();
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
CompatibilityResult verify_compatibility();
} // namespace HTCompat
