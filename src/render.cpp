#include <cmath>

#include "render.hpp"

#include "compat/renderer_compat.hpp"
#include "logic/geometry_model.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include "globals.hpp"
#include "src/helpers/time/Time.hpp"
#include "types.hpp"

// Note: box is relative to (0, 0), not monitor
void render_window_at_box(PHLWINDOW window, PHLMONITOR monitor, const Time::steady_tp& time, CBox box) {
    if (!window || !monitor)
        return;

    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);
    box.x -= monitor_pos.x;
    box.y -= monitor_pos.y;

    const Vector2D window_size = HTCompat::window_real_size(window);
    const float monitor_scale = HTCompat::monitor_scale(monitor);
    if (!HTLogic::isPositiveFinite(monitor_scale))
        return;

    const auto scale =
        HTLogic::windowRenderScale(box.w, box.h, window_size.x, window_size.y);
    if (!scale.has_value())
        return;

    const Vector2D transform =
        (monitor_pos - HTCompat::window_real_position(window) + box.pos() / *scale)
        * monitor_scale;
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y))
        return;

    SRenderModifData data {};
    data.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, transform});
    data.modifs.push_back({SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, *scale});
    HTCompat::add_renderer_hints_pass(data);

    HTCompat::damage_window(window);
    HTCompat::render_window_original(
        window,
        monitor,
        time,
        true,
        RENDER_PASS_MAIN,
        false,
        true
    );

    HTCompat::add_renderer_hints_pass(SRenderModifData {});
}
