#pragma once

#include <cstdint>

namespace cocos2d {
class CCLabelBMFont;
}

struct PvpOverlayRecentChatMessageModel {
    std::int64_t id = 0;
    float timeLeft = 0.0f;
    cocos2d::CCLabelBMFont* label = nullptr;
};
