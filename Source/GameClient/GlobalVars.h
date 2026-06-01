#pragma once

#include <CS2/Classes/GlobalVars.h>
#include <MemoryPatterns/PatternTypes/GlobalVarsPatternTypes.h>
#include <Utils/Optional.h>

template <typename HookContext>
struct GlobalVars {
    [[nodiscard]] Optional<float> curtime() const noexcept
    {
        if (globalVars)
            return globalVars->curtime;
        return {};
    }

    [[nodiscard]] Optional<float> frametime() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToFrametime>().of(globalVars).toOptional();
    }

    [[nodiscard]] const char* mapPath() const noexcept
    {
        return hookContext.patternSearchResults().template get<OffsetToMapPath>().of(globalVars).get();
    }

    HookContext& hookContext;
    cs2::GlobalVars* globalVars;
};
