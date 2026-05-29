#pragma once

#include <Geode/Geode.hpp>
#include <string>

namespace gdvn::models {

struct OtpResponseModel {
	bool valid = false;
	std::string code;

	static OtpResponseModel fromJson(matjson::Value const& json) {
		OtpResponseModel model;

		if (json["code"].isString()) {
			model.valid = true;
			model.code = json["code"].asString().unwrapOrDefault();
		}

		return model;
	}
};

struct OtpGrantResponseModel {
	bool valid = false;
	bool granted = false;
	std::string key;
	std::string player;

	static OtpGrantResponseModel fromJson(matjson::Value const& json) {
		OtpGrantResponseModel model;
		model.granted = json["granted"].asBool().unwrapOr(false);

		if (!model.granted) {
			model.valid = true;
			return model;
		}

		if (json["key"].isString() && json["player"].isString()) {
			model.valid = true;
			model.key = json["key"].asString().unwrapOrDefault();
			model.player = json["player"].asString().unwrapOrDefault();
		}

		return model;
	}
};

struct AuthMeResponseModel {
	bool valid = false;
	std::string name;

	static AuthMeResponseModel fromJson(matjson::Value const& json) {
		AuthMeResponseModel model;

		if (json["name"].isString()) {
			model.valid = true;
			model.name = json["name"].asString().unwrapOrDefault();
		}

		return model;
	}
};

}
