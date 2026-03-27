#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>

#include "compat/renderer_compat.hpp"

class HTScopedMonitorWorkspace {
  public:
    HTScopedMonitorWorkspace(PHLMONITOR monitor, bool use_change_workspace)
        : monitor(monitor),
          previous_workspace(
              monitor == nullptr ? PHLWORKSPACEREF {}
                                 : PHLWORKSPACEREF {HTCompat::active_monitor_workspace(monitor)}
          ),
          use_change_workspace(use_change_workspace) {}

    ~HTScopedMonitorWorkspace() {
        if (!restore_on_destroy)
            return;
        if (monitor == nullptr || previous_workspace == nullptr)
            return;

        const PHLWORKSPACE workspace = previous_workspace.lock();
        if (workspace == nullptr)
            return;

        HTCompat::restore_monitor_workspace(monitor, workspace, use_change_workspace);
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
          previous_workspace(
              monitor == nullptr ? PHLWORKSPACEREF {}
                                 : PHLWORKSPACEREF {HTCompat::active_monitor_workspace(monitor)}
          ) {
        HTCompat::restore_monitor_workspace(monitor, workspace, false);
    }

    ~HTScopedActiveWorkspace() {
        if (!restore_on_destroy)
            return;
        if (monitor == nullptr)
            return;

        const PHLWORKSPACE workspace = previous_workspace.lock();
        if (workspace == nullptr)
            return;

        HTCompat::restore_monitor_workspace(monitor, workspace, false);
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
          previous_visible(HTCompat::workspace_render_visible(workspace)) {
        HTCompat::set_workspace_render_visibility(workspace, visible);
    }

    ~HTScopedWorkspaceVisibility() {
        if (!restore_on_destroy)
            return;

        HTCompat::set_workspace_render_visibility(workspace, previous_visible);
    }

    void dismiss() {
        restore_on_destroy = false;
    }

  private:
    PHLWORKSPACE workspace = nullptr;
    bool         previous_visible = false;
    bool         restore_on_destroy = true;
};

class HTScopedWorkspaceRender {
  public:
    HTScopedWorkspaceRender(PHLMONITOR monitor, PHLWORKSPACE workspace)
        : active_workspace(monitor, workspace),
          visible_workspace(workspace, true) {}

    void dismiss() {
        active_workspace.dismiss();
        visible_workspace.dismiss();
    }

  private:
    HTScopedActiveWorkspace     active_workspace;
    HTScopedWorkspaceVisibility visible_workspace;
};
