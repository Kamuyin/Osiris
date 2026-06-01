#pragma once

#include <cstdint>

#include <Config/ConfigVariable.h>

namespace anti_aim_punch_vars
{

CONFIG_VARIABLE(Enabled, bool, false);
CONFIG_VARIABLE(MinBullets, std::uint8_t, 1);

}
