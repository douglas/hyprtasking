#!/usr/bin/env bash

compat_core_contracts_stream() {
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
    "signature fallback API" \
    "src/plugins/PluginAPI.hpp" \
    "src/compat/profile.cpp" \
    "APICALL [[deprecated]] void* getFunctionAddressFromSignature(HANDLE handle, const std::string& sig);"
  printf '%s\t%s\t%s\t%s\n' \
    "signature fallback implementation" \
    "src/plugins/PluginAPI.cpp" \
    "src/compat/profile.cpp" \
    "APICALL void* HyprlandAPI::getFunctionAddressFromSignature(HANDLE handle, const std::string& sig)"
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
    "src/helpers/Monitor.hpp" \
    "src/compat/profile.cpp" \
    "isSolitaryBlocked"
  printf '%s\t%s\t%s\t%s\n' \
    "plugin API version check" \
    "src/plugins/PluginSystem.cpp" \
    "src/compat/profile.cpp" \
    "HYPRLAND_API_VERSION"
}

compat_surface_contracts_stream() {
  printf '%s\t%s\t%s\t%s\n' \
    "focus-state APIs" \
    "src/desktop/state/FocusState.hpp" \
    "src/compat/runtime_compat.cpp" \
    "void                   fullWindowFocus(PHLWINDOW w, eFocusReason reason|||void                   rawMonitorFocus(PHLMONITOR m);|||PHLMONITOR             monitor();"
  printf '%s\t%s\t%s\t%s\n' \
    "seat pointer focus state" \
    "src/managers/SeatManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "WP<CWLSurfaceResource> pointerFocus;|||WP<CWLSeatResource>    pointerFocusResource;|||} m_state;"
  printf '%s\t%s\t%s\t%s\n' \
    "input mouse helpers" \
    "src/managers/input/InputManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "Vector2D           getMouseCoordsInternal();|||void               simulateMouseMovement();|||void               onMouseButton(IPointer::SButtonEvent);"
  printf '%s\t%s\t%s\t%s\n' \
    "mouse bind mode API" \
    "src/managers/KeybindManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "m_dispatchers|||static SDispatchResult                         changeMouseBindMode(const eMouseBindMode mode);"
  printf '%s\t%s\t%s\t%s\n' \
    "cursor override controller" \
    "src/managers/cursor/CursorShapeOverrideController.hpp" \
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
    "src/managers/PointerManager.hpp" \
    "src/compat/renderer_compat.cpp" \
    "void warpTo(const Vector2D& logical);"
  printf '%s\t%s\t%s\t%s\n' \
    "compositor lookup and workspace APIs" \
    "src/Compositor.hpp" \
    "src/compat/runtime_compat.cpp, src/compat/renderer_compat.cpp" \
    "m_monitors|||PHLMONITOR             getMonitorFromID(const MONITORID&);|||PHLMONITOR             getMonitorFromCursor();|||PHLWINDOW              vectorToWindowUnified(const Vector2D&, uint8_t properties, PHLWINDOW pIgnoreWindow = nullptr);|||PHLWORKSPACE           getWorkspaceByID(const WORKSPACEID&);|||std::vector<PHLWORKSPACE> getWorkspacesCopy();|||void                   moveWorkspaceToMonitor(PHLWORKSPACE, PHLMONITOR, bool noWarpCursor = false);|||void                   scheduleFrameForMonitor(PHLMONITOR, Aquamarine::IOutput::scheduleFrameReason reason = Aquamarine::IOutput::AQ_SCHEDULE_CLIENT_UNKNOWN);|||void                   closeWindow(PHLWINDOW);|||[[nodiscard]] PHLWORKSPACE          createNewWorkspace(const WORKSPACEID&, const MONITORID&,|||void                                moveWindowToWorkspaceSafe(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace);"
  printf '%s\t%s\t%s\t%s\n' \
    "monitor focus and render fields" \
    "src/helpers/Monitor.hpp" \
    "src/compat/renderer_compat.cpp" \
    "PHLWORKSPACE                m_activeWorkspace        = nullptr;|||std::string                 m_description      = \"\";|||void        changeWorkspace(const PHLWORKSPACE& pWorkspace, bool internal = false, bool noMouseMove = false, bool noFocus = false);|||Vector2D                    m_position         = Vector2D(-1, -1);|||MONITORID                   m_id                     = MONITOR_INVALID;|||float                       m_scale                  = 1;|||Vector2D                    m_transformedSize  = Vector2D(0, 0);|||Vector2D                    m_pixelSize        = Vector2D(0, 0);|||wl_output_transform         m_transform       = WL_OUTPUT_TRANSFORM_NORMAL;|||uint32_t    isSolitaryBlocked(bool full = false);"
  printf '%s\t%s\t%s\t%s\n' \
    "workspace render state fields" \
    "src/desktop/Workspace.hpp" \
    "src/compat/renderer_compat.cpp" \
    "PHLMONITORREF   m_monitor;|||PHLANIMVAR<Vector2D> m_renderOffset;|||bool m_visible = false;|||bool m_isSpecialWorkspace = false;|||WORKSPACEID     m_id   = WORKSPACE_INVALID;"
  printf '%s\t%s\t%s\t%s\n' \
    "window animation and workspace fields" \
    "src/desktop/view/Window.hpp" \
    "src/compat/renderer_compat.cpp, src/compat/runtime_compat.cpp" \
    "PHLANIMVAR<Vector2D> m_realPosition;|||PHLANIMVAR<Vector2D> m_realSize;|||PHLWORKSPACE     m_workspace;|||PHLMONITORREF    m_monitor, m_prevMonitor;|||PHLANIMVAR<float> m_movingFromWorkspaceAlpha;|||PHLANIMVAR<float> m_movingToWorkspaceAlpha;|||void                       warpCursor(bool force = false);"
  printf '%s\t%s\t%s\t%s\n' \
    "render pass API" \
    "src/render/pass/Pass.hpp" \
    "src/compat/renderer_compat.cpp" \
    "void    add(UP<IPassElement>&& elem);|||void    removeAllOfType(const std::string& type);"
  printf '%s\t%s\t%s\t%s\n' \
    "OpenGL monitor blur render flag" \
    "src/render/OpenGL.hpp" \
    "src/compat/renderer_compat.cpp" \
    "bool         blurFBShouldRender = false;|||SCurrentRenderData                                m_renderData;|||SMonitorRenderData*    pCurrentMonData = nullptr;|||inline UP<CHyprOpenGLImpl> g_pHyprOpenGL;"
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
    "renderer singleton and pass handles" \
    "src/render/Renderer.hpp" \
    "src/compat/runtime_compat.cpp, src/compat/renderer_compat.cpp" \
    "inline UP<CHyprRenderer> g_pHyprRenderer;|||CRenderPass m_renderPass = {};"
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
    "src/managers/PointerManager.hpp" \
    "src/compat/renderer_compat.cpp" \
    "inline UP<CPointerManager> g_pPointerManager;"
  printf '%s\t%s\t%s\t%s\n' \
    "seat singleton handle" \
    "src/managers/SeatManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "inline UP<CSeatManager> g_pSeatManager;"
  printf '%s\t%s\t%s\t%s\n' \
    "event-loop singleton handle" \
    "src/managers/eventLoop/EventLoopManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "inline UP<CEventLoopManager> g_pEventLoopManager;"
  printf '%s\t%s\t%s\t%s\n' \
    "config singleton handle" \
    "src/config/ConfigManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "inline UP<CConfigManager> g_pConfigManager;"
  printf '%s\t%s\t%s\t%s\n' \
    "animation singleton handles" \
    "src/managers/animation/AnimationManager.hpp" \
    "src/compat/runtime_compat.cpp" \
    "inline UP<CHyprAnimationManager> g_pAnimationManager;"
  printf '%s\t%s\t%s\t%s\n' \
    "desktop animation singleton handle" \
    "src/managers/animation/DesktopAnimationManager.hpp" \
    "src/compat/renderer_compat.cpp" \
    "inline UP<CDesktopAnimationManager> g_pDesktopAnimationManager = makeUnique<CDesktopAnimationManager>();"
}
