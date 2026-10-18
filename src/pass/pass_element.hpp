#pragma once

#include "../build_contract.hpp"

#include <hyprland/src/render/pass/PassElement.hpp>

inline constexpr const char* HT_PASS_ELEMENT_NAME = "HTDisableSimplification";

class HTPassElement: public IPassElement {
  public:
    HTPassElement();
    ~HTPassElement() override = default;

    std::vector<UP<IPassElement>> draw() override;
    bool needsLiveBlur() override;
    bool needsPrecomputeBlur() override;
    bool disableSimplification() override;
    ePassElementType type() override;

    const char* passName() override {
        return HT_PASS_ELEMENT_NAME;
    }
};
