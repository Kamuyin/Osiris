#pragma once

#include <Features/Misc/ChatTranslate/ChatTranslateConfigVariables.h>
#include <Features/Combat/SniperRifles/NoScopeInaccuracyVis/NoScopeInaccuracyVisConfigVariables.h>
#include <Features/Visuals/PlayerInfoInWorld/PlayerInfoInWorld.h>
#include <GameClient/Panorama/Slider.h>
#include <GameClient/Panorama/TextEntry.h>
#include <Platform/Macros/FunctionAttributes.h>
#include <string_view>
#include <Utils/StringParser.h>
#include "Tabs/VisualsTab/HueSlider.h"
#include "Tabs/VisualsTab/IntSlider.h"

template <typename HookContext>
struct SetCommandHandler {
    SetCommandHandler(StringParser& parser, HookContext& hookContext) noexcept
        : parser{parser}
        , hookContext{hookContext}
    {
    }

    void operator()() noexcept
    {
        if (const auto section = parser.getLine('/'); section == "combat") {
            handleCombatSection();
        } else if (section == "hud") {
            handleHudSection();
        } else if (section == "misc") {
            handleMiscSection();
        } else if (section == "visuals") {
            handleVisualsSection();
        } else if (section == "sound") {
            handleSoundSection();
        }
    }

private:
    void handleCombatSection() noexcept
    {
    }

    void handleHudSection() const noexcept
    {
    }

    void handleSoundSection() const noexcept
    {
    }

    void handleMiscSection() const noexcept
    {
        if (const auto feature = parser.getLine('/'); feature == "chat_translate_deepl_api_key") {
            handleTextEntry<chat_translate_vars::DeepLApiKey>();
        } else if (feature == "chat_translate_microsoft_api_key") {
            handleTextEntry<chat_translate_vars::MicrosoftApiKey>();
        } else if (feature == "chat_translate_microsoft_region") {
            handleTextEntry<chat_translate_vars::MicrosoftRegion>();
        }
    }

    void handleVisualsSection() const noexcept
    {
        if (const auto feature = parser.getLine('/'); feature == "viewmodel_fov") {
            handleIntSlider<viewmodel_mod_vars::Fov>("viewmodel_fov");
        } else if (feature == "viewmodel_fov_text") {
            handleIntSliderTextEntry<viewmodel_mod_vars::Fov>("viewmodel_fov");
        }
    }

    template <typename ConfigVariable>
    void handleIntSlider(const char* sliderId) const noexcept
    {
        const auto newVariableValue = handleIntSlider(sliderId, ConfigVariable::ValueType::kMin, ConfigVariable::ValueType::kMax, GET_CONFIG_VAR(ConfigVariable));
        hookContext.config().template setVariable<ConfigVariable>(typename ConfigVariable::ValueType{newVariableValue});
    }

    [[nodiscard]] std::uint8_t handleIntSlider(const char* sliderId, std::uint8_t min, std::uint8_t max, std::uint8_t current) const noexcept
    {
        std::uint8_t value{};
        if (!parser.parseInt(value) || value == current || value < min || value > max)
            return current;

        auto&& hueSlider = getIntSlider(sliderId);
        hueSlider.updateTextEntry(value);
        return value;
    }

    template <typename ConfigVariable>
    void handleIntSliderTextEntry(const char* sliderId) const noexcept
    {
        const auto newVariableValue = handleIntSliderTextEntry(sliderId, ConfigVariable::ValueType::kMin, ConfigVariable::ValueType::kMax, GET_CONFIG_VAR(ConfigVariable));
        hookContext.config().template setVariable<ConfigVariable>(typename ConfigVariable::ValueType{newVariableValue});
    }

    [[nodiscard]] std::uint8_t handleIntSliderTextEntry(const char* sliderId, std::uint8_t min, std::uint8_t max, std::uint8_t current) const noexcept
    {
        auto&& slider = getIntSlider(sliderId);
        std::uint8_t value{};
        if (!parser.parseInt(value) || value < min || value > max) {
            slider.updateTextEntry(current);
            return current;
        }

        if (value == current)
            return current;

        slider.updateSlider(value);
        return value;
    }

    [[nodiscard]] decltype(auto) getIntSlider(const char* sliderId) const noexcept
    {
        const auto mainMenuPointer = hookContext.patternSearchResults().template get<MainMenuPanelPointer>();
        auto&& mainMenu = hookContext.template make<ClientPanel>(mainMenuPointer ? *mainMenuPointer : nullptr).uiPanel();
        return hookContext.template make<IntSlider>(mainMenu.findChildInLayoutFile(sliderId));
    }

    template <typename ConfigVariable>
    void handleTextEntry() const noexcept
    {
        typename ConfigVariable::ValueType value;
        percentDecodeInto(value, parser.getLine(' '));
        hookContext.config().template setVariable<ConfigVariable>(value);
    }

    template <typename ConfigStringType>
    static void percentDecodeInto(ConfigStringType& decoded, std::string_view text) noexcept
    {
        char buffer[256]{};
        std::size_t writeIndex = 0;
        for (std::size_t i = 0; i < text.size() && writeIndex + 1 < sizeof(buffer); ++i) {
            if (text[i] == '%' && i + 2 < text.size()) {
                if (const auto high = hexValue(text[i + 1]), low = hexValue(text[i + 2]); high >= 0 && low >= 0) {
                    buffer[writeIndex++] = static_cast<char>((high << 4) | low);
                    i += 2;
                    continue;
                }
            }
            buffer[writeIndex++] = text[i] == '+' ? ' ' : text[i];
        }
        decoded.assign(std::string_view{buffer, writeIndex});
    }

    [[nodiscard]] static int hexValue(char ch) noexcept
    {
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        if (ch >= 'a' && ch <= 'f')
            return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        return -1;
    }

    StringParser& parser;
    HookContext& hookContext;
};
