#!/usr/bin/env bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
CONTRACTS_DIR="$SCRIPT_DIR/contracts"
CORE_GENERATED_CONTRACTS_FILE="$CONTRACTS_DIR/core.generated.tsv"
CORE_OVERRIDE_CONTRACTS_FILE="$CONTRACTS_DIR/core.overrides.tsv"
SURFACE_GENERATED_CONTRACTS_FILE="$CONTRACTS_DIR/surface.generated.tsv"
SURFACE_OVERRIDE_CONTRACTS_FILE="$CONTRACTS_DIR/surface.overrides.tsv"

compat_core_builtin_contracts_stream() {
  printf '%s\t%s\t%s\t%s\n' \
    "public version API" \
    "src/plugins/PluginAPI.hpp" \
    "src/compat/profile.cpp" \
    "APICALL SVersionInfo getHyprlandVersion(HANDLE handle);"
  printf '%s\t%s\t%s\t%s\n' \
    "version API implementation" \
    "src/plugins/PluginAPI.cpp" \
    "src/compat/profile.cpp" \
    "APICALL SVersionInfo HyprlandAPI::getHyprlandVersion(HANDLE handle)"
  printf '%s\t%s\t%s\t%s\n' \
    "function lookup API" \
    "src/plugins/PluginAPI.hpp" \
    "src/compat/profile.cpp" \
    "APICALL std::vector<SFunctionMatch> findFunctionsByName(HANDLE handle, const std::string& name);"
  printf '%s\t%s\t%s\t%s\n' \
    "function lookup implementation" \
    "src/plugins/PluginAPI.cpp" \
    "src/compat/profile.cpp" \
    "APICALL std::vector<SFunctionMatch> HyprlandAPI::findFunctionsByName(HANDLE handle, const std::string& name)"
  printf '%s\t%s\t%s\t%s\n' \
    "renderWorkspace symbol" \
    "src/render/Renderer.cpp" \
    "src/compat/profile.cpp, src/compat/renderer_compat.cpp" \
    "renderWorkspace"
  printf '%s\t%s\t%s\t%s\n' \
    "shouldRenderWindow symbol" \
    "src/render/Renderer.cpp" \
    "src/compat/profile.cpp, src/compat/renderer_compat.cpp" \
    "shouldRenderWindow"
  printf '%s\t%s\t%s\t%s\n' \
    "renderWindow symbol" \
    "src/render/Renderer.cpp" \
    "src/compat/profile.cpp, src/compat/renderer_compat.cpp" \
    "renderWindow"
  printf '%s\t%s\t%s\t%s\n' \
    "isSolitaryBlocked symbol" \
    "src/helpers/Monitor.hpp@@@src/output/Monitor.hpp" \
    "src/compat/profile.cpp" \
    "isSolitaryBlocked"
  printf '%s\t%s\t%s\t%s\n' \
    "plugin API version check" \
    "src/plugins/PluginSystem.cpp" \
    "src/compat/profile.cpp" \
    "HYPRLAND_API_VERSION"
}

compat_surface_builtin_contracts_stream() {
  printf '%s\t%s\t%s\t%s\n' \
    "input mouse helpers" \
    "src/managers/input/InputManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "Vector2D           getMouseCoordsInternal();|||void               simulateMouseMovement();|||onMouseButton(IPointer::SButtonEvent"
  printf '%s\t%s\t%s\t%s\n' \
    "mouse bind mode API" \
    "src/managers/KeybindManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "m_dispatchers|||static SDispatchResult                         changeMouseBindMode(const eMouseBindMode mode);"
  printf '%s\t%s\t%s\t%s\n' \
    "cursor override controller" \
    "src/managers/cursor/CursorShapeOverrideController.hpp@@@src/pointer/cursor/CursorShapeOverrideController.hpp" \
    "src/compat/runtime_compat.cpp" \
    "void setOverride(const std::string& name, eCursorShapeOverrideGroup group);|||void unsetOverride(eCursorShapeOverrideGroup group);|||inline UP<CShapeOverrideController> overrideController = makeUnique<CShapeOverrideController>();"
  printf '%s\t%s\t%s\t%s\n' \
    "layout drag controller entrypoint" \
    "src/layout/LayoutManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "const UP<Supplementary::CDragStateController>& dragController();"
  printf '%s\t%s\t%s\t%s\n' \
    "drag controller state accessors" \
    "src/layout/supplementary/DragController.hpp" \
    "src/compat/runtime_compat.cpp" \
    "void           dragEnd();|||eMouseBindMode mode() const;|||bool           draggingTiled() const;|||SP<ITarget> target() const;"
  printf '%s\t%s\t%s\t%s\n' \
    "pointer warp API" \
    "src/managers/PointerManager.hpp@@@src/pointer/PointerManager.hpp" \
    "src/compat/renderer_compat.cpp" \
    "void warpTo(const Vector2D& logical);"
  printf '%s\t%s\t%s\t%s\n' \
    "compositor lookup and workspace APIs" \
    "src/Compositor.hpp@@@src/state/MonitorState.hpp@@@src/state/MonitorQuery.hpp@@@src/state/WorkspaceState.hpp@@@src/state/WorkspaceQuery.hpp@@@src/state/WorkspacePlacementController.hpp@@@src/desktop/state/GlobalWindowController.hpp@@@src/desktop/state/ViewHitTester.hpp@@@src/output/Monitor.hpp" \
    "src/compat/runtime_compat.cpp, src/compat/renderer_compat.cpp" \
    "m_monitors@@@const std::vector<PHLMONITOR>& monitors() const;|||PHLMONITOR             getMonitorFromID(const MONITORID&);@@@CMonitorQuery&& id(MONITORID id) &&;|||PHLMONITOR             getMonitorFromCursor();@@@CMonitorQuery&& vec(Vector2D vec) &&;|||vectorToWindowUnified(const Vector2D&@@@PHLWINDOW              windowAt(const Vector2D& pos, uint16_t properties, PHLWINDOW ignoreWindow = nullptr) const;|||PHLWORKSPACE           getWorkspaceByID(const WORKSPACEID&);@@@CWorkspaceQuery&& id(const WORKSPACEID& id) &&;|||void                   moveWorkspaceToMonitor(PHLWORKSPACE, PHLMONITOR, bool noWarpCursor = false);@@@void moveWorkspaceToMonitor(PHLWORKSPACE, PHLMONITOR, bool noWarpCursor = false) const;|||void                   scheduleFrameForMonitor(PHLMONITOR, Aquamarine::IOutput::scheduleFrameReason reason = Aquamarine::IOutput::AQ_SCHEDULE_CLIENT_UNKNOWN);@@@void         scheduleFrame(Aquamarine::IOutput::scheduleFrameReason reason = Aquamarine::IOutput::AQ_SCHEDULE_CLIENT_UNKNOWN);|||[[nodiscard]] PHLWORKSPACE          createNewWorkspace(const WORKSPACEID&, const MONITORID&,@@@[[nodiscard]] PHLWORKSPACE create(const WORKSPACEID& id, const MONITORID& monid, const std::string& name = \"\", bool isEmpty = true);|||void                                moveWindowToWorkspaceSafe(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace);@@@void moveWindowToWorkspace(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace) const;"
  printf '%s\t%s\t%s\t%s\n' \
    "monitor focus and render fields" \
    "src/helpers/Monitor.hpp@@@src/output/Monitor.hpp" \
    "src/compat/renderer_compat.cpp" \
    "PHLWORKSPACE                m_activeWorkspace        = nullptr;|||std::string                 m_description      = \"\";|||void        changeWorkspace(const PHLWORKSPACE& pWorkspace, bool internal = false, bool noMouseMove = false, bool noFocus = false);|||Vector2D                    m_position         = Vector2D(-1, -1);|||MONITORID                   m_id                     = MONITOR_INVALID;|||float                       m_scale                  = 1;|||Vector2D                    m_transformedSize  = Vector2D(0, 0);|||wl_output_transform         m_transform       = WL_OUTPUT_TRANSFORM_NORMAL;|||uint32_t    isSolitaryBlocked(bool full = false);|||CBox        logicalBox();@@@virtual CBox                        logicalBox() const override;"
  printf '%s\t%s\t%s\t%s\n' \
    "workspace render state fields" \
    "src/desktop/Workspace.hpp" \
    "src/compat/renderer_compat.cpp" \
    "PHLMONITORREF   m_monitor;|||PHLANIMVAR<Vector2D> m_renderOffset;|||bool m_visible = false;|||WORKSPACEID     m_id   = WORKSPACE_INVALID;|||MONITORID   monitorID();"
  printf '%s\t%s\t%s\t%s\n' \
    "window animation and workspace fields" \
    "src/desktop/view/Window.hpp@@@src/desktop/view/types/GeometricMovableAnimated.hpp" \
    "src/compat/renderer_compat.cpp, src/compat/runtime_compat.cpp" \
    "PHLANIMVAR<Vector2D> m_realPosition;@@@PHLANIMVAR<Vector2D>& positionAnimation();|||PHLANIMVAR<Vector2D> m_realSize;@@@PHLANIMVAR<Vector2D>& sizeAnimation();|||PHLWORKSPACE     m_workspace;|||PHLMONITORREF    m_monitor, m_prevMonitor;|||PHLANIMVAR<float> m_movingFromWorkspaceAlpha;@@@PHLANIMVAR<float>&       alpha(eWindowAlpha type);|||PHLANIMVAR<float> m_movingToWorkspaceAlpha;@@@const PHLANIMVAR<float>& alpha(eWindowAlpha type) const;|||WORKSPACEID                workspaceID();|||CBox                       getWindowMainSurfaceBox() const"
  printf '%s\t%s\t%s\t%s\n' \
    "render pass API" \
    "src/render/pass/Pass.hpp" \
    "src/compat/renderer_compat.cpp" \
    "void    add(UP<IPassElement>&& elem);|||void    removeAllOfType(const std::string& type);"
  printf '%s\t%s\t%s\t%s\n' \
    "event bus hooks" \
    "src/event/EventBus.hpp" \
    "src/compat/runtime_compat.cpp" \
    "UP<CEventBus>& bus();|||} m_events;"
  printf '%s\t%s\t%s\t%s\n' \
    "event bus singleton implementation" \
    "src/event/EventBus.cpp" \
    "src/compat/runtime_compat.cpp" \
    "UP<CEventBus>& Event::bus()"
  printf '%s\t%s\t%s\t%s\n' \
    "hook original call-through API" \
    "src/plugins/HookSystem.hpp" \
    "src/compat/runtime_compat.cpp, src/compat/renderer_compat.cpp" \
    "void*          m_original = nullptr;"
  printf '%s\t%s\t%s\t%s\n' \
    "hook removal API" \
    "src/plugins/PluginAPI.hpp" \
    "src/compat/renderer_compat.cpp, src/plugin/runtime.cpp" \
    "APICALL bool removeFunctionHook(HANDLE handle, CFunctionHook* hook);"
  printf '%s\t%s\t%s\t%s\n' \
    "renderer singleton and pass handles" \
    "src/render/Renderer.hpp" \
    "src/compat/runtime_compat.cpp, src/compat/renderer_compat.cpp" \
    "inline UP<CHyprRenderer> g_pHyprRenderer;@@@inline UP<Render::IHyprRenderer> g_pHyprRenderer;|||CRenderPass m_renderPass = {};@@@CRenderPass  m_renderPass;|||SRenderData                  m_renderData;"
  printf '%s\t%s\t%s\t%s\n' \
    "compositor singleton handle" \
    "src/Compositor.hpp" \
    "src/compat/runtime_compat.cpp, src/compat/renderer_compat.cpp" \
    "inline UP<CCompositor> g_pCompositor;"
  printf '%s\t%s\t%s\t%s\n' \
    "input singleton handle" \
    "src/managers/input/InputManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "inline UP<CInputManager> g_pInputManager;"
  printf '%s\t%s\t%s\t%s\n' \
    "keybind singleton handle" \
    "src/managers/KeybindManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "inline UP<CKeybindManager> g_pKeybindManager;"
  printf '%s\t%s\t%s\t%s\n' \
    "pointer singleton handle" \
    "src/managers/PointerManager.hpp@@@src/pointer/PointerManager.hpp" \
    "src/compat/renderer_compat.cpp" \
    "inline UP<CPointerManager> g_pPointerManager;@@@UP<CPointerManager>& mgr();"
  printf '%s\t%s\t%s\t%s\n' \
    "event-loop singleton handle" \
    "src/managers/eventLoop/EventLoopManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "inline UP<CEventLoopManager> g_pEventLoopManager;"
  printf '%s\t%s\t%s\t%s\n' \
    "config and animation config handles" \
    "src/config/ConfigManager.hpp@@@src/config/shared/animation/AnimationTree.hpp" \
    "src/compat/runtime_compat.cpp" \
    "inline UP<CConfigManager> g_pConfigManager;@@@UP<IConfigManager>& mgr();|||getAnimationPropertyConfig(const std::string&);"
  printf '%s\t%s\t%s\t%s\n' \
    "animation singleton handles" \
    "src/managers/animation/AnimationManager.hpp@@@src/animation/AnimationManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "inline UP<CHyprAnimationManager> g_pAnimationManager;@@@UP<CHyprAnimationManager>& mgr();"
  printf '%s\t%s\t%s\t%s\n' \
    "desktop animation singleton handle" \
    "src/managers/animation/DesktopAnimationManager.hpp@@@src/animation/WorkspaceAnimationController.hpp" \
    "src/compat/renderer_compat.cpp" \
    "inline UP<CDesktopAnimationManager> g_pDesktopAnimationManager = makeUnique<CDesktopAnimationManager>();@@@void startAnimation(PHLWORKSPACE ws, eAnimationType type, bool left = true, bool instant = false, std::optional<std::string> style = std::nullopt);"
}

stream_contract_file() {
  local path=$1
  if [[ ! -f "$path" ]]; then
    return
  fi

  rg -v '^[[:space:]]*(#|$)' "$path" || true
}

compat_core_generated_contracts_stream() {
  if [[ -f "$CORE_GENERATED_CONTRACTS_FILE" ]]; then
    stream_contract_file "$CORE_GENERATED_CONTRACTS_FILE"
    return
  fi

  compat_core_builtin_contracts_stream
}

compat_core_overrides_stream() {
  stream_contract_file "$CORE_OVERRIDE_CONTRACTS_FILE"
}

compat_surface_generated_contracts_stream() {
  if [[ -f "$SURFACE_GENERATED_CONTRACTS_FILE" ]]; then
    stream_contract_file "$SURFACE_GENERATED_CONTRACTS_FILE"
    return
  fi

  compat_surface_builtin_contracts_stream
}

compat_surface_overrides_stream() {
  stream_contract_file "$SURFACE_OVERRIDE_CONTRACTS_FILE"
}

compat_core_contracts_stream() {
  compat_core_generated_contracts_stream
  compat_core_overrides_stream
}

compat_surface_contracts_stream() {
  compat_surface_generated_contracts_stream
  compat_surface_overrides_stream
}
