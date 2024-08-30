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
#include "score.h"

const UString RichPresence::KEY_STEAM_STATUS = "status";
const UString RichPresence::KEY_DISCORD_STATUS = "state";
const UString RichPresence::KEY_DISCORD_DETAILS = "details";

UString last_status = "[neosu]\nWaking up";
Action last_action = IDLE;

void RichPresence::setBanchoStatus(const char *info_text, Action action) {
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
    snprintf(fancy_text, 1023, "[neosu]\n%s", info_text);

    last_status = fancy_text;
    last_action = action;

    Packet packet;
    packet.id = CHANGE_ACTION;
    write<u8>(&packet, action);
    write_string(&packet, fancy_text);
    write_string(&packet, map_md5.hash);
    write<u32>(&packet, osu->m_modSelector->getModFlags());
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
    write<u32>(&packet, osu->m_modSelector->getModFlags());
    write<u8>(&packet, 0);  // osu!std
    write<u32>(&packet, map_id);
    send_packet(packet);

    // Servers like akatsuki send different leaderboards based on what mods
    // you have selected. Reset leaderboard when switching mods.
    osu->m_songBrowser2->m_db->m_online_scores.clear();
    osu->m_songBrowser2->rebuildScoreButtons();
}

void RichPresence::onMainMenu() {
    setStatus("Main Menu");
    setBanchoStatus("Main Menu", AFK);
}

void RichPresence::onSongBrowser() {
    setStatus("Song Selection");

    if(osu->m_room->isVisible()) {
        setBanchoStatus("Picking a map", MULTIPLAYER);
    } else {
        setBanchoStatus("Song selection", IDLE);
    }

    // also update window title
    if(cv_rich_presence_dynamic_windowtitle.getBool()) env->setWindowTitle("neosu");
}

void RichPresence::onPlayStart() {
    UString playingInfo /*= "Playing "*/;
    playingInfo.append(osu->getSelectedBeatmap()->getSelectedDifficulty2()->getArtist().c_str());
    playingInfo.append(" - ");
    playingInfo.append(osu->getSelectedBeatmap()->getSelectedDifficulty2()->getTitle().c_str());
    playingInfo.append(" [");
    playingInfo.append(osu->getSelectedBeatmap()->getSelectedDifficulty2()->getDifficultyName().c_str());
    playingInfo.append("]");

    setStatus(playingInfo);
    setBanchoStatus(playingInfo.toUtf8(), bancho.is_in_a_multi_room() ? MULTIPLAYER : PLAYING);

    // also update window title
    if(cv_rich_presence_dynamic_windowtitle.getBool()) {
        UString windowTitle = UString(playingInfo);
        windowTitle.insert(0, "neosu - ");
        env->setWindowTitle(windowTitle);
    }
}

void RichPresence::onPlayEnd(bool quit) {
    if(!quit && cv_rich_presence_show_recentplaystats.getBool()) {
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

        setStatus(scoreInfo);
        setBanchoStatus(scoreInfo.toUtf8(), SUBMITTING);
    }
}

void RichPresence::setStatus(UString status, bool force) {
    if(!cv_rich_presence.getBool() && !force) return;

    // discord
    discord->setRichPresence("largeImageKey", "logo_512", true);
    discord->setRichPresence("smallImageKey", "logo_discord_512_blackfill", true);
    discord->setRichPresence("largeImageText",
                             cv_rich_presence_discord_show_totalpp.getBool()
                                 ? "Top = Status / Recent Play; Bottom = Total weighted pp (neosu scores only!)"
                                 : "",
                             true);
    discord->setRichPresence("smallImageText",
                             cv_rich_presence_discord_show_totalpp.getBool()
                                 ? "Total weighted pp only work after the database has been loaded!"
                                 : "",
                             true);
    discord->setRichPresence(KEY_DISCORD_DETAILS, status);

    if(osu->getSongBrowser() != NULL) {
        if(cv_rich_presence_discord_show_totalpp.getBool()) {
            const int ppRounded =
                (int)(std::round(osu->getSongBrowser()->getDatabase()->calculatePlayerStats(cv_name.getString()).pp));
            if(ppRounded > 0) discord->setRichPresence(KEY_DISCORD_STATUS, UString::format("%ipp (Mc)", ppRounded));
        }
    } else if(force && status.length() < 1)
        discord->setRichPresence(KEY_DISCORD_STATUS, "");
}

void RichPresence::onRichPresenceChange(UString oldValue, UString newValue) {
    if(!cv_rich_presence.getBool())
        onRichPresenceDisable();
    else
        onRichPresenceEnable();
}

void RichPresence::onRichPresenceEnable() { setStatus("..."); }

void RichPresence::onRichPresenceDisable() { setStatus("", true); }
