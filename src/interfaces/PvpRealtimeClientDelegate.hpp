#pragma once

#include "../dtos/pvp/match/PvpMatchRealtimeMessageDto.hpp"

class PvpRealtimeClientDelegate {
  public:
    virtual ~PvpRealtimeClientDelegate() = default;
    virtual void onRealtimeOpen() = 0;
    virtual void onRealtimeMessage(PvpMatchRealtimeMessageDto const& message) = 0;
    virtual void onRealtimeClose() = 0;
};
