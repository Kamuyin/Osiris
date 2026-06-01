#pragma once

#include <HookContext/HookContextMacros.h>
#include <Platform/Macros/IsPlatform.h>
#include "AntiAimPunchConfigVariables.h"
#include "AntiAimPunchState.h"

template <typename HookContext>
class AntiAimPunch {
public:
    AntiAimPunch(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void update() const
    {
#if IS_WIN64()
        if (!GET_CONFIG_VAR(anti_aim_punch_vars::Enabled))
            return;

        auto&& pawn = hookContext.activeLocalPlayerPawn();
        if (!pawn)
            return;

        const auto shots = pawn.shotsFired().valueOr(0);
        if (shots < GET_CONFIG_VAR(anti_aim_punch_vars::MinBullets))
            return;

        if (auto* punchAngle = pawn.aimPunchAnglePtr()) {
            punchAngle->x = 0.0f;
            punchAngle->y = 0.0f;
            punchAngle->z = 0.0f;
        }
#endif
    }

private:
    HookContext& hookContext;
};
