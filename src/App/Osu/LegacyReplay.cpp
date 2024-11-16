#include "LegacyReplay.h"

#ifndef LZMA_API_STATIC
#define LZMA_API_STATIC
#endif
#include <lzma.h>

#include <sstream>
#include <string>

#include "Bancho.h"
#include "BanchoProtocol.h"
#include "Beatmap.h"
#include "Database.h"
#include "Engine.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "SongBrowser/SongBrowser.h"
#include "score.h"

LegacyReplay::BEATMAP_VALUES LegacyReplay::getBeatmapValuesForModsLegacy(int modsLegacy, float legacyAR, float legacyCS,
                                                                         float legacyOD, float legacyHP) {
    LegacyReplay::BEATMAP_VALUES v;

    // HACKHACK: code duplication, see Osu::getDifficultyMultiplier()
    v.difficultyMultiplier = 1.0f;
    {
        if(modsLegacy & LegacyFlags::HardRock) v.difficultyMultiplier = 1.4f;
        if(modsLegacy & LegacyFlags::Easy) v.difficultyMultiplier = 0.5f;
    }

    // HACKHACK: code duplication, see Osu::getCSDifficultyMultiplier()
    v.csDifficultyMultiplier = 1.0f;
    {
        if(modsLegacy & LegacyFlags::HardRock) v.csDifficultyMultiplier = 1.3f;  // different!
        if(modsLegacy & LegacyFlags::Easy) v.csDifficultyMultiplier = 0.5f;
    }

    // apply legacy mods to legacy beatmap values
    v.AR = clamp<float>(legacyAR * v.difficultyMultiplier, 0.0f, 10.0f);
    v.CS = clamp<float>(legacyCS * v.csDifficultyMultiplier, 0.0f, 10.0f);
    v.OD = clamp<float>(legacyOD * v.difficultyMultiplier, 0.0f, 10.0f);
    v.HP = clamp<float>(legacyHP * v.difficultyMultiplier, 0.0f, 10.0f);

    return v;
}

std::vector<LegacyReplay::Frame> LegacyReplay::get_frames(u8* replay_data, i32 replay_size) {
    std::vector<LegacyReplay::Frame> replay_frames;
    if(replay_size <= 0) return replay_frames;

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_alone_decoder(&strm, UINT64_MAX);
    if(ret != LZMA_OK) {
        debugLog("Failed to init lzma library (%d).\n", ret);
        return replay_frames;
    }

    long cur_music_pos = 0;
    u8 outbuf[BUFSIZ];
    Packet output;
    strm.next_in = replay_data;
    strm.avail_in = replay_size;
    do {
        strm.next_out = outbuf;
        strm.avail_out = sizeof(outbuf);

        ret = lzma_code(&strm, LZMA_FINISH);
        if(ret != LZMA_OK && ret != LZMA_STREAM_END) {
            debugLog("Decompression error (%d).\n", ret);
            goto end;
        }

        write_bytes(&output, outbuf, sizeof(outbuf) - strm.avail_out);
    } while(strm.avail_out == 0);
    write<u8>(&output, '\0');

    {
        char* line = (char*)output.memory;
        while(*line) {
            LegacyReplay::Frame frame;

            char* ms = strtok_x('|', &line);
            frame.milliseconds_since_last_frame = strtoul(ms, NULL, 10);

            char* x = strtok_x('|', &line);
            frame.x = strtof(x, NULL);

            char* y = strtok_x('|', &line);
            frame.y = strtof(y, NULL);

            char* flags = strtok_x(',', &line);
            frame.key_flags = strtoul(flags, NULL, 10);

            if(frame.milliseconds_since_last_frame != -12345) {
                cur_music_pos += frame.milliseconds_since_last_frame;
                frame.cur_music_pos = cur_music_pos;
                replay_frames.push_back(frame);
            }
        }
    }

end:
    free(output.memory);
    lzma_end(&strm);
    return replay_frames;
}

void LegacyReplay::compress_frames(const std::vector<LegacyReplay::Frame>& frames, u8** compressed,
                                   size_t* s_compressed) {
    lzma_stream stream = LZMA_STREAM_INIT;
    lzma_options_lzma options;
    lzma_lzma_preset(&options, LZMA_PRESET_DEFAULT);
    lzma_ret ret = lzma_alone_encoder(&stream, &options);
    if(ret != LZMA_OK) {
        debugLog("Failed to initialize lzma encoder: error %d\n", ret);
        *compressed = NULL;
        *s_compressed = 0;
        return;
    }

    std::string replay_string;
    for(auto frame : frames) {
        auto frame_str = UString::format("%lld|%.4f|%.4f|%hhu,", frame.milliseconds_since_last_frame, frame.x, frame.y,
                                         frame.key_flags);
        replay_string.append(frame_str.toUtf8(), frame_str.lengthUtf8());
    }

    // osu!stable doesn't consider a replay valid unless it ends with this
    replay_string.append("-12345|0.0000|0.0000|0,");

    *s_compressed = replay_string.length();
    *compressed = (u8*)malloc(*s_compressed);

    stream.avail_in = replay_string.length();
    stream.next_in = (const u8*)replay_string.c_str();
    stream.avail_out = *s_compressed;
    stream.next_out = *compressed;
    do {
        ret = lzma_code(&stream, LZMA_FINISH);
        if(ret == LZMA_OK) {
            *s_compressed *= 2;
            *compressed = (u8*)realloc(*compressed, *s_compressed);
            stream.avail_out = *s_compressed - stream.total_out;
            stream.next_out = *compressed + stream.total_out;
        } else if(ret != LZMA_STREAM_END) {
            debugLog("Error while compressing replay: error %d\n", ret);
            *compressed = NULL;
            *s_compressed = 0;
            lzma_end(&stream);
            return;
        }
    } while(ret != LZMA_STREAM_END);

    *s_compressed = stream.total_out;
    lzma_end(&stream);
}

LegacyReplay::Info LegacyReplay::from_bytes(u8* data, int s_data) {
    LegacyReplay::Info info;

    Packet replay;
    replay.memory = data;
    replay.size = s_data;

    info.gamemode = read<u8>(&replay);
    if(info.gamemode != 0) {
        debugLog("Replay has unexpected gamemode %d!", info.gamemode);
        return info;
    }

    info.osu_version = read<u32>(&replay);
    info.diff2_md5 = read_string(&replay);
    info.username = read_string(&replay);
    info.replay_md5 = read_string(&replay);
    info.num300s = read<u16>(&replay);
    info.num100s = read<u16>(&replay);
    info.num50s = read<u16>(&replay);
    info.numGekis = read<u16>(&replay);
    info.numKatus = read<u16>(&replay);
    info.numMisses = read<u16>(&replay);
    info.score = read<u32>(&replay);
    info.comboMax = read<u16>(&replay);
    info.perfect = read<u8>(&replay);
    info.mod_flags = read<u32>(&replay);
    info.life_bar_graph = read_string(&replay);
    info.timestamp = read<u64>(&replay) / 10;

    i32 replay_size = read<u32>(&replay);
    if(replay_size <= 0) return info;
    auto replay_data = new u8[replay_size];
    read_bytes(&replay, replay_data, replay_size);
    info.frames = LegacyReplay::get_frames(replay_data, replay_size);
    delete[] replay_data;

    return info;
}

bool LegacyReplay::load_from_disk(FinishedScore* score, bool update_db) {
    if(score->peppy_replay_tms > 0) {
        auto osu_folder = cv_osu_folder.getString();
        auto path = UString::format("%s/Data/r/%s-%llu.osr", osu_folder.toUtf8(), score->beatmap_hash.hash,
                                    score->peppy_replay_tms);

        FILE* replay_file = fopen(path.toUtf8(), "rb");
        if(replay_file == NULL) return false;

        fseek(replay_file, 0, SEEK_END);
        size_t s_full_replay = ftell(replay_file);
        rewind(replay_file);

        u8* full_replay = new u8[s_full_replay];
        fread(full_replay, s_full_replay, 1, replay_file);
        fclose(replay_file);
        auto info = LegacyReplay::from_bytes(full_replay, s_full_replay);
        score->replay = info.frames;
        delete[] full_replay;
    } else {
        auto path = UString::format(MCENGINE_DATA_DIR "replays/%s/%llu.replay.lzma", score->server.c_str(),
                                    score->unixTimestamp);

        FILE* replay_file = fopen(path.toUtf8(), "rb");
        if(replay_file == NULL) return false;

        fseek(replay_file, 0, SEEK_END);
        size_t s_compressed_replay = ftell(replay_file);
        rewind(replay_file);

        u8* compressed_replay = new u8[s_compressed_replay];
        fread(compressed_replay, s_compressed_replay, 1, replay_file);
        fclose(replay_file);
        score->replay = LegacyReplay::get_frames(compressed_replay, s_compressed_replay);
        delete[] compressed_replay;
    }

    if(update_db) {
        std::lock_guard<std::mutex> lock(db->m_scores_mtx);
        auto& map_scores = (*(db->getScores()))[score->beatmap_hash];
        for(auto& db_score : map_scores) {
            if(db_score.unixTimestamp != score->unixTimestamp) continue;
            if(&db_score != score) {
                db_score.replay = score->replay;
            }

            break;
        }
    }

    return true;
}

void LegacyReplay::load_and_watch(FinishedScore score) {
    // Check if replay is loaded
    if(score.replay.empty()) {
        if(!load_from_disk(&score, true)) {
            // @neonet: try loading replay from neonet

            if(strcmp(score.server.c_str(), bancho.endpoint.toUtf8()) != 0) {
                auto msg = UString::format("Please connect to %s to view this replay!", score.server.c_str());
                osu->m_notificationOverlay->addToast(msg);
            }

            // Need to allocate with calloc since APIRequests free() the .extra
            void* mem = calloc(1, sizeof(FinishedScore));
            FinishedScore* score_cpy = new(mem) FinishedScore;
            *score_cpy = score;

            APIRequest request;
            request.type = GET_REPLAY;
            request.path = UString::format("/web/osu-getreplay.php?u=%s&h=%s&m=0&c=%d", bancho.username.toUtf8(),
                                           bancho.pw_md5.toUtf8(), score.bancho_score_id);
            request.mime = NULL;
            request.extra = (u8*)score_cpy;
            send_api_request(request);

            osu->m_notificationOverlay->addNotification("Downloading replay...");
            return;
        }
    }

    // We tried loading from memory, we tried loading from file, we tried loading from server... RIP
    if(score.replay.empty()) {
        osu->m_notificationOverlay->addToast("Failed to load replay");
        return;
    }

    auto diff = db->getBeatmapDifficulty(score.beatmap_hash);
    if(diff == NULL) {
        // XXX: Auto-download beatmap
        osu->m_notificationOverlay->addToast("Missing beatmap for this replay");
    } else {
        osu->getSongBrowser()->onDifficultySelected(diff, false);
        osu->getSongBrowser()->selectSelectedBeatmapSongButton();
        osu->getSelectedBeatmap()->watch(score, 0.f);
    }
}
