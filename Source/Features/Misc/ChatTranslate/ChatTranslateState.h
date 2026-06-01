#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <Platform/Macros/IsPlatform.h>

#if IS_WIN64()
#include <Windows.h>
#endif

struct ChatTranslateState {
    using HudChatPushNotice = void(void*, const char*, std::uint32_t, const char*);
    using HudChatSubmit = void(void*);

    enum class JobState : std::uint8_t {
        Empty,
        Pending,
        Completed
    };

    struct Job {
        JobState state{JobState::Empty};
        std::uint8_t provider{};
        std::uint8_t targetLanguage{};
        std::uint32_t context{};
        void* thisptr{};
        char hudPrefix[256]{};
        char chatPrefix[160]{};
        char original[512]{};
        char displayText[1024]{};
    };

    struct CacheEntry {
        bool valid{};
        std::uint8_t provider{};
        std::uint8_t targetLanguage{};
        char original[512]{};
        char translated[512]{};
    };

    struct OutgoingJob {
        JobState state{JobState::Empty};
        std::uint8_t provider{};
        std::uint8_t targetLanguage{};
        std::uint32_t chatMode{};
        void* hudChat{};
        char original[512]{};
        char translated[512]{};
    };

    static constexpr auto kMaxJobs = 8;
    static constexpr auto kMaxOutgoingJobs = 4;
    static constexpr auto kMaxCacheEntries = 16;
    static constexpr auto kPatchSize = 16;
    static constexpr auto kSubmitPatchSize = 19;

    std::array<Job, kMaxJobs> jobs{};
    std::array<OutgoingJob, kMaxOutgoingJobs> outgoingJobs{};
    std::array<CacheEntry, kMaxCacheEntries> cache{};
    std::array<std::array<char, 512>, 4> recentOwnNormalMessages{};
    std::size_t nextCacheEntry{};
    std::size_t nextRecentOwnNormalMessage{};

#if IS_WIN64()
    SRWLOCK jobsLock{SRWLOCK_INIT};
#endif

    HudChatPushNotice* target{};
    HudChatPushNotice* original{};
    std::byte originalBytes[kPatchSize]{};
    static constexpr auto kTrampolineSize = 64;
    std::byte* trampoline{};
    bool hookInstalled{};
    bool reinjecting{};

    HudChatSubmit* submitTarget{};
    HudChatSubmit* submitOriginal{};
    std::byte submitOriginalBytes[kSubmitPatchSize]{};
    std::byte* submitTrampoline{};
    bool submitHookInstalled{};
    bool submittingTranslated{};
};
