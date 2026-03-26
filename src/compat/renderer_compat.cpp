#include "renderer_compat.hpp"

#define private public
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/ClearPassElement.hpp>
#undef private

#include "../globals.hpp"
#include "../pass/pass_element.hpp"
#include "../types.hpp"

namespace {

const std::string CLEAR_PASS_ELEMENT_NAME = "CClearPassElement";

}

namespace HTCompat {

bool should_render_window_original(void* renderer, PHLWINDOW window, PHLMONITOR monitor) {
    if (renderer == nullptr || should_render_window_hook == nullptr || window == nullptr
        || monitor == nullptr)
        return false;

    return ((should_render_window_t)(should_render_window_hook->m_original))(renderer, window, monitor);
}

void render_workspace_original(
    void* thisptr,
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    if (thisptr == nullptr || render_workspace_hook == nullptr)
        return;

    ((render_workspace_t)(render_workspace_hook->m_original))(thisptr, monitor, workspace, now, geometry);
}

void render_window_original(
    void* thisptr,
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    bool decorate,
    eRenderPassMode mode,
    bool ignore_position,
    bool standalone
) {
    if (thisptr == nullptr || render_window == nullptr || window == nullptr || monitor == nullptr)
        return;

    ((render_window_t)render_window)(
        thisptr,
        window,
        monitor,
        time,
        decorate,
        mode,
        ignore_position,
        standalone
    );
}

void add_clear_pass() {
    if (!g_pHyprRenderer.get())
        return;

    CClearPassElement::SClearData data;
    data.color = CHyprColor {0};
    g_pHyprRenderer->m_renderPass.add(makeUnique<CClearPassElement>(data));
}

void finalize_overview_render_pass() {
    if (!g_pHyprRenderer.get())
        return;

    bool first = true;
    std::erase_if(g_pHyprRenderer->m_renderPass.m_passElements, [&first](const auto& e) {
        const bool remove = e->element->passName() == CLEAR_PASS_ELEMENT_NAME && !first;
        first = false;
        return remove;
    });
    g_pHyprRenderer->m_renderPass.add(makeUnique<HTPassElement>());
}

} // namespace HTCompat
