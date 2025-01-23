#pragma once
#include <atomic>
#include <future>

#include "DifficultyCalculator.h"
#include "Osu.h"
#include "Overrides.h"
#include "Resource.h"

using namespace std;

class BeatmapInterface;
class HitObject;

class Database;

class BackgroundImageHandler;

// purpose:
// 1) contain all infos which are ALWAYS kept in memory for beatmaps
// 2) be the data source for Beatmap when starting a difficulty
// 3) allow async calculations/loaders to work on the contained data (e.g. background image loader)
// 4) be a container for difficulties (all top level DatabaseBeatmap objects are containers)

class DatabaseBeatmap;
typedef DatabaseBeatmap BeatmapDifficulty;
typedef DatabaseBeatmap BeatmapSet;

class DatabaseBeatmap {
   public:
    // raw structs

    struct TIMINGPOINT {
        double offset;
        double msPerBeat;

        int sampleType;
        int sampleSet;
        int volume;

        bool timingChange;
        bool kiai;
    };

    struct BREAK {
        i64 startTime;
        i64 endTime;
    };

    // custom structs

    struct LOAD_DIFFOBJ_RESULT {
        int errorCode;

        std::vector<OsuDifficultyHitObject> diffobjects;

        int maxPossibleCombo;

        LOAD_DIFFOBJ_RESULT() {
            this->errorCode = 0;

            this->maxPossibleCombo = 0;
        }
    };

    struct LOAD_GAMEPLAY_RESULT {
        int errorCode;

        std::vector<HitObject *> hitobjects;
        std::vector<BREAK> breaks;
        std::vector<Color> combocolors;

        LOAD_GAMEPLAY_RESULT() { this->errorCode = 0; }
    };

    struct TIMING_INFO {
        long offset;

        float beatLengthBase;
        float beatLength;

        float volume;
        int sampleType;
        int sampleSet;

        bool isNaN;
    };

    enum class BeatmapType {
        NEOSU_BEATMAPSET,
        PEPPY_BEATMAPSET,
        NEOSU_DIFFICULTY,
        PEPPY_DIFFICULTY,
    };

    // primitive objects

    struct HITCIRCLE {
        int x, y;
        u32 time;
        int sampleType;
        int number;
        int colorCounter;
        int colorOffset;
        bool clicked;
    };

    struct SLIDER {
        int x, y;
        char type;
        int repeat;
        float pixelLength;
        u32 time;
        int sampleType;
        int number;
        int colorCounter;
        int colorOffset;
        std::vector<Vector2> points;
        std::vector<int> hitSounds;

        float sliderTime;
        float sliderTimeWithoutRepeats;
        std::vector<float> ticks;

        std::vector<OsuDifficultyHitObject::SLIDER_SCORING_TIME> scoringTimesForStarCalc;
    };

    struct SPINNER {
        int x, y;
        u32 time;
        int sampleType;
        u32 endTime;
    };

    struct PRIMITIVE_CONTAINER {
        int errorCode;

        std::vector<HITCIRCLE> hitcircles;
        std::vector<SLIDER> sliders;
        std::vector<SPINNER> spinners;
        std::vector<BREAK> breaks;

        zarray<TIMINGPOINT> timingpoints;
        std::vector<Color> combocolors;

        float stackLeniency;

        float sliderMultiplier;
        float sliderTickRate;

        u32 numCircles;
        u32 numSliders;
        u32 numSpinners;
        u32 numHitobjects;

        int version;
    };

    DatabaseBeatmap(std::string filePath, std::string folder, BeatmapType type);
    DatabaseBeatmap(std::vector<DatabaseBeatmap *> *difficulties, BeatmapType type);
    ~DatabaseBeatmap();

    static LOAD_DIFFOBJ_RESULT loadDifficultyHitObjects(const std::string osuFilePath, float AR, float CS,
                                                        float speedMultiplier, bool calculateStarsInaccurately = false);
    static LOAD_DIFFOBJ_RESULT loadDifficultyHitObjects(const std::string osuFilePath, float AR, float CS,
                                                        float speedMultiplier, bool calculateStarsInaccurately,
                                                        const std::atomic<bool> &dead);
    static LOAD_DIFFOBJ_RESULT loadDifficultyHitObjects(PRIMITIVE_CONTAINER &c, float AR, float CS,
                                                        float speedMultiplier, bool calculateStarsInaccurately,
                                                        const std::atomic<bool> &dead);
    bool loadMetadata(bool compute_md5 = true);
    static LOAD_GAMEPLAY_RESULT loadGameplay(DatabaseBeatmap *databaseBeatmap, BeatmapInterface *beatmap);
    MapOverrides get_overrides();
    void update_overrides();

    void setLocalOffset(long localOffset) {
        this->iLocalOffset = localOffset;
        this->update_overrides();
    }

    void setOnlineOffset(long onlineOffset) {
        this->iOnlineOffset = onlineOffset;
        this->update_overrides();
    }

    std::string sFolder;    // path to folder containing .osu file (e.g. "/path/to/beatmapfolder/")
    std::string sFilePath;  // path to .osu file (e.g. "/path/to/beatmapfolder/beatmap.osu")
    inline std::string getFolder() const { return this->sFolder; }
    inline std::string getFilePath() const { return this->sFilePath; }

    inline const std::vector<DatabaseBeatmap *> &getDifficulties() const {
        static std::vector<DatabaseBeatmap *> empty;
        return this->difficulties == NULL ? empty : *this->difficulties;
    }

    inline const MD5Hash &getMD5Hash() const { return this->sMD5Hash; }

    TIMING_INFO getTimingInfoForTime(unsigned long positionMS);
    static TIMING_INFO getTimingInfoForTimeAndTimingPoints(unsigned long positionMS,
                                                           const zarray<TIMINGPOINT> &timingpoints);

    // raw metadata

    inline int getVersion() const { return this->iVersion; }
    inline int getGameMode() const { return this->iGameMode; }
    inline int getID() const { return this->iID; }
    inline int getSetID() const { return this->iSetID; }

    inline const std::string &getTitle() const { return this->sTitle; }
    inline const std::string &getArtist() const { return this->sArtist; }
    inline const std::string &getCreator() const { return this->sCreator; }
    inline const std::string &getDifficultyName() const { return this->sDifficultyName; }
    inline const std::string &getSource() const { return this->sSource; }
    inline const std::string &getTags() const { return this->sTags; }
    inline const std::string &getBackgroundImageFileName() const { return this->sBackgroundImageFileName; }
    inline const std::string &getAudioFileName() const { return this->sAudioFileName; }

    inline unsigned long getLengthMS() const { return this->iLengthMS; }
    inline int getPreviewTime() const { return this->iPreviewTime; }

    inline float getAR() const { return this->fAR; }
    inline float getCS() const { return this->fCS; }
    inline float getHP() const { return this->fHP; }
    inline float getOD() const { return this->fOD; }

    inline float getStackLeniency() const { return this->fStackLeniency; }
    inline float getSliderTickRate() const { return this->fSliderTickRate; }
    inline float getSliderMultiplier() const { return this->fSliderMultiplier; }

    inline const zarray<TIMINGPOINT> &getTimingpoints() const { return this->timingpoints; }

    std::string getFullSoundFilePath();

    // redundant data
    inline const std::string &getFullBackgroundImageFilePath() const { return this->sFullBackgroundImageFilePath; }

    // precomputed data

    inline float getStarsNomod() const { return this->fStarsNomod; }

    inline int getMinBPM() const { return this->iMinBPM; }
    inline int getMaxBPM() const { return this->iMaxBPM; }
    inline int getMostCommonBPM() const { return this->iMostCommonBPM; }

    inline int getNumObjects() const { return this->iNumObjects; }
    inline int getNumCircles() const { return this->iNumCircles; }
    inline int getNumSliders() const { return this->iNumSliders; }
    inline int getNumSpinners() const { return this->iNumSpinners; }

    // custom data

    i64 last_modification_time = 0;

    inline long getLocalOffset() const { return this->iLocalOffset; }
    inline long getOnlineOffset() const { return this->iOnlineOffset; }

    bool draw_background = true;
    bool do_not_store = false;

    // song select mod-adjusted pp/stars
    pp_info pp;

    // raw metadata

    int iVersion;   // e.g. "osu file format v12" -> 12
    int iGameMode;  // 0 = osu!standard, 1 = Taiko, 2 = Catch the Beat, 3 = osu!mania
    long iID;       // online ID, if uploaded
    int iSetID;     // online set ID, if uploaded

    std::string sTitle;
    std::string sArtist;
    std::string sCreator;
    std::string sDifficultyName;  // difficulty name ("Version")
    std::string sSource;          // only used by search
    std::string sTags;            // only used by search
    std::string sBackgroundImageFileName;
    std::string sAudioFileName;

    unsigned long iLengthMS;
    int iPreviewTime;

    float fAR;
    float fCS;
    float fHP;
    float fOD;

    float fStackLeniency;
    float fSliderTickRate;
    float fSliderMultiplier;

    zarray<TIMINGPOINT> timingpoints;  // necessary for main menu anim

    // redundant data (technically contained in metadata, but precomputed anyway)

    std::string sFullSoundFilePath;
    std::string sFullBackgroundImageFilePath;

    // precomputed data (can-run-without-but-nice-to-have data)

    float fStarsNomod;

    int iMinBPM = 0;
    int iMaxBPM = 0;
    int iMostCommonBPM = 0;

    int iNumObjects;
    int iNumCircles;
    int iNumSliders;
    int iNumSpinners;

    // custom data (not necessary, not part of the beatmap file, and not precomputed)

    long iLocalOffset;
    long iOnlineOffset;
    std::atomic<f32> loudness = 0.f;

    struct CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT {
        int errorCode;
    };

    // class internal data (custom)

    friend class Database;
    friend class BackgroundImageHandler;

    static PRIMITIVE_CONTAINER loadPrimitiveObjects(const std::string osuFilePath);
    static PRIMITIVE_CONTAINER loadPrimitiveObjects(const std::string osuFilePath, const std::atomic<bool> &dead);
    static CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT calculateSliderTimesClicksTicks(int beatmapVersion,
                                                                                      std::vector<SLIDER> &sliders,
                                                                                      zarray<TIMINGPOINT> &timingpoints,
                                                                                      float sliderMultiplier,
                                                                                      float sliderTickRate);
    static CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT calculateSliderTimesClicksTicks(
        int beatmapVersion, std::vector<SLIDER> &sliders, zarray<TIMINGPOINT> &timingpoints, float sliderMultiplier,
        float sliderTickRate, const std::atomic<bool> &dead);

    std::vector<DatabaseBeatmap *> *difficulties = NULL;
    BeatmapType type;

    MD5Hash sMD5Hash;

    // helper functions

    struct TimingPointSortComparator {
        bool operator()(DatabaseBeatmap::TIMINGPOINT const &a, DatabaseBeatmap::TIMINGPOINT const &b) const {
            if(a.offset != b.offset) return a.offset < b.offset;

            // non-inherited timingpoints go before inherited timingpoints
            bool a_inherited = a.msPerBeat >= 0;
            bool b_inherited = b.msPerBeat >= 0;
            if(a_inherited != b_inherited) return a_inherited;

            return &a < &b;
        }
    };
};

class DatabaseBeatmapBackgroundImagePathLoader : public Resource {
   public:
    DatabaseBeatmapBackgroundImagePathLoader(const std::string &filePath);

    inline const std::string &getLoadedBackgroundImageFileName() const { return this->sLoadedBackgroundImageFileName; }

   private:
    virtual void init();
    virtual void initAsync();
    virtual void destroy() { ; }

    std::string sFilePath;
    std::string sLoadedBackgroundImageFileName;
};

struct BPMInfo {
    i32 min;
    i32 max;
    i32 most_common;
};

template <typename T>
struct BPMInfo getBPM(const zarray<T> &timing_points) {
    if(timing_points.empty()) {
        return BPMInfo{
            .min = 0,
            .max = 0,
            .most_common = 0,
        };
    }

    struct Tuple {
        i32 bpm;
        double duration;
    };

    zarray<Tuple> bpms;
    bpms.reserve(timing_points.size());

    double lastTime = timing_points[timing_points.size() - 1].offset;
    for(size_t i = 0; i < timing_points.size(); i++) {
        const T &t = timing_points[i];
        if(t.offset > lastTime) continue;
        if(t.msPerBeat <= 0.0) continue;

        // "osu-stable forced the first control point to start at 0."
        // "This is reproduced here to maintain compatibility around osu!mania scroll speed and song
        // select display."
        double currentTime = (i == 0 ? 0 : t.offset);
        double nextTime = (i == timing_points.size() - 1 ? lastTime : timing_points[i + 1].offset);

        i32 bpm = min(60000.0 / t.msPerBeat, 9001.0);
        double duration = max(nextTime - currentTime, 0.0);

        bool found = false;
        for(auto tuple : bpms) {
            if(tuple.bpm == bpm) {
                tuple.duration += duration;
                found = true;
                break;
            }
        }

        if(!found) {
            bpms.push_back(Tuple{
                .bpm = bpm,
                .duration = duration,
            });
        }
    }

    i32 min = 9001;
    i32 max = 0;
    i32 mostCommonBPM = 0;
    double longestDuration = 0;
    for(auto tuple : bpms) {
        if(tuple.bpm > max) max = tuple.bpm;
        if(tuple.bpm < min) min = tuple.bpm;
        if(tuple.duration > longestDuration || (tuple.duration == longestDuration && tuple.bpm > mostCommonBPM)) {
            longestDuration = tuple.duration;
            mostCommonBPM = tuple.bpm;
        }
    }
    if(min > max) min = max;

    return BPMInfo{
        .min = min,
        .max = max,
        .most_common = mostCommonBPM,
    };
}
