#pragma once
#include <unordered_map>

#include "BanchoProtocol.h"
#include "UString.h"

class Image;

enum class ServerPolicy {
    NO,
    YES,
    NO_PREFERENCE,
};

struct Bancho {
    UString neosu_version;

    UString endpoint;
    i32 user_id = 0;
    UString username;
    MD5Hash pw_md5;
    Room room;

    i32 spectated_player_id = 0;
    std::vector<u32> spectators;
    std::vector<u32> fellow_spectators;

    UString server_icon_url;
    Image *server_icon = NULL;

    ServerPolicy score_submission_policy = ServerPolicy::YES;
    bool submit_scores();

    UString user_agent;
    UString client_hashes;
    UString disk_uuid;
    UString install_id;

    bool is_online() { return this->user_id > 0; }

    // Room ID can be 0 on private servers! So we check if the room has players instead.
    bool is_in_a_multi_room() { return this->room.nb_players > 0; }

    bool match_started = false;
    bool is_playing_a_multi_map() { return this->match_started; }

    Slot last_scores[16];
};

struct Channel {
    UString name;
    UString topic;
    u32 nb_members;
};

MD5Hash md5(u8 *msg, size_t msg_len);

void handle_packet(Packet *packet);

Packet build_login_packet();

extern Bancho bancho;
extern std::unordered_map<std::string, Channel *> chat_channels;
