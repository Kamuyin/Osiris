#pragma once

#include <Features/Misc/ChatTranslate/ChatTranslateConfigVariables.h>
#include <GameClient/Panorama/PanoramaDropDown.h>
#include <GameClient/Panorama/TextEntry.h>
#include <Platform/Macros/FunctionAttributes.h>

#include "OnOffDropdownSelectionChangeHandler.h"
#include <EntryPoints/GuiEntryPoints.h>

template <typename HookContext>
struct ChatTranslateProviderDropdownSelectionChangeHandler {
    explicit ChatTranslateProviderDropdownSelectionChangeHandler(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void onSelectionChanged(int selectedIndex)
    {
        if (selectedIndex == chat_translate_vars::ProviderDeepL || selectedIndex == chat_translate_vars::ProviderMicrosoft)
            SET_CONFIG_VAR(chat_translate_vars::Provider, static_cast<std::uint8_t>(selectedIndex));
    }

private:
    HookContext& hookContext;
};

template <typename HookContext>
struct ChatTranslateTargetLanguageDropdownSelectionChangeHandler {
    explicit ChatTranslateTargetLanguageDropdownSelectionChangeHandler(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void onSelectionChanged(int selectedIndex)
    {
        if (selectedIndex >= chat_translate_vars::TargetEnglish && selectedIndex <= chat_translate_vars::TargetChinese)
            SET_CONFIG_VAR(chat_translate_vars::TargetLanguage, static_cast<std::uint8_t>(selectedIndex));
    }

private:
    HookContext& hookContext;
};

template <typename HookContext>
class MiscTab {
public:
    explicit MiscTab(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void init(auto&& guiPanel) const
    {
        initDropDown<OnOffDropdownSelectionChangeHandler<HookContext, chat_translate_vars::Enabled>>(guiPanel, "chat_translate");
        initDropDown<ChatTranslateProviderDropdownSelectionChangeHandler<HookContext>>(guiPanel, "chat_translate_provider");
        initDropDown<ChatTranslateTargetLanguageDropdownSelectionChangeHandler<HookContext>>(guiPanel, "chat_translate_target_language");
    }

    void updateFromConfig(auto&& mainMenu) const noexcept
    {
        setDropDownSelectedIndex(mainMenu, "chat_translate", !GET_CONFIG_VAR(chat_translate_vars::Enabled));
        setDropDownSelectedIndex(mainMenu, "chat_translate_provider", GET_CONFIG_VAR(chat_translate_vars::Provider));
        setDropDownSelectedIndex(mainMenu, "chat_translate_target_language", GET_CONFIG_VAR(chat_translate_vars::TargetLanguage));
        setTextEntry(mainMenu, "chat_translate_deepl_api_key", GET_CONFIG_VAR(chat_translate_vars::DeepLApiKey).c_str());
        setTextEntry(mainMenu, "chat_translate_microsoft_api_key", GET_CONFIG_VAR(chat_translate_vars::MicrosoftApiKey).c_str());
        setTextEntry(mainMenu, "chat_translate_microsoft_region", GET_CONFIG_VAR(chat_translate_vars::MicrosoftRegion).c_str());
    }

private:
    template <typename Handler>
    void initDropDown(auto&& guiPanel, const char* panelId) const
    {
        auto&& dropDown = guiPanel.findChildInLayoutFile(panelId).clientPanel().template as<PanoramaDropDown>();
        dropDown.registerSelectionChangedHandler(&GuiEntryPoints<HookContext>::template dropDownSelectionChanged<Handler>);
    }

    [[NOINLINE]] void setDropDownSelectedIndex(auto&& mainMenu, const char* dropDownId, int selectedIndex) const noexcept
    {
        mainMenu.findChildInLayoutFile(dropDownId).clientPanel().template as<PanoramaDropDown>().setSelectedIndex(selectedIndex);
    }

    [[NOINLINE]] void setTextEntry(auto&& mainMenu, const char* textEntryId, const char* value) const noexcept
    {
        mainMenu.findChildInLayoutFile(textEntryId).clientPanel().template as<TextEntry>().setText(value);
    }

    HookContext& hookContext;
};
