#pragma once

#include <string>

struct PvpOverlayPlayerProgressModel {
    std::string uid;
    float progress = 0.0f;
    std::string playMode = "normal";
};
