#pragma once
#include "ModFlags.h"
#include "cbase.h"

struct FinishedScore;

namespace LegacyReplay {
struct Frame {
    long long int cur_music_pos;
    long long int milliseconds_since_last_frame;

    float x;  // 0 - 512
    float y;  // 0 - 384

    u8 key_flags;
};

enum KeyFlags {
    M1 = 1,
    M2 = 2,
    K1 = 4,
    K2 = 8,
    Smoke = 16,
};

struct BEATMAP_VALUES {
    float AR;
    float CS;
    float OD;
    float HP;

    float difficultyMultiplier;
    float csDifficultyMultiplier;
};

struct Info {
    u8 gamemode;
    u32 osu_version;
    UString diff2_md5;
    UString username;
    UString replay_md5;
    int num300s;
    int num100s;
    int num50s;
    int numGekis;
    int numKatus;
    int numMisses;
    i32 score;
    int comboMax;
    bool perfect;
    i32 mod_flags;
    UString life_bar_graph;
    i64 timestamp;
    std::vector<Frame> frames;
};

BEATMAP_VALUES getBeatmapValuesForModsLegacy(int modsLegacy, float legacyAR, float legacyCS, float legacyOD,
                                             float legacyHP);

Info from_bytes(u8* data, int s_data);
std::vector<Frame> get_frames(u8* replay_data, i32 replay_size);
void compress_frames(const std::vector<Frame>& frames, u8** compressed, size_t* s_compressed);
bool load_from_disk(FinishedScore* score);
void load_and_watch(FinishedScore score);

}  // namespace LegacyReplay
