#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <functional>

#include "../models/LevelModels.hpp"

using namespace geode::prelude;

class LevelService {
	static async::TaskHolder<web::WebResponse> m_get_holder;

public:
	using GetLevelCallback = std::function<void(
		gdvn::models::LevelInfoResponseModel const& level,
		web::WebResponse& response
	)>;

	static void getLevel(int id, GetLevelCallback callback);
};
