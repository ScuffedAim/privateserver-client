#pragma once
#include "Replay.h"
#include "cbase.h"

class ConVar;
class DatabaseBeatmap;
class BeatmapInterface;
class HitObject;

struct FinishedScore {
    u64 score = 0;
    u64 spinner_bonus = 0;
    Replay::Mods mods;

    u64 unixTimestamp = 0;
    u32 player_id = 0;
    std::string playerName;
    DatabaseBeatmap *diff2 = NULL;
    u64 play_time_ms = 0;

    enum class Grade {
        XH,
        SH,
        X,
        S,
        A,
        B,
        C,
        D,
        F,
        N  // means "no grade"
    };
    Grade grade = Grade::N;

    std::string client;
    std::string server;
    u64 bancho_score_id = 0;
    u64 peppy_replay_tms = 0;  // online scores don't have peppy_replay_tms

    int num300s = 0;
    int num100s = 0;
    int num50s = 0;
    int numGekis = 0;
    int numKatus = 0;
    int numMisses = 0;

    int comboMax = 0;
    bool perfect = false;
    bool passed = false;
    bool ragequit = false;

    u32 ppv2_version = 0;
    float ppv2_score = 0.f;
    float ppv2_total_stars = 0.f;
    float ppv2_aim_stars = 0.f;
    float ppv2_speed_stars = 0.f;

    std::string ppv3_algorithm;
    float ppv3_score = 0.f;

    // Absolute hit deltas of every hitobject (0-254). 255 == miss
    // This is exclusive to PPV3-converted scores
    std::vector<u8> hitdeltas;

    int numSliderBreaks = 0;
    float unstableRate = 0.f;
    float hitErrorAvgMin = 0.f;
    float hitErrorAvgMax = 0.f;
    int maxPossibleCombo = -1;
    int numHitObjects = -1;
    int numCircles = -1;

    MD5Hash beatmap_hash;
    std::vector<LegacyReplay::Frame> replay;  // not always loaded

    bool is_peppy_imported() { return this->bancho_score_id != 0 || this->peppy_replay_tms != 0; }
    f64 get_pp() const;
    Grade calculate_grade() const;
};

class LiveScore {
   public:
    enum class HIT {
        // score
        HIT_NULL,
        HIT_MISS,
        HIT_50,
        HIT_100,
        HIT_300,

        // only used for health + SS/PF mods
        HIT_MISS_SLIDERBREAK,
        HIT_MU,
        HIT_100K,
        HIT_300K,
        HIT_300G,
        HIT_SLIDER10,  // tick
        HIT_SLIDER30,  // repeat
        HIT_SPINNERSPIN,
        HIT_SPINNERBONUS
    };

    static float calculateAccuracy(int num300s, int num100s, int num50s, int numMisses);

   public:
    LiveScore(bool simulating = true);

    void reset();  // only Beatmap may call this function!

    // only Beatmap/SimulatedBeatmap may call this function!
    void addHitResult(BeatmapInterface *beatmap, HitObject *hitObject, LiveScore::HIT hit, long delta,
                      bool ignoreOnHitErrorBar, bool hitErrorBarOnly, bool ignoreCombo, bool ignoreScore);

    void addHitResultComboEnd(LiveScore::HIT hit);
    void addSliderBreak();  // only Beatmap may call this function!
    void addPoints(int points, bool isSpinner);
    void setComboFull(int comboFull) { this->iComboFull = comboFull; }
    void setComboEndBitmask(int comboEndBitmask) { this->iComboEndBitmask = comboEndBitmask; }
    void setDead(bool dead);

    void addKeyCount(int key);

    void setStarsTomTotal(float starsTomTotal) { this->fStarsTomTotal = starsTomTotal; }
    void setStarsTomAim(float starsTomAim) { this->fStarsTomAim = starsTomAim; }
    void setStarsTomSpeed(float starsTomSpeed) { this->fStarsTomSpeed = starsTomSpeed; }
    void setPPv2(float ppv2) { this->fPPv2 = ppv2; }
    void setIndex(int index) { this->iIndex = index; }

    void setNumEZRetries(int numEZRetries) { this->iNumEZRetries = numEZRetries; }

    inline float getStarsTomTotal() const { return this->fStarsTomTotal; }
    inline float getStarsTomAim() const { return this->fStarsTomAim; }
    inline float getStarsTomSpeed() const { return this->fStarsTomSpeed; }
    inline float getPPv2() const { return this->fPPv2; }
    inline int getIndex() const { return this->iIndex; }

    unsigned long long getScore();
    inline FinishedScore::Grade getGrade() const { return this->grade; }
    inline int getCombo() const { return this->iCombo; }
    inline int getComboMax() const { return this->iComboMax; }
    inline int getComboFull() const { return this->iComboFull; }
    inline int getComboEndBitmask() const { return this->iComboEndBitmask; }
    inline float getAccuracy() const { return this->fAccuracy; }
    inline float getUnstableRate() const { return this->fUnstableRate; }
    inline float getHitErrorAvgMin() const { return this->fHitErrorAvgMin; }
    inline float getHitErrorAvgMax() const { return this->fHitErrorAvgMax; }
    inline float getHitErrorAvgCustomMin() const { return this->fHitErrorAvgCustomMin; }
    inline float getHitErrorAvgCustomMax() const { return this->fHitErrorAvgCustomMax; }
    inline int getNumMisses() const { return this->iNumMisses; }
    inline int getNumSliderBreaks() const { return this->iNumSliderBreaks; }
    inline int getNum50s() const { return this->iNum50s; }
    inline int getNum100s() const { return this->iNum100s; }
    inline int getNum100ks() const { return this->iNum100ks; }
    inline int getNum300s() const { return this->iNum300s; }
    inline int getNum300gs() const { return this->iNum300gs; }

    inline int getNumEZRetries() const { return this->iNumEZRetries; }

    inline bool isDead() const { return this->bDead; }
    inline bool hasDied() const { return this->bDied; }

    inline bool isUnranked() const { return this->bIsUnranked; }
    void setCheated() { this->bIsUnranked = true; }

    static double getHealthIncrease(BeatmapInterface *beatmap, LiveScore::HIT hit);
    static double getHealthIncrease(LiveScore::HIT hit, double HP = 5.0f, double hpMultiplierNormal = 1.0f,
                                    double hpMultiplierComboEnd = 1.0f, double hpBarMaximumForNormalization = 200.0f);

    int getKeyCount(int key);
    u32 getModsLegacy();
    UString getModsStringForRichPresence();
    Replay::Mods mods;
    bool simulating;

   private:
    f32 getScoreMultiplier();
    void onScoreChange();

    std::vector<HIT> hitresults;
    std::vector<int> hitdeltas;

    FinishedScore::Grade grade;

    float fStarsTomTotal;
    float fStarsTomAim;
    float fStarsTomSpeed;
    float fPPv2;
    int iIndex;

    unsigned long long iScoreV1;
    unsigned long long iScoreV2;
    unsigned long long iScoreV2ComboPortion;
    unsigned long long iBonusPoints;
    int iCombo;
    int iComboMax;
    int iComboFull;
    int iComboEndBitmask;
    float fAccuracy;
    float fHitErrorAvgMin;
    float fHitErrorAvgMax;
    float fHitErrorAvgCustomMin;
    float fHitErrorAvgCustomMax;
    float fUnstableRate;

    int iNumMisses;
    int iNumSliderBreaks;
    int iNum50s;
    int iNum100s;
    int iNum100ks;
    int iNum300s;
    int iNum300gs;

    bool bDead;
    bool bDied;

    int iNumK1;
    int iNumK2;
    int iNumM1;
    int iNumM2;

    // custom
    int iNumEZRetries;
    bool bIsUnranked;
};
