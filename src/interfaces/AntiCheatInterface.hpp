#pragma once

#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <string_view>

class AntiCheatInterface {
  public:
    virtual ~AntiCheatInterface() = default;

    virtual void reset(PlayLayer* playLayer) = 0;
    virtual void onUpdate(float dt) = 0;
    virtual void onPlayerDestroyed(PlayerObject* player) = 0;
    virtual bool isCheated() const = 0;
    virtual std::string_view getCheatReason() const = 0;
};
