#pragma once

#include <MemoryPatterns/PatternTypes/GlobalVarsPatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct GlobalVarsPatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<OffsetToFrametime, CodePattern{"0F 10 ? ? 39 ? ? ? ? ? 75 ? 48"}.add(3).read()>()
            .template addPattern<OffsetToMapPath, CodePattern{"48 8D 88 ? ? ? ? 48 8B D3 48 8B 01 FF 50 ? 84 C0 75"}.add(3).read()>();
    }
};
