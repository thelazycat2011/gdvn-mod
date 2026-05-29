#pragma once

#include <Geode/Geode.hpp>
#include <string>

namespace gdvn::models {

struct GithubReleaseResponseModel {
	bool valid = false;
	std::string tagName;

	static GithubReleaseResponseModel fromJson(matjson::Value const& json) {
		GithubReleaseResponseModel model;

		if (json["tag_name"].isString()) {
			model.valid = true;
			model.tagName = json["tag_name"].asString().unwrapOrDefault();
		}

		return model;
	}
};

}
