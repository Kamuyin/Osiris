#pragma once

#include <cstdint>

#include <Config/ConfigVariable.h>

namespace grenade_helper_vars
{

CONFIG_VARIABLE(Enabled, bool, false);
CONFIG_VARIABLE(ShowAimIndicator, bool, true);
CONFIG_VARIABLE(CircleDistance, std::uint16_t, 150);

}
