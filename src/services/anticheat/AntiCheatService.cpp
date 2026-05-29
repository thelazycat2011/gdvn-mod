#include "AntiCheatService.hpp"

#include "AntiCheatConfig.hpp"
#include "EclipseAntiCheatService.hpp"
#include "OpenHackAntiCheatService.hpp"
#include "PrismAntiCheatService.hpp"
#include "QolModAntiCheatService.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>

#include <optional>
#include <string_view>

namespace {
bool isStartPositionRun() {
    auto playLayer = PlayLayer::get();
    return playLayer && playLayer->m_startPosObject;
}
} // namespace

bool AntiCheatService::isGameplayCheated() {
    return AntiCheatService::getGameplayCheatReason().has_value();
}

std::optional<std::string_view> AntiCheatService::getGameplayCheatReason() {
    if (isStartPositionRun()) {
        return "start position";
    }

    if (EclipseAntiCheatService::isCheated()) {
        return "Eclipse cheat state";
    }

    if constexpr (gdvn::anti_cheat::ENABLE_CONFIG_BASED_CHEAT_CHECKS) {
        if (QolModAntiCheatService::isCheated()) {
            return "QOLMod config";
        }

        if (PrismAntiCheatService::isCheated()) {
            return "Prism config";
        }
    }

    if (OpenHackAntiCheatService::isCheated()) {
        return "OpenHack runtime state";
    }

    return std::nullopt;
}
