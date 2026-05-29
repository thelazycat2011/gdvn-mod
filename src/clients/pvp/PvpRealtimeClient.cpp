#include "PvpRealtimeClient.hpp"

#include "../../adapters/PvpMatchAdapter.hpp"
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXSocketTLSOptions.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>

#include <Geode/loader/Loader.hpp>

#include <atomic>

namespace {
constexpr float HEARTBEAT_INTERVAL = 25.0f;
}

namespace gdvn::pvp_realtime_client_detail {
std::string trimTrailingSlash(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::string realtimeUrl(std::string url, std::string const& anonKey) {
    url = trimTrailingSlash(std::move(url));

    if (url.rfind("https://", 0) == 0) {
        url.replace(0, 5, "wss");
    } else if (url.rfind("http://", 0) == 0) {
        url.replace(0, 4, "ws");
    }

    return url + "/realtime/v1/websocket?apikey=" + anonKey + "&vsn=1.0.0";
}

matjson::Value makeChange(std::string const& event, std::string const& table, std::string const& filter) {
    auto change = matjson::Value::object();
    change["event"] = event;
    change["schema"] = "public";
    change["table"] = table;
    change["filter"] = filter;
    return change;
}

void ensureIxNetSystem() {
    static std::atomic_bool initialized = false;
    bool expected = false;
    if (initialized.compare_exchange_strong(expected, true)) {
        ix::initNetSystem();
    }
}

ix::SocketTLSOptions tlsOptions() {
    ix::SocketTLSOptions options;

#ifdef __ANDROID__
    // IXWebSocket's mbedTLS backend cannot read Android's system certificate store.
    // HTTPS API calls still use Geode/libcurl verification; this only affects the
    // Supabase realtime websocket on Android.
    options.caFile = "NONE";
#else
    options.caFile = "SYSTEM";
#endif

    return options;
}

class IxPvpRealtimeClient final : public PvpRealtimeClient, public std::enable_shared_from_this<IxPvpRealtimeClient> {
  public:
    explicit IxPvpRealtimeClient(PvpRealtimeClientDelegate* delegate) : m_delegate(delegate) {
    }

    ~IxPvpRealtimeClient() override {
        this->close();
    }

    bool connect(std::string supabaseUrl, std::string anonKey, int matchID, std::string accessToken) override {
        if (m_socket || m_closed.load() || supabaseUrl.empty() || anonKey.empty() || matchID <= 0) {
            return false;
        }

        m_matchID = matchID;
        m_accessToken = std::move(accessToken);
        m_topic = "realtime:gdvn-pvp-" + std::to_string(m_matchID);
        m_heartbeatTimer = 0.0f;
        m_ref = 1;

        ensureIxNetSystem();

        auto socket = std::make_unique<ix::WebSocket>();
        socket->setUrl(realtimeUrl(std::move(supabaseUrl), anonKey));
        socket->setTLSOptions(tlsOptions());
        socket->setHandshakeTimeout(10);
        socket->disableAutomaticReconnection();
        socket->disablePerMessageDeflate();

        auto weakSelf = this->weak_from_this();
        socket->setOnMessageCallback([weakSelf](ix::WebSocketMessagePtr const& msg) {
            auto self = weakSelf.lock();
            if (!self || !msg) {
                return;
            }

            switch (msg->type) {
            case ix::WebSocketMessageType::Open:
                self->handleOpen();
                break;

            case ix::WebSocketMessageType::Message:
                if (!msg->binary) {
                    self->handleMessage(msg->str);
                }
                break;

            case ix::WebSocketMessageType::Close:
            case ix::WebSocketMessageType::Error:
                self->handleClosed();
                break;

            default:
                break;
            }
        });

        m_socket = std::move(socket);
        m_socket->start();
        return true;
    }

    void update(float dt) override {
        if (!this->isOpen()) {
            return;
        }

        m_heartbeatTimer += dt;
        if (m_heartbeatTimer >= HEARTBEAT_INTERVAL) {
            m_heartbeatTimer = 0.0f;
            this->sendHeartbeat();
        }
    }

    void close() override {
        if (m_closed.exchange(true)) {
            return;
        }

        m_open.store(false);
        m_delegate.store(nullptr);

        if (m_socket) {
            m_socket->stop();
            m_socket.reset();
        }
    }

    bool isOpen() const override {
        return m_socket && m_open.load() && !m_closed.load() && m_socket->getReadyState() == ix::ReadyState::Open;
    }

  private:
    std::atomic<PvpRealtimeClientDelegate*> m_delegate = nullptr;
    std::unique_ptr<ix::WebSocket> m_socket;
    std::atomic_bool m_open = false;
    std::atomic_bool m_closed = false;
    int m_matchID = 0;
    int m_ref = 1;
    float m_heartbeatTimer = 0.0f;
    bool m_joined = false;
    std::string m_accessToken;
    std::string m_topic;

    void handleOpen() {
        if (m_closed.load()) {
            return;
        }

        m_open.store(true);
        this->sendJoin();
        auto weakSelf = this->weak_from_this();

        geode::Loader::get()->queueInMainThread([weakSelf] {
            if (auto self = weakSelf.lock()) {
                if (auto* delegate = self->m_delegate.load(); delegate && !self->m_closed.load()) {
                    delegate->onRealtimeOpen();
                }
            }
        });
    }

    void handleMessage(std::string const& message) {
        if (m_closed.load()) {
            return;
        }

        auto mapped = PvpMatchAdapter::realtimeMessageFromString(message);
        if (!mapped.valid) {
            geode::log::warn("Failed to parse Versus realtime message");
            return;
        }

        if (mapped.event == "phx_reply") {
            this->handleJoinReply(mapped);
            return;
        }

        if (mapped.event != "postgres_changes") {
            return;
        }

        auto weakSelf = this->weak_from_this();

        geode::Loader::get()->queueInMainThread([weakSelf, message = std::move(mapped)] {
            if (auto self = weakSelf.lock()) {
                if (auto* delegate = self->m_delegate.load(); delegate && !self->m_closed.load()) {
                    delegate->onRealtimeMessage(message);
                }
            }
        });
    }

    void handleJoinReply(PvpMatchRealtimeMessageDto const& message) {
        if (message.replyOk) {
            m_joined = true;
            return;
        }

        geode::log::warn("Failed to join Versus realtime channel");
        if (!m_accessToken.empty()) {
            m_accessToken.clear();
            this->sendJoin();
        }
    }

    void handleClosed() {
        if (m_closed.exchange(true)) {
            return;
        }

        m_open.store(false);
        auto weakSelf = this->weak_from_this();

        geode::Loader::get()->queueInMainThread([weakSelf] {
            if (auto self = weakSelf.lock()) {
                if (auto* delegate = self->m_delegate.exchange(nullptr)) {
                    delegate->onRealtimeClose();
                }
            }
        });
    }

    void sendJoin() {
        auto changes = matjson::Value::array();
        auto matchID = std::to_string(m_matchID);
        changes.push(makeChange("INSERT", "pvpMatchResults", "matchId=eq." + matchID));
        changes.push(makeChange("UPDATE", "pvpMatchResults", "matchId=eq." + matchID));
        changes.push(makeChange("UPDATE", "pvpMatches", "id=eq." + matchID));
        changes.push(makeChange("INSERT", "pvpMatchMessages", "matchId=eq." + matchID));

        auto broadcast = matjson::Value::object();
        broadcast["ack"] = false;
        broadcast["self"] = false;

        auto presence = matjson::Value::object();
        presence["enabled"] = false;

        auto config = matjson::Value::object();
        config["broadcast"] = broadcast;
        config["presence"] = presence;
        config["postgres_changes"] = changes;
        config["private"] = false;

        auto payload = matjson::Value::object();
        payload["config"] = config;
        if (!m_accessToken.empty()) {
            payload["access_token"] = m_accessToken;
        }

        auto ref = this->nextRef();
        auto message = matjson::Value::object();
        message["topic"] = m_topic;
        message["event"] = "phx_join";
        message["payload"] = payload;
        message["ref"] = ref;
        message["join_ref"] = ref;

        this->sendJson(message);
    }

    void sendHeartbeat() {
        auto message = matjson::Value::object();
        message["topic"] = "phoenix";
        message["event"] = "heartbeat";
        message["payload"] = matjson::Value::object();
        message["ref"] = this->nextRef();
        this->sendJson(message);
    }

    void sendJson(matjson::Value const& json) {
        if (!this->isOpen() || !m_socket) {
            return;
        }

        m_socket->sendUtf8Text(json.dump(matjson::NO_INDENTATION));
    }

    std::string nextRef() {
        return std::to_string(m_ref++);
    }
};
} // namespace gdvn::pvp_realtime_client_detail
using namespace gdvn::pvp_realtime_client_detail;

std::shared_ptr<PvpRealtimeClient> PvpRealtimeClient::create(PvpRealtimeClientDelegate* delegate) {
    return std::make_shared<IxPvpRealtimeClient>(delegate);
}
