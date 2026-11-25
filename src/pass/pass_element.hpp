#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>

#include "../build_contract.hpp"

#if HT_HYPRLAND_GE_0_57
    #include <hyprland/src/render/Context.hpp>
// 0.57 passes the render context to the pass element hooks.
    #define HT_PASS_CTX Render::CRenderContext&
#else
    #define HT_PASS_CTX
#endif

inline constexpr const char* HT_PASS_ELEMENT_NAME = "HTDisableSimplification";

// Marks an overview frame: forces blur precompute and keeps Hyprland from
// simplifying the pass, without drawing anything itself.
class HTPassElement: public IPassElement {
  public:
    std::vector<UP<IPassElement>> draw(HT_PASS_CTX) override {
        return {};
    }

    bool needsLiveBlur(HT_PASS_CTX) override {
        return false;
    }

    bool needsPrecomputeBlur(HT_PASS_CTX) override {
        return true;
    }

    bool disableSimplification(HT_PASS_CTX) override {
        return true;
    }

    ePassElementType type() override {
        return EK_CUSTOM;
    }
    const char* passName() override {
        return HT_PASS_ELEMENT_NAME;
    }
};

#undef HT_PASS_CTX
