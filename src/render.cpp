#include "render.hpp"

#include <cmath>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "compat/renderer_compat.hpp"
#include "globals.hpp"
#include "logic/geometry_model.hpp"
#include "types.hpp"

using Hyprutils::Utils::CScopeGuard;

// Note: box is relative to (0, 0), not monitor
void render_window_at_box(
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    CBox box
) {
    if (!window || !monitor)
        return;

    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);
    box.x -= monitor_pos.x;
    box.y -= monitor_pos.y;

    const Vector2D window_size = HTCompat::window_real_size(window);
    const float monitor_scale = HTCompat::monitor_scale(monitor);
    if (!HTLogic::isPositiveFinite(monitor_scale))
        return;

    const auto scale = HTLogic::windowRenderScale(box.w, box.h, window_size.x, window_size.y);
    if (!scale.has_value())
        return;

    const Vector2D transform =
        (monitor_pos - HTCompat::window_real_position(window) + box.pos() / *scale) * monitor_scale;
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y))
        return;

    HTRenderModifData data {};
    data.modifs.push_back({HTRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, transform});
    data.modifs.push_back({HTRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, *scale});
    HTCompat::add_renderer_hints_pass(data);
    CScopeGuard reset_hints([] { HTCompat::add_renderer_hints_pass(HTRenderModifData {}); });

    HTCompat::damage_window(window);
    HTCompat::render_window_original(window, monitor, time, true, HT_RENDER_PASS_MAIN, false, true);
}
