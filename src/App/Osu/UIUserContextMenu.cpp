#include "UIUserContextMenu.h"

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "Chat.h"
#include "Engine.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "SpectatorScreen.h"
#include "UIContextMenu.h"

UIUserContextMenuScreen::UIUserContextMenuScreen() : OsuScreen() {
    this->bVisible = true;
    this->menu = new UIContextMenu();
    this->addBaseUIElement(this->menu);
}

void UIUserContextMenuScreen::onResolutionChange(Vector2 newResolution) {
    this->setSize(newResolution);
    OsuScreen::onResolutionChange(newResolution);
}

void UIUserContextMenuScreen::stealFocus() {
    OsuScreen::stealFocus();
    this->close();
}

void UIUserContextMenuScreen::open(u32 user_id) {
    if(!bancho.is_online()) return;

    this->close();
    this->user_id = user_id;

    auto user_info = get_user_info(user_id);
    bool has_user_info = user_info->privileges != 0;

    int slot_number = -1;
    if(bancho.is_in_a_multi_room()) {
        for(int i = 0; i < 16; i++) {
            if(bancho.room.slots[i].player_id == user_id) {
                slot_number = i;
                break;
            }
        }
    }

    this->menu->begin();

    this->menu->addButton("View profile", VIEW_PROFILE);

    if(user_id != bancho.user_id) {
        if(bancho.room.is_host() && slot_number != -1) {
            this->menu->addButton("Set as Host", UA_TRANSFER_HOST);
            this->menu->addButton("Kick", KICK);
        }

        if(has_user_info) {
            // Without user info, we don't have the username
            this->menu->addButton("Start Chat", START_CHAT);

            // XXX: Not implemented
            // menu->addButton("Invite to game", INVITE_TO_GAME);
        }

        if(user_info->is_friend()) {
            this->menu->addButton("Revoke friendship", UA_REMOVE_FRIEND);
        } else {
            this->menu->addButton("Add as friend", UA_ADD_FRIEND);
        }

        // TODO: Disabled for now, since it's not finished
        // if(bancho.spectated_player_id == user_id) {
        //     menu->addButton("Stop spectating", TOGGLE_SPECTATE);
        // } else {
        //     menu->addButton("Spectate", TOGGLE_SPECTATE);
        // }
    }

    this->menu->end(false, false);
    this->menu->setPos(engine->getMouse()->getPos());
    this->menu->setClickCallback(fastdelegate::MakeDelegate(this, &UIUserContextMenuScreen::on_action));
    this->menu->setVisible(true);
}

void UIUserContextMenuScreen::close() { this->menu->setVisible(false); }

void UIUserContextMenuScreen::on_action(UString text, int user_action) {
    auto user_info = get_user_info(this->user_id);
    int slot_number = -1;
    if(bancho.is_in_a_multi_room()) {
        for(int i = 0; i < 16; i++) {
            if(bancho.room.slots[i].player_id == this->user_id) {
                slot_number = i;
                break;
            }
        }
    }

    if(user_action == UA_TRANSFER_HOST) {
        Packet packet;
        packet.id = TRANSFER_HOST;
        write<u32>(&packet, slot_number);
        send_packet(packet);
    } else if(user_action == KICK) {
        Packet packet;
        packet.id = MATCH_LOCK;
        write<u32>(&packet, slot_number);
        send_packet(packet);  // kick by locking the slot
        send_packet(packet);  // unlock the slot
    } else if(user_action == START_CHAT) {
        osu->chat->addChannel(user_info->name, true);
    } else if(user_action == VIEW_PROFILE) {
        auto url = UString::format("https://osu.%s/u/%d", bancho.endpoint.toUtf8(), this->user_id);
        osu->getNotificationOverlay()->addNotification("Opening browser, please wait ...", 0xffffffff, false, 0.75f);
        env->openURLInDefaultBrowser(url.toUtf8());
    } else if(user_action == UA_ADD_FRIEND) {
        Packet packet;
        packet.id = FRIEND_ADD;
        write<u32>(&packet, this->user_id);
        send_packet(packet);
        friends.push_back(this->user_id);
    } else if(user_action == UA_REMOVE_FRIEND) {
        Packet packet;
        packet.id = FRIEND_REMOVE;
        write<u32>(&packet, this->user_id);
        send_packet(packet);

        auto it = std::find(friends.begin(), friends.end(), this->user_id);
        if(it != friends.end()) {
            friends.erase(it);
        }
    } else if(user_action == TOGGLE_SPECTATE) {
        if(bancho.spectated_player_id == this->user_id) {
            stop_spectating();
        } else {
            start_spectating(this->user_id);
        }
    }

    this->menu->setVisible(false);
}

UIUserLabel::UIUserLabel(u32 user_id, UString username) : CBaseUILabel() {
    this->user_id = user_id;
    this->setText(username);
    this->setDrawFrame(false);
    this->setDrawBackground(false);
}

void UIUserLabel::onMouseUpInside() { osu->user_actions->open(this->user_id); }
