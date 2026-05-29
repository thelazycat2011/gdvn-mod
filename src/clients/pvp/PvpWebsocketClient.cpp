#include "PvpWebsocketClient.hpp"

#include "../../adapters/PvpMatchAdapter.hpp"
#include "../../utils/StringUtils.hpp"

#include <Geode/Geode.hpp>

#include <utility>

namespace {
constexpr float HEARTBEAT_INTERVAL = 25.0f;
}

namespace gdvn::pvp_websocket_client_detail {
std::string realtimeUrl(std::string url, std::string const& anonKey) {
    url = gdvn::utils::string::trimTrailingSlash(std::move(url));

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
} // namespace gdvn::pvp_websocket_client_detail
using namespace gdvn::pvp_websocket_client_detail;

PvpWebsocketClient::PvpWebsocketClient(Callbacks callbacks) : m_callbacks(std::move(callbacks)) {
}

PvpWebsocketClient::~PvpWebsocketClient() {
    this->close();
}

std::shared_ptr<PvpWebsocketClient> PvpWebsocketClient::create(Callbacks callbacks) {
    return std::make_shared<PvpWebsocketClient>(std::move(callbacks));
}

bool PvpWebsocketClient::connect(std::string supabaseUrl, std::string anonKey, int matchID, std::string accessToken) {
    if (this->hasSocket() || this->isClosed() || supabaseUrl.empty() || anonKey.empty() || matchID <= 0) {
        return false;
    }

    m_matchID = matchID;
    m_accessToken = std::move(accessToken);
    m_topic = "realtime:gdvn-pvp-" + std::to_string(m_matchID);
    m_heartbeatTimer = 0.0f;
    m_ref = 1;
    m_joined = false;

    return this->connectSocket(realtimeUrl(std::move(supabaseUrl), anonKey),
                               {
                                   [this] { this->handleOpen(); },
                                   [this](std::string const& message) { this->handleMessage(message); },
                                   [this] { this->handleClosed(); },
                               });
}

void PvpWebsocketClient::update(float dt) {
    if (!this->isOpen()) {
        return;
    }

    m_heartbeatTimer += dt;
    if (m_heartbeatTimer >= HEARTBEAT_INTERVAL) {
        m_heartbeatTimer = 0.0f;
        this->sendHeartbeat();
    }
}

void PvpWebsocketClient::close() {
    this->clearCallbacks();
    WebsocketClient::close();
}

void PvpWebsocketClient::handleOpen() {
    this->sendJoin();

    if (m_callbacks.onOpen) {
        m_callbacks.onOpen();
    }
}

void PvpWebsocketClient::handleMessage(std::string const& message) {
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

    if (m_callbacks.onMessage) {
        m_callbacks.onMessage(mapped);
    }
}

void PvpWebsocketClient::handleJoinReply(PvpMatchRealtimeMessageDto const& message) {
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

void PvpWebsocketClient::handleClosed() {
    auto callback = m_callbacks.onClose;
    this->clearCallbacks();

    if (callback) {
        callback();
    }
}

void PvpWebsocketClient::sendJoin() {
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

void PvpWebsocketClient::sendHeartbeat() {
    auto message = matjson::Value::object();
    message["topic"] = "phoenix";
    message["event"] = "heartbeat";
    message["payload"] = matjson::Value::object();
    message["ref"] = this->nextRef();
    this->sendJson(message);
}

void PvpWebsocketClient::sendJson(matjson::Value const& json) {
    this->sendText(json.dump(matjson::NO_INDENTATION));
}

std::string PvpWebsocketClient::nextRef() {
    return std::to_string(m_ref++);
}

void PvpWebsocketClient::clearCallbacks() {
    m_callbacks = {};
}
