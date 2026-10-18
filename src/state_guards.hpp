#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>

#include "compat/renderer_compat.hpp"
#include "scope_guard.hpp"

class [[nodiscard]] HTScopedMonitorWorkspace {
  public:
    // Copying or moving would restore the saved state twice.
    HTScopedMonitorWorkspace(const HTScopedMonitorWorkspace&) = delete;
    HTScopedMonitorWorkspace& operator=(const HTScopedMonitorWorkspace&) = delete;

    explicit HTScopedMonitorWorkspace(PHLMONITOR monitor) :
        monitor(monitor),
        previous_workspace(HTCompat::active_monitor_workspace(monitor)) {}

    ~HTScopedMonitorWorkspace() {
        if (restore_on_destroy)
            run_cleanup([&] {
                HTCompat::restore_monitor_workspace(monitor, previous_workspace.lock(), true);
            });
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

    HTScopedActiveWorkspace(PHLMONITOR monitor, PHLWORKSPACE workspace) :
        monitor(monitor),
        previous_workspace(HTCompat::active_monitor_workspace(monitor)) {
        HTCompat::restore_monitor_workspace(monitor, workspace, false);
    }

    ~HTScopedActiveWorkspace() {
        run_cleanup([&] {
            HTCompat::restore_monitor_workspace(monitor, previous_workspace.lock(), false);
        });
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

        run_cleanup([&] {
            HTCompat::set_workspace_render_visibility(workspace, previous_visible);
        });
    }

    void dismiss() noexcept {
        restore_on_destroy = false;
    }

  private:
    PHLWORKSPACE workspace = nullptr;
    bool         previous_visible = false;
    bool         restore_on_destroy = true;
};

// Not copyable or movable: its members are not.
class [[nodiscard]] HTScopedWorkspaceRender {
  public:
    HTScopedWorkspaceRender(PHLMONITOR monitor, PHLWORKSPACE workspace)
        : active_workspace(monitor, workspace),
          visible_workspace(workspace, true) {}

  private:
    HTScopedActiveWorkspace     active_workspace;
    HTScopedWorkspaceVisibility visible_workspace;
};
