#include "DatabaseBeatmap.h"

#include <assert.h>

#include <iostream>
#include <limits>
#include <sstream>

#include "Bancho.h"  // md5
#include "Beatmap.h"
#include "Circle.h"
#include "ConVar.h"
#include "Database.h"
#include "Engine.h"
#include "File.h"
#include "GameRules.h"
#include "HitObject.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "Skin.h"
#include "Slider.h"
#include "SliderCurves.h"
#include "SongBrowser/SongBrowser.h"
#include "Spinner.h"

ConVar osu_mod_reverse_sliders("osu_mod_reverse_sliders", false, FCVAR_UNLOCKED);
ConVar osu_mod_strict_tracking("osu_mod_strict_tracking", false, FCVAR_UNLOCKED);
ConVar osu_mod_strict_tracking_remove_slider_ticks("osu_mod_strict_tracking_remove_slider_ticks", false, FCVAR_LOCKED,
                                                   "whether the strict tracking mod should remove slider ticks or not, "
                                                   "this changed after its initial implementation in lazer");

ConVar osu_show_approach_circle_on_first_hidden_object("osu_show_approach_circle_on_first_hidden_object", true,
                                                       FCVAR_DEFAULT);

ConVar osu_stars_stacking("osu_stars_stacking", true, FCVAR_DEFAULT,
                          "respect hitobject stacking before calculating stars/pp");

ConVar osu_slider_max_repeats("osu_slider_max_repeats", 9000, FCVAR_LOCKED,
                              "maximum number of repeats allowed per slider (clamp range)");
ConVar osu_slider_max_ticks("osu_slider_max_ticks", 2048, FCVAR_LOCKED,
                            "maximum number of ticks allowed per slider (clamp range)");

ConVar osu_number_max("osu_number_max", 0, FCVAR_DEFAULT,
                      "0 = disabled, 1/2/3/4/etc. limits visual circle numbers to this number");
ConVar osu_ignore_beatmap_combo_numbers("osu_ignore_beatmap_combo_numbers", false, FCVAR_DEFAULT,
                                        "may be used in conjunction with osu_number_max");

ConVar osu_beatmap_version("osu_beatmap_version", 128, FCVAR_DEFAULT,
                           "maximum supported .osu file version, above this will simply not load (this was 14 but got "
                           "bumped to 128 due to lazer backports)");
ConVar osu_beatmap_max_num_hitobjects(
    "osu_beatmap_max_num_hitobjects", 40000, FCVAR_DEFAULT,
    "maximum number of total allowed hitobjects per beatmap (prevent crashing on deliberate game-breaking beatmaps)");
ConVar osu_beatmap_max_num_slider_scoringtimes("osu_beatmap_max_num_slider_scoringtimes", 32768, FCVAR_DEFAULT,
                                               "maximum number of slider score increase events allowed per slider "
                                               "(prevent crashing on deliberate game-breaking beatmaps)");

unsigned long long DatabaseBeatmap::sortHackCounter = 0;

ConVar *DatabaseBeatmap::m_osu_slider_curve_max_length_ref = NULL;
ConVar *DatabaseBeatmap::m_osu_stars_stacking_ref = NULL;
ConVar *DatabaseBeatmap::m_osu_debug_pp_ref = NULL;
ConVar *DatabaseBeatmap::m_osu_slider_end_inside_check_offset_ref = NULL;

DatabaseBeatmap::DatabaseBeatmap(std::string filePath, std::string folder) {
    m_sFilePath = filePath;

    m_sFolder = folder;

    m_iSortHack = sortHackCounter++;

    // convar refs
    if(m_osu_slider_curve_max_length_ref == NULL)
        m_osu_slider_curve_max_length_ref = convar->getConVarByName("osu_slider_curve_max_length");
    if(m_osu_stars_stacking_ref == NULL) m_osu_stars_stacking_ref = convar->getConVarByName("osu_stars_stacking");
    if(m_osu_debug_pp_ref == NULL) m_osu_debug_pp_ref = convar->getConVarByName("osu_debug_pp");
    if(m_osu_slider_end_inside_check_offset_ref == NULL)
        m_osu_slider_end_inside_check_offset_ref = convar->getConVarByName("osu_slider_end_inside_check_offset");

    // raw metadata (note the special default values)

    m_iVersion = osu_beatmap_version.getInt();
    m_iGameMode = 0;
    m_iID = 0;
    m_iSetID = -1;

    m_iLengthMS = 0;
    m_iPreviewTime = -1;

    m_fAR = 5.0f;
    m_fCS = 5.0f;
    m_fHP = 5.0f;
    m_fOD = 5.0f;

    m_fStackLeniency = 0.7f;
    m_fSliderTickRate = 1.0f;
    m_fSliderMultiplier = 1.0f;

    // precomputed data

    m_fStarsNomod = 0.0f;

    m_iMinBPM = 0;
    m_iMaxBPM = 0;
    m_iMostCommonBPM = 0;

    m_iNumObjects = 0;
    m_iNumCircles = 0;
    m_iNumSliders = 0;
    m_iNumSpinners = 0;

    // custom data

    last_modification_time = ((std::numeric_limits<long long>::max)() / 2) + m_iSortHack;

    m_iLocalOffset = 0;
    m_iOnlineOffset = 0;
}

DatabaseBeatmap::DatabaseBeatmap(std::vector<DatabaseBeatmap *> *difficulties) : DatabaseBeatmap("", "") {
    m_difficulties = difficulties;
    if(m_difficulties->empty()) return;

    // set representative values for this container (i.e. use values from first difficulty)
    m_sTitle = (*m_difficulties)[0]->m_sTitle;
    m_sArtist = (*m_difficulties)[0]->m_sArtist;
    m_sCreator = (*m_difficulties)[0]->m_sCreator;
    m_sBackgroundImageFileName = (*m_difficulties)[0]->m_sBackgroundImageFileName;
    m_iSetID = (*m_difficulties)[0]->m_iSetID;

    // also calculate largest representative values
    m_iLengthMS = 0;
    m_fCS = 99.f;
    m_fAR = 0.0f;
    m_fOD = 0.0f;
    m_fHP = 0.0f;
    m_fStarsNomod = 0.0f;
    m_iMinBPM = 9001;
    m_iMaxBPM = 0;
    m_iMostCommonBPM = 0;
    last_modification_time = 0;
    for(auto diff : (*m_difficulties)) {
        if(diff->getLengthMS() > m_iLengthMS) m_iLengthMS = diff->getLengthMS();
        if(diff->getCS() < m_fCS) m_fCS = diff->getCS();
        if(diff->getAR() > m_fAR) m_fAR = diff->getAR();
        if(diff->getHP() > m_fHP) m_fHP = diff->getHP();
        if(diff->getOD() > m_fOD) m_fOD = diff->getOD();
        if(diff->getStarsNomod() > m_fStarsNomod) m_fStarsNomod = diff->getStarsNomod();
        if(diff->getMinBPM() < m_iMinBPM) m_iMinBPM = diff->getMinBPM();
        if(diff->getMaxBPM() > m_iMaxBPM) m_iMaxBPM = diff->getMaxBPM();
        if(diff->getMostCommonBPM() > m_iMostCommonBPM) m_iMostCommonBPM = diff->getMostCommonBPM();
        if(diff->last_modification_time > last_modification_time) last_modification_time = diff->last_modification_time;
    }
}

DatabaseBeatmap::~DatabaseBeatmap() {
    if(m_difficulties != NULL) {
        for(auto diff : (*m_difficulties)) {
            assert(diff->m_difficulties == NULL);
            delete diff;
        }
        delete m_difficulties;
    }
}

DatabaseBeatmap::PRIMITIVE_CONTAINER DatabaseBeatmap::loadPrimitiveObjects(const std::string osuFilePath) {
    std::atomic<bool> dead;
    dead = false;
    return loadPrimitiveObjects(osuFilePath, dead);
}

DatabaseBeatmap::PRIMITIVE_CONTAINER DatabaseBeatmap::loadPrimitiveObjects(const std::string osuFilePath,
                                                                           const std::atomic<bool> &dead) {
    PRIMITIVE_CONTAINER c;
    {
        c.errorCode = 0;

        c.stackLeniency = 0.7f;

        c.sliderMultiplier = 1.0f;
        c.sliderTickRate = 1.0f;

        c.version = 14;
    }

    const float sliderSanityRange =
        m_osu_slider_curve_max_length_ref->getFloat();  // infinity sanity check, same as before
    const int sliderMaxRepeatRange =
        osu_slider_max_repeats.getInt();  // NOTE: osu! will refuse to play any beatmap which has sliders with more than
                                          // 9000 repeats, here we just clamp it instead

    // open osu file for parsing
    {
        File file(osuFilePath);
        if(!file.canRead()) {
            c.errorCode = 2;
            return c;
        }

        std::istringstream ss("");

        // load the actual beatmap
        unsigned long long timingPointSortHack = 0;
        int hitobjectsWithoutSpinnerCounter = 0;
        int colorCounter = 1;
        int colorOffset = 0;
        int comboNumber = 1;
        int curBlock = -1;
        std::string curLine;
        while(file.canRead()) {
            if(dead.load()) {
                c.errorCode = 6;
                return c;
            }

            curLine = file.readLine();

            const char *curLineChar = curLine.c_str();
            const int commentIndex = curLine.find("//");
            if(commentIndex == std::string::npos ||
               commentIndex != 0)  // ignore comments, but only if at the beginning of a line (e.g. allow
                                   // Artist:DJ'TEKINA//SOMETHING)
            {
                if(curLine.find("[General]") != std::string::npos)
                    curBlock = 1;
                else if(curLine.find("[Difficulty]") != std::string::npos)
                    curBlock = 2;
                else if(curLine.find("[Events]") != std::string::npos)
                    curBlock = 3;
                else if(curLine.find("[TimingPoints]") != std::string::npos)
                    curBlock = 4;
                else if(curLine.find("[Colours]") != std::string::npos)
                    curBlock = 5;
                else if(curLine.find("[HitObjects]") != std::string::npos)
                    curBlock = 6;

                switch(curBlock) {
                    case -1:  // header (e.g. "osu file format v12")
                    {
                        sscanf(curLineChar, " osu file format v %i \n", &c.version);
                    } break;

                    case 1:  // General
                    {
                        sscanf(curLineChar, " StackLeniency : %f \n", &c.stackLeniency);
                    } break;

                    case 2:  // Difficulty
                    {
                        sscanf(curLineChar, " SliderMultiplier : %f \n", &c.sliderMultiplier);
                        sscanf(curLineChar, " SliderTickRate : %f \n", &c.sliderTickRate);
                    } break;

                    case 3:  // Events
                    {
                        int type, startTime, endTime;
                        if(sscanf(curLineChar, " %i , %i , %i \n", &type, &startTime, &endTime) == 3) {
                            if(type == 2) {
                                BREAK b;
                                {
                                    b.startTime = startTime;
                                    b.endTime = endTime;
                                }
                                c.breaks.push_back(b);
                            }
                        }
                    } break;

                    case 4:  // TimingPoints
                    {
                        // old beatmaps: Offset, Milliseconds per Beat
                        // old new beatmaps: Offset, Milliseconds per Beat, Meter, Sample Type, Sample Set, Volume,
                        // !Inherited new new beatmaps: Offset, Milliseconds per Beat, Meter, Sample Type, Sample Set,
                        // Volume, !Inherited, Kiai Mode

                        double tpOffset;
                        float tpMSPerBeat;
                        int tpMeter;
                        int tpSampleType, tpSampleSet;
                        int tpVolume;
                        int tpTimingChange;
                        int tpKiai = 0;  // optional
                        if(sscanf(curLineChar, " %lf , %f , %i , %i , %i , %i , %i , %i", &tpOffset, &tpMSPerBeat,
                                  &tpMeter, &tpSampleType, &tpSampleSet, &tpVolume, &tpTimingChange, &tpKiai) == 8 ||
                           sscanf(curLineChar, " %lf , %f , %i , %i , %i , %i , %i", &tpOffset, &tpMSPerBeat, &tpMeter,
                                  &tpSampleType, &tpSampleSet, &tpVolume, &tpTimingChange) == 7) {
                            TIMINGPOINT t;
                            {
                                t.offset = (long)std::round(tpOffset);
                                t.msPerBeat = tpMSPerBeat;

                                t.sampleType = tpSampleType;
                                t.sampleSet = tpSampleSet;
                                t.volume = tpVolume;

                                t.timingChange = tpTimingChange == 1;
                                t.kiai = tpKiai > 0;

                                t.sortHack = timingPointSortHack++;
                            }
                            c.timingpoints.push_back(t);
                        } else if(sscanf(curLineChar, " %lf , %f", &tpOffset, &tpMSPerBeat) == 2) {
                            TIMINGPOINT t;
                            {
                                t.offset = (long)std::round(tpOffset);
                                t.msPerBeat = tpMSPerBeat;

                                t.sampleType = 0;
                                t.sampleSet = 0;
                                t.volume = 100;

                                t.timingChange = true;
                                t.kiai = false;

                                t.sortHack = timingPointSortHack++;
                            }
                            c.timingpoints.push_back(t);
                        }
                    } break;

                    case 5:  // Colours
                    {
                        int comboNum;
                        int r, g, b;
                        if(sscanf(curLineChar, " Combo %i : %i , %i , %i \n", &comboNum, &r, &g, &b) == 4)
                            c.combocolors.push_back(COLOR(255, r, g, b));
                    } break;

                    case 6:  // HitObjects

                        // circles:
                        // x,y,time,type,hitSound,addition
                        // sliders:
                        // x,y,time,type,hitSound,sliderType|curveX:curveY|...,repeat,pixelLength,edgeHitsound,edgeAddition,addition
                        // spinners:
                        // x,y,time,type,hitSound,endTime,addition

                        // NOTE: calculating combo numbers and color offsets based on the parsing order is dangerous.
                        // maybe the hitobjects are not sorted by time in the file; these values should be calculated
                        // after sorting just to be sure?

                        int x, y;
                        u32 time;
                        int type;
                        int hitSound;

                        const bool intScan =
                            (sscanf(curLineChar, " %i , %i , %d , %i , %i", &x, &y, &time, &type, &hitSound) == 5);
                        bool floatScan = false;
                        if(!intScan) {
                            float fX, fY;
                            floatScan = (sscanf(curLineChar, " %f , %f , %d , %i , %i", &fX, &fY, &time, &type,
                                                &hitSound) == 5);
                            x = (int)fX;
                            y = (int)fY;
                        }

                        if(intScan || floatScan) {
                            if(!(type & 0x8)) hitobjectsWithoutSpinnerCounter++;

                            if(type & 0x4)  // new combo
                            {
                                comboNumber = 1;

                                // special case 1: if the current object is a spinner, then the raw color counter is not
                                // increased (but the offset still is!) special case 2: the first (non-spinner)
                                // hitobject in a beatmap is always a new combo, therefore the raw color counter is not
                                // increased for it (but the offset still is!)
                                if(!(type & 0x8) && hitobjectsWithoutSpinnerCounter > 1) colorCounter++;

                                colorOffset +=
                                    (type >> 4) & 7;  // special case 3: "Bits 4-6 (16, 32, 64) form a 3-bit number
                                                      // (0-7) that chooses how many combo colours to skip."
                            }

                            if(type & 0x1)  // circle
                            {
                                HITCIRCLE h;
                                {
                                    h.x = x;
                                    h.y = y;
                                    h.time = time;
                                    h.sampleType = hitSound;
                                    h.number = comboNumber++;
                                    h.colorCounter = colorCounter;
                                    h.colorOffset = colorOffset;
                                    h.clicked = false;
                                }
                                c.hitcircles.push_back(h);
                            } else if(type & 0x2)  // slider
                            {
                                UString curLineString = UString(curLineChar);
                                std::vector<UString> tokens = curLineString.split(",");
                                if(tokens.size() < 8) {
                                    debugLog("Invalid slider in beatmap: %s\n\ncurLine = %s\n", osuFilePath.c_str(),
                                             curLineChar);
                                    continue;
                                    // engine->showMessageError("Error", UString::format("Invalid slider in beatmap:
                                    // %s\n\ncurLine = %s", m_sFilePath.c_str(), curLine)); return false;
                                }

                                std::vector<UString> sliderTokens = tokens[5].split("|");
                                if(sliderTokens.size() < 1)  // partially allow bullshit sliders (no controlpoints!),
                                                             // e.g. https://osu.ppy.sh/beatmapsets/791900#osu/1676490
                                {
                                    debugLog("Invalid slider tokens: %s\n\nIn beatmap: %s\n", curLineChar,
                                             osuFilePath.c_str());
                                    continue;
                                    // engine->showMessageError("Error", UString::format("Invalid slider tokens:
                                    // %s\n\nIn beatmap: %s", curLineChar, m_sFilePath.c_str())); return false;
                                }

                                std::vector<Vector2> points;
                                for(int i = 1; i < sliderTokens.size();
                                    i++)  // NOTE: starting at 1 due to slider type char
                                {
                                    std::vector<UString> sliderXY = sliderTokens[i].split(":");

                                    // array size check
                                    // infinity sanity check (this only exists because of https://osu.ppy.sh/b/1029976)
                                    // not a very elegant check, but it does the job
                                    if(sliderXY.size() != 2 || sliderXY[0].find("E") != -1 ||
                                       sliderXY[0].find("e") != -1 || sliderXY[1].find("E") != -1 ||
                                       sliderXY[1].find("e") != -1) {
                                        debugLog("Invalid slider positions: %s\n\nIn Beatmap: %s\n", curLineChar,
                                                 osuFilePath.c_str());
                                        continue;
                                        // engine->showMessageError("Error", UString::format("Invalid slider positions:
                                        // %s\n\nIn beatmap: %s", curLine, m_sFilePath.c_str())); return false;
                                    }

                                    points.push_back(Vector2(
                                        (int)clamp<float>(sliderXY[0].toFloat(), -sliderSanityRange, sliderSanityRange),
                                        (int)clamp<float>(sliderXY[1].toFloat(), -sliderSanityRange,
                                                          sliderSanityRange)));
                                }

                                // special case: osu! logic for handling the hitobject point vs the controlpoints (since
                                // sliders have both, and older beatmaps store the start point inside the control
                                // points)
                                {
                                    const Vector2 xy = Vector2(clamp<float>(x, -sliderSanityRange, sliderSanityRange),
                                                               clamp<float>(y, -sliderSanityRange, sliderSanityRange));
                                    if(points.size() > 0) {
                                        if(points[0] != xy) points.insert(points.begin(), xy);
                                    } else
                                        points.push_back(xy);
                                }

                                // partially allow bullshit sliders (add second point to make valid), e.g.
                                // https://osu.ppy.sh/beatmapsets/791900#osu/1676490
                                if(sliderTokens.size() < 2 && points.size() > 0) points.push_back(points[0]);

                                SLIDER s;
                                {
                                    s.x = x;
                                    s.y = y;
                                    s.type = sliderTokens[0][0];
                                    s.repeat = clamp<int>((int)tokens[6].toFloat(), -sliderMaxRepeatRange,
                                                          sliderMaxRepeatRange);
                                    s.repeat = s.repeat >= 0 ? s.repeat : 0;  // sanity check
                                    s.pixelLength =
                                        clamp<float>(tokens[7].toFloat(), -sliderSanityRange, sliderSanityRange);
                                    s.time = time;
                                    s.sampleType = hitSound;
                                    s.number = comboNumber++;
                                    s.colorCounter = colorCounter;
                                    s.colorOffset = colorOffset;
                                    s.points = points;

                                    // new beatmaps: slider hitsounds
                                    if(tokens.size() > 8) {
                                        std::vector<UString> hitSoundTokens = tokens[8].split("|");
                                        for(int i = 0; i < hitSoundTokens.size(); i++) {
                                            s.hitSounds.push_back(hitSoundTokens[i].toInt());
                                        }
                                    }
                                }
                                c.sliders.push_back(s);
                            } else if(type & 0x8)  // spinner
                            {
                                UString curLineString = UString(curLineChar);
                                std::vector<UString> tokens = curLineString.split(",");
                                if(tokens.size() < 6) {
                                    debugLog("Invalid spinner in beatmap: %s\n\ncurLine = %s\n", osuFilePath.c_str(),
                                             curLineChar);
                                    continue;
                                    // engine->showMessageError("Error", UString::format("Invalid spinner in beatmap:
                                    // %s\n\ncurLine = %s", m_sFilePath.c_str(), curLine)); return false;
                                }

                                SPINNER s;
                                {
                                    s.x = x;
                                    s.y = y;
                                    s.time = time;
                                    s.sampleType = hitSound;
                                    s.endTime = tokens[5].toFloat();
                                }
                                c.spinners.push_back(s);
                            }
                        }
                        break;
                }
            }
        }
    }

    // late bail if too many hitobjects would run out of memory and crash
    const size_t numHitobjects = c.hitcircles.size() + c.sliders.size() + c.spinners.size();
    if(numHitobjects > (size_t)osu_beatmap_max_num_hitobjects.getInt()) {
        c.errorCode = 5;
        return c;
    }

    // sort timingpoints by time
    if(c.timingpoints.size() > 0) std::sort(c.timingpoints.begin(), c.timingpoints.end(), TimingPointSortComparator());

    return c;
}

DatabaseBeatmap::CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT DatabaseBeatmap::calculateSliderTimesClicksTicks(
    int beatmapVersion, std::vector<SLIDER> &sliders, zarray<TIMINGPOINT> &timingpoints, float sliderMultiplier,
    float sliderTickRate) {
    std::atomic<bool> dead;
    dead = false;
    return calculateSliderTimesClicksTicks(beatmapVersion, sliders, timingpoints, sliderMultiplier, sliderTickRate,
                                           false);
}

DatabaseBeatmap::CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT DatabaseBeatmap::calculateSliderTimesClicksTicks(
    int beatmapVersion, std::vector<SLIDER> &sliders, zarray<TIMINGPOINT> &timingpoints, float sliderMultiplier,
    float sliderTickRate, const std::atomic<bool> &dead) {
    CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT r;
    { r.errorCode = 0; }

    if(timingpoints.size() < 1) {
        r.errorCode = 3;
        return r;
    }

    struct SliderHelper {
        static float getSliderTickDistance(float sliderMultiplier, float sliderTickRate) {
            return ((100.0f * sliderMultiplier) / sliderTickRate);
        }

        static float getSliderTimeForSlider(const SLIDER &slider, const TIMING_INFO &timingInfo,
                                            float sliderMultiplier) {
            const float duration = timingInfo.beatLength * (slider.pixelLength / sliderMultiplier) / 100.0f;
            return duration >= 1.0f ? duration : 1.0f;  // sanity check
        }

        static float getSliderVelocity(const SLIDER &slider, const TIMING_INFO &timingInfo, float sliderMultiplier,
                                       float sliderTickRate) {
            const float beatLength = timingInfo.beatLength;
            if(beatLength > 0.0f)
                return (getSliderTickDistance(sliderMultiplier, sliderTickRate) * sliderTickRate *
                        (1000.0f / beatLength));
            else
                return getSliderTickDistance(sliderMultiplier, sliderTickRate) * sliderTickRate;
        }

        static float getTimingPointMultiplierForSlider(const SLIDER &slider,
                                                       const TIMING_INFO &timingInfo)  // needed for slider ticks
        {
            float beatLengthBase = timingInfo.beatLengthBase;
            if(beatLengthBase == 0.0f)  // sanity check
                beatLengthBase = 1.0f;

            return timingInfo.beatLength / beatLengthBase;
        }
    };

    unsigned long long sortHackCounter = 0;
    for(int i = 0; i < sliders.size(); i++) {
        if(dead.load()) {
            r.errorCode = 6;
            return r;
        }

        SLIDER &s = sliders[i];

        // sanity reset
        s.ticks.clear();
        s.scoringTimesForStarCalc.clear();

        // calculate duration
        const TIMING_INFO timingInfo = getTimingInfoForTimeAndTimingPoints(s.time, timingpoints);
        s.sliderTimeWithoutRepeats = SliderHelper::getSliderTimeForSlider(s, timingInfo, sliderMultiplier);
        s.sliderTime = s.sliderTimeWithoutRepeats * s.repeat;

        // calculate ticks
        {
            const float minTickPixelDistanceFromEnd =
                0.01f * SliderHelper::getSliderVelocity(s, timingInfo, sliderMultiplier, sliderTickRate);
            const float tickPixelLength =
                (beatmapVersion < 8 ? SliderHelper::getSliderTickDistance(sliderMultiplier, sliderTickRate)
                                    : SliderHelper::getSliderTickDistance(sliderMultiplier, sliderTickRate) /
                                          SliderHelper::getTimingPointMultiplierForSlider(s, timingInfo));
            const float tickDurationPercentOfSliderLength =
                tickPixelLength / (s.pixelLength == 0.0f ? 1.0f : s.pixelLength);
            const int max_ticks = osu_slider_max_ticks.getInt();
            const int tickCount = min((int)std::ceil(s.pixelLength / tickPixelLength) - 1,
                                      max_ticks);  // NOTE: hard sanity limit number of ticks per slider

            if(tickCount > 0 && !timingInfo.isNaN && !std::isnan(s.pixelLength) &&
               !std::isnan(tickPixelLength))  // don't generate ticks for NaN timingpoints and infinite values
            {
                const float tickTOffset = tickDurationPercentOfSliderLength;
                float pixelDistanceToEnd = s.pixelLength;
                float t = tickTOffset;
                for(int i = 0; i < tickCount; i++, t += tickTOffset) {
                    // skip ticks which are too close to the end of the slider
                    pixelDistanceToEnd -= tickPixelLength;
                    if(pixelDistanceToEnd <= minTickPixelDistanceFromEnd) break;

                    s.ticks.push_back(t);
                }
            }
        }

        // bail if too many predicted heuristic scoringTimes would run out of memory and crash
        if((size_t)std::abs(s.repeat) * s.ticks.size() > (size_t)osu_beatmap_max_num_slider_scoringtimes.getInt()) {
            r.errorCode = 5;
            return r;
        }

        // calculate s.scoringTimesForStarCalc, which should include every point in time where the cursor must be within
        // the followcircle radius and at least one key must be pressed: see
        // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs
        const long osuSliderEndInsideCheckOffset = (long)m_osu_slider_end_inside_check_offset_ref->getInt();

        // 1) "skip the head circle"

        // 2) add repeat times (either at slider begin or end)
        for(int i = 0; i < (s.repeat - 1); i++) {
            const u32 time = s.time + (u32)(s.sliderTimeWithoutRepeats * (i + 1));  // see Slider.cpp
            s.scoringTimesForStarCalc.push_back(OsuDifficultyHitObject::SLIDER_SCORING_TIME{
                .type = OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::REPEAT,
                .time = time,
                .sortHack = sortHackCounter++,
            });
        }

        // 3) add tick times (somewhere within slider, repeated for every repeat)
        for(int i = 0; i < s.repeat; i++) {
            for(int t = 0; t < s.ticks.size(); t++) {
                const float tickPercentRelativeToRepeatFromStartAbs =
                    (((i + 1) % 2) != 0 ? s.ticks[t] : 1.0f - s.ticks[t]);  // see Slider.cpp
                const u32 time =
                    s.time + (u32)(s.sliderTimeWithoutRepeats * i) +
                    (u32)(tickPercentRelativeToRepeatFromStartAbs * s.sliderTimeWithoutRepeats);  // see Slider.cpp
                s.scoringTimesForStarCalc.push_back(OsuDifficultyHitObject::SLIDER_SCORING_TIME{
                    .type = OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::TICK,
                    .time = time,
                    .sortHack = sortHackCounter++,
                });
            }
        }

        // 4) add slider end (potentially before last tick for bullshit sliders, but sorting takes care of that)
        // see https://github.com/ppy/osu/pull/4193#issuecomment-460127543
        u32 time_a = s.time + (u32)s.sliderTime / 2;
        u32 time_b = (s.time + (u32)s.sliderTime) - osuSliderEndInsideCheckOffset;
        const u32 time = max(time_a, time_b);
        s.scoringTimesForStarCalc.push_back(OsuDifficultyHitObject::SLIDER_SCORING_TIME{
            .type = OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::END,
            .time = time,
            .sortHack = sortHackCounter++,
        });

        // 5) sort scoringTimes from earliest to latest
        std::sort(s.scoringTimesForStarCalc.begin(), s.scoringTimesForStarCalc.end(),
                  OsuDifficultyHitObject::SliderScoringTimeComparator());
    }

    return r;
}

DatabaseBeatmap::LOAD_DIFFOBJ_RESULT DatabaseBeatmap::loadDifficultyHitObjects(const std::string osuFilePath, float AR,
                                                                               float CS, float speedMultiplier,
                                                                               bool calculateStarsInaccurately) {
    std::atomic<bool> dead;
    dead = false;
    return loadDifficultyHitObjects(osuFilePath, AR, CS, speedMultiplier, calculateStarsInaccurately, dead);
}

DatabaseBeatmap::LOAD_DIFFOBJ_RESULT DatabaseBeatmap::loadDifficultyHitObjects(const std::string osuFilePath, float AR,
                                                                               float CS, float speedMultiplier,
                                                                               bool calculateStarsInaccurately,
                                                                               const std::atomic<bool> &dead) {
    LOAD_DIFFOBJ_RESULT result = LOAD_DIFFOBJ_RESULT();

    // build generalized OsuDifficultyHitObjects from the vectors (hitcircles, sliders, spinners)
    // the OsuDifficultyHitObject class is the one getting used in all pp/star calculations, it encompasses every object
    // type for simplicity

    // load primitive arrays
    PRIMITIVE_CONTAINER c = loadPrimitiveObjects(osuFilePath, dead);
    if(c.errorCode != 0) {
        result.errorCode = c.errorCode;
        return result;
    }

    // calculate sliderTimes, and build slider clicks and ticks
    CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT sliderTimeCalcResult = calculateSliderTimesClicksTicks(
        c.version, c.sliders, c.timingpoints, c.sliderMultiplier, c.sliderTickRate, dead);
    if(sliderTimeCalcResult.errorCode != 0) {
        result.errorCode = sliderTimeCalcResult.errorCode;
        return result;
    }

    // now we can calculate the max possible combo (because that needs ticks/clicks to be filled, mostly convenience)
    {
        result.maxPossibleCombo += c.hitcircles.size();
        for(int i = 0; i < c.sliders.size(); i++) {
            const SLIDER &s = c.sliders[i];

            const int repeats = max((s.repeat - 1), 0);
            result.maxPossibleCombo +=
                2 + repeats + (repeats + 1) * s.ticks.size();  // start/end + repeat arrow + ticks
        }
        result.maxPossibleCombo += c.spinners.size();
    }

    // and generate the difficultyhitobjects
    result.diffobjects.reserve(c.hitcircles.size() + c.sliders.size() + c.spinners.size());

    for(int i = 0; i < c.hitcircles.size(); i++) {
        result.diffobjects.push_back(OsuDifficultyHitObject(OsuDifficultyHitObject::TYPE::CIRCLE,
                                                            Vector2(c.hitcircles[i].x, c.hitcircles[i].y),
                                                            (long)c.hitcircles[i].time));
    }

    const bool calculateSliderCurveInConstructor =
        (c.sliders.size() < 5000);  // NOTE: for explanation see OsuDifficultyHitObject constructor
    for(int i = 0; i < c.sliders.size(); i++) {
        if(dead.load()) {
            result.errorCode = 6;
            return result;
        }

        if(!calculateStarsInaccurately) {
            result.diffobjects.push_back(OsuDifficultyHitObject(
                OsuDifficultyHitObject::TYPE::SLIDER, Vector2(c.sliders[i].x, c.sliders[i].y), c.sliders[i].time,
                c.sliders[i].time + (long)c.sliders[i].sliderTime, c.sliders[i].sliderTimeWithoutRepeats,
                c.sliders[i].type, c.sliders[i].points, c.sliders[i].pixelLength, c.sliders[i].scoringTimesForStarCalc,
                c.sliders[i].repeat, calculateSliderCurveInConstructor));
        } else {
            result.diffobjects.push_back(OsuDifficultyHitObject(
                OsuDifficultyHitObject::TYPE::SLIDER, Vector2(c.sliders[i].x, c.sliders[i].y), c.sliders[i].time,
                c.sliders[i].time + (long)c.sliders[i].sliderTime, c.sliders[i].sliderTimeWithoutRepeats,
                c.sliders[i].type,
                std::vector<Vector2>(),  // NOTE: ignore curve when calculating inaccurately
                c.sliders[i].pixelLength,
                std::vector<OsuDifficultyHitObject::SLIDER_SCORING_TIME>(),  // NOTE: ignore curve when calculating
                                                                             // inaccurately
                c.sliders[i].repeat,
                false));  // NOTE: ignore curve when calculating inaccurately
        }
    }

    for(int i = 0; i < c.spinners.size(); i++) {
        result.diffobjects.push_back(OsuDifficultyHitObject(OsuDifficultyHitObject::TYPE::SPINNER,
                                                            Vector2(c.spinners[i].x, c.spinners[i].y),
                                                            (long)c.spinners[i].time, (long)c.spinners[i].endTime));
    }

    // sort hitobjects by time
    struct DiffHitObjectSortComparator {
        bool operator()(const OsuDifficultyHitObject &a, const OsuDifficultyHitObject &b) const {
            // strict weak ordering!
            if(a.time == b.time)
                return a.sortHack < b.sortHack;
            else
                return a.time < b.time;
        }
    };
    std::sort(result.diffobjects.begin(), result.diffobjects.end(), DiffHitObjectSortComparator());

    // calculate stacks
    // see Beatmap.cpp
    // NOTE: this must be done before the speed multiplier is applied!
    // HACKHACK: code duplication ffs
    if(m_osu_stars_stacking_ref->getBool() &&
       !calculateStarsInaccurately)  // NOTE: ignore stacking when calculating inaccurately
    {
        const float finalAR = AR;
        const float finalCS = CS;
        const float rawHitCircleDiameter = GameRules::getRawHitCircleDiameter(finalCS);

        const float STACK_LENIENCE = 3.0f;

        const float approachTime = GameRules::getApproachTimeForStacking(finalAR);

        if(c.version > 5) {
            // peppy's algorithm
            // https://gist.github.com/peppy/1167470

            for(int i = result.diffobjects.size() - 1; i >= 0; i--) {
                int n = i;

                OsuDifficultyHitObject *objectI = &result.diffobjects[i];

                const bool isSpinner = (objectI->type == OsuDifficultyHitObject::TYPE::SPINNER);

                if(objectI->stack != 0 || isSpinner) continue;

                const bool isHitCircle = (objectI->type == OsuDifficultyHitObject::TYPE::CIRCLE);
                const bool isSlider = (objectI->type == OsuDifficultyHitObject::TYPE::SLIDER);

                if(isHitCircle) {
                    while(--n >= 0) {
                        OsuDifficultyHitObject *objectN = &result.diffobjects[n];

                        const bool isSpinnerN = (objectN->type == OsuDifficultyHitObject::TYPE::SPINNER);

                        if(isSpinnerN) continue;

                        if(objectI->time - (approachTime * c.stackLeniency) > (objectN->endTime)) break;

                        Vector2 objectNEndPosition =
                            objectN->getOriginalRawPosAt(objectN->time + objectN->getDuration());
                        if(objectN->getDuration() != 0 &&
                           (objectNEndPosition - objectI->getOriginalRawPosAt(objectI->time)).length() <
                               STACK_LENIENCE) {
                            int offset = objectI->stack - objectN->stack + 1;
                            for(int j = n + 1; j <= i; j++) {
                                if((objectNEndPosition -
                                    result.diffobjects[j].getOriginalRawPosAt(result.diffobjects[j].time))
                                       .length() < STACK_LENIENCE)
                                    result.diffobjects[j].stack = (result.diffobjects[j].stack - offset);
                            }

                            break;
                        }

                        if((objectN->getOriginalRawPosAt(objectN->time) - objectI->getOriginalRawPosAt(objectI->time))
                               .length() < STACK_LENIENCE) {
                            objectN->stack = (objectI->stack + 1);
                            objectI = objectN;
                        }
                    }
                } else if(isSlider) {
                    while(--n >= 0) {
                        OsuDifficultyHitObject *objectN = &result.diffobjects[n];

                        const bool isSpinner = (objectN->type == OsuDifficultyHitObject::TYPE::SPINNER);

                        if(isSpinner) continue;

                        if(objectI->time - (approachTime * c.stackLeniency) > objectN->time) break;

                        if(((objectN->getDuration() != 0
                                 ? objectN->getOriginalRawPosAt(objectN->time + objectN->getDuration())
                                 : objectN->getOriginalRawPosAt(objectN->time)) -
                            objectI->getOriginalRawPosAt(objectI->time))
                               .length() < STACK_LENIENCE) {
                            objectN->stack = (objectI->stack + 1);
                            objectI = objectN;
                        }
                    }
                }
            }
        } else  // version < 6
        {
            // old stacking algorithm for old beatmaps
            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Beatmaps/BeatmapProcessor.cs

            for(int i = 0; i < result.diffobjects.size(); i++) {
                OsuDifficultyHitObject *currHitObject = &result.diffobjects[i];

                const bool isSlider = (currHitObject->type == OsuDifficultyHitObject::TYPE::SLIDER);

                if(currHitObject->stack != 0 && !isSlider) continue;

                long startTime = currHitObject->time + currHitObject->getDuration();
                int sliderStack = 0;

                for(int j = i + 1; j < result.diffobjects.size(); j++) {
                    OsuDifficultyHitObject *objectJ = &result.diffobjects[j];

                    if(objectJ->time - (approachTime * c.stackLeniency) > startTime) break;

                    // "The start position of the hitobject, or the position at the end of the path if the hitobject is
                    // a slider"
                    Vector2 position2 =
                        isSlider
                            ? currHitObject->getOriginalRawPosAt(currHitObject->time + currHitObject->getDuration())
                            : currHitObject->getOriginalRawPosAt(currHitObject->time);

                    if((objectJ->getOriginalRawPosAt(objectJ->time) -
                        currHitObject->getOriginalRawPosAt(currHitObject->time))
                           .length() < 3) {
                        currHitObject->stack++;
                        startTime = objectJ->time + objectJ->getDuration();
                    } else if((objectJ->getOriginalRawPosAt(objectJ->time) - position2).length() < 3) {
                        // "Case for sliders - bump notes down and right, rather than up and left."
                        sliderStack++;
                        objectJ->stack -= sliderStack;
                        startTime = objectJ->time + objectJ->getDuration();
                    }
                }
            }
        }

        // update hitobject positions
        float stackOffset = rawHitCircleDiameter / 128.0f / GameRules::broken_gamefield_rounding_allowance * 6.4f;
        for(int i = 0; i < result.diffobjects.size(); i++) {
            if(dead.load()) {
                result.errorCode = 6;
                return result;
            }

            if(result.diffobjects[i].stack != 0) result.diffobjects[i].updateStackPosition(stackOffset);
        }
    }

    // apply speed multiplier (if present)
    if(speedMultiplier != 1.0f && speedMultiplier > 0.0f) {
        const double invSpeedMultiplier = 1.0 / (double)speedMultiplier;
        for(int i = 0; i < result.diffobjects.size(); i++) {
            if(dead.load()) {
                result.errorCode = 6;
                return result;
            }

            result.diffobjects[i].time = (long)((double)result.diffobjects[i].time * invSpeedMultiplier);
            result.diffobjects[i].endTime = (long)((double)result.diffobjects[i].endTime * invSpeedMultiplier);

            if(!calculateStarsInaccurately)  // NOTE: ignore slider curves when calculating inaccurately
            {
                result.diffobjects[i].spanDuration = (double)result.diffobjects[i].spanDuration * invSpeedMultiplier;
                for(int s = 0; s < result.diffobjects[i].scoringTimes.size(); s++) {
                    result.diffobjects[i].scoringTimes[s].time =
                        (long)((double)result.diffobjects[i].scoringTimes[s].time * invSpeedMultiplier);
                }
            }
        }
    }

    return result;
}

bool DatabaseBeatmap::loadMetadata() {
    if(m_difficulties != NULL) return false;  // we are a beatmapset, not a difficulty

    // reset
    m_timingpoints.clear();

    if(Osu::debug->getBool()) debugLog("DatabaseBeatmap::loadMetadata() : %s\n", m_sFilePath.c_str());

    // generate MD5 hash (loads entire file, very slow)
    {
        File file(m_sFilePath);

        const u8 *beatmapFile = NULL;
        size_t beatmapFileSize = 0;
        {
            if(file.canRead()) {
                beatmapFile = file.readFile();
                beatmapFileSize = file.getFileSize();
            }
        }

        if(beatmapFile != NULL) {
            auto hash = md5((u8 *)beatmapFile, beatmapFileSize);
            m_sMD5Hash = MD5Hash(hash.toUtf8());
        }
    }

    // open osu file again, but this time for parsing
    bool foundAR = false;

    File file(m_sFilePath);
    if(!file.canRead()) {
        debugLog("Osu Error: Couldn't read file %s\n", m_sFilePath.c_str());
        return false;
    }

    std::istringstream ss("");

    // load metadata only
    int curBlock = -1;
    unsigned long long timingPointSortHack = 0;
    char stringBuffer[1024];
    std::string curLine;
    while(file.canRead()) {
        curLine = file.readLine();

        // ignore comments, but only if at the beginning of
        // a line (e.g. allow Artist:DJ'TEKINA//SOMETHING)
        if(curLine.find("//") == 0) continue;

        const char *curLineChar = curLine.c_str();
        if(curLine.find("[General]") != std::string::npos)
            curBlock = 0;
        else if(curLine.find("[Metadata]") != std::string::npos)
            curBlock = 1;
        else if(curLine.find("[Difficulty]") != std::string::npos)
            curBlock = 2;
        else if(curLine.find("[Events]") != std::string::npos)
            curBlock = 3;
        else if(curLine.find("[TimingPoints]") != std::string::npos)
            curBlock = 4;
        else if(curLine.find("[HitObjects]") != std::string::npos)
            break;  // NOTE: stop early

        switch(curBlock) {
            case -1:  // header (e.g. "osu file format v12")
            {
                if(sscanf(curLineChar, " osu file format v %i \n", &m_iVersion) == 1) {
                    if(m_iVersion > osu_beatmap_version.getInt()) {
                        debugLog("Ignoring unknown/invalid beatmap version %i\n", m_iVersion);
                        return false;
                    }
                }
            } break;

            case 0:  // General
            {
                memset(stringBuffer, '\0', 1024);
                if(sscanf(curLineChar, " AudioFilename : %1023[^\n]", stringBuffer) == 1) {
                    m_sAudioFileName = stringBuffer;
                    trim(&m_sAudioFileName);
                }

                sscanf(curLineChar, " StackLeniency : %f \n", &m_fStackLeniency);
                sscanf(curLineChar, " PreviewTime : %i \n", &m_iPreviewTime);
                sscanf(curLineChar, " Mode : %i \n", &m_iGameMode);
            } break;

            case 1:  // Metadata
            {
                memset(stringBuffer, '\0', 1024);
                if(sscanf(curLineChar, " Title :%1023[^\n]", stringBuffer) == 1) {
                    m_sTitle = UString(stringBuffer);
                    trim(&m_sTitle);
                }

                memset(stringBuffer, '\0', 1024);
                if(sscanf(curLineChar, " Artist :%1023[^\n]", stringBuffer) == 1) {
                    m_sArtist = UString(stringBuffer);
                    trim(&m_sArtist);
                }

                memset(stringBuffer, '\0', 1024);
                if(sscanf(curLineChar, " Creator :%1023[^\n]", stringBuffer) == 1) {
                    m_sCreator = UString(stringBuffer);
                    trim(&m_sCreator);
                }

                memset(stringBuffer, '\0', 1024);
                if(sscanf(curLineChar, " Version :%1023[^\n]", stringBuffer) == 1) {
                    m_sDifficultyName = UString(stringBuffer);
                    trim(&m_sDifficultyName);
                }

                memset(stringBuffer, '\0', 1024);
                if(sscanf(curLineChar, " Source :%1023[^\n]", stringBuffer) == 1) {
                    m_sSource = stringBuffer;
                    trim(&m_sSource);
                }

                memset(stringBuffer, '\0', 1024);
                if(sscanf(curLineChar, " Tags :%1023[^\n]", stringBuffer) == 1) {
                    m_sTags = stringBuffer;
                    trim(&m_sTags);
                }

                sscanf(curLineChar, " BeatmapID : %ld \n", &m_iID);
                sscanf(curLineChar, " BeatmapSetID : %i \n", &m_iSetID);
            } break;

            case 2:  // Difficulty
            {
                sscanf(curLineChar, " CircleSize : %f \n", &m_fCS);
                if(sscanf(curLineChar, " ApproachRate : %f \n", &m_fAR) == 1) foundAR = true;
                sscanf(curLineChar, " HPDrainRate : %f \n", &m_fHP);
                sscanf(curLineChar, " OverallDifficulty : %f \n", &m_fOD);
                sscanf(curLineChar, " SliderMultiplier : %f \n", &m_fSliderMultiplier);
                sscanf(curLineChar, " SliderTickRate : %f \n", &m_fSliderTickRate);
            } break;

            case 3:  // Events
            {
                memset(stringBuffer, '\0', 1024);
                int type, startTime;
                if(sscanf(curLineChar, " %i , %i , \"%1023[^\"]\"", &type, &startTime, stringBuffer) == 3) {
                    if(type == 0) {
                        m_sBackgroundImageFileName = UString(stringBuffer);
                        m_sFullBackgroundImageFilePath = m_sFolder;
                        m_sFullBackgroundImageFilePath.append(m_sBackgroundImageFileName);
                    }
                }
            } break;

            case 4:  // TimingPoints
            {
                // old beatmaps: Offset, Milliseconds per Beat
                // old new beatmaps: Offset, Milliseconds per Beat, Meter, Sample Type, Sample Set, Volume,
                // !Inherited new new beatmaps: Offset, Milliseconds per Beat, Meter, Sample Type, Sample Set,
                // Volume, !Inherited, Kiai Mode

                double tpOffset;
                float tpMSPerBeat;
                int tpMeter;
                int tpSampleType, tpSampleSet;
                int tpVolume;
                int tpTimingChange;
                int tpKiai = 0;  // optional
                if(sscanf(curLineChar, " %lf , %f , %i , %i , %i , %i , %i , %i", &tpOffset, &tpMSPerBeat, &tpMeter,
                          &tpSampleType, &tpSampleSet, &tpVolume, &tpTimingChange, &tpKiai) == 8 ||
                   sscanf(curLineChar, " %lf , %f , %i , %i , %i , %i , %i", &tpOffset, &tpMSPerBeat, &tpMeter,
                          &tpSampleType, &tpSampleSet, &tpVolume, &tpTimingChange) == 7) {
                    TIMINGPOINT t;
                    {
                        t.offset = (long)std::round(tpOffset);
                        t.msPerBeat = tpMSPerBeat;

                        t.sampleType = tpSampleType;
                        t.sampleSet = tpSampleSet;
                        t.volume = tpVolume;

                        t.timingChange = tpTimingChange == 1;
                        t.kiai = tpKiai > 0;

                        t.sortHack = timingPointSortHack++;
                    }
                    m_timingpoints.push_back(t);
                } else if(sscanf(curLineChar, " %lf , %f", &tpOffset, &tpMSPerBeat) == 2) {
                    TIMINGPOINT t;
                    {
                        t.offset = (long)std::round(tpOffset);
                        t.msPerBeat = tpMSPerBeat;

                        t.sampleType = 0;
                        t.sampleSet = 0;
                        t.volume = 100;

                        t.timingChange = true;
                        t.kiai = false;

                        t.sortHack = timingPointSortHack++;
                    }
                    m_timingpoints.push_back(t);
                }
            } break;
        }
    }

    // gamemode filter
    if(m_iGameMode != 0) return false;  // nothing more to do here

    // general sanity checks
    if((m_timingpoints.size() < 1)) {
        if(Osu::debug->getBool()) debugLog("DatabaseBeatmap::loadMetadata() : no timingpoints in beatmap!\n");
        return false;  // nothing more to do here
    }

    // build sound file path
    m_sFullSoundFilePath = m_sFolder;
    m_sFullSoundFilePath.append(m_sAudioFileName);

    // sort timingpoints and calculate BPM range
    if(m_timingpoints.size() > 0) {
        // sort timingpoints by time
        std::sort(m_timingpoints.begin(), m_timingpoints.end(), TimingPointSortComparator());

        // NOTE: if we have our own stars/bpm cached then use that
        bool bpm_was_cached = false;
        auto db = osu->getSongBrowser()->getDatabase();
        const auto result = db->m_starsCache.find(getMD5Hash());
        if(result != db->m_starsCache.end()) {
            if(result->second.starsNomod >= 0.f) {
                m_fStarsNomod = result->second.starsNomod;
            }
            if(result->second.min_bpm >= 0) {
                m_iMinBPM = result->second.min_bpm;
                m_iMaxBPM = result->second.max_bpm;
                m_iMostCommonBPM = result->second.common_bpm;
                bpm_was_cached = true;
            }
        }

        if(!bpm_was_cached) {
            if(Osu::debug->getBool()) debugLog("DatabaseBeatmap::loadMetadata() : calculating BPM range ...\n");
            auto bpm = getBPM(m_timingpoints);
            m_iMinBPM = bpm.min;
            m_iMaxBPM = bpm.max;
            m_iMostCommonBPM = bpm.most_common;
        }
    }

    // special case: old beatmaps have AR = OD, there is no ApproachRate stored
    if(!foundAR) m_fAR = m_fOD;

    return true;
}

DatabaseBeatmap::LOAD_GAMEPLAY_RESULT DatabaseBeatmap::loadGameplay(DatabaseBeatmap *databaseBeatmap,
                                                                    Beatmap *beatmap) {
    LOAD_GAMEPLAY_RESULT result = LOAD_GAMEPLAY_RESULT();

    // NOTE: reload metadata (force ensures that all necessary data is ready for creating hitobjects and playing etc.,
    // also if beatmap file is changed manually in the meantime)
    if(!databaseBeatmap->loadMetadata()) {
        result.errorCode = 1;
        return result;
    }

    // load primitives, put in temporary container
    PRIMITIVE_CONTAINER c = loadPrimitiveObjects(databaseBeatmap->m_sFilePath);
    if(c.errorCode != 0) {
        result.errorCode = c.errorCode;
        return result;
    }
    result.breaks = std::move(c.breaks);
    result.combocolors = std::move(c.combocolors);

    // override some values with data from primitive load, even though they should already be loaded from metadata
    // (sanity)
    databaseBeatmap->m_timingpoints.swap(c.timingpoints);
    databaseBeatmap->m_fSliderMultiplier = c.sliderMultiplier;
    databaseBeatmap->m_fSliderTickRate = c.sliderTickRate;
    databaseBeatmap->m_fStackLeniency = c.stackLeniency;
    databaseBeatmap->m_iVersion = c.version;

    // check if we have any timingpoints at all
    if(databaseBeatmap->m_timingpoints.size() == 0) {
        result.errorCode = 3;
        return result;
    }

    // update numObjects
    databaseBeatmap->m_iNumObjects = c.hitcircles.size() + c.sliders.size() + c.spinners.size();
    databaseBeatmap->m_iNumCircles = c.hitcircles.size();
    databaseBeatmap->m_iNumSliders = c.sliders.size();
    databaseBeatmap->m_iNumSpinners = c.spinners.size();

    // check if we have any hitobjects at all
    if(databaseBeatmap->m_iNumObjects < 1) {
        result.errorCode = 4;
        return result;
    }

    // calculate sliderTimes, and build slider clicks and ticks
    CALCULATE_SLIDER_TIMES_CLICKS_TICKS_RESULT sliderTimeCalcResult =
        calculateSliderTimesClicksTicks(c.version, c.sliders, databaseBeatmap->m_timingpoints,
                                        databaseBeatmap->m_fSliderMultiplier, databaseBeatmap->m_fSliderTickRate);
    if(sliderTimeCalcResult.errorCode != 0) {
        result.errorCode = sliderTimeCalcResult.errorCode;
        return result;
    }

    // build hitobjects from the primitive data we loaded from the osu file
    {
        // also calculate max possible combo
        int maxPossibleCombo = 0;

        for(size_t i = 0; i < c.hitcircles.size(); i++) {
            HITCIRCLE &h = c.hitcircles[i];

            result.hitobjects.push_back(
                new Circle(h.x, h.y, h.time, h.sampleType, h.number, false, h.colorCounter, h.colorOffset, beatmap));
        }
        maxPossibleCombo += c.hitcircles.size();

        for(size_t i = 0; i < c.sliders.size(); i++) {
            SLIDER &s = c.sliders[i];

            if(osu_mod_strict_tracking.getBool() && osu_mod_strict_tracking_remove_slider_ticks.getBool())
                s.ticks.clear();

            if(osu_mod_reverse_sliders.getBool()) std::reverse(s.points.begin(), s.points.end());

            result.hitobjects.push_back(new Slider(s.type, s.repeat, s.pixelLength, s.points, s.hitSounds, s.ticks,
                                                   s.sliderTime, s.sliderTimeWithoutRepeats, s.time, s.sampleType,
                                                   s.number, false, s.colorCounter, s.colorOffset, beatmap));

            const int repeats = max((s.repeat - 1), 0);
            maxPossibleCombo += 2 + repeats + (repeats + 1) * s.ticks.size();  // start/end + repeat arrow + ticks
        }

        for(size_t i = 0; i < c.spinners.size(); i++) {
            SPINNER &s = c.spinners[i];
            result.hitobjects.push_back(new Spinner(s.x, s.y, s.time, s.sampleType, false, s.endTime, beatmap));
        }
        maxPossibleCombo += c.spinners.size();

        beatmap->setMaxPossibleCombo(maxPossibleCombo);

        // debug
        if(m_osu_debug_pp_ref->getBool()) {
            const std::string &osuFilePath = databaseBeatmap->m_sFilePath;
            const float AR = beatmap->getAR();
            const float CS = beatmap->getCS();
            const float OD = beatmap->getOD();
            const float speedMultiplier = osu->getSpeedMultiplier();  // NOTE: not this->getSpeedMultiplier()!
            const bool relax = osu->getModRelax();
            const bool touchDevice = osu->getModTD();

            LOAD_DIFFOBJ_RESULT diffres =
                DatabaseBeatmap::loadDifficultyHitObjects(osuFilePath, AR, CS, speedMultiplier);

            double aim = 0.0;
            double aimSliderFactor = 0.0;
            double speed = 0.0;
            double speedNotes = 0.0;
            std::vector<double> m_aimStrains;
            std::vector<double> m_speedStrains;
            double stars = DifficultyCalculator::calculateStarDiffForHitObjects(
                diffres.diffobjects, CS, OD, speedMultiplier, relax, touchDevice, &aim, &aimSliderFactor, &speed,
                &speedNotes, -1, &m_aimStrains, &m_speedStrains);
            double pp = DifficultyCalculator::calculatePPv2(
                beatmap, aim, aimSliderFactor, speed, speedNotes, databaseBeatmap->m_iNumObjects,
                databaseBeatmap->m_iNumCircles, databaseBeatmap->m_iNumSliders, databaseBeatmap->m_iNumSpinners,
                maxPossibleCombo);

            engine->showMessageInfo(
                "PP",
                UString::format("pp = %f, stars = %f, aimstars = %f, speedstars = %f, %i circles, %i "
                                "sliders, %i spinners, %i hitobjects, maxcombo = %i",
                                pp, stars, aim, speed, databaseBeatmap->m_iNumCircles, databaseBeatmap->m_iNumSliders,
                                databaseBeatmap->m_iNumSpinners, databaseBeatmap->m_iNumObjects, maxPossibleCombo));
        }
    }

    // sort hitobjects by starttime
    struct HitObjectSortComparator {
        bool operator()(HitObject const *a, HitObject const *b) const {
            // strict weak ordering!
            if(a->getTime() == b->getTime())
                return a->getSortHack() < b->getSortHack();
            else
                return a->getTime() < b->getTime();
        }
    };
    std::sort(result.hitobjects.begin(), result.hitobjects.end(), HitObjectSortComparator());

    // update beatmap length stat
    if(databaseBeatmap->m_iLengthMS == 0 && result.hitobjects.size() > 0)
        databaseBeatmap->m_iLengthMS = result.hitobjects[result.hitobjects.size() - 1]->getTime() +
                                       result.hitobjects[result.hitobjects.size() - 1]->getDuration();

    // set isEndOfCombo + precalculate Score v2 combo portion maximum
    if(beatmap != NULL) {
        unsigned long long scoreV2ComboPortionMaximum = 1;

        if(result.hitobjects.size() > 0) scoreV2ComboPortionMaximum = 0;

        int combo = 0;
        for(size_t i = 0; i < result.hitobjects.size(); i++) {
            HitObject *currentHitObject = result.hitobjects[i];
            const HitObject *nextHitObject = (i + 1 < result.hitobjects.size() ? result.hitobjects[i + 1] : NULL);

            const Circle *circlePointer = dynamic_cast<Circle *>(currentHitObject);
            const Slider *sliderPointer = dynamic_cast<Slider *>(currentHitObject);
            const Spinner *spinnerPointer = dynamic_cast<Spinner *>(currentHitObject);

            int scoreComboMultiplier = max(combo - 1, 0);

            if(circlePointer != NULL || spinnerPointer != NULL) {
                scoreV2ComboPortionMaximum += (unsigned long long)(300.0 * (1.0 + (double)scoreComboMultiplier / 10.0));
                combo++;
            } else if(sliderPointer != NULL) {
                combo += 1 + sliderPointer->getClicks().size();
                scoreComboMultiplier = max(combo - 1, 0);
                scoreV2ComboPortionMaximum += (unsigned long long)(300.0 * (1.0 + (double)scoreComboMultiplier / 10.0));
                combo++;
            }

            if(nextHitObject == NULL || nextHitObject->getComboNumber() == 1) currentHitObject->setIsEndOfCombo(true);
        }

        beatmap->setScoreV2ComboPortionMaximum(scoreV2ComboPortionMaximum);
    }

    // special rule for first hitobject (for 1 approach circle with HD)
    if(osu_show_approach_circle_on_first_hidden_object.getBool()) {
        if(result.hitobjects.size() > 0) result.hitobjects[0]->setForceDrawApproachCircle(true);
    }

    // custom override for forcing a hard number cap and/or sequence (visually only)
    // NOTE: this is done after we have already calculated/set isEndOfCombos
    {
        if(osu_ignore_beatmap_combo_numbers.getBool()) {
            // NOTE: spinners don't increment the combo number
            int comboNumber = 1;
            for(size_t i = 0; i < result.hitobjects.size(); i++) {
                HitObject *currentHitObject = result.hitobjects[i];

                const Spinner *spinnerPointer = dynamic_cast<Spinner *>(currentHitObject);

                if(spinnerPointer == NULL) {
                    currentHitObject->setComboNumber(comboNumber);
                    comboNumber++;
                }
            }
        }

        const int numberMax = osu_number_max.getInt();
        if(numberMax > 0) {
            for(size_t i = 0; i < result.hitobjects.size(); i++) {
                HitObject *currentHitObject = result.hitobjects[i];

                const int currentComboNumber = currentHitObject->getComboNumber();
                const int newComboNumber = (currentComboNumber % numberMax);

                currentHitObject->setComboNumber(newComboNumber == 0 ? numberMax : newComboNumber);
            }
        }
    }

    debugLog("DatabaseBeatmap::load() loaded %i hitobjects\n", result.hitobjects.size());

    return result;
}

DatabaseBeatmap::TIMING_INFO DatabaseBeatmap::getTimingInfoForTime(unsigned long positionMS) {
    return getTimingInfoForTimeAndTimingPoints(positionMS, m_timingpoints);
}

DatabaseBeatmap::TIMING_INFO DatabaseBeatmap::getTimingInfoForTimeAndTimingPoints(
    unsigned long positionMS, const zarray<TIMINGPOINT> &timingpoints) {
    TIMING_INFO ti;
    ti.offset = 0;
    ti.beatLengthBase = 1;
    ti.beatLength = 1;
    ti.volume = 100;
    ti.sampleType = 0;
    ti.sampleSet = 0;
    ti.isNaN = false;

    if(timingpoints.size() < 1) return ti;

    // initial values
    ti.offset = timingpoints[0].offset;
    ti.volume = timingpoints[0].volume;
    ti.sampleSet = timingpoints[0].sampleSet;
    ti.sampleType = timingpoints[0].sampleType;

    // new (peppy's algorithm)
    // (correctly handles aspire & NaNs)
    {
        const bool allowMultiplier = true;

        int point = 0;
        int samplePoint = 0;
        int audioPoint = 0;

        for(int i = 0; i < timingpoints.size(); i++) {
            if(timingpoints[i].offset <= positionMS) {
                audioPoint = i;

                if(timingpoints[i].timingChange)
                    point = i;
                else
                    samplePoint = i;
            }
        }

        double mult = 1;

        if(allowMultiplier && samplePoint > point && timingpoints[samplePoint].msPerBeat < 0) {
            if(timingpoints[samplePoint].msPerBeat >= 0)
                mult = 1;
            else
                mult = clamp<float>((float)-timingpoints[samplePoint].msPerBeat, 10.0f, 1000.0f) / 100.0f;
        }

        ti.beatLengthBase = timingpoints[point].msPerBeat;
        ti.offset = timingpoints[point].offset;

        ti.isNaN = std::isnan(timingpoints[samplePoint].msPerBeat) || std::isnan(timingpoints[point].msPerBeat);
        ti.beatLength = ti.beatLengthBase * mult;

        ti.volume = timingpoints[audioPoint].volume;
        ti.sampleType = timingpoints[audioPoint].sampleType;
        ti.sampleSet = timingpoints[audioPoint].sampleSet;
    }

    return ti;
}

DatabaseBeatmapBackgroundImagePathLoader::DatabaseBeatmapBackgroundImagePathLoader(const std::string &filePath)
    : Resource() {
    m_sFilePath = filePath;
}

void DatabaseBeatmapBackgroundImagePathLoader::init() {
    // (nothing)
    m_bReady = true;
}

void DatabaseBeatmapBackgroundImagePathLoader::initAsync() {
    File file(m_sFilePath);
    if(!file.canRead()) return;

    int curBlock = -1;
    char stringBuffer[1024];
    while(file.canRead()) {
        std::string curLine = file.readLine();
        const int commentIndex = curLine.find("//");
        if(commentIndex == std::string::npos ||
           commentIndex !=
               0)  // ignore comments, but only if at the beginning of a line (e.g. allow Artist:DJ'TEKINA//SOMETHING)
        {
            if(curLine.find("[Events]") != std::string::npos)
                curBlock = 1;
            else if(curLine.find("[TimingPoints]") != std::string::npos)
                break;  // NOTE: stop early
            else if(curLine.find("[Colours]") != std::string::npos)
                break;  // NOTE: stop early
            else if(curLine.find("[HitObjects]") != std::string::npos)
                break;  // NOTE: stop early

            switch(curBlock) {
                case 1:  // Events
                {
                    memset(stringBuffer, '\0', 1024);
                    int type, startTime;
                    if(sscanf(curLine.c_str(), " %i , %i , \"%1023[^\"]\"", &type, &startTime, stringBuffer) == 3) {
                        if(type == 0) m_sLoadedBackgroundImageFileName = UString(stringBuffer);
                    }
                } break;
            }
        }
    }

    m_bAsyncReady = true;
    m_bReady = true;  // NOTE: on purpose. there is nothing to do in init(), so finish 1 frame early
}

std::string DatabaseBeatmap::getFullSoundFilePath() {
    // On linux, paths are case sensitive, so we retry different variations
    if(env->getOS() != Environment::OS::LINUX || env->fileExists(m_sFullSoundFilePath)) {
        return m_sFullSoundFilePath;
    }

    // try uppercasing file extension
    for(int s = m_sFullSoundFilePath.size(); s >= 0; s--) {
        if(m_sFullSoundFilePath[s] == '.') {
            for(int i = s + 1; i < m_sFullSoundFilePath.size(); i++) {
                m_sFullSoundFilePath[i] = std::toupper(m_sFullSoundFilePath[i]);
            }
            break;
        }
    }
    if(env->fileExists(m_sFullSoundFilePath)) {
        return m_sFullSoundFilePath;
    }

    // try lowercasing filename, uppercasing file extension
    bool foundFilenameStart = false;
    for(int s = m_sFullSoundFilePath.size(); s >= 0; s--) {
        if(foundFilenameStart) {
            if(m_sFullSoundFilePath[s] == '/') break;
            m_sFullSoundFilePath[s] = std::tolower(m_sFullSoundFilePath[s]);
        }
        if(m_sFullSoundFilePath[s] == '.') {
            foundFilenameStart = true;
        }
    }
    if(env->fileExists(m_sFullSoundFilePath)) {
        return m_sFullSoundFilePath;
    }

    // try lowercasing everything
    for(int s = m_sFullSoundFilePath.size(); s >= 0; s--) {
        if(m_sFullSoundFilePath[s] == '/') {
            break;
        }
        m_sFullSoundFilePath[s] = std::tolower(m_sFullSoundFilePath[s]);
    }
    if(env->fileExists(m_sFullSoundFilePath)) {
        return m_sFullSoundFilePath;
    }

    // give up
    return m_sFullSoundFilePath;
}
