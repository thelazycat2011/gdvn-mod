#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>

#include <string_view>

class DamageBypassAntiCheatService {
  private:
    PlayLayer* playLayer = nullptr;

  public:
    void reset(PlayLayer* playLayer);
    void onUpdate(float dt);
    bool isCheated() const;
    std::string_view getCheatReason() const;
};
