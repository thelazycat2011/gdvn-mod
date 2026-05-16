#include "PvpOverlay.hpp"

#include "AuthService.hpp"
#include "../common.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>

using namespace geode::prelude;

namespace {
constexpr float HEARTBEAT_INTERVAL = 25.0f;
constexpr float LABEL_MARGIN = 8.0f;
constexpr float CHAT_MARGIN = 8.0f;
constexpr float CHAT_MESSAGE_LIFETIME = 8.0f;
constexpr float CHAT_FADE_TIME = 1.5f;
constexpr float CHAT_GRACE_SECONDS = 3.0f * 60.0f;
constexpr float MESSAGE_REFRESH_COALESCE = 0.2f;
constexpr int MAX_RECENT_MESSAGES = 4;
constexpr int MAX_CHAT_MESSAGE_LENGTH = 500;
constexpr int MESSAGE_FETCH_LIMIT = 100;

PvpOverlay* s_activeOverlay = nullptr;

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

std::string trimCopy(std::string value) {
	auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
		return std::isspace(c);
	});
	auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
		return std::isspace(c);
	}).base();

	if (begin >= end) {
		return "";
	}

	return std::string(begin, end);
}

std::string toLabelSafeText(std::string text) {
	std::string safe;
	safe.reserve(text.size());

	for (unsigned char c : text) {
		if (c == '\n' || c == '\r' || c == '\t') {
			safe.push_back(' ');
		} else if (c >= 32 && c < 127) {
			safe.push_back(static_cast<char>(c));
		}
	}

	return safe;
}

std::string truncateLabel(std::string text, size_t maxLength) {
	if (text.size() <= maxLength) {
		return text;
	}

	if (maxLength <= 3) {
		return text.substr(0, maxLength);
	}

	return text.substr(0, maxLength - 3) + "...";
}

std::string getString(matjson::Value const& json, char const* key) {
	if (!json[key].isString()) {
		return "";
	}

	return json[key].asString().unwrapOrDefault();
}

std::int64_t getInteger(matjson::Value const& json, char const* key) {
	if (!json[key].isNumber()) {
		return 0;
	}

	return static_cast<std::int64_t>(json[key].asDouble().unwrapOr(0.0));
}

float getNumber(matjson::Value const& json, char const* key) {
	if (!json[key].isNumber()) {
		return 0.0f;
	}

	return static_cast<float>(json[key].asDouble().unwrapOr(0.0));
}

bool getBool(matjson::Value const& json, char const* key) {
	return json[key].isBool() && json[key].asBool().unwrapOr(false);
}

std::int64_t currentEpochSeconds() {
	return std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
}

matjson::Value makeChange(std::string const& event, std::string const& table, std::string const& filter) {
	auto change = matjson::Value::object();
	change["event"] = event;
	change["schema"] = "public";
	change["table"] = table;
	change["filter"] = filter;
	return change;
}

matjson::Value const& realtimeRecord(matjson::Value const& payload) {
	auto const& data = payload["data"];

	if (data["record"].isObject()) {
		return data["record"];
	}

	if (payload["new"].isObject()) {
		return payload["new"];
	}

	return payload;
}
}

class PvpChatPopup final : public Popup, public TextInputDelegate {
public:
	static PvpChatPopup* create(PvpOverlay* overlay) {
		auto ret = new PvpChatPopup();
		if (ret->init(overlay)) {
			ret->autorelease();
			return ret;
		}

		delete ret;
		return nullptr;
	}

	void focusInput() {
		if (m_input) {
			m_input->focus();
		}
	}

	void setSending(bool sending) {
		m_sending = sending;
		if (m_sendButton) {
			m_sendButton->setEnabled(!sending);
		}
	}

	void setStatus(std::string const& status) {
		if (m_statusLabel) {
			m_statusLabel->setString(status.c_str());
		}
	}

	void closeAfterSend() {
		this->onClose(nullptr);
	}

	void closeFromOverlay() {
		m_overlay = nullptr;
		this->onClose(nullptr);
	}

protected:
	bool init(PvpOverlay* overlay) {
		if (!Popup::init(360.0f, 120.0f)) {
			return false;
		}

		m_overlay = overlay;
		this->setTitle("PvP Chat");

		m_input = TextInput::create(250.0f, "Type a message...");
		m_input->setCommonFilter(CommonFilter::Any);
		m_input->setMaxCharCount(MAX_CHAT_MESSAGE_LENGTH);
		m_input->setTextAlign(TextInputAlign::Left);
		m_input->setDelegate(this);
		m_mainLayer->addChildAtPosition(m_input, Anchor::Center, { -38.0f, 5.0f });

		auto sendSprite = ButtonSprite::create("Send", "goldFont.fnt", "GJ_button_01.png", 0.8f);
		sendSprite->setScale(0.55f);
		m_sendButton = CCMenuItemExt::createSpriteExtra(sendSprite, [this](auto*) {
			this->submit();
		});
		m_buttonMenu->addChildAtPosition(m_sendButton, Anchor::Center, { 128.0f, 5.0f });

		m_statusLabel = CCLabelBMFont::create("", "bigFont.fnt");
		m_statusLabel->setScale(0.32f);
		m_statusLabel->setOpacity(180);
		m_mainLayer->addChildAtPosition(m_statusLabel, Anchor::Bottom, { 0.0f, 18.0f });

		return true;
	}

	void onClose(CCObject* sender) override {
		if (m_overlay) {
			auto overlay = m_overlay;
			m_overlay = nullptr;
			overlay->notifyChatPopupClosed(this);
		}

		Popup::onClose(sender);
	}

	void keyDown(enumKeyCodes key, double timestamp) override {
		if (key == KEY_Enter || key == KEY_NumEnter) {
			this->submit();
			return;
		}

		Popup::keyDown(key, timestamp);
	}

	void textChanged(CCTextInputNode*) override {}

	void textInputReturn(CCTextInputNode*) override {
		this->submit();
	}

	void enterPressed(CCTextInputNode*) override {
		this->submit();
	}

private:
	PvpOverlay* m_overlay = nullptr;
	TextInput* m_input = nullptr;
	CCLabelBMFont* m_statusLabel = nullptr;
	CCMenuItemSpriteExtra* m_sendButton = nullptr;
	bool m_sending = false;

	void submit() {
		if (!m_overlay || !m_input || m_sending) {
			return;
		}

		auto content = trimCopy(static_cast<std::string>(m_input->getString()));
		if (content.empty()) {
			this->setStatus("Message cannot be empty");
			return;
		}

		this->setSending(true);
		this->setStatus("Sending...");
		m_overlay->submitChatMessage(std::move(content));
	}
};

PvpOverlay::PvpOverlay(PlayLayer* layer, int levelID) : m_layer(layer), m_levelID(levelID) {
	s_activeOverlay = this;
	m_chatMuted = Mod::get()->getSavedValue<bool>("pvp-chat-muted", false);
	this->createLabel();
	this->createChatNodes();
	this->requestMatch();
}

PvpOverlay::~PvpOverlay() {
	this->cleanup();
}

PvpOverlay* PvpOverlay::getActive() {
	return s_activeOverlay;
}

bool PvpOverlay::isChatMuted() const {
	return m_chatMuted;
}

bool PvpOverlay::hasPvpMatch() const {
	return m_matchID > 0 && m_chatOpen && !m_cleanedUp;
}

bool PvpOverlay::isChatUsable() const {
	return m_matchID > 0 && m_chatOpen && !m_chatMuted && !m_cleanedUp;
}

bool PvpOverlay::openChat() {
	if (!this->isChatUsable()) {
		return false;
	}

	if (m_chatPopup) {
		m_chatPopup->focusInput();
		return true;
	}

	m_chatPopup = PvpChatPopup::create(this);
	if (!m_chatPopup) {
		return false;
	}

	m_chatPopup->show();
	m_chatPopup->focusInput();
	return true;
}

void PvpOverlay::setChatMuted(bool muted) {
	if (m_chatMuted == muted) {
		return;
	}

	m_chatMuted = muted;
	Mod::get()->setSavedValue<bool>("pvp-chat-muted", muted);

	if (muted) {
		for (auto& message : m_recentMessages) {
			if (message.label) {
				message.label->removeFromParentAndCleanup(true);
			}
		}
		m_recentMessages.clear();

		if (m_chatPopup) {
			m_chatPopup->closeFromOverlay();
			m_chatPopup = nullptr;
		}
	}

	this->refreshChatVisibility();
}

void PvpOverlay::notifyChatPopupClosed(PvpChatPopup* popup) {
	if (m_chatPopup == popup) {
		m_chatPopup = nullptr;
	}
}

void PvpOverlay::createLabel() {
	if (!m_layer) {
		return;
	}

	m_label = CCLabelBMFont::create("PvP\nYou: 0.00%\nOpponent: 0.00%", "bigFont.fnt");
	m_label->setScale(0.32f);
	m_label->setOpacity(180);
	m_label->setVisible(false);
	m_label->setID("gdvn-pvp-overlay"_spr);

	auto parent = m_layer->m_uiLayer ? static_cast<CCNode*>(m_layer->m_uiLayer) : static_cast<CCNode*>(m_layer);
	parent->addChild(m_label, 1000);
	this->updateLabelPosition();
}

void PvpOverlay::createChatNodes() {
	if (!m_layer) {
		return;
	}

	auto parent = m_layer->m_uiLayer ? static_cast<CCNode*>(m_layer->m_uiLayer) : static_cast<CCNode*>(m_layer);

	m_chatStack = CCNode::create();
	m_chatStack->setID("gdvn-pvp-chat-stack"_spr);
	m_chatStack->setVisible(false);
	parent->addChild(m_chatStack, 1001);

#ifdef GEODE_IS_MOBILE
	m_mobileChatMenu = CCMenu::create();
	m_mobileChatMenu->setID("gdvn-pvp-chat-menu"_spr);
	m_mobileChatMenu->setPosition({ 0.0f, 0.0f });
	m_mobileChatMenu->setVisible(false);

	auto chatSprite = ButtonSprite::create("Chat", "goldFont.fnt", "GJ_button_01.png", 0.8f);
	chatSprite->setScale(0.5f);
	m_mobileChatButton = CCMenuItemExt::createSpriteExtra(chatSprite, [this](auto*) {
		this->openChat();
	});
	m_mobileChatMenu->addChild(m_mobileChatButton);
	parent->addChild(m_mobileChatMenu, 1002);
#endif

	this->updateChatPositions();
	this->refreshChatVisibility();
}

void PvpOverlay::requestMatch() {
	if (!AuthService::isLoggedIn() || m_cleanedUp) {
		return;
	}

	web::WebRequest req;
	req.header("Authorization", "Bearer " + AuthService::getToken());
	std::string url = API_URL + "/levels/" + std::to_string(m_levelID) + "/inPvp";

	m_matchHolder.spawn(req.get(url), [this](web::WebResponse res) {
		if (m_cleanedUp) {
			return;
		}

		if (!res.ok()) {
			this->setOverlayVisible(false);
			return;
		}

		try {
			auto json = res.json().unwrap();
			this->parseMatchSnapshot(json);
			this->refreshLabel();

			if (this->isReadyForRealtime()) {
				this->requestRealtimeToken();
			}
		} catch (...) {
			log::warn("Failed to parse PvP overlay match snapshot");
		}
	});
}

void PvpOverlay::parseMatchSnapshot(matjson::Value const& json) {
	m_matchID = static_cast<int>(getNumber(json, "matchId"));
	m_currentUid = getString(json, "currentUid");
	auto status = getString(json, "status");
	m_active = this->isActiveStatus(status);
	m_chatOpen = m_active || this->isCompletedStatus(status);
	m_chatGraceTimer = this->isCompletedStatus(status) ? CHAT_GRACE_SECONDS : -1.0f;

	m_self = {};
	m_opponent = {};

	if (json["participants"].isArray()) {
		for (auto const& participant : json["participants"].asArray().unwrap()) {
			PlayerProgress player;
			player.uid = getString(participant, "uid");
			player.progress = getNumber(participant, "progress");

			if (!participant["result"].isNull() && participant["result"].isObject()) {
				player.progress = std::max(player.progress, getNumber(participant["result"], "progress"));
			}

			if (!m_currentUid.empty() && player.uid == m_currentUid) {
				m_self = player;
			} else if (m_opponent.uid.empty()) {
				m_opponent = player;
			}
		}
	}

	if (json["results"].isArray()) {
		for (auto const& result : json["results"].asArray().unwrap()) {
			this->handleResultRow(result);
		}
	}

	this->setOverlayVisible(m_active);
	this->refreshChatVisibility();

	if (m_matchID > 0) {
		this->requestMessages(false, false);
	}
}

void PvpOverlay::requestRealtimeToken() {
	if (m_cleanedUp || m_requestingRealtimeToken) {
		return;
	}

	m_requestingRealtimeToken = true;

	web::WebRequest req;
	req.header("Authorization", "Bearer " + AuthService::getToken());
	std::string url = API_URL + "/auth/realtime-token";

	m_tokenHolder.spawn(req.get(url), [this](web::WebResponse res) {
		m_requestingRealtimeToken = false;

		if (m_cleanedUp) {
			return;
		}

		if (!res.ok()) {
			log::warn("Failed to get PvP realtime token: HTTP {}", res.code());
			return;
		}

		try {
			auto json = res.json().unwrap();
			m_supabaseUrl = getString(json, "supabaseUrl");
			m_anonKey = getString(json, "anonKey");
			m_realtimeAccessToken = getString(json, "accessToken");
			m_realtimeTokenExpiresAt = getInteger(json, "expiresAt");
			this->connectRealtime();
		} catch (...) {
			log::warn("Failed to parse PvP realtime token");
		}
	});
}

void PvpOverlay::requestMessages(bool animateNew, bool incremental) {
	if (!AuthService::isLoggedIn() || m_cleanedUp || m_matchID <= 0) {
		return;
	}

	web::WebRequest req;
	req.header("Authorization", "Bearer " + AuthService::getToken());

	auto url = API_URL + "/pvp/matches/" + std::to_string(m_matchID) + "/messages";
	std::vector<std::string> params;

	if (incremental && m_latestMessageID > 0) {
		params.push_back("afterId=" + std::to_string(m_latestMessageID));
	}

	if (incremental) {
		params.push_back("limit=" + std::to_string(MESSAGE_FETCH_LIMIT));
	}

	if (!params.empty()) {
		url += "?";
		for (size_t i = 0; i < params.size(); ++i) {
			if (i > 0) {
				url += "&";
			}
			url += params[i];
		}
	}

	m_messagesHolder.spawn(req.get(url), [this, animateNew](web::WebResponse res) {
		if (m_cleanedUp) {
			return;
		}

		if (!res.ok()) {
			log::warn("Failed to load PvP chat messages: HTTP {}", res.code());
			return;
		}

		try {
			this->handleMessagesPayload(res.json().unwrap(), animateNew);
		} catch (...) {
			log::warn("Failed to parse PvP chat messages");
		}
	});
}

void PvpOverlay::submitChatMessage(std::string content) {
	if (!this->isChatUsable() || m_chatSending || m_matchID <= 0) {
		if (m_chatPopup) {
			m_chatPopup->setSending(false);
			m_chatPopup->setStatus("Chat is unavailable");
		}
		return;
	}

	content = trimCopy(std::move(content));
	if (content.empty()) {
		if (m_chatPopup) {
			m_chatPopup->setSending(false);
			m_chatPopup->setStatus("Message cannot be empty");
		}
		return;
	}

	if (content.size() > MAX_CHAT_MESSAGE_LENGTH) {
		content = content.substr(0, MAX_CHAT_MESSAGE_LENGTH);
	}

	m_chatSending = true;

	web::WebRequest req;
	req.header("Authorization", "Bearer " + AuthService::getToken());

	auto body = matjson::Value::object();
	body["content"] = content;
	req.bodyJSON(body);

	auto url = API_URL + "/pvp/matches/" + std::to_string(m_matchID) + "/messages";
	m_sendMessageHolder.spawn(req.post(url), [this](web::WebResponse res) {
		m_chatSending = false;

		if (m_cleanedUp) {
			return;
		}

		if (!res.ok()) {
			log::warn("Failed to send PvP chat message: HTTP {}", res.code());
			if (m_chatPopup) {
				m_chatPopup->setSending(false);
				m_chatPopup->setStatus("Failed to send message");
			}
			Notification::create("Failed to send PvP chat message", NotificationIcon::Error, 2.0f)->show();
			return;
		}

		try {
			this->handleMessageRow(res.json().unwrap(), true);
		} catch (...) {
			log::warn("Failed to parse sent PvP chat message");
		}

		if (m_chatPopup) {
			m_chatPopup->closeAfterSend();
		}
	});
}

bool PvpOverlay::isReadyForRealtime() const {
	return m_matchID > 0 && m_chatOpen && !m_cleanedUp;
}

void PvpOverlay::connectRealtime() {
	if (!this->isReadyForRealtime() || m_supabaseUrl.empty() || m_anonKey.empty() || m_socket) {
		return;
	}

	m_connecting = true;
	m_joined = false;
	m_heartbeatTimer = 0.0f;
	m_topic = "realtime:gdvn-pvp-" + std::to_string(m_matchID);

	auto socket = PvpRealtimeSocket::create(this);
	auto url = realtimeUrl(m_supabaseUrl, m_anonKey);

	if (!socket->connect(url)) {
		m_connecting = false;
		this->scheduleReconnect();
		return;
	}

	m_socket = socket;
}

void PvpOverlay::onRealtimeOpen() {
	if (!m_socket || m_cleanedUp) {
		return;
	}

	m_connecting = false;
	m_reconnectAttempts = 0;
	this->sendJoin();
}

void PvpOverlay::onRealtimeMessage(std::string const& message) {
	if (!m_socket || m_cleanedUp || message.empty()) {
		return;
	}

	try {
		auto json = matjson::parse(message).unwrap();
		this->handleRealtimeMessage(json);
	} catch (...) {
		log::warn("Failed to parse PvP realtime message");
	}
}

void PvpOverlay::onRealtimeClose() {
	m_socket.reset();
	m_connecting = false;
	m_joined = false;

	if (!m_cleanedUp && m_chatOpen) {
		this->scheduleReconnect();
	}
}

void PvpOverlay::handleRealtimeMessage(matjson::Value const& json) {
	auto event = getString(json, "event");

	if (event == "phx_reply") {
		if (getString(json["payload"], "status") == "ok") {
			m_joined = true;
		}
		return;
	}

	if (event == "postgres_changes") {
		auto const& payload = json["payload"];
		auto const& data = payload["data"];
		auto table = getString(data, "table");
		auto const& row = realtimeRecord(payload);

		if (table == "pvpMatchResults") {
			this->handleResultRow(row);
			this->refreshLabel();
		} else if (table == "pvpMatches") {
			this->handleMatchRow(row);
		} else if (table == "pvpMatchMessages") {
			this->scheduleMessageRefresh();
		}
	}
}

void PvpOverlay::handleResultRow(matjson::Value const& row) {
	auto uid = getString(row, "uid");
	auto progress = getNumber(row, "progress");

	if (uid.empty()) {
		return;
	}

	if (!m_currentUid.empty() && uid == m_currentUid) {
		m_self.uid = uid;
		m_self.progress = std::max(m_self.progress, progress);
		return;
	}

	m_opponent.uid = uid;
	m_opponent.progress = std::max(m_opponent.progress, progress);
}

void PvpOverlay::handleMatchRow(matjson::Value const& row) {
	auto status = getString(row, "status");
	m_active = this->isActiveStatus(status);
	m_chatOpen = m_active || this->isCompletedStatus(status);
	m_chatGraceTimer = this->isCompletedStatus(status) ? CHAT_GRACE_SECONDS : -1.0f;
	this->setOverlayVisible(m_active);
	this->refreshChatVisibility();

	if (!m_chatOpen) {
		this->closeSocket();
	}
}

void PvpOverlay::scheduleMessageRefresh() {
	if (m_cleanedUp || m_matchID <= 0) {
		return;
	}

	m_messageRefreshTimer = MESSAGE_REFRESH_COALESCE;
}

void PvpOverlay::handleMessagesPayload(matjson::Value const& json, bool animateNew) {
	if (json.isArray()) {
		for (auto const& message : json.asArray().unwrap()) {
			this->handleMessageRow(message, animateNew);
		}
		return;
	}

	if (json["messages"].isArray()) {
		for (auto const& message : json["messages"].asArray().unwrap()) {
			this->handleMessageRow(message, animateNew);
		}
		return;
	}

	if (json["data"].isArray()) {
		for (auto const& message : json["data"].asArray().unwrap()) {
			this->handleMessageRow(message, animateNew);
		}
	}
}

void PvpOverlay::handleMessageRow(matjson::Value const& row, bool animateNew) {
	ChatMessage message;
	message.id = getInteger(row, "id");
	message.senderUid = getString(row, "senderUid");
	message.type = getString(row, "type");
	message.content = getString(row, "content");
	message.senderAnonymous = getBool(row, "senderAnonymous") || getBool(row, "sender_anonymous");

	if (row["sender"].isObject()) {
		message.senderName = getString(row["sender"], "name");
	}

	if (message.senderName.empty() && row["player"].isObject()) {
		message.senderName = getString(row["player"], "name");
	}

	if (message.content.empty()) {
		return;
	}

	auto previousLatest = m_latestMessageID;
	if (message.id > m_latestMessageID) {
		m_latestMessageID = message.id;
	}

	if (animateNew && message.id > previousLatest && !m_chatMuted) {
		this->pushRecentMessage(message);
	}
}

void PvpOverlay::pushRecentMessage(ChatMessage const& message) {
	if (!m_chatStack || m_chatMuted) {
		return;
	}

	std::string sender;
	if (message.type == "system") {
		sender = "Arena";
	} else if (!m_currentUid.empty() && message.senderUid == m_currentUid) {
		sender = "You";
	} else if (message.senderAnonymous) {
		sender = "Anonymous";
	} else if (!message.senderName.empty()) {
		sender = message.senderName;
	} else {
		sender = "Rival";
	}

	auto text = truncateLabel(toLabelSafeText(sender + ": " + message.content), 110);
	if (text.empty()) {
		return;
	}

	auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
	label->setAnchorPoint({ 0.0f, 0.0f });
	label->setScale(0.28f);
	label->setOpacity(220);

	if (message.type == "system") {
		label->setColor(ccc3(255, 225, 120));
	} else if (!m_currentUid.empty() && message.senderUid == m_currentUid) {
		label->setColor(ccc3(150, 255, 150));
	}

	label->limitLabelWidth(250.0f, 0.28f, 0.14f);
	m_chatStack->addChild(label);

	m_recentMessages.push_back({ message.id, CHAT_MESSAGE_LIFETIME, label });

	while (m_recentMessages.size() > MAX_RECENT_MESSAGES) {
		if (auto oldLabel = m_recentMessages.front().label) {
			oldLabel->removeFromParentAndCleanup(true);
		}
		m_recentMessages.erase(m_recentMessages.begin());
	}

	this->layoutRecentMessages();
	this->refreshChatVisibility();
}

void PvpOverlay::layoutRecentMessages() {
	for (size_t i = 0; i < m_recentMessages.size(); ++i) {
		if (auto label = m_recentMessages[i].label) {
			auto reverseIndex = m_recentMessages.size() - i - 1;
			label->setPosition({ 0.0f, static_cast<float>(reverseIndex) * 12.0f });
		}
	}
}

void PvpOverlay::updateRecentMessages(float dt) {
	if (m_recentMessages.empty()) {
		return;
	}

	for (auto& message : m_recentMessages) {
		message.timeLeft -= dt;
		if (message.label) {
			auto opacity = message.timeLeft < CHAT_FADE_TIME
				? static_cast<unsigned char>(std::max(0.0f, message.timeLeft / CHAT_FADE_TIME) * 220.0f)
				: static_cast<unsigned char>(220);
			message.label->setOpacity(opacity);
		}
	}

	for (auto it = m_recentMessages.begin(); it != m_recentMessages.end();) {
		if (it->timeLeft <= 0.0f) {
			if (it->label) {
				it->label->removeFromParentAndCleanup(true);
			}
			it = m_recentMessages.erase(it);
		} else {
			++it;
		}
	}

	this->layoutRecentMessages();
	this->refreshChatVisibility();
}

void PvpOverlay::sendJoin() {
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
	if (!m_realtimeAccessToken.empty()) {
		payload["access_token"] = m_realtimeAccessToken;
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

void PvpOverlay::sendHeartbeat() {
	auto message = matjson::Value::object();
	message["topic"] = "phoenix";
	message["event"] = "heartbeat";
	message["payload"] = matjson::Value::object();
	message["ref"] = this->nextRef();
	this->sendJson(message);
}

void PvpOverlay::sendJson(matjson::Value const& json) {
	if (!m_socket || !m_socket->isOpen()) {
		return;
	}

	m_socket->send(json.dump(matjson::NO_INDENTATION));
}

std::string PvpOverlay::nextRef() {
	return std::to_string(m_ref++);
}

void PvpOverlay::update(float dt) {
	if (m_cleanedUp) {
		return;
	}

	this->updateLabelPosition();
	this->updateChatPositions();
	this->updateRecentMessages(dt);

	if (m_chatGraceTimer >= 0.0f) {
		m_chatGraceTimer -= dt;
		if (m_chatGraceTimer <= 0.0f) {
			m_chatGraceTimer = -1.0f;
			if (!m_active) {
				m_chatOpen = false;
				this->refreshChatVisibility();
				this->closeSocket();
			}
		}
	}

	if (m_messageRefreshTimer >= 0.0f) {
		m_messageRefreshTimer -= dt;
		if (m_messageRefreshTimer <= 0.0f) {
			m_messageRefreshTimer = -1.0f;
			this->requestMessages(true, true);
		}
	}

	if (m_reconnectTimer >= 0.0f) {
		m_reconnectTimer -= dt;
		if (m_reconnectTimer <= 0.0f) {
			m_reconnectTimer = -1.0f;
			this->requestRealtimeToken();
		}
	}

	if (
		m_socket &&
		m_socket->isOpen() &&
		m_realtimeTokenExpiresAt > 0 &&
		currentEpochSeconds() >= m_realtimeTokenExpiresAt - 60
	) {
		m_realtimeTokenExpiresAt = 0;
		this->closeSocket();
		this->requestRealtimeToken();
		return;
	}

	if (m_socket && m_socket->isOpen()) {
		m_heartbeatTimer += dt;
		if (m_heartbeatTimer >= HEARTBEAT_INTERVAL) {
			m_heartbeatTimer = 0.0f;
			this->sendHeartbeat();
		}
	}
}

void PvpOverlay::scheduleReconnect() {
	if (m_cleanedUp || !m_chatOpen || m_connecting) {
		return;
	}

	m_reconnectAttempts = std::min(m_reconnectAttempts + 1, 4);
	m_reconnectTimer = static_cast<float>(std::min(1 << (m_reconnectAttempts - 1), 10));
}

void PvpOverlay::closeSocket() {
	if (!m_socket) {
		return;
	}

	auto socket = m_socket;
	m_socket.reset();
	m_connecting = false;
	m_joined = false;
	socket->close();
}

void PvpOverlay::cleanup() {
	if (m_cleanedUp) {
		return;
	}

	m_cleanedUp = true;
	m_matchHolder.cancel();
	m_tokenHolder.cancel();
	m_messagesHolder.cancel();
	m_sendMessageHolder.cancel();
	this->closeSocket();

	if (s_activeOverlay == this) {
		s_activeOverlay = nullptr;
	}

	if (m_chatPopup) {
		m_chatPopup->closeFromOverlay();
		m_chatPopup = nullptr;
	}

	if (m_label) {
		m_label->removeFromParentAndCleanup(true);
		m_label = nullptr;
	}

	if (m_chatStack) {
		m_chatStack->removeFromParentAndCleanup(true);
		m_chatStack = nullptr;
	}

#ifdef GEODE_IS_MOBILE
	if (m_mobileChatMenu) {
		m_mobileChatMenu->removeFromParentAndCleanup(true);
		m_mobileChatMenu = nullptr;
		m_mobileChatButton = nullptr;
	}
#endif
}

void PvpOverlay::refreshLabel() {
	if (!m_label) {
		return;
	}

	m_label->setString(fmt::format("PvP\nYou: {:.2f}%\nOpponent: {:.2f}%", m_self.progress, m_opponent.progress).c_str());
	this->setOverlayVisible(m_active);
	this->updateLabelPosition();
}

void PvpOverlay::refreshChatVisibility() {
	auto visible = m_chatOpen && !m_chatMuted && !m_cleanedUp;

	if (!visible && m_chatPopup) {
		m_chatPopup->closeFromOverlay();
		m_chatPopup = nullptr;
	}

	if (m_chatStack) {
		m_chatStack->setVisible(visible && !m_recentMessages.empty());
	}

#ifdef GEODE_IS_MOBILE
	if (m_mobileChatMenu) {
		m_mobileChatMenu->setVisible(visible);
	}
#endif
}

void PvpOverlay::updateLabelPosition() {
	if (!m_label) {
		return;
	}

	auto size = CCDirector::sharedDirector()->getWinSize();
	m_label->setAnchorPoint({ 0.0f, 1.0f });
	m_label->setPosition({ LABEL_MARGIN, size.height - LABEL_MARGIN });
}

void PvpOverlay::updateChatPositions() {
	float stackY = CHAT_MARGIN;

#ifdef GEODE_IS_MOBILE
	if (m_mobileChatButton) {
		m_mobileChatButton->setPosition({ 34.0f, 22.0f });
		stackY = 44.0f;
	}
#endif

	if (m_chatStack) {
		m_chatStack->setPosition({ CHAT_MARGIN, stackY });
	}
}

void PvpOverlay::setOverlayVisible(bool visible) {
	if (m_label) {
		m_label->setVisible(visible && Mod::get()->getSettingValue<bool>("show-pvp-overlay"));
	}
}

bool PvpOverlay::isActiveStatus(std::string const& status) const {
	return status == "in_progress" || status == "waiting_result";
}

bool PvpOverlay::isCompletedStatus(std::string const& status) const {
	return status == "completed";
}

$execute {
	listenForKeybindSettingPresses("open-pvp-chat", [](Keybind const&, bool down, bool repeat, double) {
		if (!down || repeat) {
			return false;
		}

		auto overlay = PvpOverlay::getActive();
		return overlay ? overlay->openChat() : false;
	});
}
