#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>

class HTScopedMonitorFocus {
  public:
    explicit HTScopedMonitorFocus(PHLMONITOR monitor)
        : previous_monitor(Desktop::focusState()->monitor()) {
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

inline void set_workspace_render_visibility(PHLWORKSPACE workspace, bool visible) {
    if (workspace == nullptr)
        return;

    g_pDesktopAnimationManager->startAnimation(
        workspace,
        visible ? CDesktopAnimationManager::ANIMATION_TYPE_IN
                : CDesktopAnimationManager::ANIMATION_TYPE_OUT,
        false,
        true
    );
    workspace->m_visible = visible;
}

class HTScopedMonitorWorkspace {
  public:
    HTScopedMonitorWorkspace(PHLMONITOR monitor, bool use_change_workspace)
        : monitor(monitor),
          previous_workspace(monitor == nullptr ? PHLWORKSPACEREF {} : monitor->m_activeWorkspace),
          use_change_workspace(use_change_workspace) {}

    ~HTScopedMonitorWorkspace() {
        if (!restore_on_destroy)
            return;
        if (monitor == nullptr || previous_workspace == nullptr)
            return;

        const PHLWORKSPACE workspace = previous_workspace.lock();
        if (workspace == nullptr)
            return;

        if (use_change_workspace)
            monitor->changeWorkspace(workspace, true);
        else
            monitor->m_activeWorkspace = workspace;
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

class HTScopedActiveWorkspace {
  public:
    HTScopedActiveWorkspace(PHLMONITOR monitor, PHLWORKSPACE workspace)
        : monitor(monitor),
          previous_workspace(monitor == nullptr ? PHLWORKSPACEREF {} : monitor->m_activeWorkspace) {
        if (monitor != nullptr && workspace != nullptr)
            monitor->m_activeWorkspace = workspace;
    }

    ~HTScopedActiveWorkspace() {
        if (!restore_on_destroy)
            return;
        if (monitor == nullptr)
            return;

        const PHLWORKSPACE workspace = previous_workspace.lock();
        if (workspace == nullptr)
            return;

        monitor->m_activeWorkspace = workspace;
    }

    void dismiss() {
        restore_on_destroy = false;
    }

  private:
    PHLMONITOR      monitor = nullptr;
    PHLWORKSPACEREF previous_workspace;
    bool            restore_on_destroy = true;
};

class HTScopedWorkspaceVisibility {
  public:
    HTScopedWorkspaceVisibility(PHLWORKSPACE workspace, bool visible)
        : workspace(workspace),
          previous_visible(workspace == nullptr ? false : workspace->m_visible) {
        set_workspace_render_visibility(workspace, visible);
    }

    ~HTScopedWorkspaceVisibility() {
        if (!restore_on_destroy)
            return;

        set_workspace_render_visibility(workspace, previous_visible);
    }

    void dismiss() {
        restore_on_destroy = false;
    }

  private:
    PHLWORKSPACE workspace = nullptr;
    bool         previous_visible = false;
    bool         restore_on_destroy = true;
};
