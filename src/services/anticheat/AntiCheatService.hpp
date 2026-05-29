#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include "DamageBypassAntiCheatService.hpp"
#include "EclipseAntiCheatService.hpp"
#include "NoclipAntiCheatService.hpp"
#include "OpenHackAntiCheatService.hpp"
#include "PrismAntiCheatService.hpp"
#include "QolModAntiCheatService.hpp"
#include "StartPositionAntiCheatService.hpp"

#include <optional>
#include <string>
#include <string_view>

class AntiCheatService {
  private:
    PlayLayer* playLayer = nullptr;
    bool reportedCheat = false;
    std::string reportedCheatReason;
    StartPositionAntiCheatService startPositionAntiCheat;
    EclipseAntiCheatService eclipseAntiCheat;
    QolModAntiCheatService qolModAntiCheat;
    PrismAntiCheatService prismAntiCheat;
    OpenHackAntiCheatService openHackAntiCheat;
    DamageBypassAntiCheatService damageBypassAntiCheat;
    NoclipAntiCheatService noclipAntiCheat;

    void markRunCheated(std::string const& reason);
    void markRunCheatedIfNeeded();

  public:
    AntiCheatService();
    explicit AntiCheatService(PlayLayer* playLayer);

    void reset(PlayLayer* playLayer);
    void onUpdate(float dt);
    void onPlayerDestroyed(PlayerObject* player);
    bool isCheated();
    std::optional<std::string_view> getCheatedReason() const;
    std::string const& getCheatReason() const;
};
