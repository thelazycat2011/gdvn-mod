#pragma once

#include "../../interfaces/PvpRealtimeClientDelegate.hpp"
#include <memory>
#include <string>

class PvpRealtimeClient {
  public:
    virtual ~PvpRealtimeClient() = default;

    static std::shared_ptr<PvpRealtimeClient> create(PvpRealtimeClientDelegate* delegate);

    virtual bool connect(std::string supabaseUrl, std::string anonKey, int matchID, std::string accessToken) = 0;
    virtual void update(float dt) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
};
