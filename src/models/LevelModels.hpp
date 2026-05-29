#pragma once

#include <Geode/Geode.hpp>
#include <optional>
#include <string>
#include <vector>

namespace gdvn::models {

struct LevelListItemModel {
	std::optional<double> position;
	std::optional<double> rating;

	static LevelListItemModel fromJson(matjson::Value const& json) {
		LevelListItemModel model;

		if (json["position"].isNumber()) {
			model.position = json["position"].asDouble().unwrapOr(0.0);
		}

		if (json["rating"].isNumber()) {
			model.rating = json["rating"].asDouble().unwrapOr(0.0);
		}

		return model;
	}
};

struct LevelListModel {
	std::string slug;
	std::string title;
	std::string mode;
	bool isOfficial = false;
	bool starred = false;
	bool hasStarred = false;
	bool topEnabled = false;
	bool hasTopEnabled = false;
	LevelListItemModel item;

	static LevelListModel fromJson(matjson::Value const& json) {
		LevelListModel model;

		if (json["slug"].isString()) {
			model.slug = json["slug"].asString().unwrapOrDefault();
		}

		if (json["title"].isString()) {
			model.title = json["title"].asString().unwrapOrDefault();
		}

		if (json["mode"].isString()) {
			model.mode = json["mode"].asString().unwrapOrDefault();
		}

		if (json["isOfficial"].isBool()) {
			model.isOfficial = json["isOfficial"].asBool().unwrapOr(false);
		}

		if (json["starred"].isBool()) {
			model.hasStarred = true;
			model.starred = json["starred"].asBool().unwrapOr(false);
		}

		if (json["topEnabled"].isBool()) {
			model.hasTopEnabled = true;
			model.topEnabled = json["topEnabled"].asBool().unwrapOr(false);
		}

		if (json["item"].isObject()) {
			model.item = LevelListItemModel::fromJson(json["item"]);
		}

		return model;
	}

	std::string label() const {
		if (!slug.empty()) {
			return slug;
		}

		if (!title.empty()) {
			return title;
		}

		return "List";
	}

	bool isStarredList() const {
		if (hasStarred) {
			return starred;
		}

		return !isOfficial;
	}

	bool isTopMode() const {
		if (hasTopEnabled) {
			return topEnabled;
		}

		return mode == "top";
	}
};

struct LevelInfoResponseModel {
	bool valid = false;
	std::vector<LevelListModel> lists;

	static LevelInfoResponseModel fromJson(matjson::Value const& json) {
		LevelInfoResponseModel model;

		if (!json.isArray()) {
			return model;
		}

		model.valid = true;

		for (auto const& item : json.asArray().unwrap()) {
			if (item.isObject()) {
				model.lists.push_back(LevelListModel::fromJson(item));
			}
		}

		return model;
	}
};

}
