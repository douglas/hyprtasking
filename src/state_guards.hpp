#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>

class HTScopedMonitorFocus {
  public:
    explicit HTScopedMonitorFocus(PHLMONITOR monitor) : previous_monitor(Desktop::focusState()->monitor()) {
        if (monitor != nullptr)
            Desktop::focusState()->rawMonitorFocus(monitor);
    }

    ~HTScopedMonitorFocus() {
        if (previous_monitor != nullptr)
            Desktop::focusState()->rawMonitorFocus(previous_monitor);
    }

  private:
    PHLMONITOR previous_monitor = nullptr;
};

class HTScopedMonitorWorkspace {
  public:
    HTScopedMonitorWorkspace(PHLMONITOR monitor, bool use_change_workspace)
        : monitor(monitor), previous_workspace(monitor == nullptr ? PHLWORKSPACEREF {} : monitor->m_activeWorkspace),
          use_change_workspace(use_change_workspace) {}

    ~HTScopedMonitorWorkspace() {
        if (!restore_on_destroy)
            return;
        if (monitor == nullptr || previous_workspace == nullptr)
            return;

        const PHLWORKSPACE workspace = previous_workspace.lock();
        if (workspace == nullptr)
            return;

        if (use_change_workspace) {
            monitor->changeWorkspace(workspace, true);
        } else {
            monitor->m_activeWorkspace = workspace;
        }
    }

    void dismiss() {
        restore_on_destroy = false;
    }

  private:
    PHLMONITOR      monitor = nullptr;
    PHLWORKSPACEREF previous_workspace;
    bool            use_change_workspace = false;
    bool            restore_on_destroy = true;
};

inline bool ht_activate_workspace_for_render(PHLMONITOR monitor, PHLWORKSPACE workspace) {
    if (monitor == nullptr || workspace == nullptr)
        return false;

    monitor->m_activeWorkspace = workspace;
    g_pDesktopAnimationManager->startAnimation(
        workspace,
        CDesktopAnimationManager::ANIMATION_TYPE_IN,
        false,
        true
    );
    workspace->m_visible = true;
    return true;
}

inline bool ht_deactivate_workspace_for_render(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return false;

    g_pDesktopAnimationManager->startAnimation(
        workspace,
        CDesktopAnimationManager::ANIMATION_TYPE_OUT,
        false,
        true
    );
    workspace->m_visible = false;
    return true;
}

class HTScopedWorkspaceRenderVisibility {
  public:
    HTScopedWorkspaceRenderVisibility(PHLMONITOR monitor, PHLWORKSPACE workspace) {
        if (ht_activate_workspace_for_render(monitor, workspace))
            this->workspace = workspace;
    }

    ~HTScopedWorkspaceRenderVisibility() {
        ht_deactivate_workspace_for_render(workspace);
    }

    bool active() const {
        return workspace != nullptr;
    }

  private:
    PHLWORKSPACE workspace = nullptr;
};
