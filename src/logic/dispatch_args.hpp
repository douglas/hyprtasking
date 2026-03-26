#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace HTLogic {

struct IntParseResult {
    bool        ok = false;
    int         value = 0;
    std::string error;
};

struct LayerOffsetParseResult {
    bool        ok = false;
    int         requested_offset = 0;
    int         max_offset = 0;
    std::string error;
};

IntParseResult parseOffsetArg(std::string_view arg, int original_offset);
LayerOffsetParseResult
parseLayerOffsetArg(std::string_view arg, int original_offset, int rows, int cols, int layers);
long nextLinearDummyWorkspaceID(const std::vector<long>& monitor_workspaces, int first_ws_offset);

} // namespace HTLogic
