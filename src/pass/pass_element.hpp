#pragma once

#include "../build_contract.hpp"

#include <hyprland/src/render/pass/PassElement.hpp>

class HTPassElement: public IPassElement {
  public:
    HTPassElement();
    ~HTPassElement() override = default;

#if HT_HYPRLAND_GE_0_55
    std::vector<UP<IPassElement>> draw() override;
#else
    void draw(const CRegion& damage) override;
#endif
    bool needsLiveBlur() override;
    bool needsPrecomputeBlur() override;
    bool disableSimplification() override;
#if HT_HYPRLAND_GE_0_55
    ePassElementType type() override;
#endif

    const char* passName() override {
        return "HTDisableSimplification";
    }
};
