#pragma once

#include <optional>
#include <string_view>

class AntiCheatService {
  public:
    static bool isGameplayCheated();
    static std::optional<std::string_view> getGameplayCheatReason();
};
