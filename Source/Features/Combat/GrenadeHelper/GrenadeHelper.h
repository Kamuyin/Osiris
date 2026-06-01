#pragma once

#include <cmath>
#include <string_view>

#include <CS2/Classes/Color.h>
#include <CS2/Classes/Vector.h>
#include <CS2/Panorama/CUILength.h>
#include <GameClient/Panorama/PanelHandle.h>
#include <GameClient/Panorama/PanoramaLabel.h>
#include <GameClient/Panorama/PanoramaTransformations.h>
#include <GameClient/Panorama/PanoramaUiEngine.h>
#include <GameClient/WorldToScreen/WorldToClipSpaceConverter.h>
#include <HookContext/HookContextMacros.h>
#include <Utils/Lvalue.h>
#include "GrenadeHelperConfigVariables.h"
#include "GrenadeHelperSpots.h"
#include "GrenadeHelperState.h"

template <typename HookContext>
class GrenadeHelper {
public:
    explicit GrenadeHelper(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void update() const
    {
        if (!GET_CONFIG_VAR(grenade_helper_vars::Enabled)) {
            hideAllPanels();
            return;
        }

        auto&& localPawn = hookContext.activeLocalPlayerPawn();
        if (!localPawn) {
            hideAllPanels();
            return;
        }

        const auto origin = localPawn.absOrigin();
        if (!origin.hasValue()) {
            hideAllPanels();
            return;
        }

        const auto eyeAngles = localPawn.eyeAngles();
        const auto mapName = extractMapName(hookContext.globalVars().mapPath());
        const cs2::Vector& playerPos = origin.value();

        const auto circleDistConfig = static_cast<float>(GET_CONFIG_VAR(grenade_helper_vars::CircleDistance));
        const float circleDistSq = circleDistConfig * circleDistConfig;

        int slotIndex = 0;

        for (const auto& spot : kGrenadeSpots) {
            if (slotIndex >= kGrenadeHelperMaxSpots)
                break;
            if (!mapName.empty() && spot.mapName != mapName)
                continue;

            const float dx = playerPos.x - spot.position.x;
            const float dy = playerPos.y - spot.position.y;
            if (dx * dx + dy * dy > circleDistSq)
                continue;

            const auto clipSpace = hookContext.template make<WorldToClipSpaceConverter>().toClipSpace(spot.position);
            if (!clipSpace.onScreen()) {
                hideSpotSlot(slotIndex++);
                continue;
            }

            const auto ndc = clipSpace.toNormalizedDeviceCoordinates();

            auto&& spotPanel = getOrCreateSpotPanel(slotIndex);
            spotPanel.setVisible(true);
            PanoramaTransformations{
                hookContext.panoramaTransformFactory().translate(ndc.getX(), ndc.getY())
            }.applyTo(spotPanel);
            spotPanel.clientPanel().template as<PanoramaLabel>().setText(spot.name.data());

            if (GET_CONFIG_VAR(grenade_helper_vars::ShowAimIndicator)) {
                auto&& aimPanel = getOrCreateAimPanel(slotIndex);
                const bool aligned = eyeAngles.hasValue() && isAimed(eyeAngles.value(), spot.pitch, spot.yaw);
                aimPanel.setVisible(true);
                aimPanel.setBackgroundColor(aligned ? cs2::Color{0, 220, 0, 200} : cs2::Color{220, 80, 0, 180});
                PanoramaTransformations{
                    hookContext.panoramaTransformFactory().translate(ndc.getX(), ndc.getY())
                }.applyTo(aimPanel);
            } else {
                hookContext.template make<PanelHandle>(state().aimPanels[slotIndex]).get().hide();
            }

            ++slotIndex;
        }

        for (int i = slotIndex; i < kGrenadeHelperMaxSpots; ++i)
            hideSpotSlot(i);
    }

    void onDisable() const
    {
        hideAllPanels();
    }

    void onUnload() const
    {
        auto&& engine = hookContext.template make<PanoramaUiEngine>();
        for (int i = 0; i < kGrenadeHelperMaxSpots; ++i) {
            engine.deletePanelByHandle(state().spotPanels[i]);
            engine.deletePanelByHandle(state().aimPanels[i]);
        }
    }

private:
    [[nodiscard]] static std::string_view extractMapName(const char* mapPath) noexcept
    {
        if (!mapPath || mapPath[0] == '\0')
            return {};
        const char* nameStart = mapPath;
        const char* dotPos = nullptr;
        const char* p = mapPath;
        for (; *p != '\0'; ++p) {
            if (*p == '/') {
                nameStart = p + 1;
                dotPos = nullptr;
            } else if (*p == '.' && dotPos == nullptr) {
                dotPos = p;
            }
        }
        const char* nameEnd = dotPos ? dotPos : p;
        return std::string_view{nameStart, static_cast<std::size_t>(nameEnd - nameStart)};
    }

    [[nodiscard]] static bool isAimed(const cs2::Vector& eyeAngles, float targetPitch, float targetYaw) noexcept
    {
        constexpr float kAimTolerance = 5.0f;
        float dYaw = eyeAngles.y - targetYaw;
        while (dYaw > 180.0f) dYaw -= 360.0f;
        while (dYaw < -180.0f) dYaw += 360.0f;
        return std::abs(eyeAngles.x - targetPitch) < kAimTolerance && std::abs(dYaw) < kAimTolerance;
    }

    void hideAllPanels() const
    {
        for (int i = 0; i < kGrenadeHelperMaxSpots; ++i)
            hideSpotSlot(i);
    }

    void hideSpotSlot(int i) const
    {
        hookContext.template make<PanelHandle>(state().spotPanels[i]).get().hide();
        hookContext.template make<PanelHandle>(state().aimPanels[i]).get().hide();
    }

    [[nodiscard]] decltype(auto) getOrCreateSpotPanel(int index) const
    {
        return hookContext.template make<PanelHandle>(state().spotPanels[index]).getOrInit([this]() -> decltype(auto) {
            auto&& panel = hookContext.panelFactory().createLabelPanel(hookContext.hud().getHudReticle()).uiPanel();
            panel.setColor(cs2::Color{255, 255, 255, 230});
            return utils::lvalue<decltype(panel)>(panel);
        });
    }

    [[nodiscard]] decltype(auto) getOrCreateAimPanel(int index) const
    {
        return hookContext.template make<PanelHandle>(state().aimPanels[index]).getOrInit([this]() -> decltype(auto) {
            auto&& panel = hookContext.panelFactory().createPanel(hookContext.hud().getHudReticle()).uiPanel();
            panel.setWidth(cs2::CUILength::pixels(10));
            panel.setHeight(cs2::CUILength::pixels(10));
            panel.setBorderRadius(cs2::CUILength::percent(50));
            return utils::lvalue<decltype(panel)>(panel);
        });
    }

    [[nodiscard]] auto& state() const
    {
        return hookContext.featuresStates().grenadeHelperState;
    }

    HookContext& hookContext;
};
