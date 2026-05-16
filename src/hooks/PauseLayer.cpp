#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp> // DO NOT REMOVE

#include "../services/PvpOverlay.hpp"

using namespace geode::prelude;

class $modify(GDVNPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto overlay = PvpOverlay::getActive();
		if (!overlay || !overlay->hasPvpMatch()) {
			return;
		}

		auto size = CCDirector::sharedDirector()->getWinSize();
		auto menu = CCMenu::create();
		menu->setID("gdvn-pvp-chat-pause-menu"_spr);
		menu->setPosition({ 0.0f, 0.0f });
		this->addChild(menu, 100);

		this->createToggleButton(
			"Mute Chat",
			menu_selector(GDVNPauseLayer::onGDVNMuteChat),
			overlay->isChatMuted(),
			menu,
			{ 40.0f, size.height / 2.0f }
		);
	}

	void onGDVNMuteChat(CCObject*) {
		auto overlay = PvpOverlay::getActive();
		if (!overlay) {
			auto muted = !Mod::get()->getSavedValue<bool>("pvp-chat-muted", false);
			Mod::get()->setSavedValue<bool>("pvp-chat-muted", muted);
			return;
		}

		overlay->setChatMuted(!overlay->isChatMuted());
	}
};
