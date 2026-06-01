#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <CS2/Panorama/CTextEntry.h>
#include <HookContext/HookContextMacros.h>
#include <MemoryPatterns/PatternTypes/ClientPatternTypes.h>
#include <MemoryPatterns/PatternTypes/ChatPatternTypes.h>
#include <MemoryPatterns/PatternTypes/TextEntryPatternTypes.h>
#include <Platform/Macros/IsPlatform.h>

#include "ChatTranslateConfigVariables.h"
#include "ChatTranslateProvider.h"
#include "ChatTranslateState.h"

#if IS_WIN64()
#include <Windows.h>
#endif

void ChatTranslateHook_hudPushNotice(void* thisptr, const char* text, std::uint32_t context, const char* prefix) noexcept;
void ChatTranslateHook_hudSubmit(void* hudChat) noexcept;

template <typename HookContext>
class ChatTranslate {
public:
    explicit ChatTranslate(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void installHook() noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        installPushNoticeHook(state);
        installSubmitHook(state);
#endif
    }

    void uninstallHook() noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        uninstallSubmitHook(state);
        uninstallPushNoticeHook(state);
#endif
    }

    void update() noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        updateHookState();

        for (auto& job : state.jobs) {
            char displayText[sizeof(job.displayText)]{};
            void* thisptr = nullptr;
            char hudPrefix[sizeof(job.hudPrefix)]{};
            std::uint32_t context = 0;

            AcquireSRWLockExclusive(&state.jobsLock);
            if (job.state == ChatTranslateState::JobState::Completed) {
                copyString(displayText, job.displayText);
                thisptr = job.thisptr;
                copyString(hudPrefix, job.hudPrefix);
                context = job.context;
                job.state = ChatTranslateState::JobState::Empty;
            }
            ReleaseSRWLockExclusive(&state.jobsLock);

            if (displayText[0] != '\0' && state.original) {
                state.reinjecting = true;
                state.original(thisptr, displayText, context, hudPrefix);
                state.reinjecting = false;
            }
        }

        for (auto& job : state.outgoingJobs) {
            char translated[sizeof(job.translated)]{};
            void* hudChat = nullptr;
            std::uint32_t chatMode = 0;

            AcquireSRWLockExclusive(&state.jobsLock);
            if (job.state == ChatTranslateState::JobState::Completed) {
                copyString(translated, job.translated);
                hudChat = job.hudChat;
                chatMode = job.chatMode;
                job.state = ChatTranslateState::JobState::Empty;
            }
            ReleaseSRWLockExclusive(&state.jobsLock);

            if (translated[0] != '\0')
                sendChatCommand(chatMode, translated);
        }
#endif
    }

    bool onHudChatMessage(void* thisptr, const char* text, std::uint32_t context, const char* prefix) noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        if (state.reinjecting || !GET_CONFIG_VAR(chat_translate_vars::Enabled) || !isConfigured() || !text || text[0] == '\0')
            return false;

        char cleaned[512]{};
        cleanChatText(text, cleaned);
        if (cleaned[0] == '\0')
            return false;

        const auto split = splitChatMessage(text, cleaned);
        if (split.body.empty())
            return false;

        if (consumeRecentOwnNormalMessage(split.body))
            return false;

        char cached[512]{};
        if (findCached(split.body, cached)) {
            enqueueCompleted(thisptr, context, prefix, split.prefix, cached, split.body);
            return true;
        }

        return enqueueTranslation(thisptr, context, prefix, split.prefix, split.body);
#else
        return false;
#endif
    }

    bool onHudChatSubmit(void* hudChat) noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        if (state.submittingTranslated || !GET_CONFIG_VAR(chat_translate_vars::Enabled) || !isConfigured() || !hudChat)
            return false;

        const auto text = getHudChatText(hudChat);
        if (!text || text[0] == '\0')
            return false;

        OutgoingCommand command;
        const auto chatMode = getHudChatMode(hudChat);
        if (!parseOutgoingCommand(text, command)) {
            char normalMessage[512]{};
            copyCStringBounded(normalMessage, text, sizeof(normalMessage));
            if (normalMessage[0] == '\0')
                return true;

            if (!sendChatCommand(chatMode, normalMessage))
                return false;
            rememberOwnNormalMessage(normalMessage);
            clearHudChatInput(hudChat);
            return true;
        }

        char outgoingMessage[512]{};
        copyBounded(outgoingMessage, command.message, sizeof(outgoingMessage));
        if (!clearHudChatInput(hudChat))
            return true;
        if (outgoingMessage[0] == '\0')
            return true;

        enqueueOutgoingTranslation(hudChat, chatMode, command.targetLanguage, outgoingMessage);
        return true;
#else
        return false;
#endif
    }

private:
    struct MessageParts {
        std::string_view prefix;
        std::string_view body;
    };

    struct OutgoingCommand {
        std::uint8_t targetLanguage{chat_translate_vars::TargetRussian};
        std::string_view message;
    };

    struct WorkerArgs {
        std::size_t jobIndex{};
        std::uint8_t provider{};
        std::uint8_t targetLanguage{};
        ConfigString<256> deepLApiKey{};
        ConfigString<256> microsoftApiKey{};
        ConfigString<64> microsoftRegion{};
        ChatTranslateState* state{};
    };

    struct OutgoingWorkerArgs {
        std::size_t jobIndex{};
        std::uint8_t provider{};
        std::uint8_t targetLanguage{};
        ConfigString<256> deepLApiKey{};
        ConfigString<256> microsoftApiKey{};
        ConfigString<64> microsoftRegion{};
        ChatTranslateState* state{};
    };

    [[nodiscard]] auto& chatTranslateState() const noexcept
    {
        return hookContext.featuresStates().chatTranslateState;
    }

    [[nodiscard]] bool isConfigured() const noexcept
    {
        if (GET_CONFIG_VAR(chat_translate_vars::Provider) == chat_translate_vars::ProviderMicrosoft)
            return !GET_CONFIG_VAR(chat_translate_vars::MicrosoftApiKey).empty() && !GET_CONFIG_VAR(chat_translate_vars::MicrosoftRegion).empty();
        return !GET_CONFIG_VAR(chat_translate_vars::DeepLApiKey).empty();
    }

    void updateHookState() noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        if (GET_CONFIG_VAR(chat_translate_vars::Enabled) && isConfigured()) {
            if (!state.hookInstalled)
                installHook();
        } else if (state.hookInstalled) {
            uninstallHook();
            clearJobs();
        }
#endif
    }

    void clearJobs() noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        AcquireSRWLockExclusive(&state.jobsLock);
        for (auto& job : state.jobs)
            job.state = ChatTranslateState::JobState::Empty;
        for (auto& job : state.outgoingJobs)
            job.state = ChatTranslateState::JobState::Empty;
        for (auto& recentMessage : state.recentOwnNormalMessages)
            recentMessage[0] = '\0';
        ReleaseSRWLockExclusive(&state.jobsLock);
#endif
    }

    void installPushNoticeHook(ChatTranslateState& state) noexcept
    {
#if IS_WIN64()
        if (state.hookInstalled)
            return;

        state.target = hookContext.patternSearchResults().template get<ChatHudPushNoticeFunctionPointer>();
        if (!state.target)
            return;

        std::memcpy(state.originalBytes, reinterpret_cast<const void*>(state.target), ChatTranslateState::kPatchSize);
        if (!buildTrampoline(state)) {
            state.target = nullptr;
            return;
        }

        if (!writeAbsoluteJump(state.target, ChatTranslateState::kPatchSize, reinterpret_cast<std::uintptr_t>(&ChatTranslateHook_hudPushNotice))) {
            releaseTrampoline(state);
            state.target = nullptr;
            return;
        }

        state.original = reinterpret_cast<ChatTranslateState::HudChatPushNotice*>(static_cast<void*>(state.trampoline));
        state.hookInstalled = true;
#endif
    }

    static void uninstallPushNoticeHook(ChatTranslateState& state) noexcept
    {
#if IS_WIN64()
        if (!state.hookInstalled || !state.target)
            return;

        restorePatchedBytes(state.target, state.originalBytes, ChatTranslateState::kPatchSize);
        state.hookInstalled = false;
        state.original = nullptr;
        state.target = nullptr;
        releaseTrampoline(state);
#endif
    }

    void installSubmitHook(ChatTranslateState& state) noexcept
    {
#if IS_WIN64()
        if (state.submitHookInstalled)
            return;

        state.submitTarget = hookContext.patternSearchResults().template get<ChatHudSubmitFunctionPointer>();
        if (!state.submitTarget)
            return;

        std::memcpy(state.submitOriginalBytes, reinterpret_cast<const void*>(state.submitTarget), ChatTranslateState::kSubmitPatchSize);
        if (!buildSubmitTrampoline(state)) {
            state.submitTarget = nullptr;
            return;
        }

        if (!writeAbsoluteJump(state.submitTarget, ChatTranslateState::kSubmitPatchSize, reinterpret_cast<std::uintptr_t>(&ChatTranslateHook_hudSubmit))) {
            releaseSubmitTrampoline(state);
            state.submitTarget = nullptr;
            return;
        }

        state.submitOriginal = reinterpret_cast<ChatTranslateState::HudChatSubmit*>(static_cast<void*>(state.submitTrampoline));
        state.submitHookInstalled = true;
#endif
    }

    static void uninstallSubmitHook(ChatTranslateState& state) noexcept
    {
#if IS_WIN64()
        if (!state.submitHookInstalled || !state.submitTarget)
            return;

        restorePatchedBytes(state.submitTarget, state.submitOriginalBytes, ChatTranslateState::kSubmitPatchSize);
        state.submitHookInstalled = false;
        state.submitOriginal = nullptr;
        state.submitTarget = nullptr;
        releaseSubmitTrampoline(state);
#endif
    }

    static bool writeAbsoluteJump(void* target, std::size_t patchSize, std::uintptr_t destination) noexcept
    {
#if IS_WIN64()
        DWORD oldProtect = 0;
        if (!VirtualProtect(target, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        auto* patch = static_cast<std::byte*>(target);
        patch[0] = std::byte{0x48};
        patch[1] = std::byte{0xB8};
        std::memcpy(patch + 2, &destination, sizeof(destination));
        patch[10] = std::byte{0xFF};
        patch[11] = std::byte{0xE0};
        std::fill_n(patch + 12, patchSize - 12, std::byte{0x90});

        FlushInstructionCache(GetCurrentProcess(), target, patchSize);
        VirtualProtect(target, patchSize, oldProtect, &oldProtect);
        return true;
#else
        return false;
#endif
    }

    static void restorePatchedBytes(void* target, const std::byte* originalBytes, std::size_t patchSize) noexcept
    {
#if IS_WIN64()
        DWORD oldProtect = 0;
        if (VirtualProtect(target, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            std::memcpy(target, originalBytes, patchSize);
            FlushInstructionCache(GetCurrentProcess(), target, patchSize);
            VirtualProtect(target, patchSize, oldProtect, &oldProtect);
        }
#endif
    }

    static bool buildTrampoline(ChatTranslateState& state) noexcept
    {
#if IS_WIN64()
        state.trampoline = static_cast<std::byte*>(VirtualAlloc(nullptr, ChatTranslateState::kTrampolineSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!state.trampoline)
            return false;

        std::memcpy(state.trampoline, state.originalBytes, ChatTranslateState::kPatchSize);
        auto* jumpBack = state.trampoline + ChatTranslateState::kPatchSize;
        jumpBack[0] = std::byte{0x48};
        jumpBack[1] = std::byte{0xB8};
        const auto returnAddress = reinterpret_cast<std::uintptr_t>(state.target) + ChatTranslateState::kPatchSize;
        std::memcpy(jumpBack + 2, &returnAddress, sizeof(returnAddress));
        jumpBack[10] = std::byte{0xFF};
        jumpBack[11] = std::byte{0xE0};

        DWORD oldProtect = 0;
        if (!VirtualProtect(state.trampoline, ChatTranslateState::kTrampolineSize, PAGE_EXECUTE_READ, &oldProtect)) {
            releaseTrampoline(state);
            return false;
        }

        FlushInstructionCache(GetCurrentProcess(), state.trampoline, ChatTranslateState::kTrampolineSize);
        return true;
#else
        return false;
#endif
    }

    static void releaseTrampoline(ChatTranslateState& state) noexcept
    {
#if IS_WIN64()
        if (!state.trampoline)
            return;

        VirtualFree(state.trampoline, 0, MEM_RELEASE);
        state.trampoline = nullptr;
#endif
    }

    static bool buildSubmitTrampoline(ChatTranslateState& state) noexcept
    {
#if IS_WIN64()
        state.submitTrampoline = static_cast<std::byte*>(VirtualAlloc(nullptr, ChatTranslateState::kTrampolineSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!state.submitTrampoline)
            return false;

        std::memcpy(state.submitTrampoline, state.submitOriginalBytes, ChatTranslateState::kSubmitPatchSize);
        auto* jumpBack = state.submitTrampoline + ChatTranslateState::kSubmitPatchSize;
        jumpBack[0] = std::byte{0x48};
        jumpBack[1] = std::byte{0xB8};
        const auto returnAddress = reinterpret_cast<std::uintptr_t>(state.submitTarget) + ChatTranslateState::kSubmitPatchSize;
        std::memcpy(jumpBack + 2, &returnAddress, sizeof(returnAddress));
        jumpBack[10] = std::byte{0xFF};
        jumpBack[11] = std::byte{0xE0};

        DWORD oldProtect = 0;
        if (!VirtualProtect(state.submitTrampoline, ChatTranslateState::kTrampolineSize, PAGE_EXECUTE_READ, &oldProtect)) {
            releaseSubmitTrampoline(state);
            return false;
        }

        FlushInstructionCache(GetCurrentProcess(), state.submitTrampoline, ChatTranslateState::kTrampolineSize);
        return true;
#else
        return false;
#endif
    }

    static void releaseSubmitTrampoline(ChatTranslateState& state) noexcept
    {
#if IS_WIN64()
        if (!state.submitTrampoline)
            return;

        VirtualFree(state.submitTrampoline, 0, MEM_RELEASE);
        state.submitTrampoline = nullptr;
#endif
    }

    static void copyString(char* destination, const char* source) noexcept
    {
        if (!source) {
            destination[0] = '\0';
            return;
        }

        std::size_t i = 0;
        while (source[i] != '\0') {
            destination[i] = source[i];
            ++i;
        }
        destination[i] = '\0';
    }

    static void copyString(char* destination, std::string_view source) noexcept
    {
        std::copy_n(source.data(), source.size(), destination);
        destination[source.size()] = '\0';
    }

    static void cleanChatText(const char* text, char* output) noexcept
    {
        std::size_t writeIndex = 0;
        for (std::size_t i = 0; text[i] != '\0' && writeIndex + 1 < 512; ++i) {
            const auto ch = static_cast<unsigned char>(text[i]);
            if (ch >= 0x20 || ch >= 0x80)
                output[writeIndex++] = text[i];
        }
        output[writeIndex] = '\0';
    }

    static MessageParts splitChatMessage(const char* rawText, std::string_view text) noexcept
    {
        const auto colon = text.find(": ");
        if (colon == std::string_view::npos || colon + 2 >= text.size())
            return {};

        const std::string_view body{text.data() + colon + 2, text.size() - colon - 2};
        if (containsOnlyWhitespace(body))
            return {};

        auto prefix = std::string_view{text.data(), colon + 2};
        if (rawText) {
            const auto rawPayloadStart = findRawChatPayloadStart(rawText);
            if (rawPayloadStart != std::string_view::npos)
                prefix = {rawText, rawPayloadStart};
        }

        return {prefix, body};
    }

    [[nodiscard]] static std::size_t findRawChatPayloadStart(const char* rawText) noexcept
    {
        for (std::size_t i = 0; rawText[i] != '\0'; ++i) {
            if (rawText[i] != ':' || rawText[i + 1] != ' ')
                continue;

            auto payloadStart = i + 2;
            while (rawText[payloadStart] != '\0' && static_cast<unsigned char>(rawText[payloadStart]) < 0x20)
                ++payloadStart;
            return payloadStart;
        }
        return std::string_view::npos;
    }

    [[nodiscard]] static bool containsOnlyWhitespace(std::string_view text) noexcept
    {
        for (const char ch : text) {
            if (ch != ' ' && ch != '\t')
                return false;
        }
        return true;
    }

    [[nodiscard]] static bool parseOutgoingCommand(std::string_view text, OutgoingCommand& command) noexcept
    {
        const auto shortCommand = startsWithCommand(text, "/t");
        const auto longCommand = startsWithCommand(text, "/translate");
        if (!shortCommand && !longCommand)
            return false;

        auto rest = text;
        rest.remove_prefix(shortCommand ? 2 : 10);
        rest = trimLeft(rest);
        command.targetLanguage = chat_translate_vars::TargetRussian;

        const auto firstSpace = findWhitespace(rest);
        const auto firstToken = firstSpace == std::string_view::npos ? rest : std::string_view{rest.data(), firstSpace};
        if (std::uint8_t language{}; languageCodeToTarget(firstToken, language)) {
            command.targetLanguage = language;
            if (firstSpace == std::string_view::npos) {
                rest = {};
            } else {
                rest.remove_prefix(firstSpace);
                rest = trimLeft(rest);
            }
        }

        command.message = trim(rest);
        return true;
    }

    [[nodiscard]] static bool startsWithCommand(std::string_view text, std::string_view command) noexcept
    {
        if (text.size() < command.size())
            return false;
        for (std::size_t i = 0; i < command.size(); ++i) {
            if (text[i] != command[i])
                return false;
        }
        return text.size() == command.size() || isWhitespace(text[command.size()]);
    }

    [[nodiscard]] static std::string_view trim(std::string_view text) noexcept
    {
        text = trimLeft(text);
        while (!text.empty() && isWhitespace(text.back()))
            text.remove_suffix(1);
        return text;
    }

    [[nodiscard]] static std::string_view trimLeft(std::string_view text) noexcept
    {
        while (!text.empty() && isWhitespace(text.front()))
            text.remove_prefix(1);
        return text;
    }

    [[nodiscard]] static std::size_t findWhitespace(std::string_view text) noexcept
    {
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (isWhitespace(text[i]))
                return i;
        }
        return std::string_view::npos;
    }

    [[nodiscard]] static bool isWhitespace(char ch) noexcept
    {
        return ch == ' ' || ch == '\t';
    }

    [[nodiscard]] static bool languageCodeToTarget(std::string_view code, std::uint8_t& targetLanguage) noexcept
    {
        if (code == "en") targetLanguage = chat_translate_vars::TargetEnglish;
        else if (code == "de") targetLanguage = chat_translate_vars::TargetGerman;
        else if (code == "fr") targetLanguage = chat_translate_vars::TargetFrench;
        else if (code == "es") targetLanguage = chat_translate_vars::TargetSpanish;
        else if (code == "pt") targetLanguage = chat_translate_vars::TargetPortuguese;
        else if (code == "ru") targetLanguage = chat_translate_vars::TargetRussian;
        else if (code == "tr") targetLanguage = chat_translate_vars::TargetTurkish;
        else if (code == "pl") targetLanguage = chat_translate_vars::TargetPolish;
        else if (code == "ja" || code == "jp") targetLanguage = chat_translate_vars::TargetJapanese;
        else if (code == "ko" || code == "kr") targetLanguage = chat_translate_vars::TargetKorean;
        else if (code == "zh" || code == "cn") targetLanguage = chat_translate_vars::TargetChinese;
        else return false;
        return true;
    }

    [[nodiscard]] static const char* getHudChatText(void* hudChat) noexcept
    {
        const auto textEntry = getHudChatTextEntry(hudChat);
        if (!textEntry)
            return nullptr;

        auto* textSource = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(textEntry) + 0x20);
        const auto vmt = *reinterpret_cast<std::uintptr_t**>(textSource);
        if (!vmt)
            return nullptr;

        using GetText = const char*(void*);
        const auto getText = reinterpret_cast<GetText*>(vmt[0x38 / sizeof(void*)]);
        return getText ? getText(textSource) : nullptr;
    }

    [[nodiscard]] static cs2::CTextEntry* getHudChatTextEntry(void* hudChat) noexcept
    {
        if (!hudChat)
            return nullptr;
        return reinterpret_cast<cs2::CTextEntry*>(*reinterpret_cast<std::uintptr_t*>(reinterpret_cast<std::uintptr_t>(hudChat) + 0xA0));
    }

    [[nodiscard]] static std::uint32_t getHudChatMode(void* hudChat) noexcept
    {
        if (!hudChat)
            return 1;

        const auto mode = *reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(hudChat) + 0x8C);
        return mode >= 1 && mode <= 2 ? mode : 1;
    }

    static void setHudChatMode(void* hudChat, std::uint32_t mode) noexcept
    {
        if (hudChat)
            *reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(hudChat) + 0x8C) = mode;
    }

    bool clearHudChatInput(void* hudChat) noexcept
    {
        if (!setHudChatText(hudChat, ""))
            return false;
        return true;
    }

    bool setHudChatText(void* hudChat, const char* text) noexcept
    {
        const auto textEntry = getHudChatTextEntry(hudChat);
        const auto setText = hookContext.patternSearchResults().template get<TextEntrySetTextFunction>();
        if (!textEntry || !setText)
            return false;

        setText(textEntry, text);
        return true;
    }

    bool findCached(std::string_view original, char* translated) noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        const auto provider = GET_CONFIG_VAR(chat_translate_vars::Provider);
        const auto targetLanguage = GET_CONFIG_VAR(chat_translate_vars::TargetLanguage);
        bool found = false;

        AcquireSRWLockShared(&state.jobsLock);
        for (const auto& entry : state.cache) {
            if (entry.valid && entry.provider == provider && entry.targetLanguage == targetLanguage && std::string_view{entry.original} == original) {
                copyString(translated, entry.translated);
                found = true;
                break;
            }
        }
        ReleaseSRWLockShared(&state.jobsLock);
        return found;
#else
        return false;
#endif
    }

    void rememberOwnNormalMessage(std::string_view message) noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        AcquireSRWLockExclusive(&state.jobsLock);
        copyBounded(state.recentOwnNormalMessages[state.nextRecentOwnNormalMessage++ % state.recentOwnNormalMessages.size()].data(), message, state.recentOwnNormalMessages.front().size());
        ReleaseSRWLockExclusive(&state.jobsLock);
#else
        static_cast<void>(message);
#endif
    }

    bool consumeRecentOwnNormalMessage(std::string_view message) noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        bool found = false;

        AcquireSRWLockExclusive(&state.jobsLock);
        for (auto& recentMessage : state.recentOwnNormalMessages) {
            if (std::string_view{recentMessage.data()} == message) {
                recentMessage[0] = '\0';
                found = true;
                break;
            }
        }
        ReleaseSRWLockExclusive(&state.jobsLock);
        return found;
#else
        static_cast<void>(message);
        return false;
#endif
    }

    void enqueueCompleted(void* thisptr, std::uint32_t context, const char* prefix, std::string_view chatPrefix, std::string_view translated, std::string_view original) noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        AcquireSRWLockExclusive(&state.jobsLock);
        for (auto& job : state.jobs) {
            if (job.state == ChatTranslateState::JobState::Empty) {
                job.thisptr = thisptr;
                job.context = context;
                copyCStringBounded(job.hudPrefix, prefix, sizeof(job.hudPrefix));
                copyBounded(job.chatPrefix, chatPrefix, sizeof(job.chatPrefix));
                buildDisplayText(job.displayText, chatPrefix, translated, original);
                job.state = ChatTranslateState::JobState::Completed;
                break;
            }
        }
        ReleaseSRWLockExclusive(&state.jobsLock);
#endif
    }

    bool enqueueTranslation(void* thisptr, std::uint32_t context, const char* prefix, std::string_view chatPrefix, std::string_view original) noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        WorkerArgs* workerArgs = nullptr;
        HANDLE thread = nullptr;
        bool queued = false;

        AcquireSRWLockExclusive(&state.jobsLock);
        for (std::size_t i = 0; i < state.jobs.size(); ++i) {
            auto& job = state.jobs[i];
            if (job.state != ChatTranslateState::JobState::Empty)
                continue;

            workerArgs = static_cast<WorkerArgs*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WorkerArgs)));
            if (!workerArgs)
                break;

            workerArgs->jobIndex = i;
            workerArgs->provider = GET_CONFIG_VAR(chat_translate_vars::Provider);
            workerArgs->targetLanguage = GET_CONFIG_VAR(chat_translate_vars::TargetLanguage);
            workerArgs->deepLApiKey = GET_CONFIG_VAR(chat_translate_vars::DeepLApiKey);
            workerArgs->microsoftApiKey = GET_CONFIG_VAR(chat_translate_vars::MicrosoftApiKey);
            workerArgs->microsoftRegion = GET_CONFIG_VAR(chat_translate_vars::MicrosoftRegion);
            workerArgs->state = &state;

            job.provider = workerArgs->provider;
            job.targetLanguage = workerArgs->targetLanguage;
            job.thisptr = thisptr;
            job.context = context;
            copyCStringBounded(job.hudPrefix, prefix, sizeof(job.hudPrefix));
            copyBounded(job.chatPrefix, chatPrefix, sizeof(job.chatPrefix));
            copyBounded(job.original, original, sizeof(job.original));
            buildOriginalText(job.displayText, chatPrefix, original);
            job.state = ChatTranslateState::JobState::Pending;
            queued = true;
            break;
        }
        ReleaseSRWLockExclusive(&state.jobsLock);

        if (!queued)
            return false;

        thread = CreateThread(nullptr, 0, &workerThread, workerArgs, 0, nullptr);
        if (!thread) {
            markJobCompleted(*workerArgs->state, workerArgs->jobIndex);
            HeapFree(GetProcessHeap(), 0, workerArgs);
            return true;
        }
        CloseHandle(thread);
        return true;
#else
        return false;
#endif
    }

    bool enqueueOutgoingTranslation(void* hudChat, std::uint32_t chatMode, std::uint8_t targetLanguage, std::string_view original) noexcept
    {
#if IS_WIN64()
        auto& state = chatTranslateState();
        OutgoingWorkerArgs* workerArgs = nullptr;
        HANDLE thread = nullptr;
        bool queued = false;

        AcquireSRWLockExclusive(&state.jobsLock);
        for (std::size_t i = 0; i < state.outgoingJobs.size(); ++i) {
            auto& job = state.outgoingJobs[i];
            if (job.state != ChatTranslateState::JobState::Empty)
                continue;

            workerArgs = static_cast<OutgoingWorkerArgs*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(OutgoingWorkerArgs)));
            if (!workerArgs)
                break;

            workerArgs->jobIndex = i;
            workerArgs->provider = GET_CONFIG_VAR(chat_translate_vars::Provider);
            workerArgs->targetLanguage = targetLanguage;
            workerArgs->deepLApiKey = GET_CONFIG_VAR(chat_translate_vars::DeepLApiKey);
            workerArgs->microsoftApiKey = GET_CONFIG_VAR(chat_translate_vars::MicrosoftApiKey);
            workerArgs->microsoftRegion = GET_CONFIG_VAR(chat_translate_vars::MicrosoftRegion);
            workerArgs->state = &state;

            job.provider = workerArgs->provider;
            job.targetLanguage = workerArgs->targetLanguage;
            job.hudChat = hudChat;
            job.chatMode = chatMode;
            copyBounded(job.original, original, sizeof(job.original));
            job.translated[0] = '\0';
            job.state = ChatTranslateState::JobState::Pending;
            queued = true;
            break;
        }
        ReleaseSRWLockExclusive(&state.jobsLock);

        if (!queued)
            return false;

        thread = CreateThread(nullptr, 0, &outgoingWorkerThread, workerArgs, 0, nullptr);
        if (!thread) {
            markOutgoingJobCompleted(*workerArgs->state, workerArgs->jobIndex);
            HeapFree(GetProcessHeap(), 0, workerArgs);
            return true;
        }
        CloseHandle(thread);
        return true;
#else
        return false;
#endif
    }

    static void copyBounded(char* destination, std::string_view source, std::size_t destinationSize) noexcept
    {
        const auto capacity = (std::min)(source.size(), destinationSize - 1);
        std::copy_n(source.data(), capacity, destination);
        destination[capacity] = '\0';
    }

    static void copyCStringBounded(char* destination, const char* source, std::size_t destinationSize) noexcept
    {
        if (!source || destinationSize == 0) {
            if (destinationSize != 0)
                destination[0] = '\0';
            return;
        }

        std::size_t i = 0;
        while (source[i] != '\0' && i + 1 < destinationSize) {
            destination[i] = source[i];
            ++i;
        }
        destination[i] = '\0';
    }

    static void append(char* output, std::size_t& outputSize, std::size_t outputCapacity, std::string_view text) noexcept
    {
        const auto toCopy = (std::min)(text.size(), outputCapacity > outputSize ? outputCapacity - outputSize - 1 : 0);
        std::copy_n(text.data(), toCopy, output + outputSize);
        outputSize += toCopy;
        output[outputSize] = '\0';
    }

    static void appendEscapedCommandArgument(char* output, std::size_t& outputSize, std::size_t outputCapacity, std::string_view text) noexcept
    {
        for (const char ch : text) {
            if (ch == '"' || ch == '\\')
                append(output, outputSize, outputCapacity, "\\");
            if (ch == '\n' || ch == '\r')
                append(output, outputSize, outputCapacity, " ");
            else
                append(output, outputSize, outputCapacity, std::string_view{&ch, 1});
        }
    }

    bool sendChatCommand(std::uint32_t chatMode, std::string_view text) noexcept
    {
#if IS_WIN64()
        const auto commandInterfacePointer = hookContext.patternSearchResults().template get<ClientCommandInterfacePointer>();
        if (!commandInterfacePointer || !*commandInterfacePointer)
            return false;

        const auto commandInterface = *commandInterfacePointer;
        const auto vmt = *static_cast<void***>(commandInterface);
        if (!vmt || !vmt[0x190 / sizeof(void*)])
            return false;

        char command[1024]{};
        std::size_t commandSize = 0;
        append(command, commandSize, sizeof(command), chatMode == 2 ? "say_team \"" : "say \"");
        appendEscapedCommandArgument(command, commandSize, sizeof(command), text);
        append(command, commandSize, sizeof(command), "\"");

        using ExecuteClientCommand = void(void*, int, const char*, bool, std::uint64_t, int);
        const auto executeClientCommand = reinterpret_cast<ExecuteClientCommand*>(vmt[0x190 / sizeof(void*)]);
        executeClientCommand(commandInterface, 0, command, true, 0, 0);
        return true;
#else
        static_cast<void>(chatMode);
        static_cast<void>(text);
        return false;
#endif
    }

    static void buildDisplayText(char* output, std::string_view chatPrefix, std::string_view translated, std::string_view original) noexcept
    {
        std::size_t outputSize = 0;
        append(output, outputSize, 1024, chatPrefix);
        append(output, outputSize, 1024, translated);
        append(output, outputSize, 1024, " (");
        append(output, outputSize, 1024, original);
        append(output, outputSize, 1024, ")");
    }

    static void buildOriginalText(char* output, std::string_view chatPrefix, std::string_view original) noexcept
    {
        std::size_t outputSize = 0;
        append(output, outputSize, 1024, chatPrefix);
        append(output, outputSize, 1024, original);
    }

    static void markJobCompleted(ChatTranslateState& state, std::size_t jobIndex) noexcept
    {
#if IS_WIN64()
        AcquireSRWLockExclusive(&state.jobsLock);
        if (jobIndex < state.jobs.size() && state.jobs[jobIndex].state == ChatTranslateState::JobState::Pending)
            state.jobs[jobIndex].state = ChatTranslateState::JobState::Completed;
        ReleaseSRWLockExclusive(&state.jobsLock);
#endif
    }

    static void markOutgoingJobCompleted(ChatTranslateState& state, std::size_t jobIndex) noexcept
    {
#if IS_WIN64()
        AcquireSRWLockExclusive(&state.jobsLock);
        if (jobIndex < state.outgoingJobs.size() && state.outgoingJobs[jobIndex].state == ChatTranslateState::JobState::Pending)
            state.outgoingJobs[jobIndex].state = ChatTranslateState::JobState::Completed;
        ReleaseSRWLockExclusive(&state.jobsLock);
#endif
    }

#if IS_WIN64()
    static DWORD WINAPI workerThread(void* parameter) noexcept
    {
        const auto args = static_cast<WorkerArgs*>(parameter);
        if (!args || !args->state) {
            if (args)
                HeapFree(GetProcessHeap(), 0, args);
            return 0;
        }
        auto& state = *args->state;

        char original[512]{};
        AcquireSRWLockShared(&state.jobsLock);
        if (args->jobIndex < state.jobs.size())
            copyString(original, state.jobs[args->jobIndex].original);
        ReleaseSRWLockShared(&state.jobsLock);

        char translated[512]{};
        bool success = false;
        if (args->provider == chat_translate_vars::ProviderMicrosoft) {
            success = chat_translate::translateWithMicrosoft(args->microsoftApiKey.view(), args->microsoftRegion.view(), args->targetLanguage, original, translated, sizeof(translated));
        } else {
            success = chat_translate::translateWithDeepL(args->deepLApiKey.view(), args->targetLanguage, original, translated, sizeof(translated));
        }

        AcquireSRWLockExclusive(&state.jobsLock);
        if (args->jobIndex < state.jobs.size()) {
            auto& job = state.jobs[args->jobIndex];
            if (job.state == ChatTranslateState::JobState::Pending) {
                if (success) {
                    auto& cache = state.cache[state.nextCacheEntry++ % state.cache.size()];
                    cache.valid = true;
                    cache.provider = args->provider;
                    cache.targetLanguage = args->targetLanguage;
                    copyString(cache.original, original);
                    copyString(cache.translated, translated);
                    buildDisplayText(job.displayText, job.chatPrefix, translated, original);
                }
                job.state = ChatTranslateState::JobState::Completed;
            }
        }
        ReleaseSRWLockExclusive(&state.jobsLock);

        HeapFree(GetProcessHeap(), 0, args);
        return 0;
    }

    static DWORD WINAPI outgoingWorkerThread(void* parameter) noexcept
    {
        const auto args = static_cast<OutgoingWorkerArgs*>(parameter);
        if (!args || !args->state) {
            if (args)
                HeapFree(GetProcessHeap(), 0, args);
            return 0;
        }
        auto& state = *args->state;

        char original[512]{};
        AcquireSRWLockShared(&state.jobsLock);
        if (args->jobIndex < state.outgoingJobs.size())
            copyString(original, state.outgoingJobs[args->jobIndex].original);
        ReleaseSRWLockShared(&state.jobsLock);

        char translated[512]{};
        bool success = false;
        if (args->provider == chat_translate_vars::ProviderMicrosoft) {
            success = chat_translate::translateWithMicrosoft(args->microsoftApiKey.view(), args->microsoftRegion.view(), args->targetLanguage, original, translated, sizeof(translated));
        } else {
            success = chat_translate::translateWithDeepL(args->deepLApiKey.view(), args->targetLanguage, original, translated, sizeof(translated));
        }

        AcquireSRWLockExclusive(&state.jobsLock);
        if (args->jobIndex < state.outgoingJobs.size()) {
            auto& job = state.outgoingJobs[args->jobIndex];
            if (job.state == ChatTranslateState::JobState::Pending) {
                if (success)
                    copyString(job.translated, translated);
                job.state = ChatTranslateState::JobState::Completed;
            }
        }
        ReleaseSRWLockExclusive(&state.jobsLock);

        HeapFree(GetProcessHeap(), 0, args);
        return 0;
    }
#endif

    HookContext& hookContext;
};

inline void ChatTranslateHook_hudPushNotice(void* thisptr, const char* text, std::uint32_t context, const char* prefix) noexcept
{
    HookContext<GlobalContext> hookContext;
    auto&& chatTranslate = hookContext.make<ChatTranslate>();
    if (chatTranslate.onHudChatMessage(thisptr, text, context, prefix))
        return;

    if (auto original = hookContext.featuresStates().chatTranslateState.original)
        original(thisptr, text, context, prefix ? prefix : "");
}

inline void ChatTranslateHook_hudSubmit(void* hudChat) noexcept
{
    HookContext<GlobalContext> hookContext;
    auto&& chatTranslate = hookContext.make<ChatTranslate>();
    if (chatTranslate.onHudChatSubmit(hudChat))
        return;

    if (auto original = hookContext.featuresStates().chatTranslateState.submitOriginal)
        original(hudChat);
}
