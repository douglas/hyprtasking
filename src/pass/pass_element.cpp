#include "pass_element.hpp"

#include "../build_contract.hpp"

HTPassElement::HTPassElement() {
    ;
}

std::vector<UP<IPassElement>> HTPassElement::draw() {
    return {};
}

bool HTPassElement::needsLiveBlur() {
    return false;
}

bool HTPassElement::needsPrecomputeBlur() {
    return true;
}

bool HTPassElement::disableSimplification() {
    return true;
}

ePassElementType HTPassElement::type() {
    return EK_CUSTOM;
}
