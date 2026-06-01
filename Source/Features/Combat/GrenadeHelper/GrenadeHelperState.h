#pragma once

#include <CS2/Panorama/PanelHandle.h>

inline constexpr int kGrenadeHelperMaxSpots = 8;

struct GrenadeHelperState {
    cs2::PanelHandle spotPanels[kGrenadeHelperMaxSpots]{};
    cs2::PanelHandle aimPanels[kGrenadeHelperMaxSpots]{};
};
