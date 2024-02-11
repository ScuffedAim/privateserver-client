#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "Osu.h"
#include "OsuChat.h"
#include "OsuLobby.h"
#include "OsuMainMenu.h"
#include "OsuNotificationOverlay.h"
#include "OsuPromptScreen.h"
#include "OsuRichPresence.h"
#include "OsuUIButton.h"

#include "CBaseUIButton.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "Engine.h"
#include "Keyboard.h"
#include "ResourceManager.h"


RoomUIElement::RoomUIElement(OsuLobby* multi, Room* room, float x, float y, float width, float height) : CBaseUIScrollView(x, y, width, height, "") {
    // NOTE: We can't store the room pointer, since it might expire later
    m_multi = multi;
    room_id = room->id;
    has_password = room->has_password;

    setBlockScrolling(true);
    setDrawFrame(true);

    float title_width = multi->font->getStringWidth(room->name) + 20;
    auto title_ui = new CBaseUILabel(10, 5, title_width, 30, "", room->name);
    title_ui->setDrawFrame(false);
    title_ui->setDrawBackground(false);
    getContainer()->addBaseUIElement(title_ui);

    char player_count_str[256] = {0};
    snprintf(player_count_str, 255, "Players: %d/%d", room->nb_players, room->nb_open_slots);
    float player_count_width = multi->font->getStringWidth(player_count_str) + 20;
    auto slots_ui = new CBaseUILabel(10, 33, player_count_width, 30, "", UString(player_count_str));
    slots_ui->setDrawFrame(false);
    slots_ui->setDrawBackground(false);
    getContainer()->addBaseUIElement(slots_ui);

    join_btn = new OsuUIButton(multi->m_osu, 10, 65, 120, 30, "", "Join room");
    join_btn->setUseDefaultSkin();
    join_btn->setColor(0xff00ff00);
    join_btn->setClickCallback( fastdelegate::MakeDelegate(this, &RoomUIElement::onRoomJoinButtonClick) );
    getContainer()->addBaseUIElement(join_btn);

    // TODO @kiwec: display something to show when the room is passworded
}

void RoomUIElement::onRoomJoinButtonClick(CBaseUIButton* btn) {
    if(has_password) {
        m_multi->room_to_join = room_id;
        m_multi->m_osu->m_prompt->prompt("Room password:", fastdelegate::MakeDelegate(m_multi, &OsuLobby::on_room_join_with_password));
    } else {
        m_multi->joinRoom(room_id, "");
    }
}

OsuLobby::OsuLobby(Osu *osu) : OsuScreen(osu) {
    font = engine->getResourceManager()->getFont("FONT_DEFAULT");

    m_list = new CBaseUIScrollView(0, 0, 0, 0, "");
    m_list->setDrawFrame(false);
    m_list->setDrawBackground(true);
    m_list->setBackgroundColor(0xdd000000);
    m_list->setHorizontalScrolling(false);
    addBaseUIElement(m_list);

    m_noRoomsOpenElement = new CBaseUILabel(0, 0, 0, 0, "", "There are no matches available.");
    m_noRoomsOpenElement->setTextJustification(CBaseUILabel::TEXT_JUSTIFICATION::TEXT_JUSTIFICATION_CENTERED);
    m_noRoomsOpenElement->setSizeToContent(20, 20);
    addBaseUIElement(m_noRoomsOpenElement);

    updateLayout(m_osu->getScreenSize());
}

void OsuLobby::onKeyDown(KeyboardEvent &key) {
    if(!m_bVisible) return;

    if(key.getKeyCode() == KEY_ESCAPE) {
        key.consume();
        setVisible(false);
        m_osu->m_mainMenu->setVisible(true);
        return;
    }

    // XXX: search bar
}

void OsuLobby::onKeyUp(KeyboardEvent &key) {
    if(!m_bVisible) return;

    // XXX: search bar
}

void OsuLobby::onChar(KeyboardEvent &key) {
    if(!m_bVisible) return;

    // XXX: search bar
}

void OsuLobby::onResolutionChange(Vector2 newResolution) {
    updateLayout(newResolution);
}

CBaseUIContainer* OsuLobby::setVisible(bool visible) {
    if(visible == m_bVisible) return this;
    m_bVisible = visible;

    if(visible) {
        Packet packet = {0};
        packet.id = JOIN_ROOM_LIST;
        send_packet(packet);

        packet = {0};
        packet.id = CHANNEL_JOIN;
        write_string(&packet, "#lobby");
        send_packet(packet);

        // LOBBY presence is broken so we send MULTIPLAYER
        OsuRichPresence::setBanchoStatus(m_osu, "Looking to play", MULTIPLAYER);

        // XXX: Could call refreshBeatmaps() here so we load them if not already done so.
        //      Would need to edit it a bit to work outside of songBrowser2, + display loading progress.
        //      Ideally, you'd do this in the background and still be able to browse rooms.
    } else {
        Packet packet = {0};
        packet.id = EXIT_ROOM_LIST;
        send_packet(packet);

        packet = {0};
        packet.id = CHANNEL_PART;
        write_string(&packet, "#lobby");
        send_packet(packet);

        for(auto room : rooms) {
            delete room;
        }
        rooms.clear();
    }

    m_osu->m_chat->updateVisibility();
    return this;
}

void OsuLobby::updateLayout(Vector2 newResolution) {
    m_list->clear();

    setSize(newResolution);
    m_list->setSize(newResolution);

    m_noRoomsOpenElement->setVisible(rooms.empty());
    m_noRoomsOpenElement->setPos(
        newResolution.x / 2 - m_noRoomsOpenElement->getSize().x / 2,
        newResolution.y / 3 - m_noRoomsOpenElement->getSize().y / 2
    );

    auto heading = new CBaseUILabel(50, 30, 300, 40, "", "Multiplayer rooms");
    heading->setFont(m_osu->getTitleFont());
    heading->setSizeToContent(0, 0);
    heading->setDrawFrame(false);
    heading->setDrawBackground(false);
    addBaseUIElement(heading);

    // TODO @kiwec: create room button

    float y = 200;
    const float room_height = 105;
    const float room_width = 600;
    for(auto room : rooms) {
        auto room_ui = new RoomUIElement(this, room, newResolution.x / 2 - (room_width / 2), y, room_width, room_height);
        m_list->getContainer()->addBaseUIElement(room_ui);
        y += room_height + 20;
    }

    m_list->setScrollSizeToContent();
}

void OsuLobby::addRoom(Room* room) {
    rooms.push_back(room);
    updateLayout(getSize());
}

void OsuLobby::joinRoom(uint32_t id, UString password) {
    Packet packet = {0};
    packet.id = JOIN_ROOM;
    write_int(&packet, id);
    write_string(&packet, password.toUtf8());
    send_packet(packet);

    for(CBaseUIElement *elm : m_list->getContainer()->getElements()) {
        auto room = (RoomUIElement*)elm;
        if(room->room_id != id) continue;
        room->join_btn->is_loading = true;
        break;
    }

    m_osu->getNotificationOverlay()->addNotification("Joining room...");
}

void OsuLobby::updateRoom(Room room) {
    for(auto old_room : rooms) {
        if(old_room->id == room.id) {
            *old_room = room;
            updateLayout(getSize());
            return;
        }
    }

    // On bancho.py, if a player creates a room when we're already in the lobby,
    // we won't receive a ROOM_CREATED but only a ROOM_UPDATED packet.
    auto new_room = new Room();
    *new_room = room;
    addRoom(new_room);
}

void OsuLobby::removeRoom(uint32_t room_id) {
    for(auto room : rooms) {
      if(room->id == room_id) {
        auto it = std::find(rooms.begin(), rooms.end(), room);
        rooms.erase(it);
        delete room;
        break;
      }
    }

    updateLayout(getSize());
}

void OsuLobby::on_room_join_with_password(UString password) {
    joinRoom(room_to_join, password);
}

void OsuLobby::on_room_join_failed() {
    // Updating layout will reset is_loading to false
    updateLayout(getSize());
}
