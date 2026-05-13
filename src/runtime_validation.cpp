#include "runtime_validation.hpp"

#include "config.hpp"

namespace HTRuntimeValidation {

bool ensure_grid_gesture_or_disable(std::string_view source) {
    return HTConfig::refresh_runtime_config_or_disable(source);
}

} // namespace HTRuntimeValidation
