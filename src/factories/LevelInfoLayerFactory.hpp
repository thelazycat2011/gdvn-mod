#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <vector>

namespace gdvn::level_info {

class LevelInfoLayerFactory {
  public:
    static geode::prelude::CCLabelBMFont* createLabel(GJGameLevel* level, std::string const& text, int order);
    static geode::prelude::CCMenu* createButton(std::vector<std::string> const& labels, GJGameLevel* level,
                                                geode::prelude::CCObject* target,
                                                geode::prelude::SEL_MenuHandler callback);
};

} // namespace gdvn::level_info
