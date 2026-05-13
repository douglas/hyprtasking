#include "pass_element.hpp"

#include "../build_contract.hpp"

HTPassElement::HTPassElement() {
    ;
}

#if HT_HYPRLAND_GE_0_55
std::vector<UP<IPassElement>> HTPassElement::draw() {
    return {};
}
#else
void HTPassElement::draw(const CRegion& damage) {
    ;
}
#endif

bool HTPassElement::needsLiveBlur() {
    return false;
}

bool HTPassElement::needsPrecomputeBlur() {
    return true;
}

bool HTPassElement::disableSimplification() {
    return true;
}

#if HT_HYPRLAND_GE_0_55
ePassElementType HTPassElement::type() {
    return EK_CUSTOM;
}
#endif
