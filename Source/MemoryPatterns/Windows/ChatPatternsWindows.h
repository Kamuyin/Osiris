#pragma once

#include <MemoryPatterns/PatternTypes/ChatPatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct ChatPatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<ChatHudPushNoticeFunctionPointer, CodePattern{"48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 55 41 56 41 57 48 8D AC 24 ? ? ? ? B8 ? ? ? ? E8 ? ? ? ? 48 2B E0 45 33 FF 41 8B D8 48 8B F2"}>()
            .template addPattern<ChatHudSubmitFunctionPointer, CodePattern{"4C 8B DC 55 41 56 48 81 EC 58 01 00 00 8B 81 8C 00 00 00 45 33 F6 48 8B E9 85 C0 0F 8E ? ? ? ? 83 F8 02 0F 87 ? ? ? ? 48 8B 89 A0 00 00 00 48 83 C1 20 49 89 5B E8 49 89 7B D8 48 8B 01 FF 50 38"}>();
    }
};
