#pragma once

#include <array>
#include <string_view>

#include <CS2/Classes/Vector.h>

struct GrenadeSpot {
    std::string_view mapName;
    std::string_view name;
    cs2::Vector position;
    float pitch;
    float yaw;
};

inline constexpr GrenadeSpot kGrenadeSpots[] = {
    // de_dust2 — A site smoke from T spawn
    { "de_dust2", "A Site Smoke (T Spawn)", { 56.0f, 2737.0f, 1.0f }, -14.0f, -35.0f },
    // de_dust2 — B site smoke from upper B tunnels
    { "de_dust2", "B Site Smoke (B Tunnels)", { -1500.0f, 2700.0f, 1.0f }, -15.0f, 170.0f },

    // de_mirage — A site jungle smoke
    { "de_mirage", "A Site Jungle Smoke", { -1050.0f, -375.0f, -140.0f }, -30.0f, 90.0f },
    // de_mirage — B apartments smoke
    { "de_mirage", "B Apartments Smoke", { 730.0f, -2350.0f, -200.0f }, -25.0f, -90.0f },

    // de_inferno — B site banana smoke
    { "de_inferno", "B Banana Smoke", { 600.0f, 400.0f, 100.0f }, -20.0f, -45.0f },

    // de_nuke — Outside ramp smoke
    { "de_nuke", "Outside Ramp Smoke", { -650.0f, -1200.0f, -360.0f }, -35.0f, 90.0f },
};
