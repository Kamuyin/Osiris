#pragma once

#include <cstdint>

#include <Config/ConfigString.h>
#include <Config/ConfigVariable.h>

namespace chat_translate_vars
{

enum ProviderId : std::uint8_t {
    ProviderDeepL,
    ProviderMicrosoft
};

enum TargetLanguageId : std::uint8_t {
    TargetEnglish,
    TargetGerman,
    TargetFrench,
    TargetSpanish,
    TargetPortuguese,
    TargetRussian,
    TargetTurkish,
    TargetPolish,
    TargetJapanese,
    TargetKorean,
    TargetChinese
};

CONFIG_VARIABLE(Enabled, bool, false);
CONFIG_VARIABLE(Provider, std::uint8_t, ProviderDeepL);
CONFIG_VARIABLE(TargetLanguage, std::uint8_t, TargetEnglish);
CONFIG_VARIABLE(DeepLApiKey, ConfigString<256>, "");
CONFIG_VARIABLE(MicrosoftApiKey, ConfigString<256>, "");
CONFIG_VARIABLE(MicrosoftRegion, ConfigString<64>, "");

}
