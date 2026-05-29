#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <string_view>

class NoclipAntiCheatService {
  private:
    PlayLayer* playLayer = nullptr;
    bool cheated = false;

  public:
    void reset(PlayLayer* playLayer);
    void onUpdate(float dt);
    void onPlayerDestroyed(PlayerObject* player);
    bool isCheated() const;
    std::string_view getCheatReason() const;
};
