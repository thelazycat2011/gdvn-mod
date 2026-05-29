#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/PauseLayer.hpp> // DO NOT REMOVE

#include "../services/pvp/PvpOverlayService.hpp"
#include "../ui/components/pvp/PvpChatButton.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    listenForKeybindSettingPresses("open-pvp-chat", [=](Keybind const&, bool down, bool repeat, double) {
        if (!down || repeat) {
            return false;
        }

        auto overlay = PvpOverlayService::getActive();
        return overlay && overlay->openChat();
    });
}

class $modify(GDVNPauseLayer, PauseLayer) {
    void customSetup() override {
        PauseLayer::customSetup();

        auto overlay = PvpOverlayService::getActive();
        if (!overlay || !overlay->hasPvpMatch()) {
            return;
        }

        auto menu = typeinfo_cast<CCMenu*>(this->getChildByIDRecursive("right-button-menu"));
        if (!menu) {
            log::warn("Could not find PauseLayer right-button-menu for GDVN controls");
            return;
        }

        menu->addChild(PvpChatButton::create());

        this->createToggleButton("Mute Chat", menu_selector(GDVNPauseLayer::onGDVNMuteChat), overlay->isChatMuted(),
                                 menu, {0.0f, 0.0f});

        menu->updateLayout();
    }

    void onGDVNMuteChat(CCObject*) {
        auto overlay = PvpOverlayService::getActive();
        if (!overlay) {
            auto muted = !Mod::get()->getSavedValue<bool>("pvp-chat-muted", false);
            Mod::get()->setSavedValue<bool>("pvp-chat-muted", muted);
            return;
        }

        overlay->setChatMuted(!overlay->isChatMuted());
    }
};
