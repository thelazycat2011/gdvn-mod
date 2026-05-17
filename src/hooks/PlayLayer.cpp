#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/modify/PlayLayer.hpp> // DO NOT REMOVE
#include "../services/AttemptCounter.hpp"
#include "../services/DeathCounter.hpp"
#include "../services/EventSubmitter.hpp"
#include "../services/RaidSubmitter.hpp"
#include "../services/PvpSubmitter.hpp"
#include "../services/PvpOverlay.hpp"
#include "../services/CheatGuard.hpp"
#include "../services/AuthService.hpp"

using namespace geode::prelude;

class $modify(DTPlayLayer, PlayLayer) {
	struct Fields {
		bool hasRespawned = false;
		bool isCheatedRun = false;
		bool noclipDetected = false;
		GameObject* disabledCheatObject = nullptr;
		AttemptCounter attemptCounter;
		DeathCounter deathCounter;
		EventSubmitter *eventSubmitter;
		RaidSubmitter *raidSubmitter;
		PvpSubmitter *pvpSubmitter;
		PvpOverlay *pvpOverlay = nullptr;
	};

	static void onModify(auto& self) {
		(void)self.setHookPriorityPre("PlayLayer::destroyPlayer", Priority::First);
	}

	bool isDamageBypassActive() {
		return m_isIgnoreDamageEnabled || m_ignoreDamage;
	}

	bool isRunCheated() {
		if (!m_fields->isCheatedRun && CheatGuard::isGameplayCheated()) {
			m_fields->isCheatedRun = true;
		}

		if (!m_fields->isCheatedRun && isDamageBypassActive()) {
			log::warn("Damage bypass detected on level {}", m_level->m_levelID.value());
			m_fields->isCheatedRun = true;
		}

		return m_fields->isCheatedRun || m_fields->noclipDetected;
	}

	void checkNoclip(PlayerObject* player, GameObject* hitObject) {
		if (!m_fields->disabledCheatObject) {
			m_fields->disabledCheatObject = hitObject;
		}

		if (
			!m_fields->noclipDetected &&
			m_fields->disabledCheatObject != hitObject &&
			!player->m_isDead &&
			!m_levelEndAnimationStarted
		) {
			log::warn("Noclip detected on level {}", m_level->m_levelID.value());
			m_fields->noclipDetected = true;
			m_fields->isCheatedRun = true;
		}
	}

	bool init(GJGameLevel * level, bool p1, bool p2) {
		if (!PlayLayer::init(level, p1, p2)) {
			return false;
		}

		int id = level->m_levelID.value();
		auto best = level->m_normalPercent.value();

		m_fields->deathCounter = DeathCounter(id, best >= 100);
		m_fields->eventSubmitter = new EventSubmitter(id);
		m_fields->raidSubmitter = new RaidSubmitter(id);
		m_fields->pvpSubmitter = new PvpSubmitter(id);
		m_fields->isCheatedRun = CheatGuard::isGameplayCheated();

		if (AuthService::isLoggedIn() && !m_level->isPlatformer() && !m_isPracticeMode) {
			m_fields->pvpOverlay = new PvpOverlay(this, id);
		}

		return true;
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);

		if (!m_fields->isCheatedRun && !m_level->isPlatformer() && !m_isPracticeMode && isRunCheated()) {
			log::info("Run marked as cheated on level {}", m_level->m_levelID.value());
		}

		if (m_fields->pvpOverlay) {
			m_fields->pvpOverlay->update(dt);
		}
	}

	void destroyPlayer(PlayerObject * player, GameObject * p1) {
		PlayLayer::destroyPlayer(player, p1);

		checkNoclip(player, p1);

		if (!player->m_isDead) {
			return;
		}

		if (!m_level->isPlatformer() && !m_isPracticeMode) {
			bool isCheated = isRunCheated();
			log::info("Run ended on level {} at {}%: {}", m_level->m_levelID.value(), this->getCurrentPercentInt(), isCheated ? "cheated" : "not cheated");

			if (isCheated) {
				return;
			}

		    const float progress = std::min(this->getCurrentPercent(), 99.99f);

			m_fields->attemptCounter.add();
			m_fields->deathCounter.add(progress);
			m_fields->eventSubmitter->record(progress);
			m_fields->raidSubmitter->record(progress);
			m_fields->pvpSubmitter->record(progress);
		}
	}

	void levelComplete() {
		PlayLayer::levelComplete();

		if (!m_isPracticeMode) {
			bool isCheated = isRunCheated();
			log::info("Run completed on level {}: {}", m_level->m_levelID.value(), isCheated ? "cheated" : "not cheated");

			if (isCheated) {
				return;
			}

			m_fields->eventSubmitter->record(100);
			m_fields->raidSubmitter->record(100);
			m_fields->pvpSubmitter->record(100);
		    m_fields->deathCounter.setCompleted(true);
		}
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		m_fields->hasRespawned = true;
		m_fields->isCheatedRun = CheatGuard::isGameplayCheated();
		m_fields->noclipDetected = false;
		m_fields->disabledCheatObject = nullptr;
	}

	void onQuit() {
		delete m_fields->pvpOverlay;
		m_fields->pvpOverlay = nullptr;

		bool isCheated = isRunCheated();

		PlayLayer::onQuit();

		if (isCheated) {
			log::info("Skipping gameplay API submissions because the run is cheated");
		} else {
			m_fields->attemptCounter.submit();
			m_fields->deathCounter.submit();
		}

		delete m_fields->eventSubmitter;
		delete m_fields->raidSubmitter;
		delete m_fields->pvpSubmitter;
	}
};
