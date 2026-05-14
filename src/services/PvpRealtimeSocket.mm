#ifdef __APPLE__

#import <Foundation/Foundation.h>

#include "PvpRealtimeSocket.hpp"
#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace {
class ApplePvpRealtimeSocket final : public PvpRealtimeSocket, public std::enable_shared_from_this<ApplePvpRealtimeSocket> {
public:
	explicit ApplePvpRealtimeSocket(PvpRealtimeSocketDelegate* delegate) : m_delegate(delegate) {}

	~ApplePvpRealtimeSocket() override {
		this->close();
	}

	bool connect(std::string const& url) override {
		if (m_closed || m_task) {
			return false;
		}

		auto nsURL = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
		if (!nsURL) {
			return false;
		}

		m_session = [NSURLSession sessionWithConfiguration:NSURLSessionConfiguration.defaultSessionConfiguration];
		m_task = [m_session webSocketTaskWithURL:nsURL];
		[m_task resume];
		m_open = true;

		auto weakSelf = this->weak_from_this();
		Loader::get()->queueInMainThread([weakSelf] {
			if (auto self = weakSelf.lock(); self && self->m_delegate && !self->m_closed) {
				self->m_delegate->onRealtimeOpen();
			}
		});

		this->receiveNext();
		return true;
	}

	void send(std::string const& message) override {
		if (!m_task || m_closed || !m_open) {
			return;
		}

		auto nsMessage = [[NSURLSessionWebSocketMessage alloc] initWithString:[NSString stringWithUTF8String:message.c_str()]];
		auto weakSelf = this->weak_from_this();

		[m_task sendMessage:nsMessage completionHandler:^(NSError* error) {
			if (!error) {
				return;
			}

			Loader::get()->queueInMainThread([weakSelf] {
				if (auto self = weakSelf.lock()) {
					self->finishClosed();
				}
			});
		}];
	}

	void close() override {
		if (m_closed) {
			return;
		}

		m_closed = true;
		m_open = false;
		m_delegate = nullptr;

		if (m_task) {
			[m_task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure reason:nil];
			m_task = nil;
		}

		if (m_session) {
			[m_session invalidateAndCancel];
			m_session = nil;
		}
	}

	bool isOpen() const override {
		return m_open && !m_closed && m_task;
	}

private:
	PvpRealtimeSocketDelegate* m_delegate = nullptr;
	NSURLSession* m_session = nil;
	NSURLSessionWebSocketTask* m_task = nil;
	bool m_open = false;
	bool m_closed = false;

	void receiveNext() {
		if (!m_task || m_closed) {
			return;
		}

		auto weakSelf = this->weak_from_this();

		[m_task receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage* message, NSError* error) {
			if (error) {
				Loader::get()->queueInMainThread([weakSelf] {
					if (auto self = weakSelf.lock()) {
						self->finishClosed();
					}
				});
				return;
			}

			if (message.type == NSURLSessionWebSocketMessageTypeString) {
				std::string text;
				if (message.string) {
					text = [message.string UTF8String];
				}

				Loader::get()->queueInMainThread([weakSelf, text] {
					if (auto self = weakSelf.lock(); self && self->m_delegate && !self->m_closed) {
						self->m_delegate->onRealtimeMessage(text);
					}
				});
			}

			if (auto self = weakSelf.lock(); self && !self->m_closed) {
				self->receiveNext();
			}
		}];
	}

	void finishClosed() {
		if (m_closed) {
			return;
		}

		m_open = false;
		m_closed = true;

		if (m_task) {
			[m_task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure reason:nil];
			m_task = nil;
		}

		if (m_session) {
			[m_session invalidateAndCancel];
			m_session = nil;
		}

		if (m_delegate) {
			m_delegate->onRealtimeClose();
		}
	}
};
}

std::shared_ptr<PvpRealtimeSocket> PvpRealtimeSocket::create(PvpRealtimeSocketDelegate* delegate) {
	return std::make_shared<ApplePvpRealtimeSocket>(delegate);
}

#endif
