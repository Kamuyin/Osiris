#pragma once

#include <MemoryPatterns/PatternTypes/PlayerPawnPatternTypes.h>
#include <MemorySearch/CodePattern.h>

struct PlayerPawnPatterns {
    [[nodiscard]] static consteval auto addClientPatterns(auto clientPatterns) noexcept
    {
        return clientPatterns
            .template addPattern<OffsetToPlayerPawnImmunity, CodePattern{"0F B6 83 ? ? ? ? 84 C0 75 ? ? 80"}.add(3).read()>()
            .template addPattern<OffsetToWeaponServices, CodePattern{"48 8B 88 ? ? ? ? 48 8D 15 ? ? ? ? E8 ? ? ? ? 48"}.add(3).read()>()
            .template addPattern<OffsetToPlayerController, CodePattern{"8B 8B ? ? ? ? 44 88 7C"}.add(2).read()>()
            .template addPattern<OffsetToIsDefusing, CodePattern{"BF ? ? ? ? 00 75 ? 48 8B CF E8 ? ? ? ? 85"}.add(1).read()>()
            .template addPattern<OffsetToIsPickingUpHostage, CodePattern{"86 ? ? ? ? ? ? ? ? 80 BF ? ? ? ? 01"}.add(11).read()>()
            .template addPattern<OffsetToHostageServices, CodePattern{"0F ? ? ? ? ? 48 8B 87 ? ? ? ? 48 8B 0D"}.add(9).read()>()
            .template addPattern<OffsetToFlashBangEndTime, CodePattern{"10 87 ? ? ? ? 0F 2F ? ? 0F 86"}.add(2).read()>()
            .template addPattern<OffsetToPlayerPawnSceneObjectUpdaterHandle, CodePattern{"E8 ? ? ? ? 48 8B 8B ? ? ? ? 33 FF 48 85 C9 74 18 48 8B 93 ? ? ? ?"}.add(22).read()>()
            .template addPattern<OffsetToIsScoped, CodePattern{"88 B0 ? ? ? ? 0F 57 DB"}.add(2).read()>()
            .template addPattern<OffsetToAimPunchServices, CodePattern{"48 C7 44 24 30 ? ? ? ? 4C 8D 0D ? ? ? ? C7 44 24 28 ? ? ? ? 4C 8D 45 54 48 8D 15"}.add(5).read()>()
            .template addPattern<OffsetToShotsFired, CodePattern{"41 8B 86 ? ? ? ? 8D 48 01 3B C1 74 ? 41 89 8E"}.add(3).read()>()
            .template addPattern<OffsetToAimPunchAngle, CodePattern{"48 C7 44 24 30 ? ? ? ? 69 C8 95 E9 D1 5B 4C 8D 3D ? ? ? ? C7 44 24 28 ? ? ? ? 4C 8D 45 6F 4C 8B CE C7 44 24 20 0C 00 00 00"}.add(5).read()>();
    }
};
