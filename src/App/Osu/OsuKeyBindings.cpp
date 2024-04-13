//================ Copyright (c) 2016, PG, All rights reserved. =================//
//
// Purpose:		key bindings container
//
// $NoKeywords: $osukey
//===============================================================================//

#include "OsuKeyBindings.h"

#include "Keyboard.h"

ConVar OsuKeyBindings::LEFT_CLICK("osu_key_left_click", (int)KEY_Z, FCVAR_NONE);
ConVar OsuKeyBindings::RIGHT_CLICK("osu_key_right_click", (int)KEY_X, FCVAR_NONE);
ConVar OsuKeyBindings::LEFT_CLICK_2("osu_key_left_click_2", 0, FCVAR_NONE);
ConVar OsuKeyBindings::RIGHT_CLICK_2("osu_key_right_click_2", 0, FCVAR_NONE);

ConVar OsuKeyBindings::FPOSU_ZOOM("osu_key_fposu_zoom", 0, FCVAR_NONE);

ConVar OsuKeyBindings::INCREASE_VOLUME("osu_key_increase_volume", (int)KEY_UP, FCVAR_NONE);
ConVar OsuKeyBindings::DECREASE_VOLUME("osu_key_decrease_volume", (int)KEY_DOWN, FCVAR_NONE);

ConVar OsuKeyBindings::INCREASE_LOCAL_OFFSET("osu_key_increase_local_offset", (int)KEY_ADD, FCVAR_NONE);
ConVar OsuKeyBindings::DECREASE_LOCAL_OFFSET("osu_key_decrease_local_offset", (int)KEY_SUBTRACT, FCVAR_NONE);

ConVar OsuKeyBindings::GAME_PAUSE("osu_key_game_pause", (int)KEY_ESCAPE, FCVAR_NONE);
ConVar OsuKeyBindings::SKIP_CUTSCENE("osu_key_skip_cutscene", (int)KEY_SPACE, FCVAR_NONE);
ConVar OsuKeyBindings::TOGGLE_SCOREBOARD("osu_key_toggle_scoreboard", (int)KEY_TAB, FCVAR_NONE);
ConVar OsuKeyBindings::SEEK_TIME("osu_key_seek_time", (int)KEY_SHIFT, FCVAR_NONE);
ConVar OsuKeyBindings::SEEK_TIME_BACKWARD("osu_key_seek_time_backward", (int)KEY_LEFT, FCVAR_NONE);
ConVar OsuKeyBindings::SEEK_TIME_FORWARD("osu_key_seek_time_forward", (int)KEY_RIGHT, FCVAR_NONE);
ConVar OsuKeyBindings::QUICK_RETRY("osu_key_quick_retry", (int)KEY_BACKSPACE, FCVAR_NONE);
ConVar OsuKeyBindings::QUICK_SAVE("osu_key_quick_save", (int)KEY_F6, FCVAR_NONE);
ConVar OsuKeyBindings::QUICK_LOAD("osu_key_quick_load", (int)KEY_F7, FCVAR_NONE);
ConVar OsuKeyBindings::INSTANT_REPLAY("osu_key_instant_replay", (int)KEY_F1, FCVAR_NONE);
ConVar OsuKeyBindings::TOGGLE_CHAT("osu_key_toggle_chat", (int)KEY_F8, FCVAR_NONE);
ConVar OsuKeyBindings::SAVE_SCREENSHOT("osu_key_save_screenshot", (int)KEY_F12, FCVAR_NONE);
ConVar OsuKeyBindings::DISABLE_MOUSE_BUTTONS("osu_key_disable_mouse_buttons", (int)KEY_F10, FCVAR_NONE);
ConVar OsuKeyBindings::BOSS_KEY("osu_key_boss", (int)KEY_INSERT, FCVAR_NONE);

ConVar OsuKeyBindings::TOGGLE_MODSELECT("osu_key_toggle_modselect", (int)KEY_F1, FCVAR_NONE);
ConVar OsuKeyBindings::RANDOM_BEATMAP("osu_key_random_beatmap", (int)KEY_F2, FCVAR_NONE);

ConVar OsuKeyBindings::MOD_EASY("osu_key_mod_easy", (int)KEY_Q, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_NOFAIL("osu_key_mod_nofail", (int)KEY_W, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_HALFTIME("osu_key_mod_halftime", (int)KEY_E, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_HARDROCK("osu_key_mod_hardrock", (int)KEY_A, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_SUDDENDEATH("osu_key_mod_suddendeath", (int)KEY_S, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_DOUBLETIME("osu_key_mod_doubletime", (int)KEY_D, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_HIDDEN("osu_key_mod_hidden", (int)KEY_F, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_FLASHLIGHT("osu_key_mod_flashlight", (int)KEY_G, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_RELAX("osu_key_mod_relax", (int)KEY_Z, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_AUTOPILOT("osu_key_mod_autopilot", (int)KEY_X, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_SPUNOUT("osu_key_mod_spunout", (int)KEY_C, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_AUTO("osu_key_mod_auto", (int)KEY_V, FCVAR_NONE);
ConVar OsuKeyBindings::MOD_SCOREV2("osu_key_mod_scorev2", (int)KEY_B, FCVAR_NONE);

std::vector<ConVar*> OsuKeyBindings::ALL = {&OsuKeyBindings::LEFT_CLICK,
                                            &OsuKeyBindings::RIGHT_CLICK,
                                            &OsuKeyBindings::LEFT_CLICK_2,
                                            &OsuKeyBindings::RIGHT_CLICK_2,

                                            &OsuKeyBindings::FPOSU_ZOOM,

                                            &OsuKeyBindings::INCREASE_VOLUME,
                                            &OsuKeyBindings::DECREASE_VOLUME,

                                            &OsuKeyBindings::INCREASE_LOCAL_OFFSET,
                                            &OsuKeyBindings::DECREASE_LOCAL_OFFSET,

                                            &OsuKeyBindings::GAME_PAUSE,
                                            &OsuKeyBindings::SKIP_CUTSCENE,
                                            &OsuKeyBindings::TOGGLE_SCOREBOARD,
                                            &OsuKeyBindings::SEEK_TIME,
                                            &OsuKeyBindings::SEEK_TIME_BACKWARD,
                                            &OsuKeyBindings::SEEK_TIME_FORWARD,
                                            &OsuKeyBindings::QUICK_RETRY,
                                            &OsuKeyBindings::QUICK_SAVE,
                                            &OsuKeyBindings::QUICK_LOAD,
                                            &OsuKeyBindings::INSTANT_REPLAY,
                                            &OsuKeyBindings::TOGGLE_CHAT,
                                            &OsuKeyBindings::SAVE_SCREENSHOT,
                                            &OsuKeyBindings::DISABLE_MOUSE_BUTTONS,
                                            &OsuKeyBindings::BOSS_KEY,

                                            &OsuKeyBindings::TOGGLE_MODSELECT,
                                            &OsuKeyBindings::RANDOM_BEATMAP,

                                            &OsuKeyBindings::MOD_EASY,
                                            &OsuKeyBindings::MOD_NOFAIL,
                                            &OsuKeyBindings::MOD_HALFTIME,
                                            &OsuKeyBindings::MOD_HARDROCK,
                                            &OsuKeyBindings::MOD_SUDDENDEATH,
                                            &OsuKeyBindings::MOD_DOUBLETIME,
                                            &OsuKeyBindings::MOD_HIDDEN,
                                            &OsuKeyBindings::MOD_FLASHLIGHT,
                                            &OsuKeyBindings::MOD_RELAX,
                                            &OsuKeyBindings::MOD_AUTOPILOT,
                                            &OsuKeyBindings::MOD_SPUNOUT,
                                            &OsuKeyBindings::MOD_AUTO,
                                            &OsuKeyBindings::MOD_SCOREV2};
