#include <Geode/Geode.hpp>
#include <Geode/binding/CreateMenuItem.hpp>
#include <Geode/modify/CreateMenuItem.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

class $modify(GDVNCreatorLayer, CreatorLayer) {
	bool init() {
		if (!CreatorLayer::init()) {
			return false;
		}

		return true;
	}

	void onMultiplayer(CCObject*) {
		web::openLinkInBrowser("https://gdvn.net/versus/play");
	}
};