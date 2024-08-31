#include "DifficultyCalculator.h"

#include "Beatmap.h"
#include "ConVar.h"
#include "Engine.h"
#include "GameRules.h"
#include "LegacyReplay.h"
#include "Osu.h"
#include "SliderCurves.h"

using namespace std;

u64 OsuDifficultyHitObject::sortHackCounter = 0;

OsuDifficultyHitObject::OsuDifficultyHitObject(TYPE type, Vector2 pos, i32 time)
    : OsuDifficultyHitObject(type, pos, time, time) {}

OsuDifficultyHitObject::OsuDifficultyHitObject(TYPE type, Vector2 pos, i32 time, i32 endTime)
    : OsuDifficultyHitObject(type, pos, time, endTime, 0.0f, '\0', std::vector<Vector2>(), 0.0f,
                             std::vector<SLIDER_SCORING_TIME>(), 0, true) {}

OsuDifficultyHitObject::OsuDifficultyHitObject(TYPE type, Vector2 pos, i32 time, i32 endTime, f32 spanDuration,
                                               u8 osuSliderCurveType, std::vector<Vector2> controlPoints,
                                               f32 pixelLength, std::vector<SLIDER_SCORING_TIME> scoringTimes,
                                               i32 repeats, bool calculateSliderCurveInConstructor) {
    this->type = type;
    this->pos = pos;
    this->time = time;
    this->endTime = endTime;
    this->spanDuration = spanDuration;
    this->osuSliderCurveType = osuSliderCurveType;
    this->pixelLength = pixelLength;
    this->scoringTimes = scoringTimes;

    this->curve = NULL;
    this->scheduledCurveAlloc = false;
    this->scheduledCurveAllocStackOffset = 0.0f;
    this->repeats = repeats;

    this->stack = 0;
    this->originalPos = this->pos;
    this->sortHack = sortHackCounter++;

    // build slider curve, if this is a (valid) slider
    if(this->type == TYPE::SLIDER && controlPoints.size() > 1) {
        if(calculateSliderCurveInConstructor) {
            // old: too much kept memory allocations for over 14000 sliders in
            // https://osu.ppy.sh/beatmapsets/592138#osu/1277649

            // NOTE: this is still used for beatmaps with less than 5000 sliders (because precomputing all curves is way
            // faster, especially for star cache loader) NOTE: the 5000 slider threshold was chosen by looking at the
            // longest non-aspire marathon maps NOTE: 15000 slider curves use ~1.6 GB of RAM, for 32-bit executables
            // with 2GB cap limiting to 5000 sliders gives ~530 MB which should be survivable without crashing (even
            // though the heap gets fragmented to fuck)

            // 6000 sliders @ The Weather Channel - 139 Degrees (Tiggz Mumbson) [Special Weather Statement].osu
            // 3674 sliders @ Various Artists - Alternator Compilation (Monstrata) [Marathon].osu
            // 4599 sliders @ Renard - Because Maybe! (Mismagius) [- Nogard Marathon -].osu
            // 4921 sliders @ Renard - Because Maybe! (Mismagius) [- Nogard Marathon v2 -].osu
            // 14960 sliders @ pishifat - H E L L O  T H E R E (Kondou-Shinichi) [Sliders in the 69th centries].osu
            // 5208 sliders @ MillhioreF - haitai but every hai adds another haitai in the background (Chewy-san)
            // [Weriko Rank the dream (nerf) but loli].osu

            this->curve = SliderCurve::createCurve(this->osuSliderCurveType, controlPoints, this->pixelLength,
                                                   cv_stars_slider_curve_points_separation.getFloat());
        } else {
            // new: delay curve creation to when it's needed, and also immediately delete afterwards (at the cost of
            // having to store a copy of the control points)
            this->scheduledCurveAlloc = true;
            this->scheduledCurveAllocControlPoints = controlPoints;
        }
    }
}

OsuDifficultyHitObject::~OsuDifficultyHitObject() { SAFE_DELETE(curve); }

OsuDifficultyHitObject::OsuDifficultyHitObject(OsuDifficultyHitObject &&dobj) {
    // move
    this->type = dobj.type;
    this->pos = dobj.pos;
    this->time = dobj.time;
    this->endTime = dobj.endTime;
    this->spanDuration = dobj.spanDuration;
    this->osuSliderCurveType = dobj.osuSliderCurveType;
    this->pixelLength = dobj.pixelLength;
    this->scoringTimes = std::move(dobj.scoringTimes);

    this->curve = dobj.curve;
    this->scheduledCurveAlloc = dobj.scheduledCurveAlloc;
    this->scheduledCurveAllocControlPoints = std::move(dobj.scheduledCurveAllocControlPoints);
    this->scheduledCurveAllocStackOffset = dobj.scheduledCurveAllocStackOffset;
    this->repeats = dobj.repeats;

    this->stack = dobj.stack;
    this->originalPos = dobj.originalPos;
    this->sortHack = dobj.sortHack;

    // reset source
    dobj.curve = NULL;
    dobj.scheduledCurveAlloc = false;
}

OsuDifficultyHitObject &OsuDifficultyHitObject::operator=(OsuDifficultyHitObject &&dobj) {
    // move
    this->type = dobj.type;
    this->pos = dobj.pos;
    this->time = dobj.time;
    this->endTime = dobj.endTime;
    this->spanDuration = dobj.spanDuration;
    this->osuSliderCurveType = dobj.osuSliderCurveType;
    this->pixelLength = dobj.pixelLength;
    this->scoringTimes = std::move(dobj.scoringTimes);

    this->curve = dobj.curve;
    this->scheduledCurveAlloc = dobj.scheduledCurveAlloc;
    this->scheduledCurveAllocControlPoints = std::move(dobj.scheduledCurveAllocControlPoints);
    this->scheduledCurveAllocStackOffset = dobj.scheduledCurveAllocStackOffset;
    this->repeats = dobj.repeats;

    this->stack = dobj.stack;
    this->originalPos = dobj.originalPos;
    this->sortHack = dobj.sortHack;

    // reset source
    dobj.curve = NULL;
    dobj.scheduledCurveAlloc = false;

    return *this;
}

void OsuDifficultyHitObject::updateStackPosition(f32 stackOffset) {
    scheduledCurveAllocStackOffset = stackOffset;

    pos = originalPos - Vector2(stack * stackOffset, stack * stackOffset);

    updateCurveStackPosition(stackOffset);
}

void OsuDifficultyHitObject::updateCurveStackPosition(f32 stackOffset) {
    if(curve != NULL) curve->updateStackPosition(stack * stackOffset, false);
}

Vector2 OsuDifficultyHitObject::getOriginalRawPosAt(i32 pos) {
    // NOTE: the delayed curve creation has been deliberately disabled here for stacking purposes for beatmaps with
    // insane slider counts for performance reasons NOTE: this means that these aspire maps will have incorrect stars
    // due to incorrect slider stacking, but the delta is below 0.02 even for the most insane maps which currently exist
    // NOTE: if this were to ever get implemented properly, then a sliding window for curve allocation must be added to
    // the stack algorithm itself (as doing it in here is O(n!) madness) NOTE: to validate the delta, use Acid Rain
    // [Aspire] - Karoo13 (6.76* with slider stacks -> 6.75* without slider stacks)

    if(type != TYPE::SLIDER || (curve == NULL)) {
        return originalPos;
    } else {
        if(pos <= time)
            return curve->originalPointAt(0.0f);
        else if(pos >= endTime) {
            if(repeats % 2 == 0) {
                return curve->originalPointAt(0.0f);
            } else {
                return curve->originalPointAt(1.0f);
            }
        } else {
            return curve->originalPointAt(getT(pos, false));
        }
    }
}

f32 OsuDifficultyHitObject::getT(i32 pos, bool raw) {
    f32 t = (f32)((i32)pos - (i32)time) / spanDuration;
    if(raw)
        return t;
    else {
        f32 floorVal = (f32)std::floor(t);
        return ((i32)floorVal % 2 == 0) ? t - floorVal : floorVal + 1 - t;
    }
}

f64 DifficultyCalculator::calculateStarDiffForHitObjects(std::vector<OsuDifficultyHitObject> &sortedHitObjects, f32 CS,
                                                         f32 OD, f32 speedMultiplier, bool relax, bool touchDevice,
                                                         f64 *aim, f64 *aimSliderFactor, f64 *speed, f64 *speedNotes,
                                                         i32 upToObjectIndex, std::vector<f64> *outAimStrains,
                                                         std::vector<f64> *outSpeedStrains) {
    std::atomic<bool> dead;
    dead = false;
    return calculateStarDiffForHitObjects(sortedHitObjects, CS, OD, speedMultiplier, relax, touchDevice, aim,
                                          aimSliderFactor, speed, speedNotes, upToObjectIndex, outAimStrains,
                                          outSpeedStrains, dead);
}

f64 DifficultyCalculator::calculateStarDiffForHitObjects(std::vector<OsuDifficultyHitObject> &sortedHitObjects, f32 CS,
                                                         f32 OD, f32 speedMultiplier, bool relax, bool touchDevice,
                                                         f64 *aim, f64 *aimSliderFactor, f64 *speed, f64 *speedNotes,
                                                         i32 upToObjectIndex, std::vector<f64> *outAimStrains,
                                                         std::vector<f64> *outSpeedStrains,
                                                         const std::atomic<bool> &dead) {
    std::vector<DiffObject> emptyCachedDiffObjects;
    std::vector<DiffObject> diffObjects;
    return calculateStarDiffForHitObjectsInt(emptyCachedDiffObjects, diffObjects, sortedHitObjects, CS, OD,
                                             speedMultiplier, relax, touchDevice, aim, aimSliderFactor, speed,
                                             speedNotes, upToObjectIndex, outAimStrains, outSpeedStrains, dead);
}

f64 DifficultyCalculator::calculateStarDiffForHitObjectsInt(
    std::vector<DiffObject> &cachedDiffObjects, std::vector<DiffObject> &diffObjects,
    std::vector<OsuDifficultyHitObject> &sortedHitObjects, f32 CS, f32 OD, f32 speedMultiplier, bool relax,
    bool touchDevice, f64 *aim, f64 *aimSliderFactor, f64 *speed, f64 *speedNotes, i32 upToObjectIndex,
    std::vector<f64> *outAimStrains, std::vector<f64> *outSpeedStrains, const std::atomic<bool> &dead) {
    // NOTE: depends on speed multiplier + CS + OD + relax + touchDevice

    // NOTE: upToObjectIndex is applied way below, during the construction of the 'dobjects'

    // NOTE: osu always returns 0 stars for beatmaps with only 1 object, except if that object is a slider
    if(sortedHitObjects.size() < 2) {
        if(sortedHitObjects.size() < 1) return 0.0;
        if(sortedHitObjects[0].type != OsuDifficultyHitObject::TYPE::SLIDER) return 0.0;
    }

    // global independent variables/constants
    const bool applyBrokenGamefieldRoundingAllowance =
        false;  // NOTE: un-compensate because lazer doesn't use this (and lazer code is used for calculating global
                // online ranking pp/stars now)
    f32 circleRadiusInOsuPixels =
        GameRules::getRawHitCircleDiameter(clamp<float>(CS, 0.0f, 12.142f), applyBrokenGamefieldRoundingAllowance) /
        2.0f;  // NOTE: clamped CS because neosu allows CS > ~12.1429 (at which poi32 the diameter becomes negative)
    const f32 hitWindow300 = 2.0f * GameRules::getRawHitWindow300(OD) / speedMultiplier;

    // ******************************************************************************************************************************************
    // //

    // see setDistances() @
    // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs

    static const f32 normalized_radius = 50.0f;  // normalization factor
    static const f32 maximum_slider_radius = normalized_radius * 2.4f;
    static const f32 assumed_slider_radius = normalized_radius * 1.8f;
    static const f32 circlesize_buff_treshold = 30;  // non-normalized diameter where the circlesize buff starts

    // multiplier to normalize positions so that we can calc as if everything was the same circlesize.
    // also handle high CS bonus

    f32 radius_scaling_factor = normalized_radius / circleRadiusInOsuPixels;
    if(circleRadiusInOsuPixels < circlesize_buff_treshold) {
        const f32 smallCircleBonus = min(circlesize_buff_treshold - circleRadiusInOsuPixels, 5.0f) / 50.0f;
        radius_scaling_factor *= 1.0f + smallCircleBonus;
    }

    // ******************************************************************************************************************************************
    // //

    // see
    // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs

    class DistanceCalc {
       public:
        static void computeSliderCursorPosition(DiffObject &slider, f32 circleRadius) {
            if(slider.lazyCalcFinished || slider.ho->curve == NULL) return;

            slider.lazyTravelTime = slider.ho->scoringTimes.back().time - slider.ho->time;

            f64 endTimeMin = slider.lazyTravelTime / slider.ho->spanDuration;
            if(std::fmod(endTimeMin, 2.0) >= 1.0)
                endTimeMin = 1.0 - std::fmod(endTimeMin, 1.0);
            else
                endTimeMin = std::fmod(endTimeMin, 1.0);

            slider.lazyEndPos = slider.ho->curve->pointAt(endTimeMin);

            Vector2 cursor_pos = slider.ho->pos;
            f64 scaling_factor = 50.0 / circleRadius;

            for(size_t i = 0; i < slider.ho->scoringTimes.size(); i++) {
                Vector2 diff;

                if(slider.ho->scoringTimes[i].type == OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::END) {
                    // NOTE: In lazer, the position of the slider end is at the visual end, but the time is at the
                    // scoring end
                    diff = slider.ho->curve->pointAt(slider.ho->repeats % 2 ? 1.0 : 0.0) - cursor_pos;
                } else {
                    f64 progress =
                        (clamp<long>(slider.ho->scoringTimes[i].time - slider.ho->time, 0, slider.ho->getDuration())) /
                        slider.ho->spanDuration;
                    if(std::fmod(progress, 2.0) >= 1.0)
                        progress = 1.0 - std::fmod(progress, 1.0);
                    else
                        progress = std::fmod(progress, 1.0);

                    diff = slider.ho->curve->pointAt(progress) - cursor_pos;
                }

                f64 diff_len = scaling_factor * diff.length();

                f64 req_diff = 90.0;

                if(i == slider.ho->scoringTimes.size() - 1) {
                    // Slider end
                    Vector2 lazy_diff = slider.lazyEndPos - cursor_pos;
                    if(lazy_diff.length() < diff.length()) diff = lazy_diff;
                    diff_len = scaling_factor * diff.length();
                } else if(slider.ho->scoringTimes[i].type ==
                          OsuDifficultyHitObject::SLIDER_SCORING_TIME::TYPE::REPEAT) {
                    // Slider repeat
                    req_diff = 50.0;
                }

                if(diff_len > req_diff) {
                    cursor_pos += (diff * (f32)((diff_len - req_diff) / diff_len));
                    diff_len *= (diff_len - req_diff) / diff_len;
                    slider.lazyTravelDist += diff_len;
                }

                if(i == slider.ho->scoringTimes.size() - 1) slider.lazyEndPos = cursor_pos;
            }

            slider.lazyCalcFinished = true;
        }

        static Vector2 getEndCursorPosition(DiffObject &hitObject, f32 circleRadius) {
            if(hitObject.ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                computeSliderCursorPosition(hitObject, circleRadius);
                return hitObject
                    .lazyEndPos;  // (slider.lazyEndPos is already initialized to ho->pos in DiffObject constructor)
            }

            return hitObject.ho->pos;
        }
    };

    // ******************************************************************************************************************************************
    // //

    // initialize dobjects
    const bool isUsingCachedDiffObjects = (cachedDiffObjects.size() > 0);
    diffObjects.clear();
    diffObjects.reserve((upToObjectIndex < 0) ? sortedHitObjects.size() : upToObjectIndex + 1);
    if(isUsingCachedDiffObjects) {
        // cached (assumption is cache always contains all, so we build up the partial set of diffObjects we need from
        // it here)
        // respect upToObjectIndex!
        for(size_t i = 0; i < sortedHitObjects.size() && (upToObjectIndex < 0 || i < upToObjectIndex + 1); i++) {
            if(dead.load()) return 0.0;

            diffObjects.push_back(cachedDiffObjects[i]);
        }
    } else {
        // not cached (full rebuild computation)
        // respect upToObjectIndex!
        for(size_t i = 0; i < sortedHitObjects.size() && (upToObjectIndex < 0 || i < upToObjectIndex + 1); i++) {
            if(dead.load()) return 0.0;

            // this already initializes the angle to NaN
            diffObjects.push_back(DiffObject(&sortedHitObjects[i], radius_scaling_factor, diffObjects, (i32)i - 1));
        }
    }

    const size_t numDiffObjects = diffObjects.size();

    // calculate angles and travel/jump distances (before calculating strains)
    if(!isUsingCachedDiffObjects) {
        const f32 starsSliderCurvePointsSeparation = cv_stars_slider_curve_points_separation.getFloat();
        for(size_t i = 0; i < numDiffObjects; i++) {
            if(dead.load()) return 0.0;

            // see setDistances() @
            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs

            if(i > 0) {
                // calculate travel/jump distances
                DiffObject &cur = diffObjects[i];
                DiffObject &prev1 = diffObjects[i - 1];

                // MCKAY:
                {
                    // delay curve creation to when it's needed (1)
                    if(prev1.ho->scheduledCurveAlloc && prev1.ho->curve == NULL) {
                        prev1.ho->curve = SliderCurve::createCurve(
                            prev1.ho->osuSliderCurveType, prev1.ho->scheduledCurveAllocControlPoints,
                            prev1.ho->pixelLength, starsSliderCurvePointsSeparation);
                        prev1.ho->updateCurveStackPosition(
                            prev1.ho->scheduledCurveAllocStackOffset);  // NOTE: respect stacking
                    }
                }

                if(cur.ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                    DistanceCalc::computeSliderCursorPosition(cur, circleRadiusInOsuPixels);
                    cur.travelDistance = cur.lazyTravelDist * pow(1.0 + (cur.ho->repeats - 1) / 2.5, 1.0 / 2.5);
                    cur.travelTime = max(cur.lazyTravelTime, 25.0);
                }

                // don't need to jump to reach spinners
                if(cur.ho->type == OsuDifficultyHitObject::TYPE::SPINNER ||
                   prev1.ho->type == OsuDifficultyHitObject::TYPE::SPINNER)
                    continue;

                const Vector2 lastCursorPosition = DistanceCalc::getEndCursorPosition(prev1, circleRadiusInOsuPixels);

                f64 cur_strain_time =
                    (f64)max(cur.ho->time - prev1.ho->time, 25);  // strain_time isn't initialized here
                cur.jumpDistance = (cur.norm_start - lastCursorPosition * radius_scaling_factor).length();
                cur.minJumpDistance = cur.jumpDistance;
                cur.minJumpTime = cur_strain_time;

                if(prev1.ho->type == OsuDifficultyHitObject::TYPE::SLIDER) {
                    f64 last_travel = max(prev1.lazyTravelTime, 25.0);
                    cur.minJumpTime = max(cur_strain_time - last_travel, 25.0);

                    // NOTE: "curve shouldn't be null here, but Yin [test7] causes that to happen"
                    // NOTE: the curve can be null if controlPoints.size() < 1 because the OsuDifficultyHitObject()
                    // constructor will then not set scheduledCurveAlloc to true (which is perfectly fine and correct)
                    f32 tail_jump_dist =
                        (prev1.ho->curve ? prev1.ho->curve->pointAt(prev1.ho->repeats % 2 ? 1.0 : 0.0) : prev1.ho->pos)
                            .distance(cur.ho->pos) *
                        radius_scaling_factor;
                    cur.minJumpDistance =
                        max(0.0f, min((f32)cur.minJumpDistance - (maximum_slider_radius - assumed_slider_radius),
                                      tail_jump_dist - maximum_slider_radius));
                }

                // calculate angles
                if(i > 1) {
                    DiffObject &prev2 = diffObjects[i - 2];
                    if(prev2.ho->type == OsuDifficultyHitObject::TYPE::SPINNER) continue;

                    const Vector2 lastLastCursorPosition =
                        DistanceCalc::getEndCursorPosition(prev2, circleRadiusInOsuPixels);

                    // MCKAY:
                    {
                        // and also immediately delete afterwards (2)
                        if(i > 2)  // NOTE: this trivial sliding window implementation will keep the last 2 curves alive
                                   // at the end, but they get auto deleted later anyway so w/e
                        {
                            DiffObject &prev3 = diffObjects[i - 3];

                            if(prev3.ho->scheduledCurveAlloc) SAFE_DELETE(prev3.ho->curve);
                        }
                    }

                    const Vector2 v1 = lastLastCursorPosition - prev1.ho->pos;
                    const Vector2 v2 = cur.ho->pos - lastCursorPosition;

                    const f64 dot = v1.dot(v2);
                    const f64 det = (v1.x * v2.y) - (v1.y * v2.x);

                    cur.angle = std::fabs(std::atan2(det, dot));
                }
            }
        }
    }

    // calculate strains/skills
    // NOTE(McKay): yes, this loses some extremely minor accuracy (~0.001 stars territory) for live star/pp for some
    // rare individual upToObjectIndex due to not being recomputed for the cut set of cached diffObjects every time, but
    // the performance gain is so insane I don't care
    if(!isUsingCachedDiffObjects) {
        for(size_t i = 1; i < numDiffObjects; i++)  // NOTE: start at 1
        {
            diffObjects[i].calculate_strains(diffObjects[i - 1], (i == numDiffObjects - 1) ? NULL : &diffObjects[i + 1],
                                             hitWindow300);
        }
    }

    // calculate final difficulty (weigh strains)
    f64 aimNoSliders = DiffObject::calculate_difficulty(Skills::Skill::AIM_NO_SLIDERS, diffObjects);
    *aim = DiffObject::calculate_difficulty(Skills::Skill::AIM_SLIDERS, diffObjects, outAimStrains);
    *speed = DiffObject::calculate_difficulty(Skills::Skill::SPEED, diffObjects, outSpeedStrains, speedNotes);

    static const f64 star_scaling_factor = 0.0675;

    aimNoSliders = std::sqrt(aimNoSliders) * star_scaling_factor;
    *aim = std::sqrt(*aim) * star_scaling_factor;
    *speed = std::sqrt(*speed) * star_scaling_factor;

    *aimSliderFactor = (*aim > 0) ? aimNoSliders / *aim : 1.0;

    if(touchDevice) *aim = pow(*aim, 0.8);

    if(relax) {
        *aim *= 0.9;
        *speed = 0.0;
    }

    // fill cache
    if(!isUsingCachedDiffObjects) {
        // XXX: just do cachedDiffObjects = diffObjects?
        for(size_t i = 0; i < diffObjects.size(); i++) {
            cachedDiffObjects.push_back(diffObjects[i]);
        }
    }

    f64 baseAimPerformance = pow(5.0 * max(1.0, *aim / 0.0675) - 4.0, 3.0) / 100000.0;
    f64 baseSpeedPerformance = pow(5.0 * max(1.0, *speed / 0.0675) - 4.0, 3.0) / 100000.0;
    f64 basePerformance = pow(pow(baseAimPerformance, 1.1) + pow(baseSpeedPerformance, 1.1), 1.0 / 1.1);
    return basePerformance > 0.00001
               ? 1.04464392682 /* Math.Cbrt(OsuPerformanceCalculator.PERFORMANCE_BASE_MULTIPLIER) */ * 0.027 *
                     (std::cbrt(100000.0 / pow(2.0, 1 / 1.1) * basePerformance) + 4.0)
               : 0.0;
}

f64 DifficultyCalculator::calculatePPv2(i32 modsLegacy, f64 timescale, f64 ar, f64 od, f64 aim, f64 aimSliderFactor,
                                        f64 speed, f64 speedNotes, i32 numCircles, i32 numSliders, i32 numSpinners,
                                        i32 maxPossibleCombo, i32 combo, i32 misses, i32 c300, i32 c100, i32 c50) {
    // NOTE: depends on active mods + OD + AR

    // apply "timescale" aka speed multiplier to ar/od
    // (the original incoming ar/od values are guaranteed to not yet have any speed multiplier applied to them, but they
    // do have non-time-related mods already applied, like HR or any custom overrides) (yes, this does work correctly
    // when the override slider "locking" feature is used. in this case, the stored ar/od is already compensated such
    // that it will have the locked value AFTER applying the speed multiplier here) (all UI elements which display ar/od
    // from stored scores, like the ranking screen or score buttons, also do this calculation before displaying the
    // values to the user. of course the mod selection screen does too.)
    od = GameRules::getRawOverallDifficultyForSpeedMultiplier(GameRules::getRawHitWindow300(od), timescale);
    ar = GameRules::getRawApproachRateForSpeedMultiplier(GameRules::getRawApproachTime(ar), timescale);

    if(combo < 0) combo = maxPossibleCombo;

    if(combo < 1) return 0.0;

    i32 totalHits = c300 + c100 + c50 + misses;
    f64 accuracy = (totalHits > 0 ? (f64)(c300 * 300 + c100 * 100 + c50 * 50) / (f64)(totalHits * 300) : 0.0);
    i32 amountHitObjectsWithAccuracy = (modsLegacy & LegacyFlags::ScoreV2 ? totalHits : numCircles);

    // calculateEffectiveMissCount @
    // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/OsuPerformanceCalculator.cs required
    // because slider breaks aren't exposed to pp calculation
    f64 comboBasedMissCount = 0.0;
    if(numSliders > 0) {
        f64 fullComboThreshold = maxPossibleCombo - (0.1 * numSliders);
        if(maxPossibleCombo < fullComboThreshold)
            comboBasedMissCount = fullComboThreshold / max(1.0, (f64)maxPossibleCombo);
    }
    f64 effectiveMissCount = clamp<f64>(comboBasedMissCount, (f64)misses, (f64)(c50 + c100 + misses));

    // custom multipliers for nofail and spunout
    f64 multiplier = 1.14;  // keep final pp normalized across changes
    {
        if(modsLegacy & LegacyFlags::NoFail)
            multiplier *=
                max(0.9, 1.0 - 0.02 * effectiveMissCount);  // see https://github.com/ppy/osu-performance/pull/127/files

        if((modsLegacy & LegacyFlags::SpunOut) && totalHits > 0)
            multiplier *= 1.0 - pow((f64)numSpinners / (f64)totalHits,
                                    0.85);  // see https://github.com/ppy/osu-performance/pull/110/

        if((modsLegacy & LegacyFlags::Relax)) {
            f64 okMultiplier = max(0.0, od > 0.0 ? 1.0 - pow(od / 13.33, 1.8) : 1.0);   // 100
            f64 mehMultiplier = max(0.0, od > 0.0 ? 1.0 - pow(od / 13.33, 5.0) : 1.0);  // 50
            effectiveMissCount = min(effectiveMissCount + c100 * okMultiplier + c50 * mehMultiplier, (f64)totalHits);
        }
    }

    f64 aimValue = 0.0;
    if(!(modsLegacy & LegacyFlags::Autopilot)) {
        f64 rawAim = aim;
        aimValue = pow(5.0 * max(1.0, rawAim / 0.0675) - 4.0, 3.0) / 100000.0;

        // length bonus
        f64 lengthBonus = 0.95 + 0.4 * min(1.0, ((f64)totalHits / 2000.0)) +
                          (totalHits > 2000 ? std::log10(((f64)totalHits / 2000.0)) * 0.5 : 0.0);
        aimValue *= lengthBonus;

        // see https://github.com/ppy/osu-performance/pull/129/
        // Penalize misses by assessing # of misses relative to the total # of objects. Default a 3% reduction for any #
        // of misses.
        if(effectiveMissCount > 0 && totalHits > 0)
            aimValue *= 0.97 * pow(1.0 - pow(effectiveMissCount / (f64)totalHits, 0.775), effectiveMissCount);

        // combo scaling
        if(maxPossibleCombo > 0) aimValue *= min(pow((f64)combo, 0.8) / pow((f64)maxPossibleCombo, 0.8), 1.0);

        // ar bonus
        f64 approachRateFactor = 0.0;  // see https://github.com/ppy/osu-performance/pull/125/
        if((modsLegacy & LegacyFlags::Relax) == 0) {
            if(ar > 10.33)
                approachRateFactor =
                    0.3 *
                    (ar - 10.33);  // from 0.3 to 0.4 see https://github.com/ppy/osu-performance/pull/125/ // and
                                   // completely changed the logic in https://github.com/ppy/osu-performance/pull/135/
            else if(ar < 8.0)
                approachRateFactor =
                    0.05 * (8.0 - ar);  // from 0.01 to 0.1 see https://github.com/ppy/osu-performance/pull/125/
                                        // // and back again from 0.1 to 0.01 see
                                        // https://github.com/ppy/osu-performance/pull/133/ // and completely
                                        // changed the logic in https://github.com/ppy/osu-performance/pull/135/
        }

        aimValue *= 1.0 + approachRateFactor * lengthBonus;

        // hidden
        if(modsLegacy & LegacyFlags::Hidden)
            aimValue *= 1.0 + 0.04 * (max(12.0 - ar,
                                          0.0));  // NOTE: clamped to 0 because neosu allows AR > 12

        // "We assume 15% of sliders in a map are difficult since there's no way to tell from the performance
        // calculator."
        f64 estimateDifficultSliders = numSliders * 0.15;
        if(numSliders > 0) {
            f64 estimateSliderEndsDropped =
                clamp<f64>((f64)min(c100 + c50 + misses, maxPossibleCombo - combo), 0.0, estimateDifficultSliders);
            f64 sliderNerfFactor =
                (1.0 - aimSliderFactor) * pow(1.0 - estimateSliderEndsDropped / estimateDifficultSliders, 3.0) +
                aimSliderFactor;
            aimValue *= sliderNerfFactor;
        }

        // scale aim with acc
        aimValue *= accuracy;
        // also consider acc difficulty when doing that
        aimValue *= 0.98 + pow(od, 2.0) / 2500.0;
    }

    f64 speedValue = 0.0;
    if(!(modsLegacy & LegacyFlags::Relax)) {
        speedValue = pow(5.0 * max(1.0, speed / 0.0675) - 4.0, 3.0) / 100000.0;

        // length bonus
        f64 lengthBonus = 0.95 + 0.4 * min(1.0, ((f64)totalHits / 2000.0)) +
                          (totalHits > 2000 ? std::log10(((f64)totalHits / 2000.0)) * 0.5 : 0.0);
        speedValue *= lengthBonus;

        // see https://github.com/ppy/osu-performance/pull/129/
        // Penalize misses by assessing # of misses relative to the total # of objects. Default a 3% reduction for any #
        // of misses.
        if(effectiveMissCount > 0 && totalHits > 0)
            speedValue *=
                0.97 * pow(1.0 - pow(effectiveMissCount / (f64)totalHits, 0.775), pow(effectiveMissCount, 0.875));

        // combo scaling
        if(maxPossibleCombo > 0) speedValue *= min(pow((f64)combo, 0.8) / pow((f64)maxPossibleCombo, 0.8), 1.0);

        // ar bonus
        f64 approachRateFactor = 0.0;  // see https://github.com/ppy/osu-performance/pull/125/
        if(ar > 10.33)
            approachRateFactor =
                0.3 * (ar - 10.33);  // from 0.3 to 0.4 see https://github.com/ppy/osu-performance/pull/125/ // and
                                     // completely changed the logic in https://github.com/ppy/osu-performance/pull/135/

        speedValue *= 1.0 + approachRateFactor * lengthBonus;

        // hidden
        if(modsLegacy & LegacyFlags::Hidden)
            speedValue *= 1.0 + 0.04 * (max(12.0 - ar,
                                            0.0));  // NOTE: clamped to 0 because neosu allows AR > 12

        // "Calculate accuracy assuming the worst case scenario"
        f64 relevantTotalDiff = totalHits - speedNotes;
        f64 relevantCountGreat = max(0.0, c300 - relevantTotalDiff);
        f64 relevantCountOk = max(0.0, c100 - max(0.0, relevantTotalDiff - c300));
        f64 relevantCountMeh = max(0.0, c50 - max(0.0, relevantTotalDiff - c300 - c100));
        f64 relevantAccuracy = speedNotes == 0 ? 0
                                               : (relevantCountGreat * 6.0 + relevantCountOk * 2.0 + relevantCountMeh) /
                                                     (speedNotes * 6.0);

        // see https://github.com/ppy/osu-performance/pull/128/
        // Scale the speed value with accuracy and OD
        speedValue *=
            (0.95 + pow(od, 2.0) / 750.0) * pow((accuracy + relevantAccuracy) / 2.0, (14.5 - max(od, 8.0)) / 2.0);
        // Scale the speed value with # of 50s to punish doubletapping.
        speedValue *= pow(0.99, c50 < (totalHits / 500.0) ? 0.0 : c50 - (totalHits / 500.0));
    }

    f64 accuracyValue = 0.0;
    {
        f64 betterAccuracyPercentage;
        if(amountHitObjectsWithAccuracy > 0)
            betterAccuracyPercentage =
                ((f64)(c300 - (totalHits - amountHitObjectsWithAccuracy)) * 6.0 + (c100 * 2.0) + c50) /
                (f64)(amountHitObjectsWithAccuracy * 6.0);
        else
            betterAccuracyPercentage = 0.0;

        // it's possible to reach negative accuracy, cap at zero
        if(betterAccuracyPercentage < 0.0) betterAccuracyPercentage = 0.0;

        // arbitrary values tom crafted out of trial and error
        accuracyValue = pow(1.52163, od) * pow(betterAccuracyPercentage, 24.0) * 2.83;

        // length bonus
        accuracyValue *= min(1.15, pow(amountHitObjectsWithAccuracy / 1000.0, 0.3));

        // hidden bonus
        if(modsLegacy & LegacyFlags::Hidden) accuracyValue *= 1.08;
        // flashlight bonus
        if(modsLegacy & LegacyFlags::Flashlight) accuracyValue *= 1.02;
    }

    f64 totalValue = pow(pow(aimValue, 1.1) + pow(speedValue, 1.1) + pow(accuracyValue, 1.1), 1.0 / 1.1) * multiplier;
    return totalValue;
}

DifficultyCalculator::DiffObject::DiffObject(OsuDifficultyHitObject *base_object, float radius_scaling_factor,
                                             std::vector<DiffObject> &diff_objects, int prevObjectIdx)
    : objects(diff_objects) {
    ho = base_object;

    for(int i = 0; i < Skills::NUM_SKILLS; i++) {
        strains[i] = 0.0;
    }
    raw_speed_strain = 0.0;
    rhythm = 0.0;

    norm_start = ho->pos * radius_scaling_factor;

    angle = std::numeric_limits<float>::quiet_NaN();

    jumpDistance = 0.0;
    minJumpDistance = 0.0;
    minJumpTime = 0.0;
    travelDistance = 0.0;

    delta_time = 0.0;
    strain_time = 0.0;

    lazyCalcFinished = false;
    lazyEndPos = ho->pos;
    lazyTravelDist = 0.0;
    lazyTravelTime = 0.0;
    travelTime = 0.0;

    prevObjectIndex = prevObjectIdx;
}

void DifficultyCalculator::DiffObject::calculate_strains(const DiffObject &prev, const DiffObject *next,
                                                         double hitWindow300) {
    calculate_strain(prev, next, hitWindow300, Skills::Skill::SPEED);
    calculate_strain(prev, next, hitWindow300, Skills::Skill::AIM_SLIDERS);
    calculate_strain(prev, next, hitWindow300, Skills::Skill::AIM_NO_SLIDERS);
}

void DifficultyCalculator::DiffObject::calculate_strain(const DiffObject &prev, const DiffObject *next,
                                                        double hitWindow300, const Skills::Skill dtype) {
    double currentStrainOfDiffObject = 0;

    const long time_elapsed = ho->time - prev.ho->time;

    // update our delta time
    delta_time = (double)time_elapsed;
    strain_time = (double)std::max(time_elapsed, 25l);

    switch(ho->type) {
        case OsuDifficultyHitObject::TYPE::SLIDER:
        case OsuDifficultyHitObject::TYPE::CIRCLE:
            currentStrainOfDiffObject = spacing_weight2(dtype, prev, next, hitWindow300);
            break;

        case OsuDifficultyHitObject::TYPE::SPINNER:
            break;

        case OsuDifficultyHitObject::TYPE::INVALID:
            // NOTE: silently ignore
            return;
    }

    // see Process() @ https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/Skill.cs
    double currentStrain = prev.strains[Skills::skillToIndex(dtype)];
    {
        currentStrain *= strainDecay(dtype, dtype == Skills::Skill::SPEED ? strain_time : delta_time);
        currentStrain += currentStrainOfDiffObject * weight_scaling[Skills::skillToIndex(dtype)];
    }
    strains[Skills::skillToIndex(dtype)] = currentStrain;
}

double DifficultyCalculator::DiffObject::calculate_difficulty(const Skills::Skill type,
                                                              const std::vector<DiffObject> &dobjects,
                                                              std::vector<double> *outStrains,
                                                              double *outRelevantNotes) {
    // (old) see https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/Skill.cs
    // (new) see https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/StrainSkill.cs

    static const double strain_step = 400.0;  // the length of each strain section
    static const double decay_weight =
        0.9;  // max strains are weighted from highest to lowest, and this is how much the weight decays.

    if(dobjects.size() < 1) return 0.0;

    double interval_end = std::ceil((double)dobjects[0].ho->time / strain_step) * strain_step;
    double max_strain = 0.0;

    // used for calculating relevant note count for speed pp
    std::vector<double> objectStrains;

    std::vector<double> highestStrains;
    for(size_t i = 0; i < dobjects.size(); i++) {
        const DiffObject &cur = dobjects[i];
        const DiffObject &prev = dobjects[i > 0 ? i - 1 : i];

        // make previous peak strain decay until the current object
        while(cur.ho->time > interval_end) {
            highestStrains.push_back(max_strain);

            if(i < 1)  // !prev
                max_strain = 0.0;
            else
                max_strain = prev.strains[Skills::skillToIndex(type)] *
                             (type == Skills::Skill::SPEED ? prev.rhythm : 1.0) *
                             strainDecay(type, (interval_end - (double)prev.ho->time));

            interval_end += strain_step;
        }

        // calculate max strain for this interval
        double cur_strain = cur.strains[Skills::skillToIndex(type)] * (type == Skills::Skill::SPEED ? cur.rhythm : 1.0);
        max_strain = std::max(max_strain, cur_strain);
        if(outRelevantNotes) objectStrains.push_back(cur_strain);
    }

    // the peak strain will not be saved for the last section in the above loop
    highestStrains.push_back(max_strain);

    if(outStrains != NULL) (*outStrains) = highestStrains;  // save a copy

    // calculate relevant speed note count
    // RelevantNoteCount @ https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs
    if(outRelevantNotes) {
        double maxStrain =
            (objectStrains.size() < 1 ? 0.0 : *std::max_element(objectStrains.begin(), objectStrains.end()));
        if(objectStrains.size() < 1 || maxStrain == 0.0) {
            *outRelevantNotes = 0.0;
        } else {
            double tempSum = 0.0;
            for(size_t i = 0; i < objectStrains.size(); i++) {
                tempSum += 1.0 / (1.0 + std::exp(-(objectStrains[i] / maxStrain * 12.0 - 6.0)));
            }
            *outRelevantNotes = tempSum;
        }
    }

    // (old) see DifficultyValue() @ https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/Skill.cs
    // (new) see DifficultyValue() @
    // https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/StrainSkill.cs (new) see
    // DifficultyValue() @
    // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/OsuStrainSkill.cs

    static const size_t reducedSectionCount = 10;
    static const double reducedStrainBaseline = 0.75;
    static const double difficultyMultiplier = 1.06;

    double difficulty = 0.0;
    double weight = 1.0;

    // sort strains from greatest to lowest
    std::sort(highestStrains.begin(), highestStrains.end(), std::greater<double>());

    // old implementation
    /*
    {
        // weigh the top strains
        for (size_t i=0; i<highestStrains.size(); i++)
        {
            difficulty += highestStrains[i] * weight;
            weight *= decay_weight;
        }
    }
    return difficulty;
    */

    // new implementation (https://github.com/ppy/osu/pull/13483/)
    {
        size_t skillSpecificReducedSectionCount = reducedSectionCount;
        {
            switch(type) {
                case Skills::Skill::SPEED:
                    skillSpecificReducedSectionCount = 5;
                    break;
                case Skills::Skill::AIM_SLIDERS:
                case Skills::Skill::AIM_NO_SLIDERS:
                    break;
            }
        }

        // "We are reducing the highest strains first to account for extreme difficulty spikes"
        for(size_t i = 0; i < std::min(highestStrains.size(), skillSpecificReducedSectionCount); i++) {
            const double scale = std::log10(
                lerp<double>(1.0, 10.0, clamp<double>((double)i / (double)skillSpecificReducedSectionCount, 0.0, 1.0)));
            highestStrains[i] *= lerp<double>(reducedStrainBaseline, 1.0, scale);
        }

        // re-sort
        std::sort(highestStrains.begin(), highestStrains.end(), std::greater<double>());

        // weigh the top strains
        for(size_t i = 0; i < highestStrains.size(); i++) {
            difficulty += highestStrains[i] * weight;
            weight *= decay_weight;
        }
    }

    double skillSpecificDifficultyMultiplier = difficultyMultiplier;
    {
        switch(type) {
            case Skills::Skill::SPEED:
                skillSpecificDifficultyMultiplier = 1.04;
                break;
            case Skills::Skill::AIM_SLIDERS:
            case Skills::Skill::AIM_NO_SLIDERS:
                break;
        }
    }

    return difficulty * skillSpecificDifficultyMultiplier;
}

// old implementation (ppv2.0)
double DifficultyCalculator::DiffObject::spacing_weight1(const double distance, const Skills::Skill diff_type) {
    // arbitrary tresholds to determine when a stream is spaced enough that is becomes hard to alternate.
    static const double single_spacing_threshold = 125.0;
    static const double stream_spacing = 110.0;

    // almost the normalized circle diameter (104px)
    static const double almost_diameter = 90.0;

    switch(diff_type) {
        case Skills::Skill::SPEED:
            if(distance > single_spacing_threshold)
                return 2.5;
            else if(distance > stream_spacing)
                return 1.6 + 0.9 * (distance - stream_spacing) / (single_spacing_threshold - stream_spacing);
            else if(distance > almost_diameter)
                return 1.2 + 0.4 * (distance - almost_diameter) / (stream_spacing - almost_diameter);
            else if(distance > almost_diameter / 2.0)
                return 0.95 + 0.25 * (distance - almost_diameter / 2.0) / (almost_diameter / 2.0);
            else
                return 0.95;

        case Skills::Skill::AIM_SLIDERS:
        case Skills::Skill::AIM_NO_SLIDERS:
            return std::pow(distance, 0.99);
    }

    return 0.0;
}

// new implementation, Xexxar, (ppv2.1), see
// https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/
double DifficultyCalculator::DiffObject::spacing_weight2(const Skills::Skill diff_type, const DiffObject &prev,
                                                         const DiffObject *next, double hitWindow300) {
    static const double single_spacing_threshold = 125.0;

    static const double min_speed_bonus = 75.0; /* ~200BPM 1/4 streams */
    static const double speed_balancing_factor = 40.0;

    static const int history_time_max = 5000;
    static const double rhythm_multiplier = 0.75;

    // double angle_bonus = 1.0; // (apparently unused now in lazer?)

    switch(diff_type) {
        case Skills::Skill::SPEED: {
            // see https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs
            if(ho->type == OsuDifficultyHitObject::TYPE::SPINNER) {
                raw_speed_strain = 0.0;
                rhythm = 0.0;

                return 0.0;
            }

            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Evaluators/SpeedEvaluator.cs
            const double distance = std::min(single_spacing_threshold, prev.travelDistance + minJumpDistance);

            double strain_time = this->strain_time;
            strain_time /= clamp<double>((strain_time / hitWindow300) / 0.93, 0.92, 1.0);

            double doubletapness = 1.0;
            if(next != NULL) {
                double cur_delta = std::max(1.0, delta_time);
                double next_delta = std::max(1, next->ho->time - ho->time);  // next delta time isn't initialized yet
                double delta_diff = std::abs(next_delta - cur_delta);
                double speedRatio = cur_delta / std::max(cur_delta, delta_diff);
                double windowRatio = std::pow(std::min(1.0, cur_delta / hitWindow300), 2.0);

                doubletapness = std::pow(speedRatio, 1 - windowRatio);
            }

            double speed_bonus = 1.0;
            if(strain_time < min_speed_bonus)
                speed_bonus = 1.0 + 0.75 * std::pow((min_speed_bonus - strain_time) / speed_balancing_factor, 2.0);

            raw_speed_strain = (speed_bonus + speed_bonus * std::pow(distance / single_spacing_threshold, 3.5)) *
                               doubletapness / strain_time;

            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Evaluators/RhythmEvaluator.cs
            int previousIslandSize = 0;

            double rhythmComplexitySum = 0;
            int islandSize = 1;
            double startRatio = 0;  // store the ratio of the current start of an island to buff for tighter rhythms

            bool firstDeltaSwitch = false;

            int historicalNoteCount = std::min(prevObjectIndex, 32);

            int rhythmStart = 0;

            while(rhythmStart < historicalNoteCount - 2 &&
                  ho->time - get_previous(rhythmStart)->ho->time < history_time_max) {
                rhythmStart++;
            }

            for(int i = rhythmStart; i > 0; i--) {
                const DiffObject *currObj = get_previous(i - 1);
                const DiffObject *prevObj = get_previous(i);
                const DiffObject *lastObj = get_previous(i + 1);

                double currHistoricalDecay = (double)(history_time_max - (ho->time - currObj->ho->time)) /
                                             history_time_max;  // scales note 0 to 1 from history to now

                currHistoricalDecay =
                    std::min((double)(historicalNoteCount - i) / historicalNoteCount,
                             currHistoricalDecay);  // either we're limited by time or limited by object count.

                double currDelta = currObj->strain_time;
                double prevDelta = prevObj->strain_time;
                double lastDelta = lastObj->strain_time;
                double currRatio =
                    1.0 + 6.0 * std::min(0.5, std::pow(std::sin(PI / (std::min(prevDelta, currDelta) /
                                                                      std::max(prevDelta, currDelta))),
                                                       2.0));  // fancy function to calculate rhythmbonuses.

                double windowPenalty = std::min(
                    1.0, std::max(0.0, std::abs(prevDelta - currDelta) - hitWindow300 * 0.3) / (hitWindow300 * 0.3));

                windowPenalty = std::min(1.0, windowPenalty);

                double effectiveRatio = windowPenalty * currRatio;

                if(firstDeltaSwitch) {
                    if(!(prevDelta > 1.25 * currDelta || prevDelta * 1.25 < currDelta)) {
                        if(islandSize < 7) islandSize++;  // island is still progressing, count size.
                    } else {
                        if(get_previous(i - 1)->ho->type ==
                           OsuDifficultyHitObject::TYPE::SLIDER)  // bpm change is into slider, this is easy acc window
                            effectiveRatio *= 0.125;

                        if(get_previous(i)->ho->type ==
                           OsuDifficultyHitObject::TYPE::SLIDER)  // bpm change was from a slider, this is easier
                                                                  // typically than circle -> circle
                            effectiveRatio *= 0.25;

                        if(previousIslandSize == islandSize)  // repeated island size (ex: triplet -> triplet)
                            effectiveRatio *= 0.25;

                        if(previousIslandSize % 2 == islandSize % 2)  // repeated island polartiy (2 -> 4, 3 -> 5)
                            effectiveRatio *= 0.50;

                        if(lastDelta > prevDelta + 10.0 &&
                           prevDelta > currDelta + 10.0)  // previous increase happened a note ago, 1/1->1/2-1/4, dont
                                                          // want to buff this.
                            effectiveRatio *= 0.125;

                        rhythmComplexitySum += std::sqrt(effectiveRatio * startRatio) * currHistoricalDecay *
                                               std::sqrt(4.0 + islandSize) / 2.0 * std::sqrt(4.0 + previousIslandSize) /
                                               2.0;

                        startRatio = effectiveRatio;

                        previousIslandSize = islandSize;  // log the last island size.

                        if(prevDelta * 1.25 < currDelta)  // we're slowing down, stop counting
                            firstDeltaSwitch =
                                false;  // if we're speeding up, this stays true and  we keep counting island size.

                        islandSize = 1;
                    }
                } else if(prevDelta > 1.25 * currDelta)  // we want to be speeding up.
                {
                    // Begin counting island until we change speed again.
                    firstDeltaSwitch = true;
                    startRatio = effectiveRatio;
                    islandSize = 1;
                }
            }

            rhythm = std::sqrt(4.0 + rhythmComplexitySum * rhythm_multiplier) / 2.0;

            return raw_speed_strain;
        } break;

        case Skills::Skill::AIM_SLIDERS:
        case Skills::Skill::AIM_NO_SLIDERS: {
            // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Difficulty/Evaluators/AimEvaluator.cs
            static const double wide_angle_multiplier = 1.5;
            static const double acute_angle_multiplier = 1.95;
            static const double slider_multiplier = 1.35;
            static const double velocity_change_multiplier = 0.75;

            const bool withSliders = (diff_type == Skills::Skill::AIM_SLIDERS);

            if(ho->type == OsuDifficultyHitObject::TYPE::SPINNER || prevObjectIndex <= 1 ||
               prev.ho->type == OsuDifficultyHitObject::TYPE::SPINNER)
                return 0.0;

            auto calcWideAngleBonus = [](double angle) {
                return std::pow(std::sin(3.0 / 4.0 * (std::min(5.0 / 6.0 * PI, std::max(PI / 6.0, angle)) - PI / 6.0)),
                                2.0);
            };
            auto calcAcuteAngleBonus = [=](double angle) { return 1.0 - calcWideAngleBonus(angle); };

            const DiffObject *prevPrev = get_previous(1);
            double currVelocity = jumpDistance / strain_time;

            if(prev.ho->type == OsuDifficultyHitObject::TYPE::SLIDER && withSliders) {
                double travelVelocity = prev.travelDistance / prev.travelTime;
                double movementVelocity = minJumpDistance / minJumpTime;
                currVelocity = std::max(currVelocity, movementVelocity + travelVelocity);
            }
            double aimStrain = currVelocity;

            double prevVelocity = prev.jumpDistance / prev.strain_time;
            if(prevPrev->ho->type == OsuDifficultyHitObject::TYPE::SLIDER && withSliders) {
                double travelVelocity = prevPrev->travelDistance / prevPrev->travelTime;
                double movementVelocity = prev.minJumpDistance / prev.minJumpTime;
                prevVelocity = std::max(prevVelocity, movementVelocity + travelVelocity);
            }

            double wideAngleBonus = 0;
            double acuteAngleBonus = 0;
            double sliderBonus = 0;
            double velocityChangeBonus = 0;

            if(std::max(strain_time, prev.strain_time) < 1.25 * std::min(strain_time, prev.strain_time)) {
                if(!std::isnan(angle) && !std::isnan(prev.angle) && !std::isnan(prevPrev->angle)) {
                    double angleBonus = std::min(currVelocity, prevVelocity);

                    wideAngleBonus = calcWideAngleBonus(angle);
                    acuteAngleBonus =
                        strain_time > 100
                            ? 0.0
                            : (calcAcuteAngleBonus(angle) * calcAcuteAngleBonus(prev.angle) *
                               std::min(angleBonus, 125.0 / strain_time) *
                               std::pow(std::sin(PI / 2.0 * std::min(1.0, (100.0 - strain_time) / 25.0)), 2.0) *
                               std::pow(std::sin(PI / 2.0 * (clamp<double>(jumpDistance, 50.0, 100.0) - 50.0) / 50.0),
                                        2.0));

                    wideAngleBonus *=
                        angleBonus * (1.0 - std::min(wideAngleBonus, std::pow(calcWideAngleBonus(prev.angle), 3.0)));
                    acuteAngleBonus *=
                        0.5 +
                        0.5 * (1.0 - std::min(acuteAngleBonus, std::pow(calcAcuteAngleBonus(prevPrev->angle), 3.0)));
                }
            }

            if(std::max(prevVelocity, currVelocity) != 0.0) {
                prevVelocity = (prev.jumpDistance + prevPrev->travelDistance) / prev.strain_time;
                currVelocity = (jumpDistance + prev.travelDistance) / strain_time;

                double distRatio = std::pow(
                    std::sin(PI / 2.0 * std::abs(prevVelocity - currVelocity) / std::max(prevVelocity, currVelocity)),
                    2.0);
                double overlapVelocityBuff =
                    std::min(125.0 / std::min(strain_time, prev.strain_time), std::abs(prevVelocity - currVelocity));
                velocityChangeBonus =
                    overlapVelocityBuff * distRatio *
                    std::pow(std::min(strain_time, prev.strain_time) / std::max(strain_time, prev.strain_time), 2.0);
            }

            if(prev.ho->type == OsuDifficultyHitObject::TYPE::SLIDER)
                sliderBonus = prev.travelDistance / prev.travelTime;

            aimStrain +=
                std::max(acuteAngleBonus * acute_angle_multiplier,
                         wideAngleBonus * wide_angle_multiplier + velocityChangeBonus * velocity_change_multiplier);
            if(withSliders) aimStrain += sliderBonus * slider_multiplier;

            return aimStrain;
        } break;
    }

    return 0.0;
}
