//================ Copyright (c) 2015, PG, All rights reserved. =================//
//
// Purpose:		beatmap loader
//
// $NoKeywords: $osubm
//===============================================================================//

#include "OsuBeatmap.h"

#include <string.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoProtocol.h"
#include "BanchoSubmitter.h"
#include "ConVar.h"
#include "Engine.h"
#include "Environment.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Osu.h"
#include "OsuBackgroundImageHandler.h"
#include "OsuBackgroundStarCacheLoader.h"
#include "OsuBeatmap.h"
#include "OsuChat.h"
#include "OsuCircle.h"
#include "OsuDatabase.h"
#include "OsuDatabaseBeatmap.h"
#include "OsuDifficultyCalculator.h"
#include "OsuGameRules.h"
#include "OsuHUD.h"
#include "OsuHitObject.h"
#include "OsuKeyBindings.h"
#include "OsuMainMenu.h"
#include "OsuModFPoSu.h"
#include "OsuModSelector.h"
#include "OsuNotificationOverlay.h"
#include "OsuPauseMenu.h"
#include "OsuReplay.h"
#include "OsuRichPresence.h"
#include "OsuRoom.h"
#include "OsuSkin.h"
#include "OsuSkinImage.h"
#include "OsuSlider.h"
#include "OsuSongBrowser.h"
#include "OsuSpinner.h"
#include "ResourceManager.h"
#include "SoundEngine.h"

ConVar osu_pvs("osu_pvs", true, FCVAR_NONE,
               "optimizes all loops over all hitobjects by clamping the range to the Potentially Visible Set");
ConVar osu_draw_hitobjects("osu_draw_hitobjects", true, FCVAR_NONE);
ConVar osu_draw_beatmap_background_image("osu_draw_beatmap_background_image", true, FCVAR_NONE);

ConVar osu_universal_offset("osu_universal_offset", 0.0f, FCVAR_NONE);
ConVar osu_universal_offset_hardcoded_fallback_dsound("osu_universal_offset_hardcoded_fallback_dsound", -15.0f,
                                                      FCVAR_NONE);
ConVar osu_old_beatmap_offset(
    "osu_old_beatmap_offset", 24.0f, FCVAR_NONE,
    "offset in ms which is added to beatmap versions < 5 (default value is hardcoded 24 ms in stable)");
ConVar osu_timingpoints_offset("osu_timingpoints_offset", 5.0f, FCVAR_NONE,
                               "Offset in ms which is added before determining the active timingpoint for the sample "
                               "type and sample volume (hitsounds) of the current frame");
ConVar osu_interpolate_music_pos(
    "osu_interpolate_music_pos", true, FCVAR_NONE,
    "Interpolate song position with engine time if the audio library reports the same position more than once");
ConVar osu_compensate_music_speed(
    "osu_compensate_music_speed", true, FCVAR_NONE,
    "compensates speeds slower than 1x a little bit, by adding an offset depending on the slowness");
ConVar osu_combobreak_sound_combo("osu_combobreak_sound_combo", 20, FCVAR_NONE,
                                  "Only play the combobreak sound if the combo is higher than this");
ConVar osu_beatmap_preview_mods_live(
    "osu_beatmap_preview_mods_live", false, FCVAR_NONE,
    "whether to immediately apply all currently selected mods while browsing beatmaps (e.g. speed/pitch)");
ConVar osu_beatmap_preview_music_loop("osu_beatmap_preview_music_loop", true, FCVAR_NONE);

ConVar osu_ar_override("osu_ar_override", -1.0f, FCVAR_NONVANILLA,
                       "use this to override between AR 0 and AR 12.5+. active if value is more than or equal to 0.");
ConVar osu_ar_overridenegative("osu_ar_overridenegative", 0.0f, FCVAR_NONVANILLA,
                               "use this to override below AR 0. active if value is less than 0, disabled otherwise. "
                               "this override always overrides the other override.");
ConVar osu_cs_override("osu_cs_override", -1.0f, FCVAR_NONVANILLA,
                       "use this to override between CS 0 and CS 12.1429. active if value is more than or equal to 0.");
ConVar osu_cs_overridenegative("osu_cs_overridenegative", 0.0f, FCVAR_NONVANILLA,
                               "use this to override below CS 0. active if value is less than 0, disabled otherwise. "
                               "this override always overrides the other override.");
ConVar osu_cs_cap_sanity("osu_cs_cap_sanity", true, FCVAR_CHEAT);
ConVar osu_hp_override("osu_hp_override", -1.0f, FCVAR_NONVANILLA);
ConVar osu_od_override("osu_od_override", -1.0f, FCVAR_NONVANILLA);
ConVar osu_ar_override_lock("osu_ar_override_lock", false, FCVAR_NONVANILLA,
                            "always force constant AR even through speed changes");
ConVar osu_od_override_lock("osu_od_override_lock", false, FCVAR_NONVANILLA,
                            "always force constant OD even through speed changes");

ConVar osu_background_dim("osu_background_dim", 0.9f, FCVAR_NONE);
ConVar osu_background_fade_after_load("osu_background_fade_after_load", true, FCVAR_NONE);
ConVar osu_background_dont_fade_during_breaks("osu_background_dont_fade_during_breaks", false, FCVAR_NONE);
ConVar osu_background_fade_min_duration("osu_background_fade_min_duration", 1.4f, FCVAR_NONE,
                                        "Only fade if the break is longer than this (in seconds)");
ConVar osu_background_fade_in_duration("osu_background_fade_in_duration", 0.85f, FCVAR_NONE);
ConVar osu_background_fade_out_duration("osu_background_fade_out_duration", 0.25f, FCVAR_NONE);
ConVar osu_background_brightness("osu_background_brightness", 0.0f, FCVAR_NONE,
                                 "0 to 1, if this is larger than 0 then it will replace the entire beatmap background "
                                 "image with a solid color (see osu_background_color_r/g/b)");
ConVar osu_background_color_r("osu_background_color_r", 255.0f, FCVAR_NONE,
                              "0 to 255, only relevant if osu_background_brightness is larger than 0");
ConVar osu_background_color_g("osu_background_color_g", 255.0f, FCVAR_NONE,
                              "0 to 255, only relevant if osu_background_brightness is larger than 0");
ConVar osu_background_color_b("osu_background_color_b", 255.0f, FCVAR_NONE,
                              "0 to 255, only relevant if osu_background_brightness is larger than 0");
ConVar osu_hiterrorbar_misaims("osu_hiterrorbar_misaims", true, FCVAR_NONE);

ConVar osu_followpoints_prevfadetime("osu_followpoints_prevfadetime", 400.0f,
                                     FCVAR_NONE);  // TODO: this shouldn't be in this class

ConVar osu_auto_and_relax_block_user_input("osu_auto_and_relax_block_user_input", true, FCVAR_NONE);

ConVar osu_mod_timewarp("osu_mod_timewarp", false, FCVAR_NONVANILLA);
ConVar osu_mod_timewarp_multiplier("osu_mod_timewarp_multiplier", 1.5f, FCVAR_NONE);
ConVar osu_mod_minimize("osu_mod_minimize", false, FCVAR_NONVANILLA);
ConVar osu_mod_minimize_multiplier("osu_mod_minimize_multiplier", 0.5f, FCVAR_CHEAT);
ConVar osu_mod_jigsaw1("osu_mod_jigsaw1", false, FCVAR_NONVANILLA);
ConVar osu_mod_artimewarp("osu_mod_artimewarp", false, FCVAR_NONVANILLA);
ConVar osu_mod_artimewarp_multiplier("osu_mod_artimewarp_multiplier", 0.5f, FCVAR_NONE);
ConVar osu_mod_arwobble("osu_mod_arwobble", false, FCVAR_NONVANILLA);
ConVar osu_mod_arwobble_strength("osu_mod_arwobble_strength", 1.0f, FCVAR_CHEAT);
ConVar osu_mod_arwobble_interval("osu_mod_arwobble_interval", 7.0f, FCVAR_CHEAT);
ConVar osu_mod_fullalternate("osu_mod_fullalternate", false, FCVAR_NONE);

ConVar osu_early_note_time(
    "osu_early_note_time", 1500.0f, FCVAR_NONE,
    "Timeframe in ms at the beginning of a beatmap which triggers a starting delay for easier reading");
ConVar osu_quick_retry_time(
    "osu_quick_retry_time", 2000.0f, FCVAR_NONE,
    "Timeframe in ms subtracted from the first hitobject when quick retrying (not regular retry)");
ConVar osu_end_delay_time("osu_end_delay_time", 750.0f, FCVAR_NONE,
                          "Duration in ms which is added at the end of a beatmap after the last hitobject is finished "
                          "but before the ranking screen is automatically shown");
ConVar osu_end_skip(
    "osu_end_skip", true, FCVAR_NONE,
    "whether the beatmap jumps to the ranking screen as soon as the last hitobject plus lenience has passed");
ConVar osu_end_skip_time("osu_end_skip_time", 400.0f, FCVAR_NONE,
                         "Duration in ms which is added to the endTime of the last hitobject, after which pausing the "
                         "game will immediately jump to the ranking screen");
ConVar osu_skip_time("osu_skip_time", 5000.0f, FCVAR_CHEAT,
                     "Timeframe in ms within a beatmap which allows skipping if it doesn't contain any hitobjects");
ConVar osu_fail_time("osu_fail_time", 2.25f, FCVAR_NONE,
                     "Timeframe in s for the slowdown effect after failing, before the pause menu is shown");
ConVar osu_notelock_type("osu_notelock_type", 2, FCVAR_CHEAT,
                         "which notelock algorithm to use (0 = None, 1 = McOsu, 2 = osu!stable, 3 = osu!lazer 2020)");
ConVar osu_notelock_stable_tolerance2b("osu_notelock_stable_tolerance2b", 3, FCVAR_CHEAT,
                                       "time tolerance in milliseconds to allow hitting simultaneous objects close "
                                       "together (e.g. circle at end of slider)");
ConVar osu_mod_suddendeath_restart("osu_mod_suddendeath_restart", false, FCVAR_NONE,
                                   "osu! has this set to false (i.e. you fail after missing). if set to true, then "
                                   "behave like SS/PF, instantly restarting the map");

ConVar osu_drain_type(
    "osu_drain_type", 2, FCVAR_CHEAT,
    "which hp drain algorithm to use (1 = None, 2 = osu!stable, 3 = osu!lazer 2020, 4 = osu!lazer 2018)");
ConVar osu_drain_kill("osu_drain_kill", true, FCVAR_CHEAT, "whether to kill the player upon failing");
ConVar osu_drain_kill_notification_duration(
    "osu_drain_kill_notification_duration", 1.0f, FCVAR_NONE,
    "how long to display the \"You have failed, but you can keep playing!\" notification (0 = disabled)");

ConVar osu_drain_vr_duration("osu_drain_vr_duration", 0.35f, FCVAR_NONE);
ConVar osu_drain_stable_passive_fail(
    "osu_drain_stable_passive_fail", false, FCVAR_NONE,
    "whether to fail the player instantly if health = 0, or only once a negative judgement occurs");
ConVar osu_drain_stable_break_before("osu_drain_stable_break_before", false, FCVAR_NONE,
                                     "drain after last hitobject before a break actually starts");
ConVar osu_drain_stable_break_before_old(
    "osu_drain_stable_break_before_old", true, FCVAR_NONE,
    "for beatmap versions < 8, drain after last hitobject before a break actually starts");
ConVar osu_drain_stable_break_after("osu_drain_stable_break_after", false, FCVAR_NONE,
                                    "drain after a break before the next hitobject can be clicked");
ConVar osu_drain_lazer_passive_fail(
    "osu_drain_lazer_passive_fail", false, FCVAR_NONE,
    "whether to fail the player instantly if health = 0, or only once a negative judgement occurs");
ConVar osu_drain_lazer_break_before("osu_drain_lazer_break_before", false, FCVAR_NONE,
                                    "drain after last hitobject before a break actually starts");
ConVar osu_drain_lazer_break_after("osu_drain_lazer_break_after", false, FCVAR_NONE,
                                   "drain after a break before the next hitobject can be clicked");
ConVar osu_drain_stable_spinner_nerf("osu_drain_stable_spinner_nerf", 0.25f, FCVAR_CHEAT,
                                     "drain gets multiplied with this while a spinner is active");
ConVar osu_drain_stable_hpbar_recovery("osu_drain_stable_hpbar_recovery", 160.0f, FCVAR_CHEAT,
                                       "hp gets set to this value when failing with ez and causing a recovery");

ConVar osu_play_hitsound_on_click_while_playing("osu_play_hitsound_on_click_while_playing", false, FCVAR_NONE);

ConVar osu_debug_draw_timingpoints("osu_debug_draw_timingpoints", false, FCVAR_CHEAT);

ConVar osu_draw_followpoints("osu_draw_followpoints", true, FCVAR_NONE);
ConVar osu_draw_reverse_order("osu_draw_reverse_order", false, FCVAR_NONE);
ConVar osu_draw_playfield_border("osu_draw_playfield_border", true, FCVAR_NONE);

ConVar osu_stacking("osu_stacking", true, FCVAR_NONE, "Whether to use stacking calculations or not");
ConVar osu_stacking_leniency_override("osu_stacking_leniency_override", -1.0f, FCVAR_NONE);

ConVar osu_auto_snapping_strength("osu_auto_snapping_strength", 1.0f, FCVAR_NONE,
                                  "How many iterations of quadratic interpolation to use, more = snappier, 0 = linear");
ConVar osu_auto_cursordance("osu_auto_cursordance", false, FCVAR_NONE);
ConVar osu_autopilot_snapping_strength(
    "osu_autopilot_snapping_strength", 2.0f, FCVAR_NONE,
    "How many iterations of quadratic interpolation to use, more = snappier, 0 = linear");
ConVar osu_autopilot_lenience("osu_autopilot_lenience", 0.75f, FCVAR_NONE);

ConVar osu_followpoints_clamp("osu_followpoints_clamp", false, FCVAR_NONE,
                              "clamp followpoint approach time to current circle approach time (instead of using the "
                              "hardcoded default 800 ms raw)");
ConVar osu_followpoints_anim("osu_followpoints_anim", false, FCVAR_NONE,
                             "scale + move animation while fading in followpoints (osu only does this when its "
                             "internal default skin is being used)");
ConVar osu_followpoints_connect_combos("osu_followpoints_connect_combos", false, FCVAR_NONE,
                                       "connect followpoints even if a new combo has started");
ConVar osu_followpoints_connect_spinners("osu_followpoints_connect_spinners", false, FCVAR_NONE,
                                         "connect followpoints even through spinners");
ConVar osu_followpoints_approachtime("osu_followpoints_approachtime", 800.0f, FCVAR_NONE);
ConVar osu_followpoints_scale_multiplier("osu_followpoints_scale_multiplier", 1.0f, FCVAR_NONE);
ConVar osu_followpoints_separation_multiplier("osu_followpoints_separation_multiplier", 1.0f, FCVAR_NONE);

ConVar osu_number_scale_multiplier("osu_number_scale_multiplier", 1.0f, FCVAR_NONE);

ConVar osu_playfield_mirror_horizontal("osu_playfield_mirror_horizontal", false, FCVAR_NONE);
ConVar osu_playfield_mirror_vertical("osu_playfield_mirror_vertical", false, FCVAR_NONE);

ConVar osu_playfield_rotation("osu_playfield_rotation", 0.0f, FCVAR_CHEAT,
                              "rotates the entire playfield by this many degrees");
ConVar osu_playfield_stretch_x("osu_playfield_stretch_x", 0.0f, FCVAR_CHEAT,
                               "offsets/multiplies all hitobject coordinates by it (0 = default 1x playfield size, -1 "
                               "= on a line, -0.5 = 0.5x playfield size, 0.5 = 1.5x playfield size)");
ConVar osu_playfield_stretch_y("osu_playfield_stretch_y", 0.0f, FCVAR_CHEAT,
                               "offsets/multiplies all hitobject coordinates by it (0 = default 1x playfield size, -1 "
                               "= on a line, -0.5 = 0.5x playfield size, 0.5 = 1.5x playfield size)");
ConVar osu_playfield_circular(
    "osu_playfield_circular", false, FCVAR_CHEAT,
    "whether the playfield area should be transformed from a rectangle into a circle/disc/oval");

ConVar osu_drain_lazer_health_min("osu_drain_lazer_health_min", 0.95f, FCVAR_NONE);
ConVar osu_drain_lazer_health_mid("osu_drain_lazer_health_mid", 0.70f, FCVAR_NONE);
ConVar osu_drain_lazer_health_max("osu_drain_lazer_health_max", 0.30f, FCVAR_NONE);

ConVar osu_mod_wobble("osu_mod_wobble", false, FCVAR_NONVANILLA);
ConVar osu_mod_wobble2("osu_mod_wobble2", false, FCVAR_NONVANILLA);
ConVar osu_mod_wobble_strength("osu_mod_wobble_strength", 25.0f, FCVAR_NONE);
ConVar osu_mod_wobble_frequency("osu_mod_wobble_frequency", 1.0f, FCVAR_NONE);
ConVar osu_mod_wobble_rotation_speed("osu_mod_wobble_rotation_speed", 1.0f, FCVAR_NONE);
ConVar osu_mod_jigsaw2("osu_mod_jigsaw2", false, FCVAR_NONVANILLA);
ConVar osu_mod_jigsaw_followcircle_radius_factor("osu_mod_jigsaw_followcircle_radius_factor", 0.0f, FCVAR_NONE);
ConVar osu_mod_shirone("osu_mod_shirone", false, FCVAR_NONVANILLA);
ConVar osu_mod_shirone_combo("osu_mod_shirone_combo", 20.0f, FCVAR_NONE);
ConVar osu_mod_mafham_render_chunksize("osu_mod_mafham_render_chunksize", 15, FCVAR_NONE,
                                       "render this many hitobjects per frame chunk into the scene buffer (spreads "
                                       "rendering across many frames to minimize lag)");

ConVar osu_mandala("osu_mandala", false, FCVAR_CHEAT);
ConVar osu_mandala_num("osu_mandala_num", 7, FCVAR_NONE);

ConVar osu_debug_hiterrorbar_misaims("osu_debug_hiterrorbar_misaims", false, FCVAR_NONE);

ConVar osu_pp_live_timeout(
    "osu_pp_live_timeout", 1.0f, FCVAR_NONE,
    "show message that we're still calculating stars after this many seconds, on the first start of the beatmap");

OsuBeatmap::OsuBeatmap(Osu *osu) {
    // convar refs
    m_osu_pvs = &osu_pvs;
    m_osu_draw_hitobjects_ref = &osu_draw_hitobjects;
    m_osu_followpoints_prevfadetime_ref = &osu_followpoints_prevfadetime;
    m_osu_universal_offset_ref = &osu_universal_offset;
    m_osu_early_note_time_ref = &osu_early_note_time;
    m_osu_fail_time_ref = &osu_fail_time;
    m_osu_drain_type_ref = &osu_drain_type;
    m_osu_draw_hud_ref = convar->getConVarByName("osu_draw_hud");
    m_osu_draw_scorebarbg_ref = convar->getConVarByName("osu_draw_scorebarbg");
    m_osu_hud_scorebar_hide_during_breaks_ref = convar->getConVarByName("osu_hud_scorebar_hide_during_breaks");
    m_osu_drain_stable_hpbar_maximum_ref = convar->getConVarByName("osu_drain_stable_hpbar_maximum");
    m_osu_volume_music_ref = convar->getConVarByName("osu_volume_music");
    m_osu_mod_fposu_ref = convar->getConVarByName("osu_mod_fposu");
    m_fposu_draw_scorebarbg_on_top_ref = convar->getConVarByName("fposu_draw_scorebarbg_on_top");
    m_osu_draw_statistics_pp_ref = convar->getConVarByName("osu_draw_statistics_pp");
    m_osu_draw_statistics_livestars_ref = convar->getConVarByName("osu_draw_statistics_livestars");
    m_osu_mod_fullalternate_ref = convar->getConVarByName("osu_mod_fullalternate");
    m_fposu_distance_ref = convar->getConVarByName("fposu_distance");
    m_fposu_curved_ref = convar->getConVarByName("fposu_curved");
    m_fposu_mod_strafing_ref = convar->getConVarByName("fposu_mod_strafing");
    m_fposu_mod_strafing_frequency_x_ref = convar->getConVarByName("fposu_mod_strafing_frequency_x");
    m_fposu_mod_strafing_frequency_y_ref = convar->getConVarByName("fposu_mod_strafing_frequency_y");
    m_fposu_mod_strafing_frequency_z_ref = convar->getConVarByName("fposu_mod_strafing_frequency_z");
    m_fposu_mod_strafing_strength_x_ref = convar->getConVarByName("fposu_mod_strafing_strength_x");
    m_fposu_mod_strafing_strength_y_ref = convar->getConVarByName("fposu_mod_strafing_strength_y");
    m_fposu_mod_strafing_strength_z_ref = convar->getConVarByName("fposu_mod_strafing_strength_z");
    m_fposu_mod_3d_depthwobble_ref = convar->getConVarByName("fposu_mod_3d_depthwobble");
    m_osu_slider_scorev2_ref = convar->getConVarByName("osu_slider_scorev2");

    // vars
    m_osu = osu;

    m_bIsPlaying = false;
    m_bIsPaused = false;
    m_bIsWaiting = false;
    m_bIsRestartScheduled = false;
    m_bIsRestartScheduledQuick = false;

    m_bIsInSkippableSection = false;
    m_bShouldFlashWarningArrows = false;
    m_fShouldFlashSectionPass = 0.0f;
    m_fShouldFlashSectionFail = 0.0f;
    m_bContinueScheduled = false;
    m_iContinueMusicPos = 0;
    m_fWaitTime = 0.0f;

    m_selectedDifficulty2 = NULL;

    m_music = NULL;

    m_fMusicFrequencyBackup = 0.f;
    m_iCurMusicPos = 0;
    m_iCurMusicPosWithOffsets = 0;
    m_bWasSeekFrame = false;
    m_fInterpolatedMusicPos = 0.0;
    m_fLastAudioTimeAccurateSet = 0.0;
    m_fLastRealTimeForInterpolationDelta = 0.0;
    m_iResourceLoadUpdateDelayHack = 0;
    m_bForceStreamPlayback = true;  // if this is set to true here, then the music will always be loaded as a stream
                                    // (meaning slow disk access could cause audio stalling/stuttering)
    m_fAfterMusicIsFinishedVirtualAudioTimeStart = -1.0f;
    m_bIsFirstMissSound = true;

    m_bFailed = false;
    m_fFailAnim = 1.0f;
    m_fHealth = 1.0;
    m_fHealth2 = 1.0f;

    m_fDrainRate = 0.0;
    m_fHpMultiplierNormal = 1.0;
    m_fHpMultiplierComboEnd = 1.0;

    m_fBreakBackgroundFade = 0.0f;
    m_bInBreak = false;
    m_currentHitObject = NULL;
    m_iNextHitObjectTime = 0;
    m_iPreviousHitObjectTime = 0;
    m_iPreviousSectionPassFailTime = -1;

    m_bClick1Held = false;
    m_bClick2Held = false;
    m_bClickedContinue = false;
    m_bPrevKeyWasKey1 = false;
    m_iAllowAnyNextKeyForFullAlternateUntilHitObjectIndex = 0;

    m_iRandomSeed = 0;

    m_iNPS = 0;
    m_iND = 0;
    m_iCurrentHitObjectIndex = 0;
    m_iCurrentNumCircles = 0;
    m_iCurrentNumSliders = 0;
    m_iCurrentNumSpinners = 0;
    m_iMaxPossibleCombo = 0;
    m_iScoreV2ComboPortionMaximum = 0;

    m_iPreviousFollowPointObjectIndex = -1;

    m_bIsSpinnerActive = false;

    m_fPlayfieldRotation = 0.0f;
    m_fScaleFactor = 1.0f;

    m_fXMultiplier = 1.0f;
    m_fNumberScale = 1.0f;
    m_fHitcircleOverlapScale = 1.0f;
    m_fRawHitcircleDiameter = 27.35f * 2.0f;
    m_fHitcircleDiameter = 0.0f;
    m_fSliderFollowCircleDiameter = 0.0f;
    m_fRawSliderFollowCircleDiameter = 0.0f;

    m_iAutoCursorDanceIndex = 0;

    m_fAimStars = 0.0f;
    m_fAimSliderFactor = 0.0f;
    m_fSpeedStars = 0.0f;
    m_fSpeedNotes = 0.0f;
    m_starCacheLoader = new OsuBackgroundStarCacheLoader(this);
    m_fStarCacheTime = 0.0f;

    m_bWasHREnabled = false;
    m_fPrevHitCircleDiameter = 0.0f;
    m_bWasHorizontalMirrorEnabled = false;
    m_bWasVerticalMirrorEnabled = false;
    m_bWasEZEnabled = false;
    m_bWasMafhamEnabled = false;
    m_fPrevPlayfieldRotationFromConVar = 0.0f;
    m_fPrevPlayfieldStretchX = 0.0f;
    m_fPrevPlayfieldStretchY = 0.0f;
    m_fPrevHitCircleDiameterForStarCache = 1.0f;
    m_fPrevSpeedForStarCache = 1.0f;
    m_bIsPreLoading = true;
    m_iPreLoadingIndex = 0;

    m_mafhamActiveRenderTarget = NULL;
    m_mafhamFinishedRenderTarget = NULL;
    m_bMafhamRenderScheduled = true;
    m_iMafhamHitObjectRenderIndex = 0;
    m_iMafhamPrevHitObjectIndex = 0;
    m_iMafhamActiveRenderHitObjectIndex = 0;
    m_iMafhamFinishedRenderHitObjectIndex = 0;
    m_bInMafhamRenderChunk = false;

    m_iMandalaIndex = 0;
}

OsuBeatmap::~OsuBeatmap() {
    anim->deleteExistingAnimation(&m_fBreakBackgroundFade);
    anim->deleteExistingAnimation(&m_fHealth2);
    anim->deleteExistingAnimation(&m_fFailAnim);

    unloadObjects();

    m_starCacheLoader->kill();

    if(engine->getResourceManager()->isLoadingResource(m_starCacheLoader)) {
        while(!m_starCacheLoader->isAsyncReady()) {
            // wait
        }
    }

    engine->getResourceManager()->destroyResource(m_starCacheLoader);
}

void OsuBeatmap::drawDebug(Graphics *g) {
    if(osu_debug_draw_timingpoints.getBool()) {
        McFont *debugFont = engine->getResourceManager()->getFont("FONT_DEFAULT");
        g->setColor(0xffffffff);
        g->pushTransform();
        g->translate(5, debugFont->getHeight() + 5 - getMousePos().y);
        {
            for(const OsuDatabaseBeatmap::TIMINGPOINT &t : m_selectedDifficulty2->getTimingpoints()) {
                g->drawString(debugFont, UString::format("%li,%f,%i,%i,%i", t.offset, t.msPerBeat, t.sampleType,
                                                         t.sampleSet, t.volume));
                g->translate(0, debugFont->getHeight());
            }
        }
        g->popTransform();
    }
}

void OsuBeatmap::drawBackground(Graphics *g) {
    if(!canDraw()) return;

    // draw beatmap background image
    {
        Image *backgroundImage = m_osu->getBackgroundImageHandler()->getLoadBackgroundImage(m_selectedDifficulty2);
        if(osu_draw_beatmap_background_image.getBool() && backgroundImage != NULL &&
           (osu_background_dim.getFloat() < 1.0f || m_fBreakBackgroundFade > 0.0f)) {
            const float scale = Osu::getImageScaleToFillResolution(backgroundImage, m_osu->getScreenSize());
            const Vector2 centerTrans = (m_osu->getScreenSize() / 2);

            const float backgroundFadeDimMultiplier =
                clamp<float>(1.0f - (osu_background_dim.getFloat() - 0.3f), 0.0f, 1.0f);
            const short dim = clamp<float>((1.0f - osu_background_dim.getFloat()) +
                                               m_fBreakBackgroundFade * backgroundFadeDimMultiplier,
                                           0.0f, 1.0f) *
                              255.0f;

            g->setColor(COLOR(255, dim, dim, dim));
            g->pushTransform();
            {
                g->scale(scale, scale);
                g->translate((int)centerTrans.x, (int)centerTrans.y);
                g->drawImage(backgroundImage);
            }
            g->popTransform();
        }
    }

    // draw background
    if(osu_background_brightness.getFloat() > 0.0f) {
        const float brightness = osu_background_brightness.getFloat();
        const short red = clamp<float>(brightness * osu_background_color_r.getFloat(), 0.0f, 255.0f);
        const short green = clamp<float>(brightness * osu_background_color_g.getFloat(), 0.0f, 255.0f);
        const short blue = clamp<float>(brightness * osu_background_color_b.getFloat(), 0.0f, 255.0f);
        const short alpha = clamp<float>(1.0f - m_fBreakBackgroundFade, 0.0f, 1.0f) * 255.0f;
        g->setColor(COLOR(alpha, red, green, blue));
        g->fillRect(0, 0, m_osu->getScreenWidth(), m_osu->getScreenHeight());
    }

    // draw scorebar-bg
    if(m_osu_draw_hud_ref->getBool() && m_osu_draw_scorebarbg_ref->getBool() &&
       (!m_osu_mod_fposu_ref->getBool() ||
        (!m_fposu_draw_scorebarbg_on_top_ref->getBool())))  // NOTE: special case for FPoSu
        m_osu->getHUD()->drawScorebarBg(
            g, m_osu_hud_scorebar_hide_during_breaks_ref->getBool() ? (1.0f - m_fBreakBackgroundFade) : 1.0f,
            m_osu->getHUD()->getScoreBarBreakAnim());

    if(Osu::debug->getBool()) {
        int y = 50;

        if(m_bIsPaused) {
            g->setColor(0xffffffff);
            g->fillRect(50, y, 15, 50);
            g->fillRect(50 + 50 - 15, y, 15, 50);
        }

        y += 100;

        if(m_bIsWaiting) {
            g->setColor(0xff00ff00);
            g->fillRect(50, y, 50, 50);
        }

        y += 100;

        if(m_bIsPlaying) {
            g->setColor(0xff0000ff);
            g->fillRect(50, y, 50, 50);
        }

        y += 100;

        if(m_bForceStreamPlayback) {
            g->setColor(0xffff0000);
            g->fillRect(50, y, 50, 50);
        }

        y += 100;

        if(m_hitobjectsSortedByEndTime.size() > 0) {
            OsuHitObject *lastHitObject = m_hitobjectsSortedByEndTime[m_hitobjectsSortedByEndTime.size() - 1];
            if(lastHitObject->isFinished() && m_iCurMusicPos > lastHitObject->getTime() + lastHitObject->getDuration() +
                                                                   (long)osu_end_skip_time.getInt()) {
                g->setColor(0xff00ffff);
                g->fillRect(50, y, 50, 50);
            }

            y += 100;
        }
    }
}

void OsuBeatmap::onKeyDown(KeyboardEvent &e) {
    if(e == KEY_O && engine->getKeyboard()->isControlDown()) {
        m_osu->toggleOptionsMenu();
        e.consume();
    }
}

void OsuBeatmap::skipEmptySection() {
    if(!m_bIsInSkippableSection) return;
    m_bIsInSkippableSection = false;
    m_osu->m_chat->updateVisibility();

    const float offset = 2500.0f;
    float offsetMultiplier = m_osu->getSpeedMultiplier();
    {
        // only compensate if not within "normal" osu mod range (would make the game feel too different regarding time
        // from skip until first hitobject)
        if(offsetMultiplier >= 0.74f && offsetMultiplier <= 1.51f) offsetMultiplier = 1.0f;

        // don't compensate speed increases at all actually
        if(offsetMultiplier > 1.0f) offsetMultiplier = 1.0f;

        // and cap slowdowns at sane value (~ spinner fadein start)
        if(offsetMultiplier <= 0.2f) offsetMultiplier = 0.2f;
    }

    const long nextHitObjectDelta = m_iNextHitObjectTime - (long)m_iCurMusicPosWithOffsets;

    if(!osu_end_skip.getBool() && nextHitObjectDelta < 0)
        m_music->setPositionMS(std::max(m_music->getLengthMS(), (unsigned long)1) - 1);
    else
        m_music->setPositionMS(std::max(m_iNextHitObjectTime - (long)(offset * offsetMultiplier), (long)0));

    engine->getSound()->play(m_osu->getSkin()->getMenuHit());
}

void OsuBeatmap::keyPressed1(bool mouse) {
    if(m_bIsWatchingReplay) return;

    if(m_bContinueScheduled) m_bClickedContinue = !m_osu->getModSelector()->isMouseInside();

    if(osu_mod_fullalternate.getBool() && m_bPrevKeyWasKey1) {
        if(m_iCurrentHitObjectIndex > m_iAllowAnyNextKeyForFullAlternateUntilHitObjectIndex) {
            engine->getSound()->play(getSkin()->getCombobreak());
            return;
        }
    }

    // key overlay & counter
    m_osu->getHUD()->animateInputoverlay(mouse ? 3 : 1, true);

    if(m_bFailed) return;

    if(!m_bInBreak && !m_bIsInSkippableSection && m_bIsPlaying) m_osu->getScore()->addKeyCount(mouse ? 3 : 1);

    m_bPrevKeyWasKey1 = true;
    m_bClick1Held = true;

    if((!m_osu->getModAuto() && !m_osu->getModRelax()) || !osu_auto_and_relax_block_user_input.getBool())
        m_clicks.push_back(m_iCurMusicPosWithOffsets);

    if(mouse) {
        current_keys = current_keys | OsuReplay::M1;
    } else {
        current_keys = current_keys | OsuReplay::M1 | OsuReplay::K1;
    }
}

void OsuBeatmap::keyPressed2(bool mouse) {
    if(m_bIsWatchingReplay) return;

    if(m_bContinueScheduled) m_bClickedContinue = !m_osu->getModSelector()->isMouseInside();

    if(osu_mod_fullalternate.getBool() && !m_bPrevKeyWasKey1) {
        if(m_iCurrentHitObjectIndex > m_iAllowAnyNextKeyForFullAlternateUntilHitObjectIndex) {
            engine->getSound()->play(getSkin()->getCombobreak());
            return;
        }
    }

    // key overlay & counter
    m_osu->getHUD()->animateInputoverlay(mouse ? 4 : 2, true);

    if(m_bFailed) return;

    if(!m_bInBreak && !m_bIsInSkippableSection && m_bIsPlaying) m_osu->getScore()->addKeyCount(mouse ? 4 : 2);

    m_bPrevKeyWasKey1 = false;
    m_bClick2Held = true;

    if((!m_osu->getModAuto() && !m_osu->getModRelax()) || !osu_auto_and_relax_block_user_input.getBool())
        m_clicks.push_back(m_iCurMusicPosWithOffsets);

    if(mouse) {
        current_keys = current_keys | OsuReplay::M2;
    } else {
        current_keys = current_keys | OsuReplay::M2 | OsuReplay::K2;
    }
}

void OsuBeatmap::keyReleased1(bool mouse) {
    if(m_bIsWatchingReplay) return;

    // key overlay
    m_osu->getHUD()->animateInputoverlay(1, false);
    m_osu->getHUD()->animateInputoverlay(3, false);

    m_bClick1Held = false;

    current_keys = current_keys & ~(OsuReplay::M1 | OsuReplay::K1);
}

void OsuBeatmap::keyReleased2(bool mouse) {
    if(m_bIsWatchingReplay) return;

    // key overlay
    m_osu->getHUD()->animateInputoverlay(2, false);
    m_osu->getHUD()->animateInputoverlay(4, false);

    m_bClick2Held = false;

    current_keys = current_keys & ~(OsuReplay::M2 | OsuReplay::K2);
}

void OsuBeatmap::select() {
    // if possible, continue playing where we left off
    if(m_music != NULL && (m_music->isPlaying())) m_iContinueMusicPos = m_music->getPositionMS();

    selectDifficulty2(m_selectedDifficulty2);

    loadMusic();
    handlePreviewPlay();
}

void OsuBeatmap::selectDifficulty2(OsuDatabaseBeatmap *difficulty2) {
    if(difficulty2 != NULL) {
        m_selectedDifficulty2 = difficulty2;

        // need to recheck/reload the music here since every difficulty might be using a different sound file
        loadMusic();
        handlePreviewPlay();
    }

    if(osu_beatmap_preview_mods_live.getBool()) onModUpdate();
}

void OsuBeatmap::deselect() {
    m_iContinueMusicPos = 0;

    unloadObjects();
}

bool OsuBeatmap::watch(Score score) {
    // Replay is invalid
    if(score.replay.size() < 3) {
        return false;
    }

    m_bIsWatchingReplay = true;
    m_osu->onBeforePlayStart();

    // Map failed to load
    if(!play()) {
        return false;
    }

    m_bIsWatchingReplay = true;  // play() resets this to false
    spectated_replay = score.replay;

    env->setCursorVisible(true);

    m_osu->m_songBrowser2->m_bHasSelectedAndIsPlaying = true;
    m_osu->m_songBrowser2->setVisible(false);
    m_osu->onPlayStart();

    return true;
}

bool OsuBeatmap::play() {
    if(m_selectedDifficulty2 == NULL) return false;

    static const int OSU_COORD_WIDTH = 512;
    static const int OSU_COORD_HEIGHT = 384;
    m_osu->flashlight_position = Vector2{OSU_COORD_WIDTH / 2, OSU_COORD_HEIGHT / 2};

    // reset everything, including deleting any previously loaded hitobjects from another diff which we might just have
    // played
    unloadObjects();
    resetScore();

    // some hitobjects already need this information to be up-to-date before their constructor is called
    updatePlayfieldMetrics();
    updateHitobjectMetrics();
    m_bIsPreLoading = false;

    // actually load the difficulty (and the hitobjects)
    {
        OsuDatabaseBeatmap::LOAD_GAMEPLAY_RESULT result = OsuDatabaseBeatmap::loadGameplay(m_selectedDifficulty2, this);
        if(result.errorCode != 0) {
            switch(result.errorCode) {
                case 1: {
                    UString errorMessage = "Error: Couldn't load beatmap metadata :(";
                    debugLog("Osu Error: Couldn't load beatmap metadata %s\n",
                             m_selectedDifficulty2->getFilePath().c_str());

                    if(m_osu != NULL) m_osu->getNotificationOverlay()->addNotification(errorMessage, 0xffff0000);
                } break;

                case 2: {
                    UString errorMessage = "Error: Couldn't load beatmap file :(";
                    debugLog("Osu Error: Couldn't load beatmap file %s\n",
                             m_selectedDifficulty2->getFilePath().c_str());

                    if(m_osu != NULL) m_osu->getNotificationOverlay()->addNotification(errorMessage, 0xffff0000);
                } break;

                case 3: {
                    UString errorMessage = "Error: No timingpoints in beatmap :(";
                    debugLog("Osu Error: No timingpoints in beatmap %s\n",
                             m_selectedDifficulty2->getFilePath().c_str());

                    if(m_osu != NULL) m_osu->getNotificationOverlay()->addNotification(errorMessage, 0xffff0000);
                } break;

                case 4: {
                    UString errorMessage = "Error: No hitobjects in beatmap :(";
                    debugLog("Osu Error: No hitobjects in beatmap %s\n", m_selectedDifficulty2->getFilePath().c_str());

                    if(m_osu != NULL) m_osu->getNotificationOverlay()->addNotification(errorMessage, 0xffff0000);
                } break;

                case 5: {
                    UString errorMessage = "Error: Too many hitobjects in beatmap :(";
                    debugLog("Osu Error: Too many hitobjects in beatmap %s\n",
                             m_selectedDifficulty2->getFilePath().c_str());

                    if(m_osu != NULL) m_osu->getNotificationOverlay()->addNotification(errorMessage, 0xffff0000);
                } break;
            }

            return false;
        }

        // move temp result data into beatmap
        m_iRandomSeed = result.randomSeed;
        m_hitobjects = std::move(result.hitobjects);
        m_breaks = std::move(result.breaks);
        m_osu->getSkin()->setBeatmapComboColors(std::move(result.combocolors));  // update combo colors in skin

        // load beatmap skin
        m_osu->getSkin()->loadBeatmapOverride(m_selectedDifficulty2->getFolder());
    }

    // the drawing order is different from the playing/input order.
    // for drawing, if multiple hitobjects occupy the exact same time (duration) then they get drawn on top of the
    // active hitobject
    m_hitobjectsSortedByEndTime = m_hitobjects;

    // sort hitobjects by endtime
    struct HitObjectSortComparator {
        bool operator()(OsuHitObject const *a, OsuHitObject const *b) const {
            // strict weak ordering!
            if((a->getTime() + a->getDuration()) == (b->getTime() + b->getDuration()))
                return a->getSortHack() < b->getSortHack();
            else
                return (a->getTime() + a->getDuration()) < (b->getTime() + b->getDuration());
        }
    };
    std::sort(m_hitobjectsSortedByEndTime.begin(), m_hitobjectsSortedByEndTime.end(), HitObjectSortComparator());

    // after the hitobjects have been loaded we can calculate the stacks
    calculateStacks();
    computeDrainRate();

    // start preloading (delays the play start until it's set to false, see isLoading())
    m_bIsPreLoading = true;
    m_iPreLoadingIndex = 0;

    // build stars
    m_fStarCacheTime =
        engine->getTime() +
        osu_pp_live_timeout
            .getFloat();  // first time delay only. subsequent updates should immediately show the loading spinner
    updateStarCache();

    // load music
    unloadMusic();  // need to reload in case of speed/pitch changes (just to be sure)
    loadMusic(false, m_bForceStreamPlayback);

    m_music->setLoop(false);
    m_bIsPaused = false;
    m_bContinueScheduled = false;

    m_bInBreak = osu_background_fade_after_load.getBool();
    anim->deleteExistingAnimation(&m_fBreakBackgroundFade);
    m_fBreakBackgroundFade = osu_background_fade_after_load.getBool() ? 1.0f : 0.0f;
    m_iPreviousSectionPassFailTime = -1;
    m_fShouldFlashSectionPass = 0.0f;
    m_fShouldFlashSectionFail = 0.0f;

    m_music->setPositionMS(0);
    m_iCurMusicPos = 0;

    // we are waiting for an asynchronous start of the beatmap in the next update()
    m_bIsWaiting = true;
    m_fWaitTime = engine->getTimeReal();

    m_bIsPlaying = true;
    m_bIsWatchingReplay = false;

    // NOTE: loading failures are handled dynamically in update(), so temporarily assume everything has worked in here
    return true;
}

void OsuBeatmap::restart(bool quick) {
    engine->getSound()->stop(getSkin()->getFailsound());

    if(!m_bIsWaiting) {
        m_bIsRestartScheduled = true;
        m_bIsRestartScheduledQuick = quick;
    } else if(m_bIsPaused)
        pause(false);
}

void OsuBeatmap::actualRestart() {
    // reset everything
    resetScore();
    resetHitObjects(-1000);

    // we are waiting for an asynchronous start of the beatmap in the next update()
    m_bIsWaiting = true;
    m_fWaitTime = engine->getTimeReal();

    // if the first hitobject starts immediately, add artificial wait time before starting the music
    if(m_hitobjects.size() > 0) {
        if(m_hitobjects[0]->getTime() < (long)osu_early_note_time.getInt()) {
            m_bIsWaiting = true;
            m_fWaitTime = engine->getTimeReal() + osu_early_note_time.getFloat() / 1000.0f;
        }
    }

    // pause temporarily if playing
    if(m_music->isPlaying()) engine->getSound()->pause(m_music);

    // reset/restore frequency (from potential fail before)
    m_music->setFrequency(0);

    m_music->setLoop(false);
    m_bIsPaused = false;
    m_bContinueScheduled = false;

    m_bInBreak = false;
    anim->deleteExistingAnimation(&m_fBreakBackgroundFade);
    m_fBreakBackgroundFade = 0.0f;
    m_iPreviousSectionPassFailTime = -1;
    m_fShouldFlashSectionPass = 0.0f;
    m_fShouldFlashSectionFail = 0.0f;

    onModUpdate();  // sanity

    // reset position
    m_music->setPositionMS(0);
    m_iCurMusicPos = 0;

    m_bIsPlaying = true;
}

void OsuBeatmap::pause(bool quitIfWaiting) {
    if(m_selectedDifficulty2 == NULL) return;

    const bool isFirstPause = !m_bContinueScheduled;

    // NOTE: this assumes that no beatmap ever goes far beyond the end of the music
    // NOTE: if pure virtual audio time is ever supported (playing without SoundEngine) then this needs to be adapted
    // fix pausing after music ends breaking beatmap state (by just not allowing it to be paused)
    if(m_fAfterMusicIsFinishedVirtualAudioTimeStart >= 0.0f) {
        const float delta = engine->getTimeReal() - m_fAfterMusicIsFinishedVirtualAudioTimeStart;
        if(delta < 5.0f)  // WARNING: sanity limit, always allow escaping after 5 seconds of overflow time
            return;
    }

    if(m_bIsPlaying)  // if we are playing, aka if this is the first time pausing
    {
        if(m_bIsWaiting && quitIfWaiting)  // if we are still m_bIsWaiting, pausing the game via the escape key is the
                                           // same as stopping playing
            stop();
        else {
            // first time pause pauses the music
            // case 1: the beatmap is already "finished", jump to the ranking screen if some small amount of time past
            // the last objects endTime case 2: in the middle somewhere, pause as usual
            OsuHitObject *lastHitObject = m_hitobjectsSortedByEndTime.size() > 0
                                              ? m_hitobjectsSortedByEndTime[m_hitobjectsSortedByEndTime.size() - 1]
                                              : NULL;
            if(lastHitObject != NULL && lastHitObject->isFinished() &&
               (m_iCurMusicPos >
                lastHitObject->getTime() + lastHitObject->getDuration() + (long)osu_end_skip_time.getInt()) &&
               osu_end_skip.getBool())
                stop(false);
            else {
                engine->getSound()->pause(m_music);
                m_bIsPlaying = false;
                m_bIsPaused = true;
            }
        }
    } else if(m_bIsPaused && !m_bContinueScheduled) {  // if this is the first time unpausing
        if(m_osu->getModAuto() || m_osu->getModAutopilot() || m_bIsInSkippableSection || m_bIsWatchingReplay) {
            if(!m_bIsWaiting) {  // only force play() if we were not early waiting
                engine->getSound()->play(m_music);
            }

            m_bIsPlaying = true;
            m_bIsPaused = false;
        } else {  // otherwise, schedule a continue (wait for user to click, handled in update())
            // first time unpause schedules a continue
            m_bIsPaused = false;
            m_bContinueScheduled = true;
        }
    } else  // if this is not the first time pausing/unpausing, then just toggle the pause state (the visibility of the
            // pause menu is handled in the Osu class, a bit shit)
        m_bIsPaused = !m_bIsPaused;

    if(m_bIsPaused) onPaused(isFirstPause);

    // if we have failed, and the user early exits to the pause menu, stop the failing animation
    if(m_bFailed) anim->deleteExistingAnimation(&m_fFailAnim);
}

void OsuBeatmap::pausePreviewMusic(bool toggle) {
    if(m_music != NULL) {
        if(m_music->isPlaying())
            engine->getSound()->pause(m_music);
        else if(toggle)
            engine->getSound()->play(m_music);
    }
}

bool OsuBeatmap::isPreviewMusicPlaying() {
    if(m_music != NULL) return m_music->isPlaying();

    return false;
}

void OsuBeatmap::stop(bool quit) {
    if(m_selectedDifficulty2 == NULL) return;

    if(getSkin()->getFailsound()->isPlaying()) engine->getSound()->stop(getSkin()->getFailsound());

    m_currentHitObject = NULL;

    m_bIsPlaying = false;
    m_bIsPaused = false;
    m_bContinueScheduled = false;

    onBeforeStop(quit);

    unloadObjects();

    if(bancho.is_playing_a_multi_map()) {
        if(quit) {
            m_osu->onPlayEnd(true);
            m_osu->m_room->ragequit();
        } else {
            m_osu->m_room->onClientScoreChange(true);
            Packet packet;
            packet.id = FINISH_MATCH;
            send_packet(packet);
        }
    } else {
        m_osu->onPlayEnd(quit);
    }
}

void OsuBeatmap::fail() {
    if(m_bFailed) return;
    if(m_bIsWatchingReplay) return;

    // Change behavior of relax mod when online
    if(bancho.is_online() && m_osu->getModRelax()) return;

    if(!bancho.is_playing_a_multi_map() && osu_drain_kill.getBool()) {
        engine->getSound()->play(getSkin()->getFailsound());

        m_bFailed = true;
        m_fFailAnim = 1.0f;
        anim->moveLinear(&m_fFailAnim, 0.0f, osu_fail_time.getFloat(),
                         true);  // trigger music slowdown and delayed menu, see update()
    } else if(!m_osu->getScore()->isDead()) {
        anim->deleteExistingAnimation(&m_fHealth2);
        m_fHealth = 0.0;
        m_fHealth2 = 0.0f;

        if(osu_drain_kill_notification_duration.getFloat() > 0.0f) {
            if(!m_osu->getScore()->hasDied())
                m_osu->getNotificationOverlay()->addNotification("You have failed, but you can keep playing!",
                                                                 0xffffffff, false,
                                                                 osu_drain_kill_notification_duration.getFloat());
        }
    }

    if(!m_osu->getScore()->isDead()) m_osu->getScore()->setDead(true);
}

void OsuBeatmap::cancelFailing() {
    if(!m_bFailed || m_fFailAnim <= 0.0f) return;

    m_bFailed = false;

    anim->deleteExistingAnimation(&m_fFailAnim);
    m_fFailAnim = 1.0f;

    if(m_music != NULL) m_music->setFrequency(0.0f);

    if(getSkin()->getFailsound()->isPlaying()) engine->getSound()->stop(getSkin()->getFailsound());
}

void OsuBeatmap::setVolume(float volume) {
    if(m_music != NULL) m_music->setVolume(volume);
}

void OsuBeatmap::setSpeed(float speed) {
    if(m_music != NULL) m_music->setSpeed(speed);
}

void OsuBeatmap::seekPercent(double percent) {
    if(m_selectedDifficulty2 == NULL || (!m_bIsPlaying && !m_bIsPaused) || m_music == NULL || m_bFailed) return;

    m_bWasSeekFrame = true;
    m_fWaitTime = 0.0f;

    m_music->setPosition(percent);
    m_music->setVolume(m_osu_volume_music_ref->getFloat());
    m_music->setSpeed(m_osu->getSpeedMultiplier());

    resetHitObjects(m_music->getPositionMS());
    resetScore();

    m_iPreviousSectionPassFailTime = -1;

    if(m_bIsWaiting) {
        m_bIsWaiting = false;
        m_bIsPlaying = true;
        m_bIsRestartScheduledQuick = false;

        engine->getSound()->play(m_music);

        onPlayStart();
    }

    if(!m_bIsWatchingReplay) {  // score submission already disabled when watching replay
        debugLog("Disabling score submission due to seeking\n");
        vanilla = false;
    }
}

void OsuBeatmap::seekPercentPlayable(double percent) {
    if(m_selectedDifficulty2 == NULL || (!m_bIsPlaying && !m_bIsPaused) || m_music == NULL || m_bFailed) return;

    m_bWasSeekFrame = true;
    m_fWaitTime = 0.0f;

    double actualPlayPercent = percent;
    if(m_hitobjects.size() > 0)
        actualPlayPercent = (((double)m_hitobjects[m_hitobjects.size() - 1]->getTime() +
                              (double)m_hitobjects[m_hitobjects.size() - 1]->getDuration()) *
                             percent) /
                            (double)m_music->getLengthMS();

    seekPercent(actualPlayPercent);
}

unsigned long OsuBeatmap::getTime() const {
    if(m_music != NULL && m_music->isAsyncReady())
        return m_music->getPositionMS();
    else
        return 0;
}

unsigned long OsuBeatmap::getStartTimePlayable() const {
    if(m_hitobjects.size() > 0)
        return (unsigned long)m_hitobjects[0]->getTime();
    else
        return 0;
}

unsigned long OsuBeatmap::getLength() const {
    if(m_music != NULL && m_music->isAsyncReady())
        return m_music->getLengthMS();
    else if(m_selectedDifficulty2 != NULL)
        return m_selectedDifficulty2->getLengthMS();
    else
        return 0;
}

unsigned long OsuBeatmap::getLengthPlayable() const {
    if(m_hitobjects.size() > 0)
        return (unsigned long)((m_hitobjects[m_hitobjects.size() - 1]->getTime() +
                                m_hitobjects[m_hitobjects.size() - 1]->getDuration()) -
                               m_hitobjects[0]->getTime());
    else
        return getLength();
}

float OsuBeatmap::getPercentFinished() const {
    if(m_music != NULL)
        return (float)m_iCurMusicPos / (float)m_music->getLengthMS();
    else
        return 0.0f;
}

float OsuBeatmap::getPercentFinishedPlayable() const {
    if(m_bIsWaiting) return 1.0f - (m_fWaitTime - engine->getTimeReal()) / (osu_early_note_time.getFloat() / 1000.0f);

    if(m_hitobjects.size() > 0)
        return (float)m_iCurMusicPos / ((float)m_hitobjects[m_hitobjects.size() - 1]->getTime() +
                                        (float)m_hitobjects[m_hitobjects.size() - 1]->getDuration());
    else
        return (float)m_iCurMusicPos / (float)m_music->getLengthMS();
}

int OsuBeatmap::getMostCommonBPM() const {
    if(m_selectedDifficulty2 != NULL) {
        if(m_music != NULL)
            return (int)(m_selectedDifficulty2->getMostCommonBPM() * m_music->getSpeed());
        else
            return (int)(m_selectedDifficulty2->getMostCommonBPM() * m_osu->getSpeedMultiplier());
    } else
        return 0;
}

float OsuBeatmap::getSpeedMultiplier() const {
    if(m_music != NULL)
        return std::max(m_music->getSpeed(), 0.05f);
    else
        return 1.0f;
}

OsuSkin *OsuBeatmap::getSkin() const { return m_osu->getSkin(); }

float OsuBeatmap::getRawAR() const {
    if(m_selectedDifficulty2 == NULL) return 5.0f;

    return clamp<float>(m_selectedDifficulty2->getAR() * m_osu->getDifficultyMultiplier(), 0.0f, 10.0f);
}

float OsuBeatmap::getAR() const {
    if(m_selectedDifficulty2 == NULL) return 5.0f;

    float AR = getRawAR();

    if(osu_ar_override.getFloat() >= 0.0f) AR = osu_ar_override.getFloat();

    if(osu_ar_overridenegative.getFloat() < 0.0f) AR = osu_ar_overridenegative.getFloat();

    if(osu_ar_override_lock.getBool())
        AR = OsuGameRules::getRawConstantApproachRateForSpeedMultiplier(
            OsuGameRules::getRawApproachTime(AR),
            (m_music != NULL && m_bIsPlaying ? getSpeedMultiplier() : m_osu->getSpeedMultiplier()));

    if(osu_mod_artimewarp.getBool() && m_hitobjects.size() > 0) {
        const float percent =
            1.0f - ((double)(m_iCurMusicPos - m_hitobjects[0]->getTime()) /
                    (double)(m_hitobjects[m_hitobjects.size() - 1]->getTime() +
                             m_hitobjects[m_hitobjects.size() - 1]->getDuration() - m_hitobjects[0]->getTime())) *
                       (1.0f - osu_mod_artimewarp_multiplier.getFloat());
        AR *= percent;
    }

    if(osu_mod_arwobble.getBool())
        AR += std::sin((m_iCurMusicPos / 1000.0f) * osu_mod_arwobble_interval.getFloat()) *
              osu_mod_arwobble_strength.getFloat();

    return AR;
}

float OsuBeatmap::getCS() const {
    if(m_selectedDifficulty2 == NULL) return 5.0f;

    float CS = clamp<float>(m_selectedDifficulty2->getCS() * m_osu->getCSDifficultyMultiplier(), 0.0f, 10.0f);

    if(osu_cs_override.getFloat() >= 0.0f) CS = osu_cs_override.getFloat();

    if(osu_cs_overridenegative.getFloat() < 0.0f) CS = osu_cs_overridenegative.getFloat();

    if(osu_mod_minimize.getBool() && m_hitobjects.size() > 0) {
        if(m_hitobjects.size() > 0) {
            const float percent =
                1.0f + ((double)(m_iCurMusicPos - m_hitobjects[0]->getTime()) /
                        (double)(m_hitobjects[m_hitobjects.size() - 1]->getTime() +
                                 m_hitobjects[m_hitobjects.size() - 1]->getDuration() - m_hitobjects[0]->getTime())) *
                           osu_mod_minimize_multiplier.getFloat();
            CS *= percent;
        }
    }

    if(osu_cs_cap_sanity.getBool()) CS = std::min(CS, 12.1429f);

    return CS;
}

float OsuBeatmap::getHP() const {
    if(m_selectedDifficulty2 == NULL) return 5.0f;

    float HP = clamp<float>(m_selectedDifficulty2->getHP() * m_osu->getDifficultyMultiplier(), 0.0f, 10.0f);
    if(osu_hp_override.getFloat() >= 0.0f) HP = osu_hp_override.getFloat();

    return HP;
}

float OsuBeatmap::getRawOD() const {
    if(m_selectedDifficulty2 == NULL) return 5.0f;

    return clamp<float>(m_selectedDifficulty2->getOD() * m_osu->getDifficultyMultiplier(), 0.0f, 10.0f);
}

float OsuBeatmap::getOD() const {
    float OD = getRawOD();

    if(osu_od_override.getFloat() >= 0.0f) OD = osu_od_override.getFloat();

    if(osu_od_override_lock.getBool())
        OD = OsuGameRules::getRawConstantOverallDifficultyForSpeedMultiplier(
            OsuGameRules::getRawHitWindow300(OD),
            (m_music != NULL && m_bIsPlaying ? getSpeedMultiplier() : m_osu->getSpeedMultiplier()));

    return OD;
}

bool OsuBeatmap::isKey1Down() {
    if(m_bIsWatchingReplay) {
        return current_keys & (OsuReplay::M1 | OsuReplay::K1);
    } else {
        return m_bClick1Held;
    }
}

bool OsuBeatmap::isKey2Down() {
    if(m_bIsWatchingReplay) {
        return current_keys & (OsuReplay::M2 | OsuReplay::K2);
    } else {
        return m_bClick2Held;
    }
}

bool OsuBeatmap::isClickHeld() {
    if(m_bIsWatchingReplay) {
        return current_keys & (OsuReplay::M1 | OsuReplay::K1 | OsuReplay::M2 | OsuReplay::K2);
    } else {
        return m_bClick1Held || m_bClick2Held;
    }
}

bool OsuBeatmap::isLastKeyDownKey1() {
    if(m_bIsWatchingReplay) {
        return last_keys & (OsuReplay::M1 | OsuReplay::K1);
    } else {
        return m_bPrevKeyWasKey1;
    }
}

UString OsuBeatmap::getTitle() const {
    if(m_selectedDifficulty2 != NULL)
        return m_selectedDifficulty2->getTitle();
    else
        return "NULL";
}

UString OsuBeatmap::getArtist() const {
    if(m_selectedDifficulty2 != NULL)
        return m_selectedDifficulty2->getArtist();
    else
        return "NULL";
}

unsigned long OsuBeatmap::getBreakDurationTotal() const {
    unsigned long breakDurationTotal = 0;
    for(int i = 0; i < m_breaks.size(); i++) {
        breakDurationTotal += (unsigned long)(m_breaks[i].endTime - m_breaks[i].startTime);
    }

    return breakDurationTotal;
}

OsuDatabaseBeatmap::BREAK OsuBeatmap::getBreakForTimeRange(long startMS, long positionMS, long endMS) const {
    OsuDatabaseBeatmap::BREAK curBreak;

    curBreak.startTime = -1;
    curBreak.endTime = -1;

    for(int i = 0; i < m_breaks.size(); i++) {
        if(m_breaks[i].startTime >= (int)startMS && m_breaks[i].endTime <= (int)endMS) {
            if((int)positionMS >= curBreak.startTime) curBreak = m_breaks[i];
        }
    }

    return curBreak;
}

OsuScore::HIT OsuBeatmap::addHitResult(OsuHitObject *hitObject, OsuScore::HIT hit, long delta, bool isEndOfCombo,
                                       bool ignoreOnHitErrorBar, bool hitErrorBarOnly, bool ignoreCombo,
                                       bool ignoreScore, bool ignoreHealth) {
    // Frames are already written on every keypress/release.
    // For some edge cases, we need to write extra frames to avoid replaybugs.
    {
        bool should_write_frame = false;

        // Slider interactions
        // Surely buzz sliders won't be an issue... Clueless
        should_write_frame |= (hit == OsuScore::HIT::HIT_SLIDER10);
        should_write_frame |= (hit == OsuScore::HIT::HIT_SLIDER30);
        should_write_frame |= (hit == OsuScore::HIT::HIT_MISS_SLIDERBREAK);

        // Relax: no keypresses, instead we write on every hitresult
        if(m_osu->getModRelax()) {
            should_write_frame |= (hit == OsuScore::HIT::HIT_50);
            should_write_frame |= (hit == OsuScore::HIT::HIT_100);
            should_write_frame |= (hit == OsuScore::HIT::HIT_300);
            should_write_frame |= (hit == OsuScore::HIT::HIT_MISS);
        }

        OsuBeatmap *beatmap = (OsuBeatmap *)m_osu->getSelectedBeatmap();
        if(should_write_frame && !hitErrorBarOnly && beatmap != nullptr) {
            beatmap->write_frame();
        }
    }

    // handle perfect & sudden death
    if(m_osu->getModSS()) {
        if(!hitErrorBarOnly && hit != OsuScore::HIT::HIT_300 && hit != OsuScore::HIT::HIT_300G &&
           hit != OsuScore::HIT::HIT_SLIDER10 && hit != OsuScore::HIT::HIT_SLIDER30 &&
           hit != OsuScore::HIT::HIT_SPINNERSPIN && hit != OsuScore::HIT::HIT_SPINNERBONUS) {
            restart();
            return OsuScore::HIT::HIT_MISS;
        }
    } else if(m_osu->getModSD()) {
        if(hit == OsuScore::HIT::HIT_MISS) {
            if(osu_mod_suddendeath_restart.getBool() && !bancho.is_in_a_multi_room())
                restart();
            else
                fail();

            return OsuScore::HIT::HIT_MISS;
        }
    }

    // miss sound
    if(hit == OsuScore::HIT::HIT_MISS) playMissSound();

    // score
    m_osu->getScore()->addHitResult(this, hitObject, hit, delta, ignoreOnHitErrorBar, hitErrorBarOnly, ignoreCombo,
                                    ignoreScore);

    // health
    OsuScore::HIT returnedHit = OsuScore::HIT::HIT_MISS;
    if(!ignoreHealth) {
        addHealth(m_osu->getScore()->getHealthIncrease(this, hit), true);

        // geki/katu handling
        if(isEndOfCombo) {
            const int comboEndBitmask = m_osu->getScore()->getComboEndBitmask();

            if(comboEndBitmask == 0) {
                returnedHit = OsuScore::HIT::HIT_300G;
                addHealth(m_osu->getScore()->getHealthIncrease(this, returnedHit), true);
                m_osu->getScore()->addHitResultComboEnd(returnedHit);
            } else if((comboEndBitmask & 2) == 0) {
                switch(hit) {
                    case OsuScore::HIT::HIT_100:
                        returnedHit = OsuScore::HIT::HIT_100K;
                        addHealth(m_osu->getScore()->getHealthIncrease(this, returnedHit), true);
                        m_osu->getScore()->addHitResultComboEnd(returnedHit);
                        break;

                    case OsuScore::HIT::HIT_300:
                        returnedHit = OsuScore::HIT::HIT_300K;
                        addHealth(m_osu->getScore()->getHealthIncrease(this, returnedHit), true);
                        m_osu->getScore()->addHitResultComboEnd(returnedHit);
                        break;
                }
            } else if(hit != OsuScore::HIT::HIT_MISS)
                addHealth(m_osu->getScore()->getHealthIncrease(this, OsuScore::HIT::HIT_MU), true);

            m_osu->getScore()->setComboEndBitmask(0);
        }
    }

    return returnedHit;
}

void OsuBeatmap::addSliderBreak() {
    // handle perfect & sudden death
    if(m_osu->getModSS()) {
        restart();
        return;
    } else if(m_osu->getModSD()) {
        if(osu_mod_suddendeath_restart.getBool())
            restart();
        else
            fail();

        return;
    }

    // miss sound
    playMissSound();

    // score
    m_osu->getScore()->addSliderBreak();
}

void OsuBeatmap::addScorePoints(int points, bool isSpinner) { m_osu->getScore()->addPoints(points, isSpinner); }

void OsuBeatmap::addHealth(double percent, bool isFromHitResult) {
    const int drainType = osu_drain_type.getInt();
    if(drainType < 2) return;

    // never drain before first hitobject
    if(m_hitobjects.size() > 0 && m_iCurMusicPosWithOffsets < m_hitobjects[0]->getTime()) return;

    // never drain after last hitobject
    if(m_hitobjectsSortedByEndTime.size() > 0 &&
       m_iCurMusicPosWithOffsets > (m_hitobjectsSortedByEndTime[m_hitobjectsSortedByEndTime.size() - 1]->getTime() +
                                    m_hitobjectsSortedByEndTime[m_hitobjectsSortedByEndTime.size() - 1]->getDuration()))
        return;

    if(m_bFailed) {
        anim->deleteExistingAnimation(&m_fHealth2);

        m_fHealth = 0.0;
        m_fHealth2 = 0.0f;

        return;
    }

    if(isFromHitResult && percent > 0.0) {
        m_osu->getHUD()->animateKiBulge();

        if(m_fHealth > 0.9) m_osu->getHUD()->animateKiExplode();
    }

    m_fHealth = clamp<double>(m_fHealth + percent, 0.0, 1.0);

    // handle generic fail state (2)
    const bool isDead = m_fHealth < 0.001;
    if(isDead && !m_osu->getModNF()) {
        if(m_osu->getModEZ() && m_osu->getScore()->getNumEZRetries() > 0)  // retries with ez
        {
            m_osu->getScore()->setNumEZRetries(m_osu->getScore()->getNumEZRetries() - 1);

            // special case: set health to 160/200 (osu!stable behavior, seems fine for all drains)
            m_fHealth = osu_drain_stable_hpbar_recovery.getFloat() / m_osu_drain_stable_hpbar_maximum_ref->getFloat();
            m_fHealth2 = (float)m_fHealth;

            anim->deleteExistingAnimation(&m_fHealth2);
        } else if(isFromHitResult && percent < 0.0)  // judgement fail
        {
            switch(drainType) {
                case 2:  // osu!stable
                    if(!osu_drain_stable_passive_fail.getBool()) fail();
                    break;

                case 3:  // osu!lazer 2020
                    if(!osu_drain_lazer_passive_fail.getBool()) fail();
                    break;

                case 4:  // osu!lazer 2018
                    fail();
                    break;
            }
        }
    }
}

void OsuBeatmap::updateTimingPoints(long curPos) {
    if(curPos < 0) return;  // aspire pls >:(

    /// debugLog("updateTimingPoints( %ld )\n", curPos);

    const OsuDatabaseBeatmap::TIMING_INFO t =
        m_selectedDifficulty2->getTimingInfoForTime(curPos + (long)osu_timingpoints_offset.getInt());
    m_osu->getSkin()->setSampleSet(
        t.sampleType);  // normal/soft/drum is stored in the sample type! the sample set number is for custom sets
    m_osu->getSkin()->setSampleVolume(clamp<float>(t.volume / 100.0f, 0.0f, 1.0f));
}

long OsuBeatmap::getPVS() {
    // this is an approximation with generous boundaries, it doesn't need to be exact (just good enough to filter 10000
    // hitobjects down to a few hundred or so) it will be used in both positive and negative directions (previous and
    // future hitobjects) to speed up loops which iterate over all hitobjects
    return OsuGameRules::getApproachTime(this) + OsuGameRules::getFadeInTime() +
           (long)OsuGameRules::getHitWindowMiss(this) + 1500;  // sanity
}

bool OsuBeatmap::canDraw() {
    if(!m_bIsPlaying && !m_bIsPaused && !m_bContinueScheduled && !m_bIsWaiting) return false;
    if(m_selectedDifficulty2 == NULL || m_music == NULL)  // sanity check
        return false;

    return true;
}

bool OsuBeatmap::canUpdate() {
    if(!m_bIsPlaying && !m_bIsPaused && !m_bContinueScheduled) return false;

    if(m_osu->getInstanceID() > 1) {
        m_music = engine->getResourceManager()->getSound("OSU_BEATMAP_MUSIC");
        if(m_music == NULL) return false;
    }

    return true;
}

void OsuBeatmap::handlePreviewPlay() {
    if(m_music != NULL && (!m_music->isPlaying() || m_music->getPosition() > 0.95f) && m_selectedDifficulty2 != NULL) {
        // this is an assumption, but should be good enough for most songs
        // reset playback position when the song has nearly reached the end (when the user switches back to the results
        // screen or the songbrowser after playing)
        if(m_music->getPosition() > 0.95f) m_iContinueMusicPos = 0;

        engine->getSound()->stop(m_music);

        if(engine->getSound()->play(m_music)) {
            if(m_music->getFrequency() < m_fMusicFrequencyBackup)  // player has died, reset frequency
                m_music->setFrequency(m_fMusicFrequencyBackup);

            if(m_osu->getMainMenu()->isVisible())
                m_music->setPositionMS(0);
            else if(m_iContinueMusicPos != 0)
                m_music->setPositionMS(m_iContinueMusicPos);
            else
                m_music->setPositionMS(m_selectedDifficulty2->getPreviewTime() < 0
                                           ? (unsigned long)(m_music->getLengthMS() * 0.40f)
                                           : m_selectedDifficulty2->getPreviewTime());

            m_music->setVolume(m_osu_volume_music_ref->getFloat());
            m_music->setSpeed(m_osu->getSpeedMultiplier());
        }
    }

    // always loop during preview
    if(m_music != NULL) m_music->setLoop(osu_beatmap_preview_music_loop.getBool());
}

void OsuBeatmap::loadMusic(bool stream, bool prescan) {
    if(m_osu->getInstanceID() > 1) {
        m_music = engine->getResourceManager()->getSound("OSU_BEATMAP_MUSIC");
        return;
    }

    stream = stream || m_bForceStreamPlayback;
    m_iResourceLoadUpdateDelayHack = 0;

    // load the song (again)
    if(m_selectedDifficulty2 != NULL &&
       (m_music == NULL || m_selectedDifficulty2->getFullSoundFilePath() != m_music->getFilePath() ||
        !m_music->isReady())) {
        unloadMusic();

        // if it's not a stream then we are loading the entire song into memory for playing
        if(!stream) engine->getResourceManager()->requestNextLoadAsync();

        m_music = engine->getResourceManager()->loadSoundAbs(
            m_selectedDifficulty2->getFullSoundFilePath(), "OSU_BEATMAP_MUSIC", stream, false, false, false,
            m_bForceStreamPlayback &&
                prescan);  // m_bForceStreamPlayback = prescan necessary! otherwise big mp3s will go out of sync
        m_music->setVolume(m_osu_volume_music_ref->getFloat());
        m_fMusicFrequencyBackup = m_music->getFrequency();
        m_music->setSpeed(m_osu->getSpeedMultiplier());
    }
}

void OsuBeatmap::unloadMusic() {
    if(m_osu->getInstanceID() < 2) {
        engine->getSound()->stop(m_music);
        engine->getResourceManager()->destroyResource(m_music);
    }

    m_music = NULL;
}

void OsuBeatmap::unloadObjects() {
    for(int i = 0; i < m_hitobjects.size(); i++) {
        delete m_hitobjects[i];
    }
    m_hitobjects = std::vector<OsuHitObject *>();
    m_hitobjectsSortedByEndTime = std::vector<OsuHitObject *>();
    m_misaimObjects = std::vector<OsuHitObject *>();
    m_breaks = std::vector<OsuDatabaseBeatmap::BREAK>();
    m_clicks = std::vector<long>();
}

void OsuBeatmap::resetHitObjects(long curPos) {
    for(int i = 0; i < m_hitobjects.size(); i++) {
        m_hitobjects[i]->onReset(curPos);
        m_hitobjects[i]->update(curPos);  // fgt
        m_hitobjects[i]->onReset(curPos);
    }
    m_osu->getHUD()->resetHitErrorBar();
}

void OsuBeatmap::resetScore() {
    vanilla = convar->isVanilla();

    live_replay.clear();
    live_replay.push_back(OsuReplay::Frame{
        .cur_music_pos = -1,
        .milliseconds_since_last_frame = 0,
        .x = 256,
        .y = -500,
        .key_flags = 0,
    });
    live_replay.push_back(OsuReplay::Frame{
        .cur_music_pos = -1,
        .milliseconds_since_last_frame = -1,
        .x = 256,
        .y = -500,
        .key_flags = 0,
    });

    last_event_time = engine->getTimeReal();
    last_event_ms = -1;
    current_keys = 0;
    last_keys = 0;
    current_frame_idx = 0;
    m_iCurMusicPos = 0;
    m_iCurMusicPosWithOffsets = 0;

    m_fHealth = 1.0;
    m_fHealth2 = 1.0f;
    m_bFailed = false;
    m_fFailAnim = 1.0f;
    anim->deleteExistingAnimation(&m_fFailAnim);

    m_osu->getScore()->reset();
    m_osu->holding_slider = false;
    m_osu->m_hud->resetScoreboard();

    m_bIsFirstMissSound = true;
}

void OsuBeatmap::playMissSound() {
    if((m_bIsFirstMissSound && m_osu->getScore()->getCombo() > 0) ||
       m_osu->getScore()->getCombo() > osu_combobreak_sound_combo.getInt()) {
        m_bIsFirstMissSound = false;
        engine->getSound()->play(getSkin()->getCombobreak());
    }
}

unsigned long OsuBeatmap::getMusicPositionMSInterpolated() {
    if(!osu_interpolate_music_pos.getBool() || isLoading())
        return m_music->getPositionMS();
    else {
        const double interpolationMultiplier = 1.0;

        // TODO: fix snapping at beginning for maps with instant start

        unsigned long returnPos = 0;
        const double curPos = (double)m_music->getPositionMS();
        const float speed = m_music->getSpeed();

        // not reinventing the wheel, the interpolation magic numbers here are (c) peppy

        const double realTime = engine->getTimeReal();
        const double interpolationDelta = (realTime - m_fLastRealTimeForInterpolationDelta) * 1000.0 * speed;
        const double interpolationDeltaLimit =
            ((realTime - m_fLastAudioTimeAccurateSet) * 1000.0 < 1500 || speed < 1.0f ? 11 : 33) *
            interpolationMultiplier;

        if(m_music->isPlaying() && !m_bWasSeekFrame) {
            double newInterpolatedPos = m_fInterpolatedMusicPos + interpolationDelta;
            double delta = newInterpolatedPos - curPos;

            // debugLog("delta = %ld\n", (long)delta);

            // approach and recalculate delta
            newInterpolatedPos -= delta / 8.0 / interpolationMultiplier;
            delta = newInterpolatedPos - curPos;

            if(std::abs(delta) > interpolationDeltaLimit * 2)  // we're fucked, snap back to curPos
            {
                m_fInterpolatedMusicPos = (double)curPos;
            } else if(delta < -interpolationDeltaLimit)  // undershot
            {
                m_fInterpolatedMusicPos += interpolationDelta * 2;
                m_fLastAudioTimeAccurateSet = realTime;
            } else if(delta < interpolationDeltaLimit)  // normal
            {
                m_fInterpolatedMusicPos = newInterpolatedPos;
            } else  // overshot
            {
                m_fInterpolatedMusicPos += interpolationDelta / 2;
                m_fLastAudioTimeAccurateSet = realTime;
            }

            // calculate final return value
            returnPos = (unsigned long)std::round(m_fInterpolatedMusicPos);

            bool nightcoring = m_osu->getModNC() || m_osu->getModDC();
            if(speed < 1.0f && osu_compensate_music_speed.getBool() && !nightcoring) {
                returnPos += (unsigned long)(((1.0f - speed) / 0.75f) * 5);
            }
        } else  // no interpolation
        {
            returnPos = curPos;
            m_fInterpolatedMusicPos = (unsigned long)returnPos;
            m_fLastAudioTimeAccurateSet = realTime;
        }

        m_fLastRealTimeForInterpolationDelta =
            realTime;  // this is more accurate than engine->getFrameTime() for the delta calculation, since it
                       // correctly handles all possible delays inbetween

        // debugLog("returning %lu \n", returnPos);
        // debugLog("delta = %lu\n", (long)returnPos - m_iCurMusicPos);
        // debugLog("raw delta = %ld\n", (long)returnPos - (long)curPos);

        return returnPos;
    }
}

void OsuBeatmap::draw(Graphics *g) {
    if(!canDraw()) return;

    // draw background
    drawBackground(g);

    // draw loading circle
    if(isLoading()) m_osu->getHUD()->drawLoadingSmall(g);

    if(isLoadingStarCache() && engine->getTime() > m_fStarCacheTime) {
        float progressPercent = 0.0f;
        if(m_hitobjects.size() > 0)
            progressPercent = (float)m_starCacheLoader->getProgress() / (float)m_hitobjects.size();

        g->setColor(0x44ffffff);
        UString loadingMessage =
            UString::format("Calculating stars for realtime pp/stars (%i%%) ...", (int)(progressPercent * 100.0f));
        UString loadingMessage2 = "(To get rid of this delay, disable [Draw Statistics: pp/Stars***])";
        g->pushTransform();
        {
            g->translate(
                (int)(m_osu->getScreenWidth() / 2 - m_osu->getSubTitleFont()->getStringWidth(loadingMessage) / 2),
                m_osu->getScreenHeight() - m_osu->getSubTitleFont()->getHeight() - 25);
            g->drawString(m_osu->getSubTitleFont(), loadingMessage);
        }
        g->popTransform();
        g->pushTransform();
        {
            g->translate(
                (int)(m_osu->getScreenWidth() / 2 - m_osu->getSubTitleFont()->getStringWidth(loadingMessage2) / 2),
                m_osu->getScreenHeight() - 15);
            g->drawString(m_osu->getSubTitleFont(), loadingMessage2);
        }
        g->popTransform();
    } else if(bancho.is_playing_a_multi_map() && !bancho.room.all_players_loaded) {
        if(!m_bIsPreLoading && !isLoadingStarCache())  // usability
        {
            g->setColor(0x44ffffff);
            UString loadingMessage = "Waiting for players ...";
            g->pushTransform();
            {
                g->translate(
                    (int)(m_osu->getScreenWidth() / 2 - m_osu->getSubTitleFont()->getStringWidth(loadingMessage) / 2),
                    m_osu->getScreenHeight() - m_osu->getSubTitleFont()->getHeight() - 15);
                g->drawString(m_osu->getSubTitleFont(), loadingMessage);
            }
            g->popTransform();
        }
    }

    if(isLoading()) return;  // only start drawing the rest of the playfield if everything has loaded

    // draw playfield border
    if(osu_draw_playfield_border.getBool() && !OsuGameRules::osu_mod_fps.getBool())
        m_osu->getHUD()->drawPlayfieldBorder(g, m_vPlayfieldCenter, m_vPlayfieldSize, m_fHitcircleDiameter);

    // draw hiterrorbar
    if(!m_osu_mod_fposu_ref->getBool()) m_osu->getHUD()->drawHitErrorBar(g, this);

    // draw first person crosshair
    if(OsuGameRules::osu_mod_fps.getBool()) {
        const int length = 15;
        Vector2 center =
            osuCoords2Pixels(Vector2(OsuGameRules::OSU_COORD_WIDTH / 2, OsuGameRules::OSU_COORD_HEIGHT / 2));
        g->setColor(0xff777777);
        g->drawLine(center.x, (int)(center.y - length), center.x, (int)(center.y + length + 1));
        g->drawLine((int)(center.x - length), center.y, (int)(center.x + length + 1), center.y);
    }

    // draw followpoints
    if(osu_draw_followpoints.getBool() && !OsuGameRules::osu_mod_mafham.getBool()) drawFollowPoints(g);

    // draw all hitobjects in reverse
    if(m_osu_draw_hitobjects_ref->getBool()) drawHitObjects(g);

    if(osu_mandala.getBool()) {
        for(int i = 0; i < osu_mandala_num.getInt(); i++) {
            m_iMandalaIndex = i;
            drawHitObjects(g);
        }
    }

    // debug stuff
    if(osu_debug_hiterrorbar_misaims.getBool()) {
        for(int i = 0; i < m_misaimObjects.size(); i++) {
            g->setColor(0xbb00ff00);
            Vector2 pos = osuCoords2Pixels(m_misaimObjects[i]->getRawPosAt(0));
            g->fillRect(pos.x - 50, pos.y - 50, 100, 100);
        }
    }
}

void OsuBeatmap::drawFollowPoints(Graphics *g) {
    OsuSkin *skin = m_osu->getSkin();

    const long curPos = m_iCurMusicPosWithOffsets;

    // I absolutely hate this, followpoints can be abused for cheesing high AR reading since they always fade in with a
    // fixed 800 ms custom approach time. Capping it at the current approach rate seems sensible, but unfortunately
    // that's not what osu is doing. It was non-osu-compliant-clamped since this client existed, but let's see how many
    // people notice a change after all this time (26.02.2020)

    // 0.7x means animation lasts only 0.7 of it's time
    const double animationMutiplier = m_osu->getSpeedMultiplier() / m_osu->getAnimationSpeedMultiplier();
    const long followPointApproachTime =
        animationMutiplier *
        (osu_followpoints_clamp.getBool()
             ? std::min((long)OsuGameRules::getApproachTime(this), (long)osu_followpoints_approachtime.getFloat())
             : (long)osu_followpoints_approachtime.getFloat());
    const bool followPointsConnectCombos = osu_followpoints_connect_combos.getBool();
    const bool followPointsConnectSpinners = osu_followpoints_connect_spinners.getBool();
    const float followPointSeparationMultiplier = std::max(osu_followpoints_separation_multiplier.getFloat(), 0.1f);
    const float followPointPrevFadeTime = animationMutiplier * m_osu_followpoints_prevfadetime_ref->getFloat();
    const float followPointScaleMultiplier = osu_followpoints_scale_multiplier.getFloat();

    // include previous object in followpoints
    int lastObjectIndex = -1;

    for(int index = m_iPreviousFollowPointObjectIndex; index < m_hitobjects.size(); index++) {
        lastObjectIndex = index - 1;

        // ignore future spinners
        OsuSpinner *spinnerPointer = dynamic_cast<OsuSpinner *>(m_hitobjects[index]);
        if(spinnerPointer != NULL && !followPointsConnectSpinners)  // if this is a spinner
        {
            lastObjectIndex = -1;
            continue;
        }

        // NOTE: "m_hitobjects[index]->getComboNumber() != 1" breaks (not literally) on new combos
        // NOTE: the "getComboNumber()" call has been replaced with isEndOfCombo() because of
        // osu_ignore_beatmap_combo_numbers and osu_number_max
        const bool isCurrentHitObjectNewCombo =
            (lastObjectIndex >= 0 ? m_hitobjects[lastObjectIndex]->isEndOfCombo() : false);
        const bool isCurrentHitObjectSpinner = (lastObjectIndex >= 0 && followPointsConnectSpinners
                                                    ? dynamic_cast<OsuSpinner *>(m_hitobjects[lastObjectIndex]) != NULL
                                                    : false);
        if(lastObjectIndex >= 0 && (!isCurrentHitObjectNewCombo || followPointsConnectCombos ||
                                    (isCurrentHitObjectSpinner && followPointsConnectSpinners))) {
            // ignore previous spinners
            spinnerPointer = dynamic_cast<OsuSpinner *>(m_hitobjects[lastObjectIndex]);
            if(spinnerPointer != NULL && !followPointsConnectSpinners)  // if this is a spinner
            {
                lastObjectIndex = -1;
                continue;
            }

            // get time & pos of the last and current object
            const long lastObjectEndTime =
                m_hitobjects[lastObjectIndex]->getTime() + m_hitobjects[lastObjectIndex]->getDuration() + 1;
            const long objectStartTime = m_hitobjects[index]->getTime();
            const long timeDiff = objectStartTime - lastObjectEndTime;

            const Vector2 startPoint = osuCoords2Pixels(m_hitobjects[lastObjectIndex]->getRawPosAt(lastObjectEndTime));
            const Vector2 endPoint = osuCoords2Pixels(m_hitobjects[index]->getRawPosAt(objectStartTime));

            const float xDiff = endPoint.x - startPoint.x;
            const float yDiff = endPoint.y - startPoint.y;
            const Vector2 diff = endPoint - startPoint;
            const float dist =
                std::round(diff.length() * 100.0f) / 100.0f;  // rounded to avoid flicker with playfield rotations

            // draw all points between the two objects
            const int followPointSeparation = Osu::getUIScale(m_osu, 32) * followPointSeparationMultiplier;
            for(int j = (int)(followPointSeparation * 1.5f); j < (dist - followPointSeparation);
                j += followPointSeparation) {
                const float animRatio = ((float)j / dist);

                const Vector2 animPosStart = startPoint + (animRatio - 0.1f) * diff;
                const Vector2 finalPos = startPoint + animRatio * diff;

                const long fadeInTime = (long)(lastObjectEndTime + animRatio * timeDiff) - followPointApproachTime;
                const long fadeOutTime = (long)(lastObjectEndTime + animRatio * timeDiff);

                // draw
                float alpha = 1.0f;
                float followAnimPercent =
                    clamp<float>((float)(curPos - fadeInTime) / (float)followPointPrevFadeTime, 0.0f, 1.0f);
                followAnimPercent = -followAnimPercent * (followAnimPercent - 2.0f);  // quad out

                // NOTE: only internal osu default skin uses scale + move transforms here, it is impossible to achieve
                // this effect with user skins
                const float scale = osu_followpoints_anim.getBool() ? 1.5f - 0.5f * followAnimPercent : 1.0f;
                const Vector2 followPos = osu_followpoints_anim.getBool()
                                              ? animPosStart + (finalPos - animPosStart) * followAnimPercent
                                              : finalPos;

                // bullshit performance optimization: only draw followpoints if within screen bounds (plus a bit of a
                // margin) there is only one beatmap where this matters currently: https://osu.ppy.sh/b/1145513
                if(followPos.x < -m_osu->getScreenWidth() || followPos.x > m_osu->getScreenWidth() * 2 ||
                   followPos.y < -m_osu->getScreenHeight() || followPos.y > m_osu->getScreenHeight() * 2)
                    continue;

                // calculate trail alpha
                if(curPos >= fadeInTime && curPos < fadeOutTime) {
                    // future trail
                    const float delta = curPos - fadeInTime;
                    alpha = (float)delta / (float)followPointApproachTime;
                } else if(curPos >= fadeOutTime && curPos < (fadeOutTime + (long)followPointPrevFadeTime)) {
                    // previous trail
                    const long delta = curPos - fadeOutTime;
                    alpha = 1.0f - (float)delta / (float)(followPointPrevFadeTime);
                } else
                    alpha = 0.0f;

                // draw it
                g->setColor(0xffffffff);
                g->setAlpha(alpha);
                g->pushTransform();
                {
                    g->rotate(rad2deg(std::atan2(yDiff, xDiff)));

                    skin->getFollowPoint2()->setAnimationTimeOffset(skin->getAnimationSpeed(), fadeInTime);

                    // NOTE: getSizeBaseRaw() depends on the current animation time being set correctly beforehand!
                    // (otherwise you get incorrect scales, e.g. for animated elements with inconsistent @2x mixed in)
                    // the followpoints are scaled by one eighth of the hitcirclediameter (not the raw diameter, but the
                    // scaled diameter)
                    const float followPointImageScale =
                        ((m_fHitcircleDiameter / 8.0f) / skin->getFollowPoint2()->getSizeBaseRaw().x) *
                        followPointScaleMultiplier;

                    skin->getFollowPoint2()->drawRaw(g, followPos, followPointImageScale * scale);
                }
                g->popTransform();
            }
        }

        // store current index as previous index
        lastObjectIndex = index;

        // iterate up until the "nextest" element
        if(m_hitobjects[index]->getTime() >= curPos + followPointApproachTime) break;
    }
}

void OsuBeatmap::drawHitObjects(Graphics *g) {
    const long curPos = m_iCurMusicPosWithOffsets;
    const long pvs = getPVS();
    const bool usePVS = m_osu_pvs->getBool();

    if(!OsuGameRules::osu_mod_mafham.getBool()) {
        if(!osu_draw_reverse_order.getBool()) {
            for(int i = m_hitobjectsSortedByEndTime.size() - 1; i >= 0; i--) {
                // PVS optimization (reversed)
                if(usePVS) {
                    if(m_hitobjectsSortedByEndTime[i]->isFinished() &&
                       (curPos - pvs > m_hitobjectsSortedByEndTime[i]->getTime() +
                                           m_hitobjectsSortedByEndTime[i]->getDuration()))  // past objects
                        break;
                    if(m_hitobjectsSortedByEndTime[i]->getTime() > curPos + pvs)  // future objects
                        continue;
                }

                m_hitobjectsSortedByEndTime[i]->draw(g);
            }
        } else {
            for(int i = 0; i < m_hitobjectsSortedByEndTime.size(); i++) {
                // PVS optimization
                if(usePVS) {
                    if(m_hitobjectsSortedByEndTime[i]->isFinished() &&
                       (curPos - pvs > m_hitobjectsSortedByEndTime[i]->getTime() +
                                           m_hitobjectsSortedByEndTime[i]->getDuration()))  // past objects
                        continue;
                    if(m_hitobjectsSortedByEndTime[i]->getTime() > curPos + pvs)  // future objects
                        break;
                }

                m_hitobjectsSortedByEndTime[i]->draw(g);
            }
        }
        for(int i = 0; i < m_hitobjectsSortedByEndTime.size(); i++) {
            // NOTE: to fix mayday simultaneous sliders with increasing endtime getting culled here, would have to
            // switch from m_hitobjectsSortedByEndTime to m_hitobjects PVS optimization
            if(usePVS) {
                if(m_hitobjectsSortedByEndTime[i]->isFinished() &&
                   (curPos - pvs > m_hitobjectsSortedByEndTime[i]->getTime() +
                                       m_hitobjectsSortedByEndTime[i]->getDuration()))  // past objects
                    continue;
                if(m_hitobjectsSortedByEndTime[i]->getTime() > curPos + pvs)  // future objects
                    break;
            }

            m_hitobjectsSortedByEndTime[i]->draw2(g);
        }
    } else {
        const int mafhamRenderLiveSize = OsuGameRules::osu_mod_mafham_render_livesize.getInt();

        if(m_mafhamActiveRenderTarget == NULL) m_mafhamActiveRenderTarget = m_osu->getFrameBuffer();

        if(m_mafhamFinishedRenderTarget == NULL) m_mafhamFinishedRenderTarget = m_osu->getFrameBuffer2();

        // if we have a chunk to render into the scene buffer
        const bool shouldDrawBuffer =
            (m_hitobjectsSortedByEndTime.size() - m_iCurrentHitObjectIndex) > mafhamRenderLiveSize;
        bool shouldRenderChunk = m_iMafhamHitObjectRenderIndex < m_hitobjectsSortedByEndTime.size() && shouldDrawBuffer;
        if(shouldRenderChunk) {
            m_bInMafhamRenderChunk = true;

            m_mafhamActiveRenderTarget->setClearColorOnDraw(m_iMafhamHitObjectRenderIndex == 0);
            m_mafhamActiveRenderTarget->setClearDepthOnDraw(m_iMafhamHitObjectRenderIndex == 0);

            m_mafhamActiveRenderTarget->enable();
            {
                g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_PREMUL_ALPHA);
                {
                    int chunkCounter = 0;
                    for(int i = m_hitobjectsSortedByEndTime.size() - 1 - m_iMafhamHitObjectRenderIndex; i >= 0;
                        i--, m_iMafhamHitObjectRenderIndex++) {
                        chunkCounter++;
                        if(chunkCounter > osu_mod_mafham_render_chunksize.getInt())
                            break;  // continue chunk render in next frame

                        if(i <= m_iCurrentHitObjectIndex + mafhamRenderLiveSize)  // skip live objects
                        {
                            m_iMafhamHitObjectRenderIndex = m_hitobjectsSortedByEndTime.size();  // stop chunk render
                            break;
                        }

                        // PVS optimization (reversed)
                        if(usePVS) {
                            if(m_hitobjectsSortedByEndTime[i]->isFinished() &&
                               (curPos - pvs > m_hitobjectsSortedByEndTime[i]->getTime() +
                                                   m_hitobjectsSortedByEndTime[i]->getDuration()))  // past objects
                            {
                                m_iMafhamHitObjectRenderIndex =
                                    m_hitobjectsSortedByEndTime.size();  // stop chunk render
                                break;
                            }
                            if(m_hitobjectsSortedByEndTime[i]->getTime() > curPos + pvs)  // future objects
                                continue;
                        }

                        m_hitobjectsSortedByEndTime[i]->draw(g);

                        m_iMafhamActiveRenderHitObjectIndex = i;
                    }
                }
                g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
            }
            m_mafhamActiveRenderTarget->disable();

            m_bInMafhamRenderChunk = false;
        }
        shouldRenderChunk = m_iMafhamHitObjectRenderIndex < m_hitobjectsSortedByEndTime.size() && shouldDrawBuffer;
        if(!shouldRenderChunk && m_bMafhamRenderScheduled) {
            // finished, we can now swap the active framebuffer with the one we just finished
            m_bMafhamRenderScheduled = false;

            RenderTarget *temp = m_mafhamFinishedRenderTarget;
            m_mafhamFinishedRenderTarget = m_mafhamActiveRenderTarget;
            m_mafhamActiveRenderTarget = temp;

            m_iMafhamFinishedRenderHitObjectIndex = m_iMafhamActiveRenderHitObjectIndex;
            m_iMafhamActiveRenderHitObjectIndex = m_hitobjectsSortedByEndTime.size();  // reset
        }

        // draw scene buffer
        if(shouldDrawBuffer) {
            g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_PREMUL_COLOR);
            { m_mafhamFinishedRenderTarget->draw(g, 0, 0); }
            g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
        }

        // draw followpoints
        if(osu_draw_followpoints.getBool()) drawFollowPoints(g);

        // draw live hitobjects (also, code duplication yay)
        {
            for(int i = m_hitobjectsSortedByEndTime.size() - 1; i >= 0; i--) {
                // PVS optimization (reversed)
                if(usePVS) {
                    if(m_hitobjectsSortedByEndTime[i]->isFinished() &&
                       (curPos - pvs > m_hitobjectsSortedByEndTime[i]->getTime() +
                                           m_hitobjectsSortedByEndTime[i]->getDuration()))  // past objects
                        break;
                    if(m_hitobjectsSortedByEndTime[i]->getTime() > curPos + pvs)  // future objects
                        continue;
                }

                if(i > m_iCurrentHitObjectIndex + mafhamRenderLiveSize ||
                   (i > m_iMafhamFinishedRenderHitObjectIndex - 1 && shouldDrawBuffer))  // skip non-live objects
                    continue;

                m_hitobjectsSortedByEndTime[i]->draw(g);
            }

            for(int i = 0; i < m_hitobjectsSortedByEndTime.size(); i++) {
                // PVS optimization
                if(usePVS) {
                    if(m_hitobjectsSortedByEndTime[i]->isFinished() &&
                       (curPos - pvs > m_hitobjectsSortedByEndTime[i]->getTime() +
                                           m_hitobjectsSortedByEndTime[i]->getDuration()))  // past objects
                        continue;
                    if(m_hitobjectsSortedByEndTime[i]->getTime() > curPos + pvs)  // future objects
                        break;
                }

                if(i >= m_iCurrentHitObjectIndex + mafhamRenderLiveSize ||
                   (i >= m_iMafhamFinishedRenderHitObjectIndex - 1 && shouldDrawBuffer))  // skip non-live objects
                    break;

                m_hitobjectsSortedByEndTime[i]->draw2(g);
            }
        }
    }
}

void OsuBeatmap::update() {
    if(!canUpdate()) return;

    // some things need to be updated before loading has finished, so control flow is a bit weird here.

    // live update hitobject and playfield metrics
    updateHitobjectMetrics();
    updatePlayfieldMetrics();

    // wobble mod
    if(osu_mod_wobble.getBool()) {
        const float speedMultiplierCompensation = 1.0f / getSpeedMultiplier();
        m_fPlayfieldRotation =
            (m_iCurMusicPos / 1000.0f) * 30.0f * speedMultiplierCompensation * osu_mod_wobble_rotation_speed.getFloat();
        m_fPlayfieldRotation = std::fmod(m_fPlayfieldRotation, 360.0f);
    } else
        m_fPlayfieldRotation = 0.0f;

    // do hitobject updates among other things
    // yes, this needs to happen after updating metrics and playfield rotation
    update2();

    // handle preloading (only for distributed slider vertexbuffer generation atm)
    if(m_bIsPreLoading) {
        if(Osu::debug->getBool() && m_iPreLoadingIndex == 0)
            debugLog("OsuBeatmap: Preloading slider vertexbuffers ...\n");

        double startTime = engine->getTimeReal();
        double delta = 0.0;

        // hardcoded deadline of 10 ms, will temporarily bring us down to 45fps on average (better than freezing)
        while(delta < 0.010 && m_bIsPreLoading) {
            if(m_iPreLoadingIndex >= m_hitobjects.size()) {
                m_bIsPreLoading = false;
                debugLog("OsuBeatmap: Preloading done.\n");
                break;
            } else {
                OsuSlider *sliderPointer = dynamic_cast<OsuSlider *>(m_hitobjects[m_iPreLoadingIndex]);
                if(sliderPointer != NULL) sliderPointer->rebuildVertexBuffer();
            }

            m_iPreLoadingIndex++;
            delta = engine->getTimeReal() - startTime;
        }
    }

    // notify all other players (including ourself) once we've finished loading
    if(bancho.is_playing_a_multi_map()) {
        if(!isActuallyLoading()) {
            if(!bancho.room.player_loaded) {
                bancho.room.player_loaded = true;

                Packet packet;
                packet.id = MATCH_LOAD_COMPLETE;
                send_packet(packet);
            }
        }
    }

    if(isLoading()) return;  // only continue if we have loaded everything

    // update auto (after having updated the hitobjects)
    if(m_osu->getModAuto() || m_osu->getModAutopilot()) updateAutoCursorPos();

    // spinner detection (used by osu!stable drain, and by OsuHUD for not drawing the hiterrorbar)
    if(m_currentHitObject != NULL) {
        OsuSpinner *spinnerPointer = dynamic_cast<OsuSpinner *>(m_currentHitObject);
        if(spinnerPointer != NULL && m_iCurMusicPosWithOffsets > m_currentHitObject->getTime() &&
           m_iCurMusicPosWithOffsets < m_currentHitObject->getTime() + m_currentHitObject->getDuration())
            m_bIsSpinnerActive = true;
        else
            m_bIsSpinnerActive = false;
    }

    // scene buffering logic
    if(OsuGameRules::osu_mod_mafham.getBool()) {
        if(!m_bMafhamRenderScheduled &&
           m_iCurrentHitObjectIndex !=
               m_iMafhamPrevHitObjectIndex)  // if we are not already rendering and the index changed
        {
            m_iMafhamPrevHitObjectIndex = m_iCurrentHitObjectIndex;
            m_iMafhamHitObjectRenderIndex = 0;
            m_bMafhamRenderScheduled = true;
        }
    }

    // full alternate mod lenience
    if(m_osu_mod_fullalternate_ref->getBool()) {
        if(m_bInBreak || m_bIsInSkippableSection || m_bIsSpinnerActive || m_iCurrentHitObjectIndex < 1)
            m_iAllowAnyNextKeyForFullAlternateUntilHitObjectIndex = m_iCurrentHitObjectIndex + 1;
    }

    if(last_keys != current_keys) {
        write_frame();
    } else if(last_event_time + 0.01666666666 <= engine->getTimeReal()) {
        write_frame();
    }
}

void OsuBeatmap::update2() {
    long osu_universal_offset_hardcoded = convar->getConVarByName("osu_universal_offset_hardcoded")->getInt();

    if(m_bContinueScheduled) {
        // If we paused while m_bIsWaiting (green progressbar), then we have to let the 'if (m_bIsWaiting)' block handle
        // the sound play() call
        bool isEarlyNoteContinue = (!m_bIsPaused && m_bIsWaiting);
        if(m_bClickedContinue || isEarlyNoteContinue) {
            m_bClickedContinue = false;
            m_bContinueScheduled = false;
            m_bIsPaused = false;

            if(!isEarlyNoteContinue) {
                engine->getSound()->play(m_music);
            }

            m_bIsPlaying = true;  // usually this should be checked with the result of the above play() call, but since
                                  // we are continuing we can assume that everything works

            // for nightmare mod, to avoid a miss because of the continue click
            m_clicks.clear();
        }
    }

    // handle restarts
    if(m_bIsRestartScheduled) {
        m_bIsRestartScheduled = false;
        actualRestart();
        return;
    }

    // update current music position (this variable does not include any offsets!)
    m_iCurMusicPos = getMusicPositionMSInterpolated();
    m_iContinueMusicPos = m_music->getPositionMS();
    const bool wasSeekFrame = m_bWasSeekFrame;
    m_bWasSeekFrame = false;

    // handle timewarp
    if(osu_mod_timewarp.getBool()) {
        if(m_hitobjects.size() > 0 && m_iCurMusicPos > m_hitobjects[0]->getTime()) {
            const float percentFinished =
                ((double)(m_iCurMusicPos - m_hitobjects[0]->getTime()) /
                 (double)(m_hitobjects[m_hitobjects.size() - 1]->getTime() +
                          m_hitobjects[m_hitobjects.size() - 1]->getDuration() - m_hitobjects[0]->getTime()));
            float warp_multiplier = std::max(osu_mod_timewarp_multiplier.getFloat(), 1.f);
            const float speed =
                m_osu->getSpeedMultiplier() + percentFinished * m_osu->getSpeedMultiplier() * (warp_multiplier - 1.0f);
            m_music->setSpeed(speed);
        }
    }

    // HACKHACK: clean this mess up
    // waiting to start (file loading, retry)
    // NOTE: this is dependent on being here AFTER m_iCurMusicPos has been set above, because it modifies it to fake a
    // negative start (else everything would just freeze for the waiting period)
    if(m_bIsWaiting) {
        if(isLoading()) {
            m_fWaitTime = engine->getTimeReal();

            // if the first hitobject starts immediately, add artificial wait time before starting the music
            if(!m_bIsRestartScheduledQuick && m_hitobjects.size() > 0) {
                if(m_hitobjects[0]->getTime() < (long)osu_early_note_time.getInt())
                    m_fWaitTime = engine->getTimeReal() + osu_early_note_time.getFloat() / 1000.0f;
            }
        } else {
            if(engine->getTimeReal() > m_fWaitTime) {
                if(!m_bIsPaused) {
                    m_bIsWaiting = false;
                    m_bIsPlaying = true;

                    engine->getSound()->play(m_music);
                    m_music->setPositionMS(0);
                    m_music->setVolume(m_osu_volume_music_ref->getFloat());
                    m_music->setSpeed(m_osu->getSpeedMultiplier());

                    // if we are quick restarting, jump just before the first hitobject (even if there is a long waiting
                    // period at the beginning with nothing etc.)
                    if(m_bIsRestartScheduledQuick && m_hitobjects.size() > 0 &&
                       m_hitobjects[0]->getTime() > (long)osu_quick_retry_time.getInt())
                        m_music->setPositionMS(
                            std::max((long)0, m_hitobjects[0]->getTime() - (long)osu_quick_retry_time.getInt()));

                    m_bIsRestartScheduledQuick = false;

                    onPlayStart();
                }
            } else
                m_iCurMusicPos = (engine->getTimeReal() - m_fWaitTime) * 1000.0f * m_osu->getSpeedMultiplier();
        }

        // ugh. force update all hitobjects while waiting (necessary because of pvs optimization)
        long curPos = m_iCurMusicPos + (long)(osu_universal_offset.getFloat() * m_osu->getSpeedMultiplier()) +
                      osu_universal_offset_hardcoded - m_selectedDifficulty2->getLocalOffset() -
                      m_selectedDifficulty2->getOnlineOffset() -
                      (m_selectedDifficulty2->getVersion() < 5 ? osu_old_beatmap_offset.getInt() : 0);
        if(curPos > -1)  // otherwise auto would already click elements that start at exactly 0 (while the map has not
                         // even started)
            curPos = -1;

        for(int i = 0; i < m_hitobjects.size(); i++) {
            m_hitobjects[i]->update(curPos);
        }
    }

    // only continue updating hitobjects etc. if we have loaded everything
    if(isLoading()) return;

    // handle music loading fail
    if(!m_music->isReady()) {
        m_iResourceLoadUpdateDelayHack++;  // HACKHACK: async loading takes 1 additional engine update() until both
                                           // isAsyncReady() and isReady() return true
        if(m_iResourceLoadUpdateDelayHack > 1 &&
           !m_bForceStreamPlayback)  // first: try loading a stream version of the music file
        {
            m_bForceStreamPlayback = true;
            unloadMusic();
            loadMusic(true, m_bForceStreamPlayback);

            // we are waiting for an asynchronous start of the beatmap in the next update()
            m_bIsWaiting = true;
            m_fWaitTime = engine->getTimeReal();
        } else if(m_iResourceLoadUpdateDelayHack >
                  3)  // second: if that still doesn't work, stop and display an error message
        {
            m_osu->getNotificationOverlay()->addNotification("Couldn't load music file :(", 0xffff0000);
            stop(true);
        }
    }

    // detect and handle music end
    if(!m_bIsWaiting && m_music->isReady()) {
        const bool isMusicFinished = m_music->isFinished();

        // trigger virtual audio time after music finishes
        if(!isMusicFinished)
            m_fAfterMusicIsFinishedVirtualAudioTimeStart = -1.0f;
        else if(m_fAfterMusicIsFinishedVirtualAudioTimeStart < 0.0f)
            m_fAfterMusicIsFinishedVirtualAudioTimeStart = engine->getTimeReal();

        if(isMusicFinished) {
            // continue with virtual audio time until the last hitobject is done (plus sanity offset given via
            // osu_end_delay_time) because some beatmaps have hitobjects going until >= the exact end of the music ffs
            // NOTE: this overwrites m_iCurMusicPos for the rest of the update loop
            m_iCurMusicPos = (long)m_music->getLengthMS() +
                             (long)((engine->getTimeReal() - m_fAfterMusicIsFinishedVirtualAudioTimeStart) * 1000.0f);
        }

        const bool hasAnyHitObjects = (m_hitobjects.size() > 0);
        const bool isTimePastLastHitObjectPlusLenience =
            (m_iCurMusicPos > (m_hitobjectsSortedByEndTime[m_hitobjectsSortedByEndTime.size() - 1]->getTime() +
                               m_hitobjectsSortedByEndTime[m_hitobjectsSortedByEndTime.size() - 1]->getDuration() +
                               (long)osu_end_delay_time.getInt()));
        if(!hasAnyHitObjects || (osu_end_skip.getBool() && isTimePastLastHitObjectPlusLenience) ||
           (!osu_end_skip.getBool() && isMusicFinished)) {
            if(!m_bFailed) {
                stop(false);
                return;
            }
        }
    }

    // update timing (points)
    m_iCurMusicPosWithOffsets = m_iCurMusicPos + (long)(osu_universal_offset.getFloat() * m_osu->getSpeedMultiplier()) +
                                osu_universal_offset_hardcoded - m_selectedDifficulty2->getLocalOffset() -
                                m_selectedDifficulty2->getOnlineOffset() -
                                (m_selectedDifficulty2->getVersion() < 5 ? osu_old_beatmap_offset.getInt() : 0);
    updateTimingPoints(m_iCurMusicPosWithOffsets);

    if(m_bIsWatchingReplay) {
        OsuReplay::Frame current_frame = spectated_replay[current_frame_idx];
        OsuReplay::Frame next_frame = spectated_replay[current_frame_idx + 1];

        while(next_frame.cur_music_pos <= m_iCurMusicPosWithOffsets) {
            if(current_frame_idx + 2 >= spectated_replay.size()) break;

            last_keys = current_keys;

            current_frame_idx++;
            current_frame = spectated_replay[current_frame_idx];
            next_frame = spectated_replay[current_frame_idx + 1];
            current_keys = current_frame.key_flags;

            // Flag fix to simplify logic (stable sets both K1 and M1 when K1 is pressed)
            if(current_keys & OsuReplay::K1) current_keys &= ~OsuReplay::M1;
            if(current_keys & OsuReplay::K2) current_keys &= ~OsuReplay::M2;

            // Released key 1
            if(last_keys & OsuReplay::K1 && !(current_keys & OsuReplay::K1)) {
                m_osu->getHUD()->animateInputoverlay(1, false);
            }
            if(last_keys & OsuReplay::M1 && !(current_keys & OsuReplay::M1)) {
                m_osu->getHUD()->animateInputoverlay(3, false);
            }

            // Released key 2
            if(last_keys & OsuReplay::K2 && !(current_keys & OsuReplay::K2)) {
                m_osu->getHUD()->animateInputoverlay(2, false);
            }
            if(last_keys & OsuReplay::M2 && !(current_keys & OsuReplay::M2)) {
                m_osu->getHUD()->animateInputoverlay(4, false);
            }

            // Pressed key 1
            if(!(last_keys & OsuReplay::K1) && current_keys & OsuReplay::K1) {
                m_osu->getHUD()->animateInputoverlay(1, true);
                m_clicks.push_back(current_frame.cur_music_pos);
                if(!m_bInBreak && !m_bIsInSkippableSection) m_osu->getScore()->addKeyCount(1);
            }
            if(!(last_keys & OsuReplay::M1) && current_keys & OsuReplay::M1) {
                m_osu->getHUD()->animateInputoverlay(3, true);
                m_clicks.push_back(current_frame.cur_music_pos);
                if(!m_bInBreak && !m_bIsInSkippableSection) m_osu->getScore()->addKeyCount(3);
            }

            // Pressed key 2
            if(!(last_keys & OsuReplay::K2) && current_keys & OsuReplay::K2) {
                m_osu->getHUD()->animateInputoverlay(2, true);
                m_clicks.push_back(current_frame.cur_music_pos);
                if(!m_bInBreak && !m_bIsInSkippableSection) m_osu->getScore()->addKeyCount(2);
            }
            if(!(last_keys & OsuReplay::M2) && current_keys & OsuReplay::M2) {
                m_osu->getHUD()->animateInputoverlay(4, true);
                m_clicks.push_back(current_frame.cur_music_pos);
                if(!m_bInBreak && !m_bIsInSkippableSection) m_osu->getScore()->addKeyCount(4);
            }
        }

        float percent = 0.f;
        if(next_frame.milliseconds_since_last_frame > 0) {
            long ms_since_last_frame = m_iCurMusicPosWithOffsets - current_frame.cur_music_pos;
            percent = (float)ms_since_last_frame / (float)next_frame.milliseconds_since_last_frame;
        }
        m_interpolatedMousePos =
            Vector2{lerp(current_frame.x, next_frame.x, percent), lerp(current_frame.y, next_frame.y, percent)};
        m_interpolatedMousePos *= OsuGameRules::getPlayfieldScaleFactor(m_osu);
        m_interpolatedMousePos += OsuGameRules::getPlayfieldOffset(m_osu);
    }

    // for performance reasons, a lot of operations are crammed into 1 loop over all hitobjects:
    // update all hitobjects,
    // handle click events,
    // also get the time of the next/previous hitobject and their indices for later,
    // and get the current hitobject,
    // also handle miss hiterrorbar slots,
    // also calculate nps and nd,
    // also handle note blocking
    m_currentHitObject = NULL;
    m_iNextHitObjectTime = 0;
    m_iPreviousHitObjectTime = 0;
    m_iPreviousFollowPointObjectIndex = 0;
    m_iNPS = 0;
    m_iND = 0;
    m_iCurrentNumCircles = 0;
    m_iCurrentNumSliders = 0;
    m_iCurrentNumSpinners = 0;
    {
        bool blockNextNotes = false;

        const long pvs =
            !OsuGameRules::osu_mod_mafham.getBool()
                ? getPVS()
                : (m_hitobjects.size() > 0
                       ? (m_hitobjects[clamp<int>(m_iCurrentHitObjectIndex +
                                                      OsuGameRules::osu_mod_mafham_render_livesize.getInt() + 1,
                                                  0, m_hitobjects.size() - 1)]
                              ->getTime() -
                          m_iCurMusicPosWithOffsets + 1500)
                       : getPVS());
        const bool usePVS = m_osu_pvs->getBool();

        const int notelockType = osu_notelock_type.getInt();
        const long tolerance2B = (long)osu_notelock_stable_tolerance2b.getInt();

        m_iCurrentHitObjectIndex = 0;  // reset below here, since it's needed for mafham pvs

        for(int i = 0; i < m_hitobjects.size(); i++) {
            // the order must be like this:
            // 0) miscellaneous stuff (minimal performance impact)
            // 1) prev + next time vars
            // 2) PVS optimization
            // 3) main hitobject update
            // 4) note blocking
            // 5) click events
            //
            // (because the hitobjects need to know about note blocking before handling the click events)

            // ************ live pp block start ************ //
            const bool isCircle = m_hitobjects[i]->isCircle();
            const bool isSlider = m_hitobjects[i]->isSlider();
            const bool isSpinner = m_hitobjects[i]->isSpinner();
            // ************ live pp block end ************** //

            // determine previous & next object time, used for auto + followpoints + warning arrows + empty section
            // skipping
            if(m_iNextHitObjectTime == 0) {
                if(m_hitobjects[i]->getTime() > m_iCurMusicPosWithOffsets)
                    m_iNextHitObjectTime = m_hitobjects[i]->getTime();
                else {
                    m_currentHitObject = m_hitobjects[i];
                    const long actualPrevHitObjectTime = m_hitobjects[i]->getTime() + m_hitobjects[i]->getDuration();
                    m_iPreviousHitObjectTime = actualPrevHitObjectTime;

                    if(m_iCurMusicPosWithOffsets >
                       actualPrevHitObjectTime + (long)osu_followpoints_prevfadetime.getFloat())
                        m_iPreviousFollowPointObjectIndex = i;
                }
            }

            // PVS optimization
            if(usePVS) {
                if(m_hitobjects[i]->isFinished() &&
                   (m_iCurMusicPosWithOffsets - pvs >
                    m_hitobjects[i]->getTime() + m_hitobjects[i]->getDuration()))  // past objects
                {
                    // ************ live pp block start ************ //
                    if(isCircle) m_iCurrentNumCircles++;
                    if(isSlider) m_iCurrentNumSliders++;
                    if(isSpinner) m_iCurrentNumSpinners++;

                    m_iCurrentHitObjectIndex = i;
                    // ************ live pp block end ************** //

                    continue;
                }
                if(m_hitobjects[i]->getTime() > m_iCurMusicPosWithOffsets + pvs)  // future objects
                    break;
            }

            // ************ live pp block start ************ //
            if(m_iCurMusicPosWithOffsets >= m_hitobjects[i]->getTime() + m_hitobjects[i]->getDuration())
                m_iCurrentHitObjectIndex = i;
            // ************ live pp block end ************** //

            // main hitobject update
            m_hitobjects[i]->update(m_iCurMusicPosWithOffsets);

            // note blocking / notelock (1)
            const OsuSlider *currentSliderPointer = dynamic_cast<OsuSlider *>(m_hitobjects[i]);
            if(notelockType > 0) {
                m_hitobjects[i]->setBlocked(blockNextNotes);

                if(notelockType == 1)  // McOsu
                {
                    // (nothing, handled in (2) block)
                } else if(notelockType == 2)  // osu!stable
                {
                    if(!m_hitobjects[i]->isFinished()) {
                        blockNextNotes = true;

                        // Sliders are "finished" after they end
                        // Extra handling for simultaneous/2b hitobjects, as these would otherwise get blocked
                        // NOTE: this will still unlock some simultaneous/2b patterns too early
                        //       (slider slider circle [circle]), but nobody from that niche has complained so far
                        {
                            const bool isSlider = (currentSliderPointer != NULL);
                            const bool isSpinner = (!isSlider && !isCircle);

                            if(isSlider || isSpinner) {
                                if((i + 1) < m_hitobjects.size()) {
                                    if((isSpinner || currentSliderPointer->isStartCircleFinished()) &&
                                       (m_hitobjects[i + 1]->getTime() <=
                                        (m_hitobjects[i]->getTime() + m_hitobjects[i]->getDuration() + tolerance2B)))
                                        blockNextNotes = false;
                                }
                            }
                        }
                    }
                } else if(notelockType == 3)  // osu!lazer 2020
                {
                    if(!m_hitobjects[i]->isFinished()) {
                        const bool isSlider = (currentSliderPointer != NULL);
                        const bool isSpinner = (!isSlider && !isCircle);

                        if(!isSpinner)  // spinners are completely ignored (transparent)
                        {
                            blockNextNotes = (m_iCurMusicPosWithOffsets <= m_hitobjects[i]->getTime());

                            // sliders are "finished" after their startcircle
                            {
                                // sliders with finished startcircles do not block
                                if(currentSliderPointer != NULL && currentSliderPointer->isStartCircleFinished())
                                    blockNextNotes = false;
                            }
                        }
                    }
                }
            } else
                m_hitobjects[i]->setBlocked(false);

            // click events (this also handles hitsounds!)
            const bool isCurrentHitObjectASliderAndHasItsStartCircleFinishedBeforeClickEvents =
                (currentSliderPointer != NULL && currentSliderPointer->isStartCircleFinished());
            const bool isCurrentHitObjectFinishedBeforeClickEvents = m_hitobjects[i]->isFinished();
            {
                if(m_clicks.size() > 0) m_hitobjects[i]->onClickEvent(m_clicks);
            }
            const bool isCurrentHitObjectFinishedAfterClickEvents = m_hitobjects[i]->isFinished();
            const bool isCurrentHitObjectASliderAndHasItsStartCircleFinishedAfterClickEvents =
                (currentSliderPointer != NULL && currentSliderPointer->isStartCircleFinished());

            // note blocking / notelock (2.1)
            if(!isCurrentHitObjectASliderAndHasItsStartCircleFinishedBeforeClickEvents &&
               isCurrentHitObjectASliderAndHasItsStartCircleFinishedAfterClickEvents) {
                // in here if a slider had its startcircle clicked successfully in this update iteration

                if(notelockType == 2)  // osu!stable
                {
                    // edge case: frame perfect double tapping on overlapping sliders would incorrectly eat the second
                    // input, because the isStartCircleFinished() 2b edge case check handling happens before
                    // m_hitobjects[i]->onClickEvent(m_clicks); so, we check if the currentSliderPointer got its
                    // isStartCircleFinished() within this m_hitobjects[i]->onClickEvent(m_clicks); and unlock
                    // blockNextNotes if that is the case note that we still only unlock within duration + tolerance2B
                    // (same as in (1))
                    if((i + 1) < m_hitobjects.size()) {
                        if((m_hitobjects[i + 1]->getTime() <=
                            (m_hitobjects[i]->getTime() + m_hitobjects[i]->getDuration() + tolerance2B)))
                            blockNextNotes = false;
                    }
                }
            }

            // note blocking / notelock (2.2)
            if(!isCurrentHitObjectFinishedBeforeClickEvents && isCurrentHitObjectFinishedAfterClickEvents) {
                // in here if a hitobject has been clicked (and finished completely) successfully in this update
                // iteration

                blockNextNotes = false;

                if(notelockType == 1)  // McOsu
                {
                    // auto miss all previous unfinished hitobjects, always
                    // (can stop reverse iteration once we get to the first finished hitobject)

                    for(int m = i - 1; m >= 0; m--) {
                        if(!m_hitobjects[m]->isFinished()) {
                            const OsuSlider *sliderPointer = dynamic_cast<OsuSlider *>(m_hitobjects[m]);

                            const bool isSlider = (sliderPointer != NULL);
                            const bool isSpinner = (!isSlider && !isCircle);

                            if(!isSpinner)  // spinners are completely ignored (transparent)
                            {
                                if(m_hitobjects[i]->getTime() >
                                   (m_hitobjects[m]->getTime() +
                                    m_hitobjects[m]->getDuration()))  // NOTE: 2b exception. only force miss if objects
                                                                      // are not overlapping.
                                    m_hitobjects[m]->miss(m_iCurMusicPosWithOffsets);
                            }
                        } else
                            break;
                    }
                } else if(notelockType == 2)  // osu!stable
                {
                    // (nothing, handled in (1) and (2.1) blocks)
                } else if(notelockType == 3)  // osu!lazer 2020
                {
                    // auto miss all previous unfinished hitobjects if the current music time is > their time (center)
                    // (can stop reverse iteration once we get to the first finished hitobject)

                    for(int m = i - 1; m >= 0; m--) {
                        if(!m_hitobjects[m]->isFinished()) {
                            const OsuSlider *sliderPointer = dynamic_cast<OsuSlider *>(m_hitobjects[m]);

                            const bool isSlider = (sliderPointer != NULL);
                            const bool isSpinner = (!isSlider && !isCircle);

                            if(!isSpinner)  // spinners are completely ignored (transparent)
                            {
                                if(m_iCurMusicPosWithOffsets > m_hitobjects[m]->getTime()) {
                                    if(m_hitobjects[i]->getTime() >
                                       (m_hitobjects[m]->getTime() +
                                        m_hitobjects[m]->getDuration()))  // NOTE: 2b exception. only force miss if
                                                                          // objects are not overlapping.
                                        m_hitobjects[m]->miss(m_iCurMusicPosWithOffsets);
                                }
                            }
                        } else
                            break;
                    }
                }
            }

            // ************ live pp block start ************ //
            if(isCircle && m_hitobjects[i]->isFinished()) m_iCurrentNumCircles++;
            if(isSlider && m_hitobjects[i]->isFinished()) m_iCurrentNumSliders++;
            if(isSpinner && m_hitobjects[i]->isFinished()) m_iCurrentNumSpinners++;

            if(m_hitobjects[i]->isFinished()) m_iCurrentHitObjectIndex = i;
            // ************ live pp block end ************** //

            // notes per second
            const long npsHalfGateSizeMS = (long)(500.0f * getSpeedMultiplier());
            if(m_hitobjects[i]->getTime() > m_iCurMusicPosWithOffsets - npsHalfGateSizeMS &&
               m_hitobjects[i]->getTime() < m_iCurMusicPosWithOffsets + npsHalfGateSizeMS)
                m_iNPS++;

            // note density
            if(m_hitobjects[i]->isVisible()) m_iND++;
        }

        // miss hiterrorbar slots
        // this gets the closest previous unfinished hitobject, as well as all following hitobjects which are in 50
        // range and could be clicked
        if(osu_hiterrorbar_misaims.getBool()) {
            m_misaimObjects.clear();
            OsuHitObject *lastUnfinishedHitObject = NULL;
            const long hitWindow50 = (long)OsuGameRules::getHitWindow50(this);
            for(int i = 0; i < m_hitobjects.size(); i++)  // this shouldn't hurt performance too much, since no
                                                          // expensive operations are happening within the loop
            {
                if(!m_hitobjects[i]->isFinished()) {
                    if(m_iCurMusicPosWithOffsets >= m_hitobjects[i]->getTime())
                        lastUnfinishedHitObject = m_hitobjects[i];
                    else if(std::abs(m_hitobjects[i]->getTime() - m_iCurMusicPosWithOffsets) < hitWindow50)
                        m_misaimObjects.push_back(m_hitobjects[i]);
                    else
                        break;
                }
            }
            if(lastUnfinishedHitObject != NULL &&
               std::abs(lastUnfinishedHitObject->getTime() - m_iCurMusicPosWithOffsets) < hitWindow50)
                m_misaimObjects.insert(m_misaimObjects.begin(), lastUnfinishedHitObject);

            // now, go through the remaining clicks, and go through the unfinished hitobjects.
            // handle misaim clicks sequentially (setting the misaim flag on the hitobjects to only allow 1 entry in the
            // hiterrorbar for misses per object) clicks don't have to be consumed here, as they are deleted below
            // anyway
            for(int c = 0; c < m_clicks.size(); c++) {
                for(int i = 0; i < m_misaimObjects.size(); i++) {
                    if(m_misaimObjects[i]->hasMisAimed())  // only 1 slot per object!
                        continue;

                    m_misaimObjects[i]->misAimed();
                    const long delta = m_clicks[c] - (long)m_misaimObjects[i]->getTime();
                    m_osu->getHUD()->addHitError(delta, false, true);

                    break;  // the current click has been dealt with (and the hitobject has been misaimed)
                }
            }
        }

        // all remaining clicks which have not been consumed by any hitobjects can safely be deleted
        if(m_clicks.size() > 0) {
            if(osu_play_hitsound_on_click_while_playing.getBool()) m_osu->getSkin()->playHitCircleSound(0);

            // nightmare mod: extra clicks = sliderbreak
            if((m_osu->getModNightmare() || osu_mod_jigsaw1.getBool()) && !m_bIsInSkippableSection && !m_bInBreak &&
               m_iCurrentHitObjectIndex > 0) {
                addSliderBreak();
                addHitResult(NULL, OsuScore::HIT::HIT_MISS_SLIDERBREAK, 0, false, true, true, true, true,
                             false);  // only decrease health
            }

            m_clicks.clear();
        }
    }

    // empty section detection & skipping
    if(m_hitobjects.size() > 0) {
        const long legacyOffset = (m_iPreviousHitObjectTime < m_hitobjects[0]->getTime() ? 0 : 1000);  // Mc
        const long nextHitObjectDelta = m_iNextHitObjectTime - (long)m_iCurMusicPosWithOffsets;
        if(nextHitObjectDelta > 0 && nextHitObjectDelta > (long)osu_skip_time.getInt() &&
           m_iCurMusicPosWithOffsets > (m_iPreviousHitObjectTime + legacyOffset))
            m_bIsInSkippableSection = true;
        else if(!osu_end_skip.getBool() && nextHitObjectDelta < 0)
            m_bIsInSkippableSection = true;
        else
            m_bIsInSkippableSection = false;

        m_osu->m_chat->updateVisibility();

        // While we want to allow the chat to pop up during breaks, we don't
        // want to be able to skip after the start in multiplayer rooms
        if(bancho.is_playing_a_multi_map() && m_iCurrentHitObjectIndex > 0) {
            m_bIsInSkippableSection = false;
        }
    }

    // warning arrow logic
    if(m_hitobjects.size() > 0) {
        const long legacyOffset = (m_iPreviousHitObjectTime < m_hitobjects[0]->getTime() ? 0 : 1000);  // Mc
        const long minGapSize = 1000;
        const long lastVisibleMin = 400;
        const long blinkDelta = 100;

        const long gapSize = m_iNextHitObjectTime - (m_iPreviousHitObjectTime + legacyOffset);
        const long nextDelta = (m_iNextHitObjectTime - m_iCurMusicPosWithOffsets);
        const bool drawWarningArrows = gapSize > minGapSize && nextDelta > 0;
        if(drawWarningArrows &&
           ((nextDelta <= lastVisibleMin + blinkDelta * 13 && nextDelta > lastVisibleMin + blinkDelta * 12) ||
            (nextDelta <= lastVisibleMin + blinkDelta * 11 && nextDelta > lastVisibleMin + blinkDelta * 10) ||
            (nextDelta <= lastVisibleMin + blinkDelta * 9 && nextDelta > lastVisibleMin + blinkDelta * 8) ||
            (nextDelta <= lastVisibleMin + blinkDelta * 7 && nextDelta > lastVisibleMin + blinkDelta * 6) ||
            (nextDelta <= lastVisibleMin + blinkDelta * 5 && nextDelta > lastVisibleMin + blinkDelta * 4) ||
            (nextDelta <= lastVisibleMin + blinkDelta * 3 && nextDelta > lastVisibleMin + blinkDelta * 2) ||
            (nextDelta <= lastVisibleMin + blinkDelta * 1 && nextDelta > lastVisibleMin)))
            m_bShouldFlashWarningArrows = true;
        else
            m_bShouldFlashWarningArrows = false;
    }

    // break time detection, and background fade during breaks
    const OsuDatabaseBeatmap::BREAK breakEvent =
        getBreakForTimeRange(m_iPreviousHitObjectTime, m_iCurMusicPosWithOffsets, m_iNextHitObjectTime);
    const bool isInBreak = ((int)m_iCurMusicPosWithOffsets >= breakEvent.startTime &&
                            (int)m_iCurMusicPosWithOffsets <= breakEvent.endTime);
    if(isInBreak != m_bInBreak) {
        m_bInBreak = !m_bInBreak;

        if(!osu_background_dont_fade_during_breaks.getBool() || m_fBreakBackgroundFade != 0.0f) {
            if(m_bInBreak && !osu_background_dont_fade_during_breaks.getBool()) {
                const int breakDuration = breakEvent.endTime - breakEvent.startTime;
                if(breakDuration > (int)(osu_background_fade_min_duration.getFloat() * 1000.0f))
                    anim->moveLinear(&m_fBreakBackgroundFade, 1.0f, osu_background_fade_in_duration.getFloat(), true);
            } else
                anim->moveLinear(&m_fBreakBackgroundFade, 0.0f, osu_background_fade_out_duration.getFloat(), true);
        }
    }

    // section pass/fail logic
    if(m_hitobjects.size() > 0) {
        const long minGapSize = 2880;
        const long fadeStart = 1280;
        const long fadeEnd = 1480;

        const long gapSize = m_iNextHitObjectTime - m_iPreviousHitObjectTime;
        const long start =
            (gapSize / 2 > minGapSize ? m_iPreviousHitObjectTime + (gapSize / 2) : m_iNextHitObjectTime - minGapSize);
        const long nextDelta = m_iCurMusicPosWithOffsets - start;
        const bool inSectionPassFail =
            (gapSize > minGapSize && nextDelta > 0) && m_iCurMusicPosWithOffsets > m_hitobjects[0]->getTime() &&
            m_iCurMusicPosWithOffsets <
                (m_hitobjectsSortedByEndTime[m_hitobjectsSortedByEndTime.size() - 1]->getTime() +
                 m_hitobjectsSortedByEndTime[m_hitobjectsSortedByEndTime.size() - 1]->getDuration()) &&
            !m_bFailed && m_bInBreak && (breakEvent.endTime - breakEvent.startTime) > minGapSize;

        const bool passing = (m_fHealth >= 0.5);

        // draw logic
        if(passing) {
            if(inSectionPassFail && ((nextDelta <= fadeEnd && nextDelta >= 280) ||
                                     (nextDelta <= 230 && nextDelta >= 160) || (nextDelta <= 100 && nextDelta >= 20))) {
                const float fadeAlpha = 1.0f - (float)(nextDelta - fadeStart) / (float)(fadeEnd - fadeStart);
                m_fShouldFlashSectionPass = (nextDelta > fadeStart ? fadeAlpha : 1.0f);
            } else
                m_fShouldFlashSectionPass = 0.0f;
        } else {
            if(inSectionPassFail &&
               ((nextDelta <= fadeEnd && nextDelta >= 280) || (nextDelta <= 230 && nextDelta >= 130))) {
                const float fadeAlpha = 1.0f - (float)(nextDelta - fadeStart) / (float)(fadeEnd - fadeStart);
                m_fShouldFlashSectionFail = (nextDelta > fadeStart ? fadeAlpha : 1.0f);
            } else
                m_fShouldFlashSectionFail = 0.0f;
        }

        // sound logic
        if(inSectionPassFail) {
            if(m_iPreviousSectionPassFailTime != start &&
               ((passing && nextDelta >= 20) || (!passing && nextDelta >= 130))) {
                m_iPreviousSectionPassFailTime = start;

                if(!wasSeekFrame) {
                    if(passing)
                        engine->getSound()->play(m_osu->getSkin()->getSectionPassSound());
                    else
                        engine->getSound()->play(m_osu->getSkin()->getSectionFailSound());
                }
            }
        }
    }

    // hp drain & failing
    if(osu_drain_type.getInt() > 1) {
        const int drainType = osu_drain_type.getInt();

        // handle constant drain
        if(drainType == 2 || drainType == 3)  // osu!stable + osu!lazer 2020
        {
            if(m_fDrainRate > 0.0) {
                if(m_bIsPlaying                  // not paused
                   && !m_bInBreak                // not in a break
                   && !m_bIsInSkippableSection)  // not in a skippable section
                {
                    // special case: break drain edge cases
                    bool drainAfterLastHitobjectBeforeBreakStart = false;
                    bool drainBeforeFirstHitobjectAfterBreakEnd = false;

                    if(drainType == 2)  // osu!stable
                    {
                        drainAfterLastHitobjectBeforeBreakStart =
                            (m_selectedDifficulty2->getVersion() < 8 ? osu_drain_stable_break_before_old.getBool()
                                                                     : osu_drain_stable_break_before.getBool());
                        drainBeforeFirstHitobjectAfterBreakEnd = osu_drain_stable_break_after.getBool();
                    } else if(drainType == 3)  // osu!lazer 2020
                    {
                        drainAfterLastHitobjectBeforeBreakStart = osu_drain_lazer_break_before.getBool();
                        drainBeforeFirstHitobjectAfterBreakEnd = osu_drain_lazer_break_after.getBool();
                    }

                    const bool isBetweenHitobjectsAndBreak = (int)m_iPreviousHitObjectTime <= breakEvent.startTime &&
                                                             (int)m_iNextHitObjectTime >= breakEvent.endTime &&
                                                             m_iCurMusicPosWithOffsets > m_iPreviousHitObjectTime;
                    const bool isLastHitobjectBeforeBreakStart =
                        isBetweenHitobjectsAndBreak && (int)m_iCurMusicPosWithOffsets <= breakEvent.startTime;
                    const bool isFirstHitobjectAfterBreakEnd =
                        isBetweenHitobjectsAndBreak && (int)m_iCurMusicPosWithOffsets >= breakEvent.endTime;

                    if(!isBetweenHitobjectsAndBreak ||
                       (drainAfterLastHitobjectBeforeBreakStart && isLastHitobjectBeforeBreakStart) ||
                       (drainBeforeFirstHitobjectAfterBreakEnd && isFirstHitobjectAfterBreakEnd)) {
                        // special case: spinner nerf
                        double spinnerDrainNerf = 1.0;

                        if(drainType == 2)  // osu!stable
                        {
                            if(isSpinnerActive()) spinnerDrainNerf = (double)osu_drain_stable_spinner_nerf.getFloat();
                        }

                        addHealth(
                            -m_fDrainRate * engine->getFrameTime() * (double)getSpeedMultiplier() * spinnerDrainNerf,
                            false);
                    }
                }
            }
        }

        // handle generic fail state (1) (see addHealth())
        {
            bool hasFailed = false;

            switch(drainType) {
                case 2:  // osu!stable
                    hasFailed = (m_fHealth < 0.001) && osu_drain_stable_passive_fail.getBool();
                    break;

                case 3:  // osu!lazer 2020
                    hasFailed = (m_fHealth < 0.001) && osu_drain_lazer_passive_fail.getBool();
                    break;

                default:
                    hasFailed = (m_fHealth < 0.001);
                    break;
            }

            if(hasFailed && !m_osu->getModNF()) fail();
        }

        // revive in mp
        if(m_fHealth > 0.999 && m_osu->getScore()->isDead()) m_osu->getScore()->setDead(false);

        // handle fail animation
        if(m_bFailed) {
            if(m_fFailAnim <= 0.0f) {
                if(m_music->isPlaying() || !m_osu->getPauseMenu()->isVisible()) {
                    engine->getSound()->pause(m_music);
                    m_bIsPaused = true;

                    m_osu->getPauseMenu()->setVisible(true);
                    m_osu->updateConfineCursor();
                }
            } else
                m_music->setFrequency(
                    m_fMusicFrequencyBackup * m_fFailAnim > 100 ? m_fMusicFrequencyBackup * m_fFailAnim : 100);
        }
    }
}

void OsuBeatmap::write_frame() {
    if(!m_bIsPlaying || m_bFailed || m_bIsWatchingReplay) return;

    long delta = m_iCurMusicPosWithOffsets - last_event_ms;
    if(delta < 0) return;
    if(delta == 0 && last_keys == current_keys) return;

    Vector2 pos = pixels2OsuCoords(getCursorPos());
    if(osu_playfield_mirror_horizontal.getBool()) pos.y = OsuGameRules::OSU_COORD_HEIGHT - pos.y;
    if(osu_playfield_mirror_vertical.getBool()) pos.x = OsuGameRules::OSU_COORD_WIDTH - pos.x;

    live_replay.push_back(OsuReplay::Frame{
        .cur_music_pos = m_iCurMusicPosWithOffsets,
        .milliseconds_since_last_frame = delta,
        .x = pos.x,
        .y = pos.y,
        .key_flags = current_keys,
    });
    last_event_time = m_fLastRealTimeForInterpolationDelta;
    last_event_ms = m_iCurMusicPosWithOffsets;
    last_keys = current_keys;
}

void OsuBeatmap::onModUpdate(bool rebuildSliderVertexBuffers, bool recomputeDrainRate) {
    if(Osu::debug->getBool()) debugLog("OsuBeatmap::onModUpdate() @ %f\n", engine->getTime());

    updatePlayfieldMetrics();
    updateHitobjectMetrics();

    if(recomputeDrainRate) computeDrainRate();

    if(m_music != NULL) {
        m_music->setSpeed(m_osu->getSpeedMultiplier());
    }

    // recalculate slider vertexbuffers
    if(m_osu->getModHR() != m_bWasHREnabled ||
       osu_playfield_mirror_horizontal.getBool() != m_bWasHorizontalMirrorEnabled ||
       osu_playfield_mirror_vertical.getBool() != m_bWasVerticalMirrorEnabled) {
        m_bWasHREnabled = m_osu->getModHR();
        m_bWasHorizontalMirrorEnabled = osu_playfield_mirror_horizontal.getBool();
        m_bWasVerticalMirrorEnabled = osu_playfield_mirror_vertical.getBool();

        calculateStacks();

        if(rebuildSliderVertexBuffers) updateSliderVertexBuffers();
    }
    if(m_osu->getModEZ() != m_bWasEZEnabled) {
        calculateStacks();

        m_bWasEZEnabled = m_osu->getModEZ();
        if(rebuildSliderVertexBuffers) updateSliderVertexBuffers();
    }
    if(getHitcircleDiameter() != m_fPrevHitCircleDiameter && m_hitobjects.size() > 0) {
        calculateStacks();

        m_fPrevHitCircleDiameter = getHitcircleDiameter();
        if(rebuildSliderVertexBuffers) updateSliderVertexBuffers();
    }
    if(osu_playfield_rotation.getFloat() != m_fPrevPlayfieldRotationFromConVar) {
        m_fPrevPlayfieldRotationFromConVar = osu_playfield_rotation.getFloat();
        if(rebuildSliderVertexBuffers) updateSliderVertexBuffers();
    }
    if(osu_playfield_stretch_x.getFloat() != m_fPrevPlayfieldStretchX) {
        calculateStacks();

        m_fPrevPlayfieldStretchX = osu_playfield_stretch_x.getFloat();
        if(rebuildSliderVertexBuffers) updateSliderVertexBuffers();
    }
    if(osu_playfield_stretch_y.getFloat() != m_fPrevPlayfieldStretchY) {
        calculateStacks();

        m_fPrevPlayfieldStretchY = osu_playfield_stretch_y.getFloat();
        if(rebuildSliderVertexBuffers) updateSliderVertexBuffers();
    }
    if(OsuGameRules::osu_mod_mafham.getBool() != m_bWasMafhamEnabled) {
        m_bWasMafhamEnabled = OsuGameRules::osu_mod_mafham.getBool();
        for(int i = 0; i < m_hitobjects.size(); i++) {
            m_hitobjects[i]->update(m_iCurMusicPosWithOffsets);
        }
    }

    // recalculate star cache for live pp
    if(m_osu_draw_statistics_pp_ref->getBool() ||
       m_osu_draw_statistics_livestars_ref->getBool())  // sanity + performance/usability
    {
        bool didCSChange = false;
        if(getHitcircleDiameter() != m_fPrevHitCircleDiameterForStarCache && m_hitobjects.size() > 0) {
            m_fPrevHitCircleDiameterForStarCache = getHitcircleDiameter();
            didCSChange = true;
        }

        bool didSpeedChange = false;
        if(m_osu->getSpeedMultiplier() != m_fPrevSpeedForStarCache && m_hitobjects.size() > 0) {
            m_fPrevSpeedForStarCache =
                m_osu->getSpeedMultiplier();  // this is not using the beatmap function for speed on purpose, because
                                              // that wouldn't work while the music is still loading
            didSpeedChange = true;
        }

        if(didCSChange || didSpeedChange) {
            if(m_selectedDifficulty2 != NULL) updateStarCache();
        }
    }
}

bool OsuBeatmap::isLoading() {
    return (isActuallyLoading() || (bancho.is_playing_a_multi_map() && !bancho.room.all_players_loaded));
}

bool OsuBeatmap::isActuallyLoading() { return (!m_music->isAsyncReady() || m_bIsPreLoading || isLoadingStarCache()); }

Vector2 OsuBeatmap::pixels2OsuCoords(Vector2 pixelCoords) const {
    // un-first-person
    if(OsuGameRules::osu_mod_fps.getBool()) {
        // HACKHACK: this is the worst hack possible (engine->isDrawing()), but it works
        // the problem is that this same function is called while draw()ing and update()ing
        if(!((engine->isDrawing() && (m_osu->getModAuto() || m_osu->getModAutopilot())) ||
             !(m_osu->getModAuto() || m_osu->getModAutopilot())))
            pixelCoords += getFirstPersonCursorDelta();
    }

    // un-offset and un-scale, reverse order
    pixelCoords -= m_vPlayfieldOffset;
    pixelCoords /= m_fScaleFactor;

    return pixelCoords;
}

Vector2 OsuBeatmap::osuCoords2Pixels(Vector2 coords) const {
    if(m_osu->getModHR()) coords.y = OsuGameRules::OSU_COORD_HEIGHT - coords.y;
    if(osu_playfield_mirror_horizontal.getBool()) coords.y = OsuGameRules::OSU_COORD_HEIGHT - coords.y;
    if(osu_playfield_mirror_vertical.getBool()) coords.x = OsuGameRules::OSU_COORD_WIDTH - coords.x;

    // wobble
    if(osu_mod_wobble.getBool()) {
        const float speedMultiplierCompensation = 1.0f / getSpeedMultiplier();
        coords.x += std::sin((m_iCurMusicPos / 1000.0f) * 5 * speedMultiplierCompensation *
                             osu_mod_wobble_frequency.getFloat()) *
                    osu_mod_wobble_strength.getFloat();
        coords.y += std::sin((m_iCurMusicPos / 1000.0f) * 4 * speedMultiplierCompensation *
                             osu_mod_wobble_frequency.getFloat()) *
                    osu_mod_wobble_strength.getFloat();
    }

    // wobble2
    if(osu_mod_wobble2.getBool()) {
        const float speedMultiplierCompensation = 1.0f / getSpeedMultiplier();
        Vector2 centerDelta = coords - Vector2(OsuGameRules::OSU_COORD_WIDTH, OsuGameRules::OSU_COORD_HEIGHT) / 2;
        coords.x += centerDelta.x * 0.25f *
                    std::sin((m_iCurMusicPos / 1000.0f) * 5 * speedMultiplierCompensation *
                             osu_mod_wobble_frequency.getFloat()) *
                    osu_mod_wobble_strength.getFloat();
        coords.y += centerDelta.y * 0.25f *
                    std::sin((m_iCurMusicPos / 1000.0f) * 3 * speedMultiplierCompensation *
                             osu_mod_wobble_frequency.getFloat()) *
                    osu_mod_wobble_strength.getFloat();
    }

    // rotation
    if(m_fPlayfieldRotation + osu_playfield_rotation.getFloat() != 0.0f) {
        coords.x -= OsuGameRules::OSU_COORD_WIDTH / 2;
        coords.y -= OsuGameRules::OSU_COORD_HEIGHT / 2;

        Vector3 coords3 = Vector3(coords.x, coords.y, 0);
        Matrix4 rot;
        rot.rotateZ(m_fPlayfieldRotation + osu_playfield_rotation.getFloat());  // (m_iCurMusicPos/1000.0f)*30

        coords3 = coords3 * rot;
        coords3.x += OsuGameRules::OSU_COORD_WIDTH / 2;
        coords3.y += OsuGameRules::OSU_COORD_HEIGHT / 2;

        coords.x = coords3.x;
        coords.y = coords3.y;
    }

    if(osu_mandala.getBool()) {
        coords.x -= OsuGameRules::OSU_COORD_WIDTH / 2;
        coords.y -= OsuGameRules::OSU_COORD_HEIGHT / 2;

        Vector3 coords3 = Vector3(coords.x, coords.y, 0);
        Matrix4 rot;
        rot.rotateZ((360.0f / osu_mandala_num.getInt()) * (m_iMandalaIndex + 1));  // (m_iCurMusicPos/1000.0f)*30

        coords3 = coords3 * rot;
        coords3.x += OsuGameRules::OSU_COORD_WIDTH / 2;
        coords3.y += OsuGameRules::OSU_COORD_HEIGHT / 2;

        coords.x = coords3.x;
        coords.y = coords3.y;
    }

    // if wobble, clamp coordinates
    if(osu_mod_wobble.getBool() || osu_mod_wobble2.getBool()) {
        coords.x = clamp<float>(coords.x, 0.0f, OsuGameRules::OSU_COORD_WIDTH);
        coords.y = clamp<float>(coords.y, 0.0f, OsuGameRules::OSU_COORD_HEIGHT);
    }

    if(m_bFailed) {
        float failTimePercentInv = 1.0f - m_fFailAnim;  // goes from 0 to 1 over the duration of osu_fail_time
        failTimePercentInv *= failTimePercentInv;

        coords.x -= OsuGameRules::OSU_COORD_WIDTH / 2;
        coords.y -= OsuGameRules::OSU_COORD_HEIGHT / 2;

        Vector3 coords3 = Vector3(coords.x, coords.y, 0);
        Matrix4 rot;
        rot.rotateZ(failTimePercentInv * 60.0f);

        coords3 = coords3 * rot;
        coords3.x += OsuGameRules::OSU_COORD_WIDTH / 2;
        coords3.y += OsuGameRules::OSU_COORD_HEIGHT / 2;

        coords.x = coords3.x + failTimePercentInv * OsuGameRules::OSU_COORD_WIDTH * 0.25f;
        coords.y = coords3.y + failTimePercentInv * OsuGameRules::OSU_COORD_HEIGHT * 1.25f;
    }

    // playfield stretching/transforming
    coords.x -= OsuGameRules::OSU_COORD_WIDTH / 2;  // center
    coords.y -= OsuGameRules::OSU_COORD_HEIGHT / 2;
    {
        if(osu_playfield_circular.getBool()) {
            // normalize to -1 +1
            coords.x /= (float)OsuGameRules::OSU_COORD_WIDTH / 2.0f;
            coords.y /= (float)OsuGameRules::OSU_COORD_HEIGHT / 2.0f;

            // clamp (for sqrt) and transform
            coords.x = clamp<float>(coords.x, -1.0f, 1.0f);
            coords.y = clamp<float>(coords.y, -1.0f, 1.0f);
            coords = mapNormalizedCoordsOntoUnitCircle(coords);

            // and scale back up
            coords.x *= (float)OsuGameRules::OSU_COORD_WIDTH / 2.0f;
            coords.y *= (float)OsuGameRules::OSU_COORD_HEIGHT / 2.0f;
        }

        // stretch
        coords.x *= 1.0f + osu_playfield_stretch_x.getFloat();
        coords.y *= 1.0f + osu_playfield_stretch_y.getFloat();
    }
    coords.x += OsuGameRules::OSU_COORD_WIDTH / 2;  // undo center
    coords.y += OsuGameRules::OSU_COORD_HEIGHT / 2;

    // scale and offset
    coords *= m_fScaleFactor;
    coords += m_vPlayfieldOffset;  // the offset is already scaled, just add it

    // first person mod, centered cursor
    if(OsuGameRules::osu_mod_fps.getBool()) {
        // this is the worst hack possible (engine->isDrawing()), but it works
        // the problem is that this same function is called while draw()ing and update()ing
        if((engine->isDrawing() && (m_osu->getModAuto() || m_osu->getModAutopilot())) ||
           !(m_osu->getModAuto() || m_osu->getModAutopilot()))
            coords += getFirstPersonCursorDelta();
    }

    return coords;
}

Vector2 OsuBeatmap::osuCoords2RawPixels(Vector2 coords) const {
    // scale and offset
    coords *= m_fScaleFactor;
    coords += m_vPlayfieldOffset;  // the offset is already scaled, just add it

    return coords;
}

Vector2 OsuBeatmap::osuCoords2LegacyPixels(Vector2 coords) const {
    if(m_osu->getModHR()) coords.y = OsuGameRules::OSU_COORD_HEIGHT - coords.y;
    if(osu_playfield_mirror_horizontal.getBool()) coords.y = OsuGameRules::OSU_COORD_HEIGHT - coords.y;
    if(osu_playfield_mirror_vertical.getBool()) coords.x = OsuGameRules::OSU_COORD_WIDTH - coords.x;

    // rotation
    if(m_fPlayfieldRotation + osu_playfield_rotation.getFloat() != 0.0f) {
        coords.x -= OsuGameRules::OSU_COORD_WIDTH / 2;
        coords.y -= OsuGameRules::OSU_COORD_HEIGHT / 2;

        Vector3 coords3 = Vector3(coords.x, coords.y, 0);
        Matrix4 rot;
        rot.rotateZ(m_fPlayfieldRotation + osu_playfield_rotation.getFloat());

        coords3 = coords3 * rot;
        coords3.x += OsuGameRules::OSU_COORD_WIDTH / 2;
        coords3.y += OsuGameRules::OSU_COORD_HEIGHT / 2;

        coords.x = coords3.x;
        coords.y = coords3.y;
    }

    // VR center
    coords.x -= OsuGameRules::OSU_COORD_WIDTH / 2;
    coords.y -= OsuGameRules::OSU_COORD_HEIGHT / 2;

    if(osu_playfield_circular.getBool()) {
        // normalize to -1 +1
        coords.x /= (float)OsuGameRules::OSU_COORD_WIDTH / 2.0f;
        coords.y /= (float)OsuGameRules::OSU_COORD_HEIGHT / 2.0f;

        // clamp (for sqrt) and transform
        coords.x = clamp<float>(coords.x, -1.0f, 1.0f);
        coords.y = clamp<float>(coords.y, -1.0f, 1.0f);
        coords = mapNormalizedCoordsOntoUnitCircle(coords);

        // and scale back up
        coords.x *= (float)OsuGameRules::OSU_COORD_WIDTH / 2.0f;
        coords.y *= (float)OsuGameRules::OSU_COORD_HEIGHT / 2.0f;
    }

    // VR scale
    coords.x *= 1.0f + osu_playfield_stretch_x.getFloat();
    coords.y *= 1.0f + osu_playfield_stretch_y.getFloat();

    return coords;
}

Vector2 OsuBeatmap::getMousePos() const {
    if(m_bIsWatchingReplay && !m_bIsPaused) {
        return m_interpolatedMousePos;
    } else {
        return engine->getMouse()->getPos();
    }
}

Vector2 OsuBeatmap::getCursorPos() const {
    if(OsuGameRules::osu_mod_fps.getBool() && !m_bIsPaused) {
        if(m_osu->getModAuto() || m_osu->getModAutopilot())
            return m_vAutoCursorPos;
        else
            return m_vPlayfieldCenter;
    } else if(m_osu->getModAuto() || m_osu->getModAutopilot())
        return m_vAutoCursorPos;
    else {
        Vector2 pos = getMousePos();
        if(osu_mod_shirone.getBool() && m_osu->getScore()->getCombo() > 0)  // <3
            return pos + Vector2(std::sin((m_iCurMusicPos / 20.0f) * 1.15f) *
                                     ((float)m_osu->getScore()->getCombo() / osu_mod_shirone_combo.getFloat()),
                                 std::cos((m_iCurMusicPos / 20.0f) * 1.3f) *
                                     ((float)m_osu->getScore()->getCombo() / osu_mod_shirone_combo.getFloat()));
        else
            return pos;
    }
}

Vector2 OsuBeatmap::getFirstPersonCursorDelta() const {
    return m_vPlayfieldCenter - (m_osu->getModAuto() || m_osu->getModAutopilot() ? m_vAutoCursorPos : getMousePos());
}

float OsuBeatmap::getHitcircleDiameter() const { return m_fHitcircleDiameter; }

void OsuBeatmap::onPlayStart() {
    debugLog("OsuBeatmap::onPlayStart()\n");

    // if there are calculations in there that need the hitobjects to be loaded, also applies speed/pitch
    onModUpdate(false, false);
}

void OsuBeatmap::onBeforeStop(bool quit) {
    debugLog("OsuBeatmap::onBeforeStop()\n");

    // kill any running star cache loader
    stopStarCacheLoader();

    // calculate stars
    double aim = 0.0;
    double aimSliderFactor = 0.0;
    double speed = 0.0;
    double speedNotes = 0.0;
    const std::string &osuFilePath = m_selectedDifficulty2->getFilePath();
    const float AR = getAR();
    const float CS = getCS();
    const float OD = getOD();
    const float speedMultiplier = m_osu->getSpeedMultiplier();  // NOTE: not this->getSpeedMultiplier()!
    const bool relax = m_osu->getModRelax();
    const bool touchDevice = m_osu->getModTD();

    OsuDatabaseBeatmap::LOAD_DIFFOBJ_RESULT diffres =
        OsuDatabaseBeatmap::loadDifficultyHitObjects(osuFilePath, AR, CS, speedMultiplier);
    const double totalStars = OsuDifficultyCalculator::calculateStarDiffForHitObjects(
        diffres.diffobjects, CS, OD, speedMultiplier, relax, touchDevice, &aim, &aimSliderFactor, &speed, &speedNotes);

    m_fAimStars = (float)aim;
    m_fSpeedStars = (float)speed;

    // calculate final pp
    const int numHitObjects = m_hitobjects.size();
    const int numCircles = m_selectedDifficulty2->getNumCircles();
    const int numSliders = m_selectedDifficulty2->getNumSliders();
    const int numSpinners = m_selectedDifficulty2->getNumSpinners();
    const int maxPossibleCombo = m_iMaxPossibleCombo;
    const int highestCombo = m_osu->getScore()->getComboMax();
    const int numMisses = m_osu->getScore()->getNumMisses();
    const int num300s = m_osu->getScore()->getNum300s();
    const int num100s = m_osu->getScore()->getNum100s();
    const int num50s = m_osu->getScore()->getNum50s();
    const float pp = OsuDifficultyCalculator::calculatePPv2(
        m_osu, this, aim, aimSliderFactor, speed, speedNotes, numHitObjects, numCircles, numSliders, numSpinners,
        maxPossibleCombo, highestCombo, numMisses, num300s, num100s, num50s);
    m_osu->getScore()->setStarsTomTotal(totalStars);
    m_osu->getScore()->setStarsTomAim(m_fAimStars);
    m_osu->getScore()->setStarsTomSpeed(m_fSpeedStars);
    m_osu->getScore()->setPPv2(pp);

    // save local score, but only under certain conditions
    const bool isComplete = (num300s + num100s + num50s + numMisses >= numHitObjects);
    const bool isZero = (m_osu->getScore()->getScore() < 1);
    const bool isCheated = (m_osu->getModAuto() || (m_osu->getModAutopilot() && m_osu->getModRelax())) ||
                           m_osu->getScore()->isUnranked() || m_bIsWatchingReplay;

    Score score;
    score.isLegacyScore = false;
    score.isImportedLegacyScore = false;
    score.version = OsuScore::VERSION;
    score.unixTimestamp =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if(bancho.is_online()) {
        score.player_id = bancho.user_id;
        score.playerName = bancho.username;
    } else {
        score.playerName = convar->getConVarByName("name")->getString();
    }
    score.passed = isComplete && !isZero && !m_osu->getScore()->hasDied();
    score.grade = score.passed ? m_osu->getScore()->getGrade() : Score::Grade::F;
    score.diff2 = m_selectedDifficulty2;
    score.ragequit = quit;
    score.play_time_ms = m_iCurMusicPos / m_osu->getSpeedMultiplier();

    score.num300s = m_osu->getScore()->getNum300s();
    score.num100s = m_osu->getScore()->getNum100s();
    score.num50s = m_osu->getScore()->getNum50s();
    score.numGekis = m_osu->getScore()->getNum300gs();
    score.numKatus = m_osu->getScore()->getNum100ks();
    score.numMisses = m_osu->getScore()->getNumMisses();
    score.score = m_osu->getScore()->getScore();
    score.comboMax = m_osu->getScore()->getComboMax();
    score.perfect = (maxPossibleCombo > 0 && score.comboMax > 0 && score.comboMax >= maxPossibleCombo);
    score.numSliderBreaks = m_osu->getScore()->getNumSliderBreaks();
    score.pp = pp;
    score.unstableRate = m_osu->getScore()->getUnstableRate();
    score.hitErrorAvgMin = m_osu->getScore()->getHitErrorAvgMin();
    score.hitErrorAvgMax = m_osu->getScore()->getHitErrorAvgMax();
    score.starsTomTotal = totalStars;
    score.starsTomAim = aim;
    score.starsTomSpeed = speed;
    score.speedMultiplier = m_osu->getSpeedMultiplier();
    score.CS = CS;
    score.AR = AR;
    score.OD = getOD();
    score.HP = getHP();
    score.maxPossibleCombo = maxPossibleCombo;
    score.numHitObjects = numHitObjects;
    score.numCircles = numCircles;
    score.modsLegacy = m_osu->getScore()->getModsLegacy();

    // special case: manual slider accuracy has been enabled (affects pp but not score), so force scorev2 for
    // potential future score recalculations
    score.modsLegacy |= (m_osu_slider_scorev2_ref->getBool() ? ModFlags::ScoreV2 : 0);

    std::vector<ConVar *> allExperimentalMods = m_osu->getExperimentalMods();
    for(int i = 0; i < allExperimentalMods.size(); i++) {
        if(allExperimentalMods[i]->getBool()) {
            score.experimentalModsConVars.append(allExperimentalMods[i]->getName());
            score.experimentalModsConVars.append(";");
        }
    }

    score.md5hash = m_selectedDifficulty2->getMD5Hash();  // NOTE: necessary for "Use Mods"
    score.has_replay = true;
    score.replay = live_replay;

    int scoreIndex = -1;

    if(!isCheated) {
        OsuRichPresence::onPlayEnd(m_osu, quit);

        if(bancho.submit_scores() && !isZero && vanilla) {
            score.server = bancho.endpoint.toUtf8();
            submit_score(score);
            // XXX: Save online_score_id after getting submission result
        }

        if(score.passed) {
            scoreIndex = m_osu->getSongBrowser()->getDatabase()->addScore(m_selectedDifficulty2->getMD5Hash(), score);
            if(scoreIndex == -1) {
                m_osu->getNotificationOverlay()->addNotification("Failed saving score!", 0xffff0000, false, 3.0f);
            }
        }
    }

    m_osu->getScore()->setIndex(scoreIndex);
    m_osu->getScore()->setComboFull(maxPossibleCombo);  // used in OsuRankingScreen/OsuUIRankingScreenRankingPanel

    // special case: incomplete scores should NEVER show pp, even if auto
    if(!isComplete) {
        m_osu->getScore()->setPPv2(0.0f);
    }

    m_bIsWatchingReplay = false;
    spectated_replay.clear();

    debugLog("OsuBeatmap::onBeforeStop() done.\n");
}

void OsuBeatmap::onPaused(bool first) {
    debugLog("OsuBeatmap::onPaused()\n");

    if(first) {
        m_vContinueCursorPoint = getMousePos();

        if(OsuGameRules::osu_mod_fps.getBool()) m_vContinueCursorPoint = OsuGameRules::getPlayfieldCenter(m_osu);
    }
}

void OsuBeatmap::updateAutoCursorPos() {
    m_vAutoCursorPos = m_vPlayfieldCenter;
    m_vAutoCursorPos.y *= 2.5f;  // start moving in offscreen from bottom

    if(!m_bIsPlaying && !m_bIsPaused) {
        m_vAutoCursorPos = m_vPlayfieldCenter;
        return;
    }
    if(m_hitobjects.size() < 1) {
        m_vAutoCursorPos = m_vPlayfieldCenter;
        return;
    }

    const long curMusicPos = m_iCurMusicPosWithOffsets;

    // general
    long prevTime = 0;
    long nextTime = m_hitobjects[0]->getTime();
    Vector2 prevPos = m_vAutoCursorPos;
    Vector2 curPos = m_vAutoCursorPos;
    Vector2 nextPos = m_vAutoCursorPos;
    bool haveCurPos = false;

    // dance
    int nextPosIndex = 0;

    if(m_hitobjects[0]->getTime() < (long)m_osu_early_note_time_ref->getInt())
        prevTime = -(long)m_osu_early_note_time_ref->getInt() * getSpeedMultiplier();

    if(m_osu->getModAuto()) {
        bool autoDanceOverride = false;
        for(int i = 0; i < m_hitobjects.size(); i++) {
            OsuHitObject *o = m_hitobjects[i];

            // get previous object
            if(o->getTime() <= curMusicPos) {
                prevTime = o->getTime() + o->getDuration();
                prevPos = o->getAutoCursorPos(curMusicPos);
                if(o->getDuration() > 0 && curMusicPos - o->getTime() <= o->getDuration()) {
                    if(osu_auto_cursordance.getBool()) {
                        OsuSlider *sliderPointer = dynamic_cast<OsuSlider *>(o);
                        if(sliderPointer != NULL) {
                            const std::vector<OsuSlider::SLIDERCLICK> &clicks = sliderPointer->getClicks();

                            // start
                            prevTime = o->getTime();
                            prevPos = osuCoords2Pixels(o->getRawPosAt(prevTime));

                            long biggestPrevious = 0;
                            long smallestNext = std::numeric_limits<long>::max();
                            bool allFinished = true;
                            long endTime = 0;

                            // middle clicks
                            for(int c = 0; c < clicks.size(); c++) {
                                // get previous click
                                if(clicks[c].time <= curMusicPos && clicks[c].time > biggestPrevious) {
                                    biggestPrevious = clicks[c].time;
                                    prevTime = clicks[c].time;
                                    prevPos = osuCoords2Pixels(o->getRawPosAt(prevTime));
                                }

                                // get next click
                                if(clicks[c].time > curMusicPos && clicks[c].time < smallestNext) {
                                    smallestNext = clicks[c].time;
                                    nextTime = clicks[c].time;
                                    nextPos = osuCoords2Pixels(o->getRawPosAt(nextTime));
                                }

                                // end hack
                                if(!clicks[c].finished)
                                    allFinished = false;
                                else if(clicks[c].time > endTime)
                                    endTime = clicks[c].time;
                            }

                            // end
                            if(allFinished) {
                                // hack for slider without middle clicks
                                if(endTime == 0) endTime = o->getTime();

                                prevTime = endTime;
                                prevPos = osuCoords2Pixels(o->getRawPosAt(prevTime));
                                nextTime = o->getTime() + o->getDuration();
                                nextPos = osuCoords2Pixels(o->getRawPosAt(nextTime));
                            }

                            haveCurPos = false;
                            autoDanceOverride = true;
                            break;
                        }
                    }

                    haveCurPos = true;
                    curPos = prevPos;
                    break;
                }
            }

            // get next object
            if(o->getTime() > curMusicPos) {
                nextPosIndex = i;
                if(!autoDanceOverride) {
                    nextPos = o->getAutoCursorPos(curMusicPos);
                    nextTime = o->getTime();
                }
                break;
            }
        }
    } else if(m_osu->getModAutopilot()) {
        for(int i = 0; i < m_hitobjects.size(); i++) {
            OsuHitObject *o = m_hitobjects[i];

            // get previous object
            if(o->isFinished() ||
               (curMusicPos > o->getTime() + o->getDuration() +
                                  (long)(OsuGameRules::getHitWindow50(this) * osu_autopilot_lenience.getFloat()))) {
                prevTime = o->getTime() + o->getDuration() + o->getAutopilotDelta();
                prevPos = o->getAutoCursorPos(curMusicPos);
            } else if(!o->isFinished())  // get next object
            {
                nextPosIndex = i;
                nextPos = o->getAutoCursorPos(curMusicPos);
                nextTime = o->getTime();

                // wait for the user to click
                if(curMusicPos >= nextTime + o->getDuration()) {
                    haveCurPos = true;
                    curPos = nextPos;

                    // long delta = curMusicPos - (nextTime + o->getDuration());
                    o->setAutopilotDelta(curMusicPos - (nextTime + o->getDuration()));
                } else if(o->getDuration() > 0 && curMusicPos >= nextTime)  // handle objects with duration
                {
                    haveCurPos = true;
                    curPos = nextPos;
                    o->setAutopilotDelta(0);
                }

                break;
            }
        }
    }

    if(haveCurPos)  // in active hitObject
        m_vAutoCursorPos = curPos;
    else {
        // interpolation
        float percent = 1.0f;
        if((nextTime == 0 && prevTime == 0) || (nextTime - prevTime) == 0)
            percent = 1.0f;
        else
            percent = (float)((long)curMusicPos - prevTime) / (float)(nextTime - prevTime);

        percent = clamp<float>(percent, 0.0f, 1.0f);

        // scaled distance (not osucoords)
        float distance = (nextPos - prevPos).length();
        if(distance > m_fHitcircleDiameter * 1.05f)  // snap only if not in a stream (heuristic)
        {
            int numIterations = clamp<int>(m_osu->getModAutopilot() ? osu_autopilot_snapping_strength.getInt()
                                                                    : osu_auto_snapping_strength.getInt(),
                                           0, 42);
            for(int i = 0; i < numIterations; i++) {
                percent = (-percent) * (percent - 2.0f);
            }
        } else  // in a stream
        {
            m_iAutoCursorDanceIndex = nextPosIndex;
        }

        m_vAutoCursorPos = prevPos + (nextPos - prevPos) * percent;

        if(osu_auto_cursordance.getBool() && !m_osu->getModAutopilot()) {
            Vector3 dir = Vector3(nextPos.x, nextPos.y, 0) - Vector3(prevPos.x, prevPos.y, 0);
            Vector3 center = dir * 0.5f;
            Matrix4 worldMatrix;
            worldMatrix.translate(center);
            worldMatrix.rotate((1.0f - percent) * 180.0f * (m_iAutoCursorDanceIndex % 2 == 0 ? 1 : -1), 0, 0, 1);
            Vector3 fancyAutoCursorPos = worldMatrix * center;
            m_vAutoCursorPos =
                prevPos + (nextPos - prevPos) * 0.5f + Vector2(fancyAutoCursorPos.x, fancyAutoCursorPos.y);
        }
    }
}

void OsuBeatmap::updatePlayfieldMetrics() {
    m_fScaleFactor = OsuGameRules::getPlayfieldScaleFactor(m_osu);
    m_vPlayfieldSize = OsuGameRules::getPlayfieldSize(m_osu);
    m_vPlayfieldOffset = OsuGameRules::getPlayfieldOffset(m_osu);
    m_vPlayfieldCenter = OsuGameRules::getPlayfieldCenter(m_osu);
}

void OsuBeatmap::updateHitobjectMetrics() {
    OsuSkin *skin = m_osu->getSkin();

    m_fRawHitcircleDiameter = OsuGameRules::getRawHitCircleDiameter(getCS());
    m_fXMultiplier = OsuGameRules::getHitCircleXMultiplier(m_osu);
    m_fHitcircleDiameter = OsuGameRules::getHitCircleDiameter(this);

    const float osuCoordScaleMultiplier = (getHitcircleDiameter() / m_fRawHitcircleDiameter);
    m_fNumberScale = (m_fRawHitcircleDiameter / (160.0f * (skin->isDefault12x() ? 2.0f : 1.0f))) *
                     osuCoordScaleMultiplier * osu_number_scale_multiplier.getFloat();
    m_fHitcircleOverlapScale =
        (m_fRawHitcircleDiameter / (160.0f)) * osuCoordScaleMultiplier * osu_number_scale_multiplier.getFloat();

    const float followcircle_size_multiplier = OsuGameRules::osu_slider_followcircle_size_multiplier.getFloat();
    const float sliderFollowCircleDiameterMultiplier =
        (m_osu->getModNightmare() || osu_mod_jigsaw2.getBool()
             ? (1.0f * (1.0f - osu_mod_jigsaw_followcircle_radius_factor.getFloat()) +
                osu_mod_jigsaw_followcircle_radius_factor.getFloat() * followcircle_size_multiplier)
             : followcircle_size_multiplier);
    m_fRawSliderFollowCircleDiameter = m_fRawHitcircleDiameter * sliderFollowCircleDiameterMultiplier;
    m_fSliderFollowCircleDiameter = getHitcircleDiameter() * sliderFollowCircleDiameterMultiplier;
}

void OsuBeatmap::updateSliderVertexBuffers() {
    updatePlayfieldMetrics();
    updateHitobjectMetrics();

    m_bWasEZEnabled = m_osu->getModEZ();                // to avoid useless double updates in onModUpdate()
    m_fPrevHitCircleDiameter = getHitcircleDiameter();  // same here
    m_fPrevPlayfieldRotationFromConVar = osu_playfield_rotation.getFloat();  // same here
    m_fPrevPlayfieldStretchX = osu_playfield_stretch_x.getFloat();           // same here
    m_fPrevPlayfieldStretchY = osu_playfield_stretch_y.getFloat();           // same here

    debugLog("OsuBeatmap::updateSliderVertexBuffers() for %i hitobjects ...\n", m_hitobjects.size());

    for(int i = 0; i < m_hitobjects.size(); i++) {
        OsuSlider *sliderPointer = dynamic_cast<OsuSlider *>(m_hitobjects[i]);
        if(sliderPointer != NULL) sliderPointer->rebuildVertexBuffer();
    }
}

void OsuBeatmap::calculateStacks() {
    if(!osu_stacking.getBool()) return;

    updateHitobjectMetrics();

    debugLog("OsuBeatmap: Calculating stacks ...\n");

    // reset
    for(int i = 0; i < m_hitobjects.size(); i++) {
        m_hitobjects[i]->setStack(0);
    }

    const float STACK_LENIENCE = 3.0f;
    const float STACK_OFFSET = 0.05f;

    const float approachTime = OsuGameRules::getApproachTimeForStacking(this);

    const float stackLeniency =
        (osu_stacking_leniency_override.getFloat() >= 0.0f ? osu_stacking_leniency_override.getFloat()
                                                           : m_selectedDifficulty2->getStackLeniency());

    if(getSelectedDifficulty2()->getVersion() > 5) {
        // peppy's algorithm
        // https://gist.github.com/peppy/1167470

        for(int i = m_hitobjects.size() - 1; i >= 0; i--) {
            int n = i;

            OsuHitObject *objectI = m_hitobjects[i];

            bool isSpinner = dynamic_cast<OsuSpinner *>(objectI) != NULL;

            if(objectI->getStack() != 0 || isSpinner) continue;

            bool isHitCircle = dynamic_cast<OsuCircle *>(objectI) != NULL;
            bool isSlider = dynamic_cast<OsuSlider *>(objectI) != NULL;

            if(isHitCircle) {
                while(--n >= 0) {
                    OsuHitObject *objectN = m_hitobjects[n];

                    bool isSpinnerN = dynamic_cast<OsuSpinner *>(objectN);

                    if(isSpinnerN) continue;

                    if(objectI->getTime() - (approachTime * stackLeniency) >
                       (objectN->getTime() + objectN->getDuration()))
                        break;

                    Vector2 objectNEndPosition =
                        objectN->getOriginalRawPosAt(objectN->getTime() + objectN->getDuration());
                    if(objectN->getDuration() != 0 &&
                       (objectNEndPosition - objectI->getOriginalRawPosAt(objectI->getTime())).length() <
                           STACK_LENIENCE) {
                        int offset = objectI->getStack() - objectN->getStack() + 1;
                        for(int j = n + 1; j <= i; j++) {
                            if((objectNEndPosition - m_hitobjects[j]->getOriginalRawPosAt(m_hitobjects[j]->getTime()))
                                   .length() < STACK_LENIENCE)
                                m_hitobjects[j]->setStack(m_hitobjects[j]->getStack() - offset);
                        }

                        break;
                    }

                    if((objectN->getOriginalRawPosAt(objectN->getTime()) -
                        objectI->getOriginalRawPosAt(objectI->getTime()))
                           .length() < STACK_LENIENCE) {
                        objectN->setStack(objectI->getStack() + 1);
                        objectI = objectN;
                    }
                }
            } else if(isSlider) {
                while(--n >= 0) {
                    OsuHitObject *objectN = m_hitobjects[n];

                    bool isSpinner = dynamic_cast<OsuSpinner *>(objectN) != NULL;

                    if(isSpinner) continue;

                    if(objectI->getTime() - (approachTime * stackLeniency) > objectN->getTime()) break;

                    if(((objectN->getDuration() != 0
                             ? objectN->getOriginalRawPosAt(objectN->getTime() + objectN->getDuration())
                             : objectN->getOriginalRawPosAt(objectN->getTime())) -
                        objectI->getOriginalRawPosAt(objectI->getTime()))
                           .length() < STACK_LENIENCE) {
                        objectN->setStack(objectI->getStack() + 1);
                        objectI = objectN;
                    }
                }
            }
        }
    } else  // getSelectedDifficulty()->version < 6
    {
        // old stacking algorithm for old beatmaps
        // https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Osu/Beatmaps/OsuBeatmapProcessor.cs

        for(int i = 0; i < m_hitobjects.size(); i++) {
            OsuHitObject *currHitObject = m_hitobjects[i];
            OsuSlider *sliderPointer = dynamic_cast<OsuSlider *>(currHitObject);

            const bool isSlider = (sliderPointer != NULL);

            if(currHitObject->getStack() != 0 && !isSlider) continue;

            long startTime = currHitObject->getTime() + currHitObject->getDuration();
            int sliderStack = 0;

            for(int j = i + 1; j < m_hitobjects.size(); j++) {
                OsuHitObject *objectJ = m_hitobjects[j];

                if(objectJ->getTime() - (approachTime * stackLeniency) > startTime) break;

                // "The start position of the hitobject, or the position at the end of the path if the hitobject is a
                // slider"
                Vector2 position2 =
                    isSlider
                        ? sliderPointer->getOriginalRawPosAt(sliderPointer->getTime() + sliderPointer->getDuration())
                        : currHitObject->getOriginalRawPosAt(currHitObject->getTime());

                if((objectJ->getOriginalRawPosAt(objectJ->getTime()) -
                    currHitObject->getOriginalRawPosAt(currHitObject->getTime()))
                       .length() < 3) {
                    currHitObject->setStack(currHitObject->getStack() + 1);
                    startTime = objectJ->getTime() + objectJ->getDuration();
                } else if((objectJ->getOriginalRawPosAt(objectJ->getTime()) - position2).length() < 3) {
                    // "Case for sliders - bump notes down and right, rather than up and left."
                    sliderStack++;
                    objectJ->setStack(objectJ->getStack() - sliderStack);
                    startTime = objectJ->getTime() + objectJ->getDuration();
                }
            }
        }
    }

    // update hitobject positions
    float stackOffset = m_fRawHitcircleDiameter * STACK_OFFSET;
    for(int i = 0; i < m_hitobjects.size(); i++) {
        if(m_hitobjects[i]->getStack() != 0) m_hitobjects[i]->updateStackPosition(stackOffset);
    }
}

void OsuBeatmap::computeDrainRate() {
    m_fDrainRate = 0.0;
    m_fHpMultiplierNormal = 1.0;
    m_fHpMultiplierComboEnd = 1.0;

    if(m_hitobjects.size() < 1 || m_selectedDifficulty2 == NULL) return;

    debugLog("OsuBeatmap: Calculating drain ...\n");

    const int drainType = m_osu_drain_type_ref->getInt();
    if(drainType == 2)  // osu!stable
    {
        // see https://github.com/ppy/osu-iPhone/blob/master/Classes/OsuPlayer.m
        // see calcHPDropRate() @ https://github.com/ppy/osu-iPhone/blob/master/Classes/OsuFiletype.m#L661

        // NOTE: all drain changes between 2014 and today have been fixed here (the link points to an old version of the
        // algorithm!) these changes include: passive spinner nerf (drain * 0.25 while spinner is active), and clamping
        // the object length drain to 0 + an extra check for that (see maxLongObjectDrop) see
        // https://osu.ppy.sh/home/changelog/stable40/20190513.2

        struct TestPlayer {
            TestPlayer(double hpBarMaximum) {
                this->hpBarMaximum = hpBarMaximum;

                hpMultiplierNormal = 1.0;
                hpMultiplierComboEnd = 1.0;

                resetHealth();
            }

            void resetHealth() {
                health = hpBarMaximum;
                healthUncapped = hpBarMaximum;
            }

            void increaseHealth(double amount) {
                healthUncapped += amount;
                health += amount;

                if(health > hpBarMaximum) health = hpBarMaximum;

                if(health < 0.0) health = 0.0;

                if(healthUncapped < 0.0) healthUncapped = 0.0;
            }

            void decreaseHealth(double amount) {
                health -= amount;

                if(health < 0.0) health = 0.0;

                if(health > hpBarMaximum) health = hpBarMaximum;

                healthUncapped -= amount;

                if(healthUncapped < 0.0) healthUncapped = 0.0;
            }

            double hpBarMaximum;

            double health;
            double healthUncapped;

            double hpMultiplierNormal;
            double hpMultiplierComboEnd;
        };
        double foo = (double)m_osu_drain_stable_hpbar_maximum_ref->getFloat();
        TestPlayer testPlayer(foo);

        const double HP = getHP();
        const int version = m_selectedDifficulty2->getVersion();

        double testDrop = 0.05;

        const double lowestHpEver = OsuGameRules::mapDifficultyRangeDouble(HP, 195.0, 160.0, 60.0);
        const double lowestHpComboEnd = OsuGameRules::mapDifficultyRangeDouble(HP, 198.0, 170.0, 80.0);
        const double lowestHpEnd = OsuGameRules::mapDifficultyRangeDouble(HP, 198.0, 180.0, 80.0);
        const double HpRecoveryAvailable = OsuGameRules::mapDifficultyRangeDouble(HP, 8.0, 4.0, 0.0);

        bool fail = false;

        do {
            testPlayer.resetHealth();

            double lowestHp = testPlayer.health;
            int lastTime = (int)(m_hitobjects[0]->getTime() - (long)OsuGameRules::getApproachTime(this));
            fail = false;

            const int breakCount = m_breaks.size();
            int breakNumber = 0;

            int comboTooLowCount = 0;

            for(int i = 0; i < m_hitobjects.size(); i++) {
                const OsuHitObject *h = m_hitobjects[i];
                const OsuSlider *sliderPointer = dynamic_cast<const OsuSlider *>(h);
                const OsuSpinner *spinnerPointer = dynamic_cast<const OsuSpinner *>(h);

                const int localLastTime = lastTime;

                int breakTime = 0;
                if(breakCount > 0 && breakNumber < breakCount) {
                    const OsuDatabaseBeatmap::BREAK &e = m_breaks[breakNumber];
                    if(e.startTime >= localLastTime && e.endTime <= h->getTime()) {
                        // consider break start equal to object end time for version 8+ since drain stops during this
                        // time
                        breakTime = (version < 8) ? (e.endTime - e.startTime) : (e.endTime - localLastTime);
                        breakNumber++;
                    }
                }

                testPlayer.decreaseHealth(testDrop * (h->getTime() - lastTime - breakTime));

                lastTime = (int)(h->getTime() + h->getDuration());

                if(testPlayer.health < lowestHp) lowestHp = testPlayer.health;

                if(testPlayer.health > lowestHpEver) {
                    const double longObjectDrop = testDrop * (double)h->getDuration();
                    const double maxLongObjectDrop = std::max(0.0, longObjectDrop - testPlayer.health);

                    testPlayer.decreaseHealth(longObjectDrop);

                    // nested hitobjects
                    if(sliderPointer != NULL) {
                        // startcircle
                        testPlayer.increaseHealth(
                            OsuScore::getHealthIncrease(OsuScore::HIT::HIT_SLIDER30, HP, testPlayer.hpMultiplierNormal,
                                                        testPlayer.hpMultiplierComboEnd, 1.0));  // slider30

                        // ticks + repeats + repeat ticks
                        const std::vector<OsuSlider::SLIDERCLICK> &clicks = sliderPointer->getClicks();
                        for(int c = 0; c < clicks.size(); c++) {
                            switch(clicks[c].type) {
                                case 0:  // repeat
                                    testPlayer.increaseHealth(OsuScore::getHealthIncrease(
                                        OsuScore::HIT::HIT_SLIDER30, HP, testPlayer.hpMultiplierNormal,
                                        testPlayer.hpMultiplierComboEnd, 1.0));  // slider30
                                    break;
                                case 1:  // tick
                                    testPlayer.increaseHealth(OsuScore::getHealthIncrease(
                                        OsuScore::HIT::HIT_SLIDER10, HP, testPlayer.hpMultiplierNormal,
                                        testPlayer.hpMultiplierComboEnd, 1.0));  // slider10
                                    break;
                            }
                        }

                        // endcircle
                        testPlayer.increaseHealth(
                            OsuScore::getHealthIncrease(OsuScore::HIT::HIT_SLIDER30, HP, testPlayer.hpMultiplierNormal,
                                                        testPlayer.hpMultiplierComboEnd, 1.0));  // slider30
                    } else if(spinnerPointer != NULL) {
                        const int rotationsNeeded = (int)((float)spinnerPointer->getDuration() / 1000.0f *
                                                          OsuGameRules::getSpinnerSpinsPerSecond(this));
                        for(int r = 0; r < rotationsNeeded; r++) {
                            testPlayer.increaseHealth(OsuScore::getHealthIncrease(
                                OsuScore::HIT::HIT_SPINNERSPIN, HP, testPlayer.hpMultiplierNormal,
                                testPlayer.hpMultiplierComboEnd, 1.0));  // spinnerspin
                        }
                    }

                    if(!(maxLongObjectDrop > 0.0) || (testPlayer.health - maxLongObjectDrop) > lowestHpEver) {
                        // regular hit (for every hitobject)
                        testPlayer.increaseHealth(
                            OsuScore::getHealthIncrease(OsuScore::HIT::HIT_300, HP, testPlayer.hpMultiplierNormal,
                                                        testPlayer.hpMultiplierComboEnd, 1.0));  // 300

                        // end of combo (new combo starts at next hitobject)
                        if((i == m_hitobjects.size() - 1) || m_hitobjects[i]->isEndOfCombo()) {
                            testPlayer.increaseHealth(
                                OsuScore::getHealthIncrease(OsuScore::HIT::HIT_300G, HP, testPlayer.hpMultiplierNormal,
                                                            testPlayer.hpMultiplierComboEnd, 1.0));  // geki

                            if(testPlayer.health < lowestHpComboEnd) {
                                if(++comboTooLowCount > 2) {
                                    testPlayer.hpMultiplierComboEnd *= 1.07;
                                    testPlayer.hpMultiplierNormal *= 1.03;
                                    fail = true;
                                    break;
                                }
                            }
                        }

                        continue;
                    }

                    fail = true;
                    testDrop *= 0.96;
                    break;
                }

                fail = true;
                testDrop *= 0.96;
                break;
            }

            if(!fail && testPlayer.health < lowestHpEnd) {
                fail = true;
                testDrop *= 0.94;
                testPlayer.hpMultiplierComboEnd *= 1.01;
                testPlayer.hpMultiplierNormal *= 1.01;
            }

            const double recovery = (testPlayer.healthUncapped - testPlayer.hpBarMaximum) / (double)m_hitobjects.size();
            if(!fail && recovery < HpRecoveryAvailable) {
                fail = true;
                testDrop *= 0.96;
                testPlayer.hpMultiplierComboEnd *= 1.02;
                testPlayer.hpMultiplierNormal *= 1.01;
            }
        } while(fail);

        m_fDrainRate =
            (testDrop / testPlayer.hpBarMaximum) * 1000.0;  // from [0, 200] to [0, 1], and from ms to seconds
        m_fHpMultiplierComboEnd = testPlayer.hpMultiplierComboEnd;
        m_fHpMultiplierNormal = testPlayer.hpMultiplierNormal;
    } else if(drainType == 3)  // osu!lazer 2020
    {
        // build healthIncreases
        std::vector<std::pair<double, double>> healthIncreases;  // [first = time, second = health]
        healthIncreases.reserve(m_hitobjects.size());
        const double healthIncreaseForHit300 = OsuScore::getHealthIncrease(OsuScore::HIT::HIT_300);
        for(int i = 0; i < m_hitobjects.size(); i++) {
            // nested hitobjects
            const OsuSlider *sliderPointer = dynamic_cast<OsuSlider *>(m_hitobjects[i]);
            if(sliderPointer != NULL) {
                // startcircle
                healthIncreases.push_back(
                    std::pair<double, double>((double)m_hitobjects[i]->getTime(), healthIncreaseForHit300));

                // ticks + repeats + repeat ticks
                const std::vector<OsuSlider::SLIDERCLICK> &clicks = sliderPointer->getClicks();
                for(int c = 0; c < clicks.size(); c++) {
                    healthIncreases.push_back(
                        std::pair<double, double>((double)clicks[c].time, healthIncreaseForHit300));
                }
            }

            // regular hitobject
            healthIncreases.push_back(std::pair<double, double>(
                m_hitobjects[i]->getTime() + m_hitobjects[i]->getDuration(), healthIncreaseForHit300));
        }

        const int numHealthIncreases = healthIncreases.size();
        const int numBreaks = m_breaks.size();
        const double drainStartTime = m_hitobjects[0]->getTime();

        // see computeDrainRate() &
        // https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Scoring/DrainingHealthProcessor.cs

        const double minimum_health_error = 0.01;

        const double min_health_target = osu_drain_lazer_health_min.getFloat();
        const double mid_health_target = osu_drain_lazer_health_mid.getFloat();
        const double max_health_target = osu_drain_lazer_health_max.getFloat();

        const double targetMinimumHealth =
            OsuGameRules::mapDifficultyRange(getHP(), min_health_target, mid_health_target, max_health_target);

        int adjustment = 1;
        double result = 1.0;

        // Although we expect the following loop to converge within 30 iterations (health within 1/2^31 accuracy of the
        // target), we'll still keep a safety measure to avoid infinite loops by detecting overflows.
        while(adjustment > 0) {
            double currentHealth = 1.0;
            double lowestHealth = 1.0;
            int currentBreak = -1;

            for(int i = 0; i < numHealthIncreases; i++) {
                double currentTime = healthIncreases[i].first;
                double lastTime = i > 0 ? healthIncreases[i - 1].first : drainStartTime;

                // Subtract any break time from the duration since the last object
                if(numBreaks > 0) {
                    // Advance the last break occuring before the current time
                    while(currentBreak + 1 < numBreaks && (double)m_breaks[currentBreak + 1].endTime < currentTime) {
                        currentBreak++;
                    }

                    if(currentBreak >= 0) lastTime = std::max(lastTime, (double)m_breaks[currentBreak].endTime);
                }

                // Apply health adjustments
                currentHealth -= (healthIncreases[i].first - lastTime) * result;
                lowestHealth = std::min(lowestHealth, currentHealth);
                currentHealth = std::min(1.0, currentHealth + healthIncreases[i].second);

                // Common scenario for when the drain rate is definitely too harsh
                if(lowestHealth < 0) break;
            }

            // Stop if the resulting health is within a reasonable offset from the target
            if(std::abs(lowestHealth - targetMinimumHealth) <= minimum_health_error) break;

            // This effectively works like a binary search - each iteration the search space moves closer to the target,
            // but may exceed it.
            adjustment *= 2;
            result += 1.0 / adjustment * sign<double>(lowestHealth - targetMinimumHealth);
        }

        m_fDrainRate = result * 1000.0;  // from ms to seconds
    }
}

void OsuBeatmap::updateStarCache() {
    if(m_osu_draw_statistics_pp_ref->getBool() || m_osu_draw_statistics_livestars_ref->getBool()) {
        // so we don't get a useless double load inside onModUpdate()
        m_fPrevHitCircleDiameterForStarCache = getHitcircleDiameter();
        m_fPrevSpeedForStarCache = m_osu->getSpeedMultiplier();

        // kill any running loader, so we get to a clean state
        stopStarCacheLoader();
        engine->getResourceManager()->destroyResource(m_starCacheLoader);

        // create new loader
        m_starCacheLoader = new OsuBackgroundStarCacheLoader(this);
        m_starCacheLoader->revive();  // activate it
        engine->getResourceManager()->requestNextLoadAsync();
        engine->getResourceManager()->loadResource(m_starCacheLoader);
    }
}

void OsuBeatmap::stopStarCacheLoader() {
    if(!m_starCacheLoader->isDead()) {
        m_starCacheLoader->kill();
        double startTime = engine->getTimeReal();
        while(!m_starCacheLoader->isAsyncReady())  // stall main thread until it's killed (this should be very quick,
                                                   // around max 1 ms, as the kill flag is checked in every iteration)
        {
            if(engine->getTimeReal() - startTime > 2) {
                debugLog("WARNING: Ignoring stuck StarCacheLoader thread!\n");
                break;
            }
        }

        // NOTE: this only works safely because OsuBackgroundStarCacheLoader does no work in load(), because it might
        // still be in the ResourceManager's sync load() queue, so future loadAsync() could crash with the old pending
        // load()
    }
}

bool OsuBeatmap::isLoadingStarCache() {
    return ((m_osu_draw_statistics_pp_ref->getBool() || m_osu_draw_statistics_livestars_ref->getBool()) &&
            !m_starCacheLoader->isReady());
}
