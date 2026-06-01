#pragma once

#include <MemoryPatterns/PatternTypes/GlobalVarsPatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct GlobalVarsPatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<OffsetToFrametime, CodePattern{"28 CF F3 0F 10 40 ?"}.add(6).read()>()
            .template addPattern<OffsetToMapPath, CodePattern{"48 8D 78 ? 48 89 C5 E8 ? ? ? ? 84 C0"}.add(3).read()>();
    }
};
