#include "NoclipAntiCheatService.hpp"

void NoclipAntiCheatService::reset(PlayLayer* playLayer) {
    this->playLayer = playLayer;
    cheated = false;
}

void NoclipAntiCheatService::onUpdate(float) {
}

void NoclipAntiCheatService::onPlayerDestroyed(PlayerObject* player) {
    if (playLayer && player && !player->m_isDead && !playLayer->m_levelEndAnimationStarted) {
        cheated = true;
    }
}

bool NoclipAntiCheatService::isCheated() const {
    return cheated;
}

std::string_view NoclipAntiCheatService::getCheatReason() const {
    return "noclip";
}
