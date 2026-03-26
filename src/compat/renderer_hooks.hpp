#pragma once

#include "../types.hpp"

namespace HTCompat {

void initializeRendererHooks();
void shutdownRendererHooks();

bool callOriginalShouldRenderWindow(PHLWINDOW window, PHLMONITOR monitor);
void renderWorkspaceOriginal(
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
);
void renderWindowOriginal(
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    bool decorate,
    eRenderPassMode mode,
    bool ignorePosition,
    bool standalone
);

bool beginOverviewRender();
void endOverviewRender();

}
