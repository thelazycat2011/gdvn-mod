#include "CheatGuard.hpp"

#include <array>
#include <Geode/Geode.hpp>
#include <legowiifun.cheat_api/include/cheatAPI.hpp>

bool CheatGuard::isGameplayCheated() {
	static constexpr std::array rulesetsToCheck = {
		ROBTOP,
		DEMONLIST,
		GDDL,
		MODMAKEROPINION,
		AREDL,
		PEMONLIST,
	};

	for (auto ruleset : rulesetsToCheck) {
		if (cheatAPI::isCheating(ruleset)) {
			return true;
		}
	}

	return false;
}
