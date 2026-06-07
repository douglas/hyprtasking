#pragma once

#include <hyprland/src/render/pass/TexPassElement.hpp>
#include <string>
#include <unordered_map>

namespace Render {
class ITexture;
}

namespace HTCompat {

using LabelTexture = SP<Render::ITexture>;

struct LabelStyle {
    int size = 44;
    int color = 0xffffffff;
};

LabelTexture get_or_create_label_texture(int64_t workspace_id, const std::string& text, const LabelStyle& style);
void clear_label_cache();
void render_label_texture(LabelTexture texture, const CBox& tile_box, float monitor_scale, float alpha = 0.95f);

} // namespace HTCompat
