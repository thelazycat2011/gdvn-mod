#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <functional>

#include "../../dtos/github/GithubReleaseResponseDto.hpp"

using namespace geode::prelude;

class UpdateClient {
public:
	using Callback = std::function<void(web::WebResponse&)>;
	using GetLatestReleaseCallback = std::function<void(GithubReleaseResponseDto const&, web::WebResponse&)>;

	static void getLatestDownload(Callback callback);
	static void getLatestRelease(GetLatestReleaseCallback callback);
};
