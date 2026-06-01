#pragma once

#include <CS2/Classes/Color.h>
#include <CS2/Panorama/CUILength.h>
#include <GameClient/Panorama/PanelAlignmentParams.h>
#include <GameClient/Panorama/PanelHandle.h>
#include <GameClient/Panorama/PanoramaUiEngine.h>
#include <HookContext/HookContextMacros.h>
#include "SniperCrosshairConfigVariables.h"
#include "SniperCrosshairState.h"

namespace sniper_crosshair_params
{
constexpr cs2::Color kFillColor{255, 255, 255, 255};
constexpr cs2::Color kBorderColor{0, 0, 0, 255};
constexpr auto kBorderWidth{cs2::CUILength::pixels(1)};
}

template <typename HookContext>
class SniperCrosshair {
public:
    SniperCrosshair(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void update() const
    {
        if (!enabled())
            return;

        const auto visible = shouldShow();
        auto&& panel = getPanel();
        panel.setVisible(visible);
        if (visible) {
            using namespace sniper_crosshair_params;
            panel.setBorder(kBorderWidth, kBorderColor);
            panel.setBackgroundColor(kFillColor);
        }
    }

    void onDisable() const
    {
        hookContext.template make<PanelHandle>(state().panelHandle).get().hide();
    }

    void onUnload() const
    {
        hookContext.template make<PanoramaUiEngine>().deletePanelByHandle(state().panelHandle);
    }

private:
    [[nodiscard]] bool enabled() const
    {
        return GET_CONFIG_VAR(sniper_crosshair_vars::Enabled);
    }

    [[nodiscard]] bool shouldShow() const
    {
        auto&& localPlayerPawn = hookContext.activeLocalPlayerPawn();
        return localPlayerPawn
            && localPlayerPawn.isScoped().valueOr(false)
            && localPlayerPawn.isUsingSniperRifle();
    }

    [[nodiscard]] decltype(auto) getPanel() const
    {
        return hookContext.template make<PanelHandle>(state().panelHandle).getOrInit(createPanel());
    }

    [[nodiscard]] auto createPanel() const noexcept
    {
        return [this]() -> decltype(auto) {
            auto&& panel = hookContext.panelFactory().createPanel(hookContext.hud().getHudReticle()).uiPanel();
            panel.setWidth(cs2::CUILength::pixels(6));
            panel.setHeight(cs2::CUILength::pixels(6));
            panel.setBorderRadius(cs2::CUILength::percent(50));
            panel.setAlign(PanelAlignmentParams{
                .horizontalAlignment = cs2::k_EHorizontalAlignmentCenter,
                .verticalAlignment = cs2::k_EVerticalAlignmentCenter});
            return utils::lvalue<decltype(panel)>(panel);
        };
    }

    [[nodiscard]] auto& state() const
    {
        return hookContext.featuresStates().sniperCrosshairState;
    }

    HookContext& hookContext;
};
