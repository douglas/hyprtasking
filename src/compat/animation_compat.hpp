#pragma once

#if __has_include(<hyprland/src/animation/AnimationManager.hpp>)
#include <hyprland/src/animation/AnimationManager.hpp>
#include <hyprland/src/animation/WorkspaceAnimationController.hpp>
namespace HTCompat {
using AnimationManager = Animation::CHyprAnimationManager;

inline AnimationManager* animation_manager() {
    return Animation::mgr().get();
}

inline void start_workspace_visibility_animation(PHLWORKSPACE workspace, bool visible) {
    Animation::Workspace::startAnimation(
        workspace,
        visible ? Animation::Workspace::ANIMATION_TYPE_IN : Animation::Workspace::ANIMATION_TYPE_OUT,
        false,
        true
    );
}
}
#else
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
namespace HTCompat {
using AnimationManager = CHyprAnimationManager;

inline AnimationManager* animation_manager() {
    return g_pAnimationManager.get();
}

inline void start_workspace_visibility_animation(PHLWORKSPACE workspace, bool visible) {
    if (!g_pDesktopAnimationManager)
        return;

    g_pDesktopAnimationManager->startAnimation(
        workspace,
        visible ? CDesktopAnimationManager::ANIMATION_TYPE_IN
                : CDesktopAnimationManager::ANIMATION_TYPE_OUT,
        false,
        true
    );
}
}
#endif
