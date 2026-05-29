#include "CheatGuardService.hpp"

#include "CheatGuardConfig.hpp"
#include "EclipseCheatGuardService.hpp"
#include "OpenHackCheatGuardService.hpp"
#include "PrismCheatGuardService.hpp"
#include "QolModCheatGuardService.hpp"
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

bool CheatGuardService::isGameplayCheated() {
    return CheatGuardService::getGameplayCheatReason().has_value();
}

std::optional<std::string_view> CheatGuardService::getGameplayCheatReason() {
    if (isStartPositionRun()) {
        return "start position";
    }

    if (EclipseCheatGuardService::isCheated()) {
        return "Eclipse cheat state";
    }

    if constexpr (gdvn::cheat_guard::ENABLE_CONFIG_BASED_CHEAT_CHECKS) {
        if (QolModCheatGuardService::isCheated()) {
            return "QOLMod config";
        }

        if (PrismCheatGuardService::isCheated()) {
            return "Prism config";
        }
    }

    if (OpenHackCheatGuardService::isCheated()) {
        return "OpenHack runtime state";
    }

    return std::nullopt;
}
