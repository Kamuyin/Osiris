#pragma once

#include <Features/Combat/GrenadeHelper/GrenadeHelperConfigVariables.h>
#include <GameClient/Panorama/PanoramaDropDown.h>
#include <EntryPoints/GuiEntryPoints.h>
#include <Platform/Macros/FunctionAttributes.h>
#include "OnOffDropdownSelectionChangeHandler.h"

template <typename HookContext>
class CombatTab {
public:
    explicit CombatTab(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void init(auto&& guiPanel) const
    {
        initDropDown<OnOffDropdownSelectionChangeHandler<HookContext, no_scope_inaccuracy_vis_vars::Enabled>>(guiPanel, "no_scope_inacc_vis");
        initDropDown<OnOffDropdownSelectionChangeHandler<HookContext, grenade_helper_vars::Enabled>>(guiPanel, "grenade_helper");
        initDropDown<OnOffDropdownSelectionChangeHandler<HookContext, grenade_helper_vars::ShowAimIndicator>>(guiPanel, "grenade_helper_aim_indicator");
    }

    void updateFromConfig(auto&& mainMenu) const noexcept
    {
        setDropDownSelectedIndex(mainMenu, "no_scope_inacc_vis", !GET_CONFIG_VAR(no_scope_inaccuracy_vis_vars::Enabled));
        setDropDownSelectedIndex(mainMenu, "grenade_helper", !GET_CONFIG_VAR(grenade_helper_vars::Enabled));
        setDropDownSelectedIndex(mainMenu, "grenade_helper_aim_indicator", !GET_CONFIG_VAR(grenade_helper_vars::ShowAimIndicator));
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

    HookContext& hookContext;
};
