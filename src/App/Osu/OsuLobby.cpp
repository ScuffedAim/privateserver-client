#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "Osu.h"
#include "OsuChat.h"
#include "OsuLobby.h"
#include "OsuMainMenu.h"
#include "OsuNotificationOverlay.h"
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

    auto btn = new OsuUIButton(multi->m_osu, 10, 65, 120, 30, "", "Join room");
    btn->setUseDefaultSkin();
    btn->setColor(0xff00ff00);
    btn->setClickCallback( fastdelegate::MakeDelegate(this, &RoomUIElement::onRoomJoinButtonClick) );
    getContainer()->addBaseUIElement(btn);
}

void RoomUIElement::onRoomJoinButtonClick(CBaseUIButton* btn) {
    Packet packet = {0};
    packet.id = JOIN_ROOM;
    write_int(&packet, room_id);
    write_string(&packet, ""); // TODO @kiwec: password support
    send_packet(packet);

    // TODO @kiwec: disable button, show loading indicator
    m_multi->m_osu->getNotificationOverlay()->addNotification("Joining room...");
}


OsuLobby::OsuLobby(Osu *osu) : OsuScreen(osu) {
    font = engine->getResourceManager()->getFont("FONT_DEFAULT");

    m_container = new CBaseUIContainer(0, 0, 0, 0, "");

    m_list = new CBaseUIScrollView(0, 0, 0, 0, "");
    m_list->setDrawFrame(false);
    m_list->setDrawBackground(true);
    m_list->setBackgroundColor(0xdd000000);
    m_list->setHorizontalScrolling(false);
    m_container->addBaseUIElement(m_list);

    updateLayout(m_osu->getScreenSize());
}

void OsuLobby::draw(Graphics *g) {
    if (!m_bVisible) return;

    m_container->draw(g);
}

void OsuLobby::update() {
    if (!m_bVisible) return;
    m_container->update();
}

void OsuLobby::onKeyDown(KeyboardEvent &key) {
    if(!m_bVisible) return;

    if(key.getKeyCode() == KEY_ESCAPE) {
        key.consume();
        setVisible(false);
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

void OsuLobby::setVisible(bool visible) {
    if(visible == m_bVisible) return;
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

        m_osu->m_mainMenu->setVisible(true);
    }

    m_osu->m_chat->updateVisibility();
}

void OsuLobby::updateLayout(Vector2 newResolution) {
    m_list->clear();

    m_container->setSize(newResolution);
    m_list->setSize(newResolution);


    auto heading = new CBaseUILabel(50, 30, 300, 40, "", "Multiplayer rooms");
    heading->setFont(m_osu->getTitleFont());
    heading->setSizeToContent(0, 0);
    heading->setDrawFrame(false);
    heading->setDrawBackground(false);
    m_list->getContainer()->addBaseUIElement(heading);

    // TODO @kiwec: display something when no rooms exist
    // TODO @kiwec: create room button
    // TODO @kiwec: back to main menu button

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
    updateLayout(m_container->getSize());
}

void OsuLobby::updateRoom(Room room) {
    for(auto old_room : rooms) {
        if(old_room->id == room.id) {
            *old_room = room;
            updateLayout(m_container->getSize());

            // TODO @kiwec: if we're in this room, we should send MATCH_HAS_BEATMAP / MATCH_NO_BEATMAP
            // TODO @kiwec: especially important, send MATCH_NO_BEATMAP if the gamemode isn't osu!std

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

    updateLayout(m_container->getSize());
}

void OsuLobby::on_room_join_failed() {
    // TODO @kiwec
}
