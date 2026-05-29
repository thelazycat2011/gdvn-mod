#pragma once

#include <Geode/Geode.hpp>
#include <cstdint>
#include <string>

namespace gdvn::models {

struct ActivePvpMatchResponseModel {
	bool valid = false;
	int matchID = 0;
	std::string mode;
	matjson::Value rawJson;

	static ActivePvpMatchResponseModel fromJson(matjson::Value const& json) {
		ActivePvpMatchResponseModel model;
		model.rawJson = json;

		if (json["matchId"].isNumber()) {
			model.valid = true;
			model.matchID = static_cast<int>(json["matchId"].asDouble().unwrapOr(0.0));
		}

		if (json["mode"].isString()) {
			model.mode = json["mode"].asString().unwrapOrDefault();
		}

		return model;
	}
};

struct RealtimeTokenResponseModel {
	bool valid = false;
	std::string supabaseUrl;
	std::string anonKey;
	std::string accessToken;
	std::int64_t expiresAt = 0;

	static RealtimeTokenResponseModel fromJson(matjson::Value const& json) {
		RealtimeTokenResponseModel model;

		if (json["supabaseUrl"].isString()) {
			model.supabaseUrl = json["supabaseUrl"].asString().unwrapOrDefault();
		}

		if (json["anonKey"].isString()) {
			model.anonKey = json["anonKey"].asString().unwrapOrDefault();
		}

		if (json["accessToken"].isString()) {
			model.accessToken = json["accessToken"].asString().unwrapOrDefault();
		}

		if (json["expiresAt"].isNumber()) {
			model.expiresAt = static_cast<std::int64_t>(json["expiresAt"].asDouble().unwrapOr(0.0));
		}

		model.valid = !model.supabaseUrl.empty() && !model.anonKey.empty() && !model.accessToken.empty();
		return model;
	}
};

}
