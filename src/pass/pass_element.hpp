#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>

inline constexpr const char* HT_PASS_ELEMENT_NAME = "HTDisableSimplification";

// Marks an overview frame: forces blur precompute and keeps Hyprland from
// simplifying the pass, without drawing anything itself.
class HTPassElement: public IPassElement {
  public:
    std::vector<UP<IPassElement>> draw() override {
        return {};
    }

    bool needsLiveBlur() override {
        return false;
    }

    bool needsPrecomputeBlur() override {
        return true;
    }

    bool disableSimplification() override {
        return true;
    }

    ePassElementType type() override {
        return EK_CUSTOM;
    }
    const char* passName() override {
        return HT_PASS_ELEMENT_NAME;
    }
};
