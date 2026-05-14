#include "PvpRealtimeSocket.hpp"

#ifndef __APPLE__

namespace {
class StubPvpRealtimeSocket final : public PvpRealtimeSocket {
public:
	explicit StubPvpRealtimeSocket(PvpRealtimeSocketDelegate*) {}

	bool connect(std::string const&) override {
		return false;
	}

	void send(std::string const&) override {}
	void close() override {}

	bool isOpen() const override {
		return false;
	}
};
}

std::shared_ptr<PvpRealtimeSocket> PvpRealtimeSocket::create(PvpRealtimeSocketDelegate* delegate) {
	return std::make_shared<StubPvpRealtimeSocket>(delegate);
}

#endif
