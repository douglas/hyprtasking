#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>

#include "compat/renderer_compat.hpp"

class [[nodiscard]] HTScopedMonitorWorkspace {
  public:
    // Copying or moving would restore the saved state twice.
    HTScopedMonitorWorkspace(const HTScopedMonitorWorkspace&) = delete;
    HTScopedMonitorWorkspace& operator=(const HTScopedMonitorWorkspace&) = delete;

    explicit HTScopedMonitorWorkspace(PHLMONITOR monitor) :
        monitor(monitor),
        previous_workspace(
            monitor == nullptr ? PHLWORKSPACEREF {}
                               : PHLWORKSPACEREF {HTCompat::active_monitor_workspace(monitor)}
        ) {}

    ~HTScopedMonitorWorkspace() {
        if (!restore_on_destroy)
            return;
        if (monitor == nullptr || previous_workspace == nullptr)
            return;

        const PHLWORKSPACE workspace = previous_workspace.lock();
        if (workspace == nullptr)
            return;

        HTCompat::restore_monitor_workspace(monitor, workspace, true);
    }

    void dismiss() noexcept {
        restore_on_destroy = false;
    }

  private:
    PHLMONITOR      monitor = nullptr;
    PHLWORKSPACEREF previous_workspace;
    bool            restore_on_destroy = true;
};

class [[nodiscard]] HTScopedActiveWorkspace {
  public:
    // Copying or moving would restore the saved state twice.
    HTScopedActiveWorkspace(const HTScopedActiveWorkspace&) = delete;
    HTScopedActiveWorkspace& operator=(const HTScopedActiveWorkspace&) = delete;

    HTScopedActiveWorkspace(PHLMONITOR monitor, PHLWORKSPACE workspace)
        : monitor(monitor),
          previous_workspace(
              monitor == nullptr ? PHLWORKSPACEREF {}
                                 : PHLWORKSPACEREF {HTCompat::active_monitor_workspace(monitor)}
          ) {
        HTCompat::restore_monitor_workspace(monitor, workspace, false);
    }

    ~HTScopedActiveWorkspace() {
        if (monitor == nullptr)
            return;

        const PHLWORKSPACE workspace = previous_workspace.lock();
        if (workspace == nullptr)
            return;

        HTCompat::restore_monitor_workspace(monitor, workspace, false);
    }

  private:
    PHLMONITOR      monitor = nullptr;
    PHLWORKSPACEREF previous_workspace;
};

class [[nodiscard]] HTScopedWorkspaceVisibility {
  public:
    // Copying or moving would restore the saved state twice.
    HTScopedWorkspaceVisibility(const HTScopedWorkspaceVisibility&) = delete;
    HTScopedWorkspaceVisibility& operator=(const HTScopedWorkspaceVisibility&) = delete;

    HTScopedWorkspaceVisibility(PHLWORKSPACE workspace, bool visible)
        : workspace(workspace),
          previous_visible(HTCompat::workspace_render_visible(workspace)) {
        HTCompat::set_workspace_render_visibility(workspace, visible);
    }

    ~HTScopedWorkspaceVisibility() {
        if (!restore_on_destroy)
            return;

        HTCompat::set_workspace_render_visibility(workspace, previous_visible);
    }

    void dismiss() noexcept {
        restore_on_destroy = false;
    }

  private:
    PHLWORKSPACE workspace = nullptr;
    bool         previous_visible = false;
    bool         restore_on_destroy = true;
};

class [[nodiscard]] HTScopedWorkspaceRender {
  public:
    // Copying or moving would restore the saved state twice.
    HTScopedWorkspaceRender(const HTScopedWorkspaceRender&) = delete;
    HTScopedWorkspaceRender& operator=(const HTScopedWorkspaceRender&) = delete;

    HTScopedWorkspaceRender(PHLMONITOR monitor, PHLWORKSPACE workspace)
        : active_workspace(monitor, workspace),
          visible_workspace(workspace, true) {}

  private:
    HTScopedActiveWorkspace     active_workspace;
    HTScopedWorkspaceVisibility visible_workspace;
};
