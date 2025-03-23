#include "RichPresence.h"

#include "Bancho.h"
#include "BanchoNetworking.h"
#include "Beatmap.h"
#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "DiscordInterface.h"
#include "Engine.h"
#include "Environment.h"
#include "ModSelector.h"
#include "Osu.h"
#include "RoomScreen.h"
#include "SongBrowser/SongBrowser.h"
#include "Sound.h"
#include "score.h"

const UString RichPresence::KEY_DISCORD_STATUS = "state";
const UString RichPresence::KEY_DISCORD_DETAILS = "details";

UString last_status = "[scuffedaim]\nWaking up";
Action last_action = IDLE;

void crop_to(UString str, char* output, int max_len) {
    if(str.lengthUtf8() < max_len) {
        strcpy(output, str.toUtf8());
    } else {
        strncpy(output, str.toUtf8(), max_len - 4);
        output[max_len - 4] = '.';
        output[max_len - 3] = '.';
        output[max_len - 2] = '.';
        output[max_len - 1] = '\0';
    }
}

// output is assumed to be a char[128] string
void diff2str(DatabaseBeatmap* diff2, char* output, bool include_difficulty) {
    if(diff2 == NULL) {
        strcpy(output, "No map selected");
        return;
    }

    UString playingInfo;
    playingInfo.append(diff2->getArtist().c_str());
    playingInfo.append(" - ");
    playingInfo.append(diff2->getTitle().c_str());

    auto diffStr = UString::format(" [%s]", diff2->getDifficultyName().c_str());
    if(playingInfo.lengthUtf8() + diffStr.lengthUtf8() < 128) {
        playingInfo.append(diffStr);
    }

    crop_to(playingInfo, output, 128);
}

void RichPresence::setBanchoStatus(const char* info_text, Action action) {
    if(osu == NULL) return;

    MD5Hash map_md5("");
    u32 map_id = 0;

    auto selected_beatmap = osu->getSelectedBeatmap();
    if(selected_beatmap != NULL) {
        auto diff = selected_beatmap->getSelectedDifficulty2();
        if(diff != NULL) {
            map_md5 = diff->getMD5Hash();
            map_id = diff->getID();
        }
    }

    char fancy_text[1024] = {0};
    snprintf(fancy_text, 1023, "[scuffedaim]\n%s", info_text);

    last_status = fancy_text;
    last_action = action;

    Packet packet;
    packet.id = CHANGE_ACTION;
    write<u8>(&packet, action);
    write_string(&packet, fancy_text);
    write_string(&packet, map_md5.hash);
    write<u32>(&packet, osu->modSelector->getModFlags());
    write<u8>(&packet, 0);  // osu!std
    write<u32>(&packet, map_id);
    send_packet(packet);
}

void RichPresence::updateBanchoMods() {
    MD5Hash map_md5("");
    u32 map_id = 0;

    auto selected_beatmap = osu->getSelectedBeatmap();
    if(selected_beatmap != NULL) {
        auto diff = selected_beatmap->getSelectedDifficulty2();
        if(diff != NULL) {
            map_md5 = diff->getMD5Hash();
            map_id = diff->getID();
        }
    }

    Packet packet;
    packet.id = CHANGE_ACTION;
    write<u8>(&packet, last_action);
    write_string(&packet, last_status.toUtf8());
    write_string(&packet, map_md5.hash);
    write<u32>(&packet, osu->modSelector->getModFlags());
    write<u8>(&packet, 0);  // osu!std
    write<u32>(&packet, map_id);
    send_packet(packet);

    // Servers like akatsuki send different leaderboards based on what mods
    // you have selected. Reset leaderboard when switching mods.
    db->online_scores.clear();
    osu->songBrowser2->rebuildScoreButtons();
}

void RichPresence::onMainMenu() {
    setBanchoStatus("Main Menu", AFK);

    // NOTE: As much as I would like to show "Listening to", the Discord SDK ignores the activity 'type'
    struct DiscordActivity activity;
    memset(&activity, 0, sizeof(activity));
    activity.type = DiscordActivityType_Listening;

    auto diff2 = osu->getSelectedBeatmap()->getSelectedDifficulty2();
    auto music = osu->getSelectedBeatmap()->getMusic();
    bool listening = diff2 != NULL && music != NULL && music->isPlaying();
    if(listening) {
        diff2str(diff2, activity.details, false);
    }

    strcpy(activity.state, "Main menu");
    set_discord_presence(&activity);
}

void RichPresence::onSongBrowser() {
    struct DiscordActivity activity;
    memset(&activity, 0, sizeof(activity));
    activity.type = DiscordActivityType_Playing;
    strcpy(activity.details, "Picking a map");

    if(osu->room->isVisible()) {
        setBanchoStatus("Picking a map", MULTIPLAYER);

        strcpy(activity.state, "Multiplayer");
        activity.party.size.current_size = bancho.room.nb_players;
        activity.party.size.max_size = bancho.room.nb_open_slots;
    } else {
        setBanchoStatus("Song selection", IDLE);

        strcpy(activity.state, "Singleplayer");
        activity.party.size.current_size = 0;
        activity.party.size.max_size = 0;
    }

    set_discord_presence(&activity);
    env->setWindowTitle("ScuffedAim");
}

void RichPresence::onPlayStart() {
    auto diff2 = osu->getSelectedBeatmap()->getSelectedDifficulty2();

    static DatabaseBeatmap* last_diff = NULL;
    static int64_t tms = 0;
    if(tms == 0 || last_diff != diff2) {
        last_diff = diff2;
        tms = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count();
    }

    struct DiscordActivity activity;
    memset(&activity, 0, sizeof(activity));
    activity.type = DiscordActivityType_Playing;
    activity.timestamps.start = tms;
    activity.timestamps.end = 0;

    diff2str(diff2, activity.details, true);

    if(bancho.is_in_a_multi_room()) {
        setBanchoStatus(activity.details, MULTIPLAYER);

        strcpy(activity.state, "Playing in a lobby");
        activity.party.size.current_size = bancho.room.nb_players;
        activity.party.size.max_size = bancho.room.nb_open_slots;
    } else {
        setBanchoStatus(activity.details, PLAYING);

        strcpy(activity.state, "Playing Solo");
        activity.party.size.current_size = 0;
        activity.party.size.max_size = 0;
    }

    // also update window title
    auto windowTitle = UString::format("scuffedaim - %s", activity.details);
    env->setWindowTitle(windowTitle);

    set_discord_presence(&activity);
}

void RichPresence::onPlayEnd(bool quit) {
    if(quit) return;

    // e.g.: 230pp 900x 95.50% HDHRDT 6*

    // pp
    UString scoreInfo = UString::format("%ipp", (int)(std::round(osu->getScore()->getPPv2())));

    // max combo
    scoreInfo.append(UString::format(" %ix", osu->getScore()->getComboMax()));

    // accuracy
    scoreInfo.append(UString::format(" %.2f%%", osu->getScore()->getAccuracy() * 100.0f));

    // mods
    UString mods = osu->getScore()->getModsStringForRichPresence();
    if(mods.length() > 0) {
        scoreInfo.append(" ");
        scoreInfo.append(mods);
    }

    // stars
    scoreInfo.append(UString::format(" %.2f*", osu->getScore()->getStarsTomTotal()));

    setBanchoStatus(scoreInfo.toUtf8(), SUBMITTING);
}

void RichPresence::onMultiplayerLobby() {
    struct DiscordActivity activity;
    memset(&activity, 0, sizeof(activity));
    activity.type = DiscordActivityType_Playing;

    crop_to(bancho.endpoint.toUtf8(), activity.state, 128);
    crop_to(bancho.room.name.toUtf8(), activity.details, 128);
    activity.party.size.current_size = bancho.room.nb_players;
    activity.party.size.max_size = bancho.room.nb_open_slots;

    set_discord_presence(&activity);
}

void RichPresence::onRichPresenceChange(UString oldValue, UString newValue) {
    if(!cv_rich_presence.getBool()) {
        clear_discord_presence();
    }
}
