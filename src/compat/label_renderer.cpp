#include "label_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include <hyprland/src/render/Renderer.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

#include "../globals.hpp"

namespace HTCompat {

namespace {

constexpr double LABEL_PI = 3.14159265358979323846;
std::unordered_map<std::string, LabelTexture> g_label_cache;

struct LabelMetrics {
    int width = 1;
    int height = 1;
};

CHyprColor color_from_argb(int argb) {
    const float a = ((argb >> 24) & 0xFF) / 255.0f;
    const float r = ((argb >> 16) & 0xFF) / 255.0f;
    const float g = ((argb >> 8) & 0xFF) / 255.0f;
    const float b = (argb & 0xFF) / 255.0f;
    return CHyprColor {r, g, b, a};
}

int label_font_pixels(const LabelStyle& style) {
    return std::max(1, static_cast<int>(std::lround(style.size * 0.58)));
}

void configure_layout(PangoLayout* layout, const std::string& text, int font_px) {
    pango_layout_set_text(layout, text.c_str(), -1);

    PangoFontDescription* font_desc = pango_font_description_from_string("sans bold");
    pango_font_description_set_size(font_desc, font_px * PANGO_SCALE);
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);
}

LabelMetrics measure_label(const std::string& text, const LabelStyle& style) {
    const int min_size = std::max(1, style.size);
    const int font_px = label_font_pixels(style);
    const int pad_x = std::max(10, style.size / 3);
    const int pad_y = std::max(8, style.size / 4);

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return LabelMetrics {min_size, min_size};
    }

    cairo_t* cr = cairo_create(surface);
    PangoLayout* layout = pango_cairo_create_layout(cr);
    configure_layout(layout, text, font_px);

    PangoRectangle logical_rect {};
    pango_layout_get_extents(layout, nullptr, &logical_rect);

    const int text_w = std::max(1, static_cast<int>(std::ceil(static_cast<double>(logical_rect.width) / PANGO_SCALE)));
    const int text_h = std::max(1, static_cast<int>(std::ceil(static_cast<double>(logical_rect.height) / PANGO_SCALE)));

    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    const int side = std::max({min_size, text_w + pad_x * 2, text_h + pad_y * 2});
    return LabelMetrics {side, side};
}

void rounded_rect(cairo_t* cr, double x, double y, double w, double h, double radius) {
    const double r = std::max(0.0, std::min(radius, std::min(w, h) / 2.0));
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -LABEL_PI / 2.0, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, LABEL_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, LABEL_PI / 2.0, LABEL_PI);
    cairo_arc(cr, x + r, y + r, r, LABEL_PI, 3.0 * LABEL_PI / 2.0);
    cairo_close_path(cr);
}

} // namespace

LabelTexture get_or_create_label_texture(int64_t workspace_id, const std::string& text, const LabelStyle& style) {
    if (style.size <= 0 || text.empty())
        return nullptr;

    const std::string key = std::to_string(workspace_id) + ":" + text + ":" + std::to_string(style.size) + ":" + std::to_string(style.color);
    auto it = g_label_cache.find(key);
    if (it != g_label_cache.end() && it->second && it->second->m_texID)
        return it->second;

    if (!g_pHyprRenderer)
        return nullptr;

    const LabelMetrics metrics = measure_label(text, style);
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, metrics.width, metrics.height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return nullptr;
    }

    cairo_t* cr = cairo_create(surface);

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

    // High-contrast dark circular badge with a subtle white ring.
    rounded_rect(cr, 2.0, 2.0, metrics.width - 4.0, metrics.height - 4.0, (metrics.width - 4.0) / 2.0);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.78);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, std::max(2.0, metrics.width / 18.0));
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.85);
    cairo_stroke(cr);

    PangoLayout* layout = pango_cairo_create_layout(cr);
    configure_layout(layout, text, label_font_pixels(style));

    PangoRectangle logical_rect {};
    pango_layout_get_extents(layout, nullptr, &logical_rect);

    const double text_w = static_cast<double>(logical_rect.width) / PANGO_SCALE;
    const double text_h = static_cast<double>(logical_rect.height) / PANGO_SCALE;
    const double text_x = static_cast<double>(logical_rect.x) / PANGO_SCALE;
    const double text_y = static_cast<double>(logical_rect.y) / PANGO_SCALE;
    const double x_offset = (metrics.width - text_w) / 2.0 - text_x;
    const double y_offset = (metrics.height - text_h) / 2.0 - text_y;

    const CHyprColor label_color = color_from_argb(style.color);
    cairo_set_source_rgba(cr, label_color.r, label_color.g, label_color.b, label_color.a);
    cairo_move_to(cr, x_offset, y_offset);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
    cairo_surface_flush(surface);

    LabelTexture texture = g_pHyprRenderer->createTexture(surface);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    if (texture && texture->m_texID)
        g_label_cache[key] = texture;
    return texture;
}

void clear_label_cache() {
    g_label_cache.clear();
}

void render_label_texture(LabelTexture texture, const CBox& tile_box, float monitor_scale, float alpha) {
    if (!texture || !texture->m_texID || !g_pHyprRenderer)
        return;

    const float label_w = texture->m_size.x * monitor_scale;
    const float label_h = texture->m_size.y * monitor_scale;
    const float padding = 14 * monitor_scale;

    const float lx = tile_box.x + padding;
    const float ly = tile_box.y + padding;

    CBox label_box {lx, ly, label_w, label_h};
    label_box.round();

    CRegion damage {0, 0, INT16_MAX, INT16_MAX};
    CTexPassElement::SRenderData data {
        .tex = texture,
        .box = label_box,
        .a = alpha,
        .damage = std::move(damage),
        .round = static_cast<int>(std::min(label_w, label_h) / 2.0f),
    };

    g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
}

} // namespace HTCompat
