//================ Copyright (c) 2016, PG, All rights reserved. =================//
//
// Purpose:		osu!.db + collection.db + raw loader + scores etc.
//
// $NoKeywords: $osubdb
//===============================================================================//

#ifndef OSUDATABASE_H
#define OSUDATABASE_H

#include "BanchoProtocol.h"  // Packet
#include "OsuScore.h"
#include "UString.h"
#include "cbase.h"

class Timer;
class ConVar;

class Osu;
class OsuFile;
class OsuDatabaseBeatmap;
class OsuDatabaseLoader;

struct TIMINGPOINT {
    double msPerBeat;
    double offset;
    bool timingChange;
};
TIMINGPOINT read_timing_point(Packet *packet);

Packet load_db(std::string path);
bool save_db(Packet *db, std::string path);

class OsuDatabase {
   public:
    struct CollectionEntry {
        bool isLegacyEntry;  // used for identifying loaded osu! collection entries
        MD5Hash hash;
    };

    struct Collection {
        bool isLegacyCollection;  // used for identifying loaded osu! collections
        UString name;
        std::vector<CollectionEntry> hashes;
        std::vector<std::pair<OsuDatabaseBeatmap *, std::vector<OsuDatabaseBeatmap *>>> beatmaps;
    };

    struct Score {
        bool isLegacyScore;  // used for identifying loaded osu! scores (which don't have any custom data available)
        bool isImportedLegacyScore;  // used for identifying imported osu! scores (which were previously legacy scores,
                                     // so they don't have any
                                     // numSliderBreaks/unstableRate/hitErrorAvgMin/hitErrorAvgMax)
        int version;
        uint64_t unixTimestamp;

        uint32_t player_id = 0;
        UString playerName;
        bool passed = false;
        bool ragequit = false;
        OsuScore::GRADE grade = OsuScore::GRADE::GRADE_N;
        OsuDatabaseBeatmap *diff2;
        uint64_t play_time_ms = 0;
        UString replay_data = "";  // TODO @kiwec: use Packet

        int num300s;
        int num100s;
        int num50s;
        int numGekis;
        int numKatus;
        int numMisses;

        unsigned long long score;
        int comboMax;
        bool perfect;
        int modsLegacy;

        // custom
        int numSliderBreaks;
        float pp;
        float unstableRate;
        float hitErrorAvgMin;
        float hitErrorAvgMax;
        float starsTomTotal;
        float starsTomAim;
        float starsTomSpeed;
        float speedMultiplier;
        float CS, AR, OD, HP;
        int maxPossibleCombo;
        int numHitObjects;
        int numCircles;
        std::string experimentalModsConVars;

        // runtime
        unsigned long long sortHack;
        MD5Hash md5hash;

        bool isLegacyScoreEqualToImportedLegacyScore(const OsuDatabase::Score &importedLegacyScore) const {
            if(!isLegacyScore) return false;
            if(!importedLegacyScore.isImportedLegacyScore) return false;

            const bool isScoreValueEqual = (score == importedLegacyScore.score);
            const bool isTimestampEqual = (unixTimestamp == importedLegacyScore.unixTimestamp);
            const bool isComboMaxEqual = (comboMax == importedLegacyScore.comboMax);
            const bool isModsLegacyEqual = (modsLegacy == importedLegacyScore.modsLegacy);
            const bool isNum300sEqual = (num300s == importedLegacyScore.num300s);
            const bool isNum100sEqual = (num100s == importedLegacyScore.num100s);
            const bool isNum50sEqual = (num50s == importedLegacyScore.num50s);
            const bool isNumGekisEqual = (numGekis == importedLegacyScore.numGekis);
            const bool isNumKatusEqual = (numKatus == importedLegacyScore.numKatus);
            const bool isNumMissesEqual = (numMisses == importedLegacyScore.numMisses);

            return (isScoreValueEqual && isTimestampEqual && isComboMaxEqual && isModsLegacyEqual && isNum300sEqual &&
                    isNum100sEqual && isNum50sEqual && isNumGekisEqual && isNumKatusEqual && isNumMissesEqual);
        }

        bool isScoreEqualToCopiedScoreIgnoringPlayerName(const OsuDatabase::Score &copiedScore) const {
            const bool isScoreValueEqual = (score == copiedScore.score);
            const bool isTimestampEqual = (unixTimestamp == copiedScore.unixTimestamp);
            const bool isComboMaxEqual = (comboMax == copiedScore.comboMax);
            const bool isModsLegacyEqual = (modsLegacy == copiedScore.modsLegacy);
            const bool isNum300sEqual = (num300s == copiedScore.num300s);
            const bool isNum100sEqual = (num100s == copiedScore.num100s);
            const bool isNum50sEqual = (num50s == copiedScore.num50s);
            const bool isNumGekisEqual = (numGekis == copiedScore.numGekis);
            const bool isNumKatusEqual = (numKatus == copiedScore.numKatus);
            const bool isNumMissesEqual = (numMisses == copiedScore.numMisses);

            const bool isSpeedMultiplierEqual = (speedMultiplier == copiedScore.speedMultiplier);
            const bool isCSEqual = (CS == copiedScore.CS);
            const bool isAREqual = (AR == copiedScore.AR);
            const bool isODEqual = (OD == copiedScore.OD);
            const bool isHPEqual = (HP == copiedScore.HP);
            const bool areExperimentalModsConVarsEqual =
                (experimentalModsConVars == copiedScore.experimentalModsConVars);

            return (isScoreValueEqual && isTimestampEqual && isComboMaxEqual && isModsLegacyEqual && isNum300sEqual &&
                    isNum100sEqual && isNum50sEqual && isNumGekisEqual && isNumKatusEqual && isNumMissesEqual

                    && isSpeedMultiplierEqual && isCSEqual && isAREqual && isODEqual && isHPEqual &&
                    areExperimentalModsConVarsEqual);
        }
    };

    struct PlayerStats {
        UString name;
        float pp;
        float accuracy;
        int numScoresWithPP;
        int level;
        float percentToNextLevel;
        unsigned long long totalScore;
    };

    struct PlayerPPScores {
        std::vector<Score *> ppScores;
        unsigned long long totalScore;
    };

    struct SCORE_SORTING_COMPARATOR {
        virtual ~SCORE_SORTING_COMPARATOR() { ; }
        virtual bool operator()(OsuDatabase::Score const &a, OsuDatabase::Score const &b) const = 0;
    };

    struct SCORE_SORTING_METHOD {
        UString name;
        SCORE_SORTING_COMPARATOR *comparator;
    };

   public:
    OsuDatabase(Osu *osu);
    ~OsuDatabase();

    void update();

    void load();
    void cancel();
    void save();

    OsuDatabaseBeatmap *addBeatmap(std::string beatmapFolderPath);

    int addScore(MD5Hash beatmapMD5Hash, OsuDatabase::Score score);
    void deleteScore(MD5Hash beatmapMD5Hash, uint64_t scoreUnixTimestamp);
    void sortScores(MD5Hash beatmapMD5Hash);
    void forceScoreUpdateOnNextCalculatePlayerStats() { m_bDidScoresChangeForStats = true; }
    void forceScoresSaveOnNextShutdown() { m_bDidScoresChangeForSave = true; }

    bool addCollection(UString collectionName);
    bool renameCollection(UString oldCollectionName, UString newCollectionName);
    void deleteCollection(UString collectionName);
    void addBeatmapToCollection(UString collectionName, MD5Hash beatmapMD5Hash, bool doSaveImmediatelyIfEnabled = true);
    void removeBeatmapFromCollection(UString collectionName, MD5Hash beatmapMD5Hash,
                                     bool doSaveImmediatelyIfEnabled = true);
    void triggerSaveCollections() { saveCollections(); }

    std::vector<UString> getPlayerNamesWithPPScores();
    std::vector<UString> getPlayerNamesWithScoresForUserSwitcher();
    PlayerPPScores getPlayerPPScores(UString playerName);
    PlayerStats calculatePlayerStats(UString playerName);
    static float getWeightForIndex(int i);
    static float getBonusPPForNumScores(int numScores);
    static unsigned long long getRequiredScoreForLevel(int level);
    static int getLevelForScore(unsigned long long score, int maxLevel = 120);

    inline float getProgress() const { return m_fLoadingProgress.load(); }
    inline bool isFinished() const { return (getProgress() >= 1.0f); }
    inline bool foundChanges() const { return m_bFoundChanges; }

    inline const std::vector<OsuDatabaseBeatmap *> getDatabaseBeatmaps() const { return m_databaseBeatmaps; }
    OsuDatabaseBeatmap *getBeatmap(const MD5Hash &md5hash);
    OsuDatabaseBeatmap *getBeatmapDifficulty(const MD5Hash &md5hash);

    inline const std::vector<Collection> &getCollections() const { return m_collections; }

    inline std::unordered_map<MD5Hash, std::vector<Score>> *getScores() { return &m_scores; }
    inline const std::vector<SCORE_SORTING_METHOD> &getScoreSortingMethods() const { return m_scoreSortingMethods; }

    inline unsigned long long getAndIncrementScoreSortHackCounter() { return m_iSortHackCounter++; }

    std::unordered_map<MD5Hash, std::vector<Score>> m_online_scores;

   private:
    friend class OsuDatabaseLoader;

    static ConVar *m_name_ref;
    static ConVar *m_osu_songbrowser_scores_sortingtype_ref;

    void addScoreRaw(const MD5Hash &beatmapMD5Hash, const OsuDatabase::Score &score);

    std::string parseLegacyCfgBeatmapDirectoryParameter();
    void scheduleLoadRaw();
    void loadDB(Packet *db, bool &fallbackToRawLoad);

    void loadStars();
    void saveStars();

    void loadScores();
    void saveScores();

    void loadCollections(std::string collectionFilePath, bool isLegacy,
                         const std::unordered_map<MD5Hash, OsuDatabaseBeatmap *> &hashToDiff2,
                         const std::unordered_map<MD5Hash, OsuDatabaseBeatmap *> &hashToBeatmap);
    void saveCollections();

    OsuDatabaseBeatmap *loadRawBeatmap(std::string beatmapPath);  // only used for raw loading without db

    void onScoresRename(UString args);
    void onScoresExport();

    Osu *m_osu;
    Timer *m_importTimer;
    bool m_bIsFirstLoad;   // only load differences after first raw load
    bool m_bFoundChanges;  // for total refresh detection of raw loading

    // global
    int m_iNumBeatmapsToLoad;
    std::atomic<float> m_fLoadingProgress;
    std::atomic<bool> m_bInterruptLoad;
    std::vector<OsuDatabaseBeatmap *> m_databaseBeatmaps;

    // osu!.db
    int m_iVersion;
    int m_iFolderCount;

    // collection.db (legacy and custom)
    std::vector<Collection> m_collections;
    bool m_bDidCollectionsChangeForSave;

    // scores.db (legacy and custom)
    bool m_bScoresLoaded;
    std::unordered_map<MD5Hash, std::vector<Score>> m_scores;
    bool m_bDidScoresChangeForSave;
    bool m_bDidScoresChangeForStats;
    unsigned long long m_iSortHackCounter;
    PlayerStats m_prevPlayerStats;
    std::vector<SCORE_SORTING_METHOD> m_scoreSortingMethods;

    // raw load
    bool m_bRawBeatmapLoadScheduled;
    int m_iCurRawBeatmapLoadIndex;
    std::string m_sRawBeatmapLoadOsuSongFolder;
    std::vector<std::string> m_rawBeatmapFolders;
    std::vector<std::string> m_rawLoadBeatmapFolders;
    std::unordered_map<MD5Hash, OsuDatabaseBeatmap *> m_rawHashToDiff2;
    std::unordered_map<MD5Hash, OsuDatabaseBeatmap *> m_rawHashToBeatmap;

    // stars.cache
    struct STARS_CACHE_ENTRY {
        float starsNomod;
    };
    std::unordered_map<MD5Hash, STARS_CACHE_ENTRY> m_starsCache;
};

#endif
