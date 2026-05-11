#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/modify/LevelInfoLayer.hpp> // DO NOT REMOVE
#include "../common.hpp"
#include "../services/AuthService.hpp"

using namespace geode::prelude;

CCLabelBMFont* createLabel(GJGameLevel* level, std::string str, int order) {
	int offset = (level->m_coins == 0) ? 17 : 4;
	auto size = CCDirector::sharedDirector()->getWinSize();
	int yoffset = 2;

	CCLabelBMFont* label = CCLabelBMFont::create(str.c_str(), "goldFont.fnt");

	label->setPosition({ size.width / 2 - 100, size.height / 2 + offset + yoffset + order * -10 });
	label->setScale(0.3f);

	return label;
}

class ButtonCreator {
public:
	void onButton(CCObject* sender) {
		int id = sender->getTag();
		std::string url = "https://www.gdvn.net/level/" + std::to_string(id);

		web::openLinkInBrowser(url);
	}

	CCMenu* create(std::vector<std::string> labels, GJGameLevel* level, CCLayer* layer) {
		int offset = (level->m_coins == 0) ? 17 : 4;
		auto size = CCDirector::sharedDirector()->getWinSize();

		std::string text;

		for (std::string& s : labels) {
			text += s;
			text += '\n';
		}

		text.pop_back();

		CCLabelBMFont* label = CCLabelBMFont::create(text.c_str(), "goldFont.fnt");

		label->setScale(0.3f);
		label->setAlignment(kCCTextAlignmentCenter);

		auto btn = CCMenuItemSpriteExtra::create(
			label, layer, menu_selector(ButtonCreator::onButton)
		);

		btn->setTag(level->m_levelID.value());

		auto menu = CCMenu::create();

		menu->addChild(btn);
		menu->setPosition({ size.width / 2 - 100, size.height / 2 + offset });

		return menu;
	}
};

std::string getListLabel(matjson::Value const& list) {
	if (list["isOfficial"].isBool() && list["isOfficial"].asBool().unwrap()) {
		if (list["slug"].isString()) {
			std::string slug = list["slug"].asString().unwrap();

			if (slug == "dl") return "DL";
			if (slug == "pl") return "PL";
			if (slug == "cl") return "CL";
			if (slug == "fl") return "FL";
		}
	}

	if (list["title"].isString()) {
		return list["title"].asString().unwrap();
	}

	return "List";
}

std::string formatNumber(matjson::Value const& value) {
	if (!value.isNumber()) {
		return "";
	}

	double number = value.asDouble().unwrap();
	int integer = static_cast<int>(number);

	if (number == static_cast<double>(integer)) {
		return std::to_string(integer);
	}

	std::string formatted = fmt::format("{:.1f}", number);

	while (formatted.size() > 1 && formatted.ends_with('0')) {
		formatted.pop_back();
	}

	if (formatted.ends_with('.')) {
		formatted.pop_back();
	}

	return formatted;
}

std::string getListValue(matjson::Value const& list) {
	auto item = list["item"];

	if (!item.isObject()) {
		return "";
	}

	bool topMode = false;

	if (list["topEnabled"].isBool()) {
		topMode = list["topEnabled"].asBool().unwrap();
	}
	else if (list["mode"].isString()) {
		topMode = list["mode"].asString().unwrap() == "top";
	}

	std::vector<std::string> values;

	if (topMode && item["position"].isNumber()) {
		values.push_back("#" + formatNumber(item["position"]));
	}

	if (item["rating"].isNumber()) {
		values.push_back(formatNumber(item["rating"]) + "pt");
	}

	if (!topMode && values.empty() && item["position"].isNumber()) {
		values.push_back("#" + formatNumber(item["position"]));
	}

	std::string text;

	for (auto& value : values) {
		if (!text.empty()) {
			text += " / ";
		}

		text += value;
	}

	return text;
}

std::vector<std::string> getListInfoLabels(matjson::Value const& json) {
	std::vector<std::string> labels;

	if (!json.isArray()) {
		return labels;
	}

	for (auto const& list : json.asArray().unwrap()) {
		if (!list.isObject()) {
			continue;
		}

		std::string value = getListValue(list);

		if (value.empty()) {
			continue;
		}

		labels.push_back(getListLabel(list) + ": " + value);
	}

	return labels;
}

class $modify(LevelInfoLayer) {
	struct Fields {
		async::TaskHolder<web::WebResponse> m_holder;
	};

	bool init(GJGameLevel* level, bool a) {
		if (!LevelInfoLayer::init(level, a)) {
			return false;
		}

		int id = level->m_levelID.value();
	    auto showLevelInfo = Mod::get()->getSettingValue<bool>("show-level-info");

	    if (showLevelInfo) {
		    auto loadingLabel = createLabel(level, "...", 0);

		    this->addChild(loadingLabel);

		    web::WebRequest req = web::WebRequest();

		    if (AuthService::isLoggedIn()) {
			    req.header("Authorization", "Bearer " + AuthService::getToken());
		    }

		    m_fields->m_holder.spawn(req.get(API_URL + "/lists/levels/" + std::to_string(id) + "/starred"), [this, level, loadingLabel, id](web::WebResponse res) mutable {
			    try {
                    if (loadingLabel) {
                        loadingLabel->removeFromParent();
                        loadingLabel = nullptr;
                    }

				    if (!res.ok()) {
					    return;
				    }

				    auto resJson = res.json().unwrap();
			        std::vector<std::string> labels = getListInfoLabels(resJson);

			        if (!labels.empty()) {
					    auto btn = ButtonCreator().create(labels, level, this);

					    this->addChild(btn);
                    }
			    } catch(...) {
                    if (loadingLabel) {
                        loadingLabel->removeFromParent();
                        loadingLabel = nullptr;
                    }
                    log::warn("Failed to load GDVN level info for level {}", id);
			    }
		    });
	    }

		return true;
	}
};
