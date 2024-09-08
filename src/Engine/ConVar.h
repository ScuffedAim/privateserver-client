#pragma once
#include <atomic>

#include "FastDelegate.h"
#include "UString.h"

enum FCVAR_FLAGS {
    // No flags: cvar is only allowed offline.
    FCVAR_LOCKED = 0,

    // If the cvar is modified, scores will not submit unless this flag is set.
    FCVAR_BANCHO_SUBMITTABLE = (1 << 0),

    // Legacy servers only support a limited number of cvars.
    // Unless this flag is set, the cvar will be forced to its default value
    // when playing in a multiplayer lobby on a legacy server.
    FCVAR_BANCHO_MULTIPLAYABLE = (1 << 1),  // NOTE: flag shouldn't be set without BANCHO_SUBMITTABLE
    FCVAR_BANCHO_COMPATIBLE = FCVAR_BANCHO_SUBMITTABLE | FCVAR_BANCHO_MULTIPLAYABLE,

    // hide cvar from console suggestions
    FCVAR_HIDDEN = (1 << 2),

    // prevent servers from touching this cvar
    FCVAR_PRIVATE = (1 << 3),

    // this cvar affects gameplay
    FCVAR_GAMEPLAY = (1 << 4),
};

class ConVar {
   public:
    enum class CONVAR_TYPE { CONVAR_TYPE_BOOL, CONVAR_TYPE_INT, CONVAR_TYPE_FLOAT, CONVAR_TYPE_STRING };

    // raw callbacks
    typedef void (*ConVarCallback)(void);
    typedef void (*ConVarChangeCallback)(UString oldValue, UString newValue);
    typedef void (*ConVarCallbackArgs)(UString args);

    // delegate callbacks
    typedef fastdelegate::FastDelegate0<> NativeConVarCallback;
    typedef fastdelegate::FastDelegate1<UString> NativeConVarCallbackArgs;
    typedef fastdelegate::FastDelegate2<UString, UString> NativeConVarChangeCallback;

   public:
    static UString typeToString(CONVAR_TYPE type);

   public:
    explicit ConVar(UString name);

    explicit ConVar(UString name, int flags, ConVarCallback callback);
    explicit ConVar(UString name, int flags, const char *helpString, ConVarCallback callback);

    explicit ConVar(UString name, int flags, ConVarCallbackArgs callbackARGS);
    explicit ConVar(UString name, int flags, const char *helpString, ConVarCallbackArgs callbackARGS);

    explicit ConVar(UString name, float defaultValue, int flags);
    explicit ConVar(UString name, float defaultValue, int flags, ConVarChangeCallback callback);
    explicit ConVar(UString name, float defaultValue, int flags, const char *helpString);
    explicit ConVar(UString name, float defaultValue, int flags, const char *helpString, ConVarChangeCallback callback);

    explicit ConVar(UString name, int defaultValue, int flags);
    explicit ConVar(UString name, int defaultValue, int flags, ConVarChangeCallback callback);
    explicit ConVar(UString name, int defaultValue, int flags, const char *helpString);
    explicit ConVar(UString name, int defaultValue, int flags, const char *helpString, ConVarChangeCallback callback);

    explicit ConVar(UString name, bool defaultValue, int flags);
    explicit ConVar(UString name, bool defaultValue, int flags, ConVarChangeCallback callback);
    explicit ConVar(UString name, bool defaultValue, int flags, const char *helpString);
    explicit ConVar(UString name, bool defaultValue, int flags, const char *helpString, ConVarChangeCallback callback);

    explicit ConVar(UString name, const char *defaultValue, int flags);
    explicit ConVar(UString name, const char *defaultValue, int flags, const char *helpString);
    explicit ConVar(UString name, const char *defaultValue, int flags, ConVarChangeCallback callback);
    explicit ConVar(UString name, const char *defaultValue, int flags, const char *helpString,
                    ConVarChangeCallback callback);

    // callbacks
    void exec();
    void execArgs(UString args);

    // set
    void resetDefaults();
    void setDefaultBool(bool defaultValue) { setDefaultFloat(defaultValue ? 1.f : 0.f); }
    void setDefaultInt(int defaultValue) { setDefaultFloat((float)defaultValue); }
    void setDefaultFloat(float defaultValue);
    void setDefaultString(UString defaultValue);

    void setValue(float value);
    void setValue(UString sValue);

    void setCallback(NativeConVarCallback callback);
    void setCallback(NativeConVarCallbackArgs callback);
    void setCallback(NativeConVarChangeCallback callback);

    void setHelpString(UString helpString);

    // get
    inline float getDefaultFloat() const { return m_fDefaultValue.load(); }
    inline const UString &getDefaultString() const { return m_sDefaultValue; }

    bool isUnlocked() const;
    std::string getFancyDefaultValue();

    bool getBool() const;
    float getFloat() const;
    int getInt();
    const UString &getString();

    inline const UString &getHelpstring() const { return m_sHelpString; }
    inline const UString &getName() const { return m_sName; }
    inline CONVAR_TYPE getType() const { return m_type; }
    inline int getFlags() const { return m_iFlags; }
    inline void setFlags(int new_flags) { m_iFlags = new_flags; }

    inline bool hasValue() const { return m_bHasValue; }
    inline bool hasCallbackArgs() const { return (m_callbackfuncargs || m_changecallback); }
    inline bool isFlagSet(int flag) const { return (bool)(m_iFlags & flag); }

   private:
    void init(int flags);
    void init(UString &name, int flags);
    void init(UString &name, int flags, ConVarCallback callback);
    void init(UString &name, int flags, UString helpString, ConVarCallback callback);
    void init(UString &name, int flags, ConVarCallbackArgs callbackARGS);
    void init(UString &name, int flags, UString helpString, ConVarCallbackArgs callbackARGS);
    void init(UString &name, float defaultValue, int flags, UString helpString, ConVarChangeCallback callback);
    void init(UString &name, UString defaultValue, int flags, UString helpString, ConVarChangeCallback callback);

   private:
    bool m_bHasValue;
    CONVAR_TYPE m_type;
    int m_iDefaultFlags;
    int m_iFlags;

    UString m_sName;
    UString m_sHelpString;

    std::atomic<float> m_fValue;
    std::atomic<float> m_fDefaultValue;
    float m_fDefaultDefaultValue;

    UString m_sValue;
    UString m_sDefaultValue;
    UString m_sDefaultDefaultValue;

    NativeConVarCallback m_callbackfunc;
    NativeConVarCallbackArgs m_callbackfuncargs;
    NativeConVarChangeCallback m_changecallback;
};

//*******************//
//  Searching/Lists  //
//*******************//

class ConVarHandler {
   public:
    static UString flagsToString(int flags);

   public:
    ConVarHandler();
    ~ConVarHandler();

    const std::vector<ConVar *> &getConVarArray() const;
    int getNumConVars() const;

    ConVar *getConVarByName(UString name, bool warnIfNotFound = true) const;
    std::vector<ConVar *> getConVarByLetter(UString letters) const;

    bool isVanilla();
};

extern ConVarHandler *convar;

extern ConVar cmd_borderless;
extern ConVar cmd_center;
extern ConVar cmd_clear;
extern ConVar cmd_dpiinfo;
extern ConVar cmd_dumpcommands;
extern ConVar cmd_echo;
extern ConVar cmd_errortest;
extern ConVar cmd_exec;
extern ConVar cmd_exit;
extern ConVar cmd_find;
extern ConVar cmd_focus;
extern ConVar cmd_fullscreen;
extern ConVar cmd_help;
extern ConVar cmd_listcommands;
extern ConVar cmd_maximize;
extern ConVar cmd_minimize;
extern ConVar cmd_printsize;
extern ConVar cmd_resizable_toggle;
extern ConVar cmd_restart;
extern ConVar cmd_showconsolebox;
extern ConVar cmd_shutdown;
extern ConVar cmd_spectate;
extern ConVar cmd_windowed;

extern ConVar cv_BOSS_KEY;
extern ConVar cv_DECREASE_LOCAL_OFFSET;
extern ConVar cv_DECREASE_VOLUME;
extern ConVar cv_DISABLE_MOUSE_BUTTONS;
extern ConVar cv_FPOSU_ZOOM;
extern ConVar cv_GAME_PAUSE;
extern ConVar cv_INCREASE_LOCAL_OFFSET;
extern ConVar cv_INCREASE_VOLUME;
extern ConVar cv_INSTANT_REPLAY;
extern ConVar cv_LEFT_CLICK;
extern ConVar cv_LEFT_CLICK_2;
extern ConVar cv_MOD_AUTO;
extern ConVar cv_MOD_AUTOPILOT;
extern ConVar cv_MOD_DOUBLETIME;
extern ConVar cv_MOD_EASY;
extern ConVar cv_MOD_FLASHLIGHT;
extern ConVar cv_MOD_HALFTIME;
extern ConVar cv_MOD_HARDROCK;
extern ConVar cv_MOD_HIDDEN;
extern ConVar cv_MOD_NOFAIL;
extern ConVar cv_MOD_RELAX;
extern ConVar cv_MOD_SCOREV2;
extern ConVar cv_MOD_SPUNOUT;
extern ConVar cv_MOD_SUDDENDEATH;
extern ConVar cv_OPEN_SKIN_SELECT_MENU;
extern ConVar cv_QUICK_LOAD;
extern ConVar cv_QUICK_RETRY;
extern ConVar cv_QUICK_SAVE;
extern ConVar cv_RANDOM_BEATMAP;
extern ConVar cv_RIGHT_CLICK;
extern ConVar cv_RIGHT_CLICK_2;
extern ConVar cv_SAVE_SCREENSHOT;
extern ConVar cv_SEEK_TIME;
extern ConVar cv_SEEK_TIME_BACKWARD;
extern ConVar cv_SEEK_TIME_FORWARD;
extern ConVar cv_SKIP_CUTSCENE;
extern ConVar cv_TOGGLE_CHAT;
extern ConVar cv_TOGGLE_MAP_BACKGROUND;
extern ConVar cv_TOGGLE_MODSELECT;
extern ConVar cv_TOGGLE_SCOREBOARD;
extern ConVar cv_alt_f4_quits_even_while_playing;
extern ConVar cv_always_render_cursor_trail;
extern ConVar cv_animation_speed_override;
extern ConVar cv_approach_circle_alpha_multiplier;
extern ConVar cv_approach_scale_multiplier;
extern ConVar cv_approachtime_max;
extern ConVar cv_approachtime_mid;
extern ConVar cv_approachtime_min;
extern ConVar cv_ar_override;
extern ConVar cv_ar_override_lock;
extern ConVar cv_ar_overridenegative;
extern ConVar cv_asio_buffer_size;
extern ConVar cv_auto_and_relax_block_user_input;
extern ConVar cv_auto_cursordance;
extern ConVar cv_auto_snapping_strength;
extern ConVar cv_auto_update;
extern ConVar cv_automatic_cursor_size;
extern ConVar cv_autopilot_lenience;
extern ConVar cv_autopilot_snapping_strength;
extern ConVar cv_avoid_flashes;
extern ConVar cv_background_brightness;
extern ConVar cv_background_color_b;
extern ConVar cv_background_color_g;
extern ConVar cv_background_color_r;
extern ConVar cv_background_dim;
extern ConVar cv_background_dont_fade_during_breaks;
extern ConVar cv_background_fade_after_load;
extern ConVar cv_background_fade_in_duration;
extern ConVar cv_background_fade_min_duration;
extern ConVar cv_background_fade_out_duration;
extern ConVar cv_background_image_cache_size;
extern ConVar cv_background_image_eviction_delay_frames;
extern ConVar cv_background_image_eviction_delay_seconds;
extern ConVar cv_background_image_loading_delay;
extern ConVar cv_beatmap_max_num_hitobjects;
extern ConVar cv_beatmap_max_num_slider_scoringtimes;
extern ConVar cv_beatmap_mirror;
extern ConVar cv_beatmap_preview_mods_live;
extern ConVar cv_beatmap_preview_music_loop;
extern ConVar cv_beatmap_version;
extern ConVar cv_bug_flicker_log;
extern ConVar cv_circle_color_saturation;
extern ConVar cv_circle_fade_out_scale;
extern ConVar cv_circle_number_rainbow;
extern ConVar cv_circle_rainbow;
extern ConVar cv_circle_shake_duration;
extern ConVar cv_circle_shake_strength;
extern ConVar cv_collections_custom_enabled;
extern ConVar cv_collections_custom_version;
extern ConVar cv_collections_legacy_enabled;
extern ConVar cv_collections_save_immediately;
extern ConVar cv_combo_anim1_duration;
extern ConVar cv_combo_anim1_size;
extern ConVar cv_combo_anim2_duration;
extern ConVar cv_combo_anim2_size;
extern ConVar cv_combobreak_sound_combo;
extern ConVar cv_compensate_music_speed;
extern ConVar cv_confine_cursor_fullscreen;
extern ConVar cv_confine_cursor_windowed;
extern ConVar cv_console_logging;
extern ConVar cv_console_overlay;
extern ConVar cv_console_overlay_lines;
extern ConVar cv_console_overlay_scale;
extern ConVar cv_consolebox_animspeed;
extern ConVar cv_consolebox_draw_helptext;
extern ConVar cv_consolebox_draw_preview;
extern ConVar cv_cs_cap_sanity;
extern ConVar cv_cs_override;
extern ConVar cv_cs_overridenegative;
extern ConVar cv_cursor_alpha;
extern ConVar cv_cursor_expand_duration;
extern ConVar cv_cursor_expand_scale_multiplier;
extern ConVar cv_cursor_ripple_additive;
extern ConVar cv_cursor_ripple_alpha;
extern ConVar cv_cursor_ripple_anim_end_scale;
extern ConVar cv_cursor_ripple_anim_start_fadeout_delay;
extern ConVar cv_cursor_ripple_anim_start_scale;
extern ConVar cv_cursor_ripple_duration;
extern ConVar cv_cursor_ripple_tint_b;
extern ConVar cv_cursor_ripple_tint_g;
extern ConVar cv_cursor_ripple_tint_r;
extern ConVar cv_cursor_scale;
extern ConVar cv_cursor_trail_alpha;
extern ConVar cv_cursor_trail_expand;
extern ConVar cv_cursor_trail_length;
extern ConVar cv_cursor_trail_max_size;
extern ConVar cv_cursor_trail_scale;
extern ConVar cv_cursor_trail_smooth_div;
extern ConVar cv_cursor_trail_smooth_force;
extern ConVar cv_cursor_trail_smooth_length;
extern ConVar cv_cursor_trail_spacing;
extern ConVar cv_database_enabled;
extern ConVar cv_database_ignore_version;
extern ConVar cv_database_ignore_version_warnings;
extern ConVar cv_database_version;
extern ConVar cv_debug;
extern ConVar cv_debug_anim;
extern ConVar cv_debug_box_shadows;
extern ConVar cv_debug_corporeal;
extern ConVar cv_debug_draw_timingpoints;
extern ConVar cv_debug_engine;
extern ConVar cv_debug_env;
extern ConVar cv_debug_file;
extern ConVar cv_debug_hiterrorbar_misaims;
extern ConVar cv_debug_mouse;
extern ConVar cv_debug_pp;
extern ConVar cv_debug_rm;
extern ConVar cv_debug_rt;
extern ConVar cv_debug_shaders;
extern ConVar cv_debug_vprof;
extern ConVar cv_disable_mousebuttons;
extern ConVar cv_disable_mousewheel;
extern ConVar cv_drain_kill;
extern ConVar cv_drain_kill_notification_duration;
extern ConVar cv_draw_accuracy;
extern ConVar cv_draw_approach_circles;
extern ConVar cv_draw_beatmap_background_image;
extern ConVar cv_draw_circles;
extern ConVar cv_draw_combo;
extern ConVar cv_draw_continue;
extern ConVar cv_draw_cursor_ripples;
extern ConVar cv_draw_cursor_trail;
extern ConVar cv_draw_followpoints;
extern ConVar cv_draw_fps;
extern ConVar cv_draw_hiterrorbar;
extern ConVar cv_draw_hiterrorbar_bottom;
extern ConVar cv_draw_hiterrorbar_left;
extern ConVar cv_draw_hiterrorbar_right;
extern ConVar cv_draw_hiterrorbar_top;
extern ConVar cv_draw_hiterrorbar_ur;
extern ConVar cv_draw_hitobjects;
extern ConVar cv_draw_hud;
extern ConVar cv_draw_inputoverlay;
extern ConVar cv_draw_menu_background;
extern ConVar cv_draw_numbers;
extern ConVar cv_draw_playfield_border;
extern ConVar cv_draw_progressbar;
extern ConVar cv_draw_rankingscreen_background_image;
extern ConVar cv_draw_reverse_order;
extern ConVar cv_draw_score;
extern ConVar cv_draw_scorebar;
extern ConVar cv_draw_scorebarbg;
extern ConVar cv_draw_scoreboard;
extern ConVar cv_draw_scoreboard_mp;
extern ConVar cv_draw_scrubbing_timeline;
extern ConVar cv_draw_scrubbing_timeline_breaks;
extern ConVar cv_draw_scrubbing_timeline_strain_graph;
extern ConVar cv_draw_songbrowser_background_image;
extern ConVar cv_draw_songbrowser_menu_background_image;
extern ConVar cv_draw_songbrowser_strain_graph;
extern ConVar cv_draw_songbrowser_thumbnails;
extern ConVar cv_draw_spectator_list;
extern ConVar cv_draw_statistics_ar;
extern ConVar cv_draw_statistics_bpm;
extern ConVar cv_draw_statistics_cs;
extern ConVar cv_draw_statistics_hitdelta;
extern ConVar cv_draw_statistics_hitwindow300;
extern ConVar cv_draw_statistics_hp;
extern ConVar cv_draw_statistics_livestars;
extern ConVar cv_draw_statistics_maxpossiblecombo;
extern ConVar cv_draw_statistics_misses;
extern ConVar cv_draw_statistics_nd;
extern ConVar cv_draw_statistics_nps;
extern ConVar cv_draw_statistics_od;
extern ConVar cv_draw_statistics_perfectpp;
extern ConVar cv_draw_statistics_pp;
extern ConVar cv_draw_statistics_sliderbreaks;
extern ConVar cv_draw_statistics_totalstars;
extern ConVar cv_draw_statistics_ur;
extern ConVar cv_draw_target_heatmap;
extern ConVar cv_early_note_time;
extern ConVar cv_end_delay_time;
extern ConVar cv_end_skip;
extern ConVar cv_end_skip_time;
extern ConVar cv_fail_time;
extern ConVar cv_file_size_max;
extern ConVar cv_flashlight_always_hard;
extern ConVar cv_flashlight_follow_delay;
extern ConVar cv_flashlight_radius;
extern ConVar cv_followpoints_anim;
extern ConVar cv_followpoints_approachtime;
extern ConVar cv_followpoints_clamp;
extern ConVar cv_followpoints_connect_combos;
extern ConVar cv_followpoints_connect_spinners;
extern ConVar cv_followpoints_prevfadetime;
extern ConVar cv_followpoints_scale_multiplier;
extern ConVar cv_followpoints_separation_multiplier;
extern ConVar cv_force_legacy_slider_renderer;
extern ConVar cv_fposu_absolute_mode;
extern ConVar cv_fposu_cube;
extern ConVar cv_fposu_cube_size;
extern ConVar cv_fposu_cube_tint_b;
extern ConVar cv_fposu_cube_tint_g;
extern ConVar cv_fposu_cube_tint_r;
extern ConVar cv_fposu_curved;
extern ConVar cv_fposu_distance;
extern ConVar cv_fposu_draw_cursor_trail;
extern ConVar cv_fposu_draw_scorebarbg_on_top;
extern ConVar cv_fposu_fov;
extern ConVar cv_fposu_invert_horizontal;
extern ConVar cv_fposu_invert_vertical;
extern ConVar cv_fposu_mod_strafing;
extern ConVar cv_fposu_mod_strafing_frequency_x;
extern ConVar cv_fposu_mod_strafing_frequency_y;
extern ConVar cv_fposu_mod_strafing_frequency_z;
extern ConVar cv_fposu_mod_strafing_strength_x;
extern ConVar cv_fposu_mod_strafing_strength_y;
extern ConVar cv_fposu_mod_strafing_strength_z;
extern ConVar cv_fposu_mouse_cm_360;
extern ConVar cv_fposu_mouse_dpi;
extern ConVar cv_fposu_noclip;
extern ConVar cv_fposu_noclipaccelerate;
extern ConVar cv_fposu_noclipfriction;
extern ConVar cv_fposu_noclipspeed;
extern ConVar cv_fposu_playfield_position_x;
extern ConVar cv_fposu_playfield_position_y;
extern ConVar cv_fposu_playfield_position_z;
extern ConVar cv_fposu_playfield_rotation_x;
extern ConVar cv_fposu_playfield_rotation_y;
extern ConVar cv_fposu_playfield_rotation_z;
extern ConVar cv_fposu_skybox;
extern ConVar cv_fposu_transparent_playfield;
extern ConVar cv_fposu_vertical_fov;
extern ConVar cv_fposu_zoom_anim_duration;
extern ConVar cv_fposu_zoom_fov;
extern ConVar cv_fposu_zoom_sensitivity_ratio;
extern ConVar cv_fposu_zoom_toggle;
extern ConVar cv_fps_max;
extern ConVar cv_fps_max_background;
extern ConVar cv_fps_max_background_interleaved;
extern ConVar cv_fps_max_yield;
extern ConVar cv_fps_unlimited;
extern ConVar cv_fps_unlimited_yield;
extern ConVar cv_fullscreen_windowed_borderless;
extern ConVar cv_hiterrorbar_misaims;
extern ConVar cv_hiterrorbar_misses;
extern ConVar cv_hitobject_fade_in_time;
extern ConVar cv_hitobject_fade_out_time;
extern ConVar cv_hitobject_fade_out_time_speed_multiplier_min;
extern ConVar cv_hitobject_hittable_dim;
extern ConVar cv_hitobject_hittable_dim_duration;
extern ConVar cv_hitobject_hittable_dim_start_percent;
extern ConVar cv_hitresult_animated;
extern ConVar cv_hitresult_delta_colorize;
extern ConVar cv_hitresult_delta_colorize_early_b;
extern ConVar cv_hitresult_delta_colorize_early_g;
extern ConVar cv_hitresult_delta_colorize_early_r;
extern ConVar cv_hitresult_delta_colorize_interpolate;
extern ConVar cv_hitresult_delta_colorize_late_b;
extern ConVar cv_hitresult_delta_colorize_late_g;
extern ConVar cv_hitresult_delta_colorize_late_r;
extern ConVar cv_hitresult_delta_colorize_multiplier;
extern ConVar cv_hitresult_draw_300s;
extern ConVar cv_hitresult_duration;
extern ConVar cv_hitresult_duration_max;
extern ConVar cv_hitresult_fadein_duration;
extern ConVar cv_hitresult_fadeout_duration;
extern ConVar cv_hitresult_fadeout_start_time;
extern ConVar cv_hitresult_miss_fadein_scale;
extern ConVar cv_hitresult_scale;
extern ConVar cv_hp_override;
extern ConVar cv_hud_accuracy_scale;
extern ConVar cv_hud_combo_scale;
extern ConVar cv_hud_fps_smoothing;
extern ConVar cv_hud_hiterrorbar_alpha;
extern ConVar cv_hud_hiterrorbar_bar_alpha;
extern ConVar cv_hud_hiterrorbar_bar_height_scale;
extern ConVar cv_hud_hiterrorbar_bar_width_scale;
extern ConVar cv_hud_hiterrorbar_centerline_alpha;
extern ConVar cv_hud_hiterrorbar_centerline_b;
extern ConVar cv_hud_hiterrorbar_centerline_g;
extern ConVar cv_hud_hiterrorbar_centerline_r;
extern ConVar cv_hud_hiterrorbar_entry_100_b;
extern ConVar cv_hud_hiterrorbar_entry_100_g;
extern ConVar cv_hud_hiterrorbar_entry_100_r;
extern ConVar cv_hud_hiterrorbar_entry_300_b;
extern ConVar cv_hud_hiterrorbar_entry_300_g;
extern ConVar cv_hud_hiterrorbar_entry_300_r;
extern ConVar cv_hud_hiterrorbar_entry_50_b;
extern ConVar cv_hud_hiterrorbar_entry_50_g;
extern ConVar cv_hud_hiterrorbar_entry_50_r;
extern ConVar cv_hud_hiterrorbar_entry_additive;
extern ConVar cv_hud_hiterrorbar_entry_alpha;
extern ConVar cv_hud_hiterrorbar_entry_hit_fade_time;
extern ConVar cv_hud_hiterrorbar_entry_miss_b;
extern ConVar cv_hud_hiterrorbar_entry_miss_fade_time;
extern ConVar cv_hud_hiterrorbar_entry_miss_g;
extern ConVar cv_hud_hiterrorbar_entry_miss_r;
extern ConVar cv_hud_hiterrorbar_height_percent;
extern ConVar cv_hud_hiterrorbar_hide_during_spinner;
extern ConVar cv_hud_hiterrorbar_max_entries;
extern ConVar cv_hud_hiterrorbar_offset_bottom_percent;
extern ConVar cv_hud_hiterrorbar_offset_left_percent;
extern ConVar cv_hud_hiterrorbar_offset_percent;
extern ConVar cv_hud_hiterrorbar_offset_right_percent;
extern ConVar cv_hud_hiterrorbar_offset_top_percent;
extern ConVar cv_hud_hiterrorbar_scale;
extern ConVar cv_hud_hiterrorbar_showmisswindow;
extern ConVar cv_hud_hiterrorbar_ur_alpha;
extern ConVar cv_hud_hiterrorbar_ur_offset_x_percent;
extern ConVar cv_hud_hiterrorbar_ur_offset_y_percent;
extern ConVar cv_hud_hiterrorbar_ur_scale;
extern ConVar cv_hud_hiterrorbar_width_percent;
extern ConVar cv_hud_hiterrorbar_width_percent_with_misswindow;
extern ConVar cv_hud_inputoverlay_anim_color_duration;
extern ConVar cv_hud_inputoverlay_anim_scale_duration;
extern ConVar cv_hud_inputoverlay_anim_scale_multiplier;
extern ConVar cv_hud_inputoverlay_offset_x;
extern ConVar cv_hud_inputoverlay_offset_y;
extern ConVar cv_hud_inputoverlay_scale;
extern ConVar cv_hud_playfield_border_size;
extern ConVar cv_hud_progressbar_scale;
extern ConVar cv_hud_scale;
extern ConVar cv_hud_score_scale;
extern ConVar cv_hud_scorebar_hide_anim_duration;
extern ConVar cv_hud_scorebar_hide_during_breaks;
extern ConVar cv_hud_scorebar_scale;
extern ConVar cv_hud_scoreboard_offset_y_percent;
extern ConVar cv_hud_scoreboard_scale;
extern ConVar cv_hud_scoreboard_use_menubuttonbackground;
extern ConVar cv_hud_scrubbing_timeline_hover_tooltip_offset_multiplier;
extern ConVar cv_hud_scrubbing_timeline_strains_aim_color_b;
extern ConVar cv_hud_scrubbing_timeline_strains_aim_color_g;
extern ConVar cv_hud_scrubbing_timeline_strains_aim_color_r;
extern ConVar cv_hud_scrubbing_timeline_strains_alpha;
extern ConVar cv_hud_scrubbing_timeline_strains_height;
extern ConVar cv_hud_scrubbing_timeline_strains_speed_color_b;
extern ConVar cv_hud_scrubbing_timeline_strains_speed_color_g;
extern ConVar cv_hud_scrubbing_timeline_strains_speed_color_r;
extern ConVar cv_hud_shift_tab_toggles_everything;
extern ConVar cv_hud_statistics_ar_offset_x;
extern ConVar cv_hud_statistics_ar_offset_y;
extern ConVar cv_hud_statistics_bpm_offset_x;
extern ConVar cv_hud_statistics_bpm_offset_y;
extern ConVar cv_hud_statistics_cs_offset_x;
extern ConVar cv_hud_statistics_cs_offset_y;
extern ConVar cv_hud_statistics_hitdelta_chunksize;
extern ConVar cv_hud_statistics_hitdelta_offset_x;
extern ConVar cv_hud_statistics_hitdelta_offset_y;
extern ConVar cv_hud_statistics_hitwindow300_offset_x;
extern ConVar cv_hud_statistics_hitwindow300_offset_y;
extern ConVar cv_hud_statistics_hp_offset_x;
extern ConVar cv_hud_statistics_hp_offset_y;
extern ConVar cv_hud_statistics_livestars_offset_x;
extern ConVar cv_hud_statistics_livestars_offset_y;
extern ConVar cv_hud_statistics_maxpossiblecombo_offset_x;
extern ConVar cv_hud_statistics_maxpossiblecombo_offset_y;
extern ConVar cv_hud_statistics_misses_offset_x;
extern ConVar cv_hud_statistics_misses_offset_y;
extern ConVar cv_hud_statistics_nd_offset_x;
extern ConVar cv_hud_statistics_nd_offset_y;
extern ConVar cv_hud_statistics_nps_offset_x;
extern ConVar cv_hud_statistics_nps_offset_y;
extern ConVar cv_hud_statistics_od_offset_x;
extern ConVar cv_hud_statistics_od_offset_y;
extern ConVar cv_hud_statistics_offset_x;
extern ConVar cv_hud_statistics_offset_y;
extern ConVar cv_hud_statistics_perfectpp_offset_x;
extern ConVar cv_hud_statistics_perfectpp_offset_y;
extern ConVar cv_hud_statistics_pp_decimal_places;
extern ConVar cv_hud_statistics_pp_offset_x;
extern ConVar cv_hud_statistics_pp_offset_y;
extern ConVar cv_hud_statistics_scale;
extern ConVar cv_hud_statistics_sliderbreaks_offset_x;
extern ConVar cv_hud_statistics_sliderbreaks_offset_y;
extern ConVar cv_hud_statistics_spacing_scale;
extern ConVar cv_hud_statistics_totalstars_offset_x;
extern ConVar cv_hud_statistics_totalstars_offset_y;
extern ConVar cv_hud_statistics_ur_offset_x;
extern ConVar cv_hud_statistics_ur_offset_y;
extern ConVar cv_hud_volume_duration;
extern ConVar cv_hud_volume_size_multiplier;
extern ConVar cv_ignore_beatmap_combo_colors;
extern ConVar cv_ignore_beatmap_combo_numbers;
extern ConVar cv_ignore_beatmap_sample_volume;
extern ConVar cv_instafade;
extern ConVar cv_instafade_sliders;
extern ConVar cv_instant_replay_duration;
extern ConVar cv_interpolate_music_pos;
extern ConVar cv_letterboxing;
extern ConVar cv_letterboxing_offset_x;
extern ConVar cv_letterboxing_offset_y;
extern ConVar cv_load_beatmap_background_images;
extern ConVar cv_loudness_fallback;
extern ConVar cv_loudness_target;
extern ConVar cv_main_menu_alpha;
extern ConVar cv_main_menu_banner_always_text;
extern ConVar cv_main_menu_banner_ifupdatedfromoldversion_le3300_text;
extern ConVar cv_main_menu_banner_ifupdatedfromoldversion_le3303_text;
extern ConVar cv_main_menu_banner_ifupdatedfromoldversion_text;
extern ConVar cv_main_menu_friend;
extern ConVar cv_main_menu_startup_anim_duration;
extern ConVar cv_main_menu_use_server_logo;
extern ConVar cv_minimize_on_focus_lost_if_borderless_windowed_fullscreen;
extern ConVar cv_minimize_on_focus_lost_if_fullscreen;
extern ConVar cv_mod_hidden;
extern ConVar cv_mod_autoplay;
extern ConVar cv_mod_autopilot;
extern ConVar cv_mod_relax;
extern ConVar cv_mod_spunout;
extern ConVar cv_mod_target;
extern ConVar cv_mod_scorev2;
extern ConVar cv_mod_flashlight;
extern ConVar cv_mod_doubletime;
extern ConVar cv_mod_nofail;
extern ConVar cv_mod_hardrock;
extern ConVar cv_mod_easy;
extern ConVar cv_mod_suddendeath;
extern ConVar cv_mod_perfect;
extern ConVar cv_mod_nightmare;
extern ConVar cv_mod_touchdevice;
extern ConVar cv_mod_touchdevice_always;
extern ConVar cv_mod_actual_flashlight;
extern ConVar cv_mod_approach_different;
extern ConVar cv_mod_approach_different_initial_size;
extern ConVar cv_mod_approach_different_style;
extern ConVar cv_mod_artimewarp;
extern ConVar cv_mod_artimewarp_multiplier;
extern ConVar cv_mod_arwobble;
extern ConVar cv_mod_arwobble_interval;
extern ConVar cv_mod_arwobble_strength;
extern ConVar cv_mod_endless;
extern ConVar cv_mod_fadingcursor;
extern ConVar cv_mod_fadingcursor_combo;
extern ConVar cv_mod_fposu;
extern ConVar cv_mod_fposu_sound_panning;
extern ConVar cv_mod_fps;
extern ConVar cv_mod_fps_sound_panning;
extern ConVar cv_mod_fullalternate;
extern ConVar cv_mod_halfwindow;
extern ConVar cv_mod_halfwindow_allow_300s;
extern ConVar cv_mod_hd_circle_fadein_end_percent;
extern ConVar cv_mod_hd_circle_fadein_start_percent;
extern ConVar cv_mod_hd_circle_fadeout_end_percent;
extern ConVar cv_mod_hd_circle_fadeout_start_percent;
extern ConVar cv_mod_hd_slider_fade_percent;
extern ConVar cv_mod_hd_slider_fast_fade;
extern ConVar cv_mod_jigsaw1;
extern ConVar cv_mod_jigsaw2;
extern ConVar cv_mod_jigsaw_followcircle_radius_factor;
extern ConVar cv_mod_mafham;
extern ConVar cv_mod_mafham_ignore_hittable_dim;
extern ConVar cv_mod_mafham_render_chunksize;
extern ConVar cv_mod_mafham_render_livesize;
extern ConVar cv_mod_millhioref;
extern ConVar cv_mod_millhioref_multiplier;
extern ConVar cv_mod_ming3012;
extern ConVar cv_mod_minimize;
extern ConVar cv_mod_minimize_multiplier;
extern ConVar cv_mod_no100s;
extern ConVar cv_mod_no50s;
extern ConVar cv_mod_reverse_sliders;
extern ConVar cv_mod_shirone;
extern ConVar cv_mod_shirone_combo;
extern ConVar cv_mod_strict_tracking;
extern ConVar cv_mod_strict_tracking_remove_slider_ticks;
extern ConVar cv_mod_suddendeath_restart;
extern ConVar cv_mod_target_100_percent;
extern ConVar cv_mod_target_300_percent;
extern ConVar cv_mod_target_50_percent;
extern ConVar cv_mod_timewarp;
extern ConVar cv_mod_timewarp_multiplier;
extern ConVar cv_mod_wobble;
extern ConVar cv_mod_wobble2;
extern ConVar cv_mod_wobble_frequency;
extern ConVar cv_mod_wobble_rotation_speed;
extern ConVar cv_mod_wobble_strength;
extern ConVar cv_monitor;
extern ConVar cv_mouse_fakelag;
extern ConVar cv_mouse_raw_input;
extern ConVar cv_mouse_raw_input_absolute_to_window;
extern ConVar cv_mouse_sensitivity;
extern ConVar cv_mp_autologin;
extern ConVar cv_mp_password;
extern ConVar cv_mp_server;
extern ConVar cv_name;
extern ConVar cv_nightcore_enjoyer;
extern ConVar cv_normalize_loudness;
extern ConVar cv_notelock_stable_tolerance2b;
extern ConVar cv_notelock_type;
extern ConVar cv_notification;
extern ConVar cv_notification_color_b;
extern ConVar cv_notification_color_g;
extern ConVar cv_notification_color_r;
extern ConVar cv_notification_duration;
extern ConVar cv_number_max;
extern ConVar cv_number_scale_multiplier;
extern ConVar cv_od_override;
extern ConVar cv_od_override_lock;
extern ConVar cv_old_beatmap_offset;
extern ConVar cv_options_high_quality_sliders;
extern ConVar cv_options_save_on_back;
extern ConVar cv_options_slider_preview_use_legacy_renderer;
extern ConVar cv_options_slider_quality;
extern ConVar cv_osu_folder;
extern ConVar cv_osu_folder_sub_skins;
extern ConVar cv_osu_folder_sub_songs;
extern ConVar cv_pause_anim_duration;
extern ConVar cv_pause_dim_alpha;
extern ConVar cv_pause_dim_background;
extern ConVar cv_pause_on_focus_loss;
extern ConVar cv_play_hitsound_on_click_while_playing;
extern ConVar cv_playfield_border_bottom_percent;
extern ConVar cv_playfield_border_top_percent;
extern ConVar cv_playfield_mirror_horizontal;
extern ConVar cv_playfield_mirror_vertical;
extern ConVar cv_playfield_rotation;
extern ConVar cv_pvs;
extern ConVar cv_quick_retry_delay;
extern ConVar cv_quick_retry_time;
extern ConVar cv_r_3dscene_zf;
extern ConVar cv_r_3dscene_zn;
extern ConVar cv_r_debug_disable_3dscene;
extern ConVar cv_r_debug_disable_cliprect;
extern ConVar cv_r_debug_drawimage;
extern ConVar cv_r_debug_drawstring_unbind;
extern ConVar cv_r_debug_flush_drawstring;
extern ConVar cv_r_drawstring_max_string_length;
extern ConVar cv_r_globaloffset_x;
extern ConVar cv_r_globaloffset_y;
extern ConVar cv_r_image_unbind_after_drawimage;
extern ConVar cv_r_opengl_legacy_vao_use_vertex_array;
extern ConVar cv_rankingscreen_pp;
extern ConVar cv_rankingscreen_topbar_height_percent;
extern ConVar cv_relax_offset;
extern ConVar cv_resolution;
extern ConVar cv_resolution_enabled;
extern ConVar cv_resolution_keep_aspect_ratio;
extern ConVar cv_restart_sound_engine_before_playing;
extern ConVar cv_rich_presence;
extern ConVar cv_rich_presence_discord_show_totalpp;
extern ConVar cv_rich_presence_dynamic_windowtitle;
extern ConVar cv_rich_presence_show_recentplaystats;
extern ConVar cv_rm_debug_async_delay;
extern ConVar cv_rm_interrupt_on_destroy;
extern ConVar cv_rm_numthreads;
extern ConVar cv_rm_warnings;
extern ConVar cv_scoreboard_animations;
extern ConVar cv_scores_bonus_pp;
extern ConVar cv_scores_enabled;
extern ConVar cv_scores_save_immediately;
extern ConVar cv_scores_sort_by_pp;
extern ConVar cv_scrubbing_smooth;
extern ConVar cv_sdl_joystick0_deadzone;
extern ConVar cv_sdl_joystick_mouse_sensitivity;
extern ConVar cv_sdl_joystick_zl_threshold;
extern ConVar cv_sdl_joystick_zr_threshold;
extern ConVar cv_seek_delta;
extern ConVar cv_show_approach_circle_on_first_hidden_object;
extern ConVar cv_skin;
extern ConVar cv_skin_animation_force;
extern ConVar cv_skin_animation_fps_override;
extern ConVar cv_skin_async;
extern ConVar cv_skin_color_index_add;
extern ConVar cv_skin_force_hitsound_sample_set;
extern ConVar cv_skin_hd;
extern ConVar cv_skin_mipmaps;
extern ConVar cv_skin_random;
extern ConVar cv_skin_random_elements;
extern ConVar cv_skin_reload;
extern ConVar cv_skin_use_skin_hitsounds;
extern ConVar cv_skip_breaks_enabled;
extern ConVar cv_skip_intro_enabled;
extern ConVar cv_skip_time;
extern ConVar cv_slider_alpha_multiplier;
extern ConVar cv_slider_ball_tint_combo_color;
extern ConVar cv_slider_body_alpha_multiplier;
extern ConVar cv_slider_body_color_saturation;
extern ConVar cv_slider_body_fade_out_time_multiplier;
extern ConVar cv_slider_body_lazer_fadeout_style;
extern ConVar cv_slider_body_smoothsnake;
extern ConVar cv_slider_body_unit_circle_subdivisions;
extern ConVar cv_slider_border_feather;
extern ConVar cv_slider_border_size_multiplier;
extern ConVar cv_slider_border_tint_combo_color;
extern ConVar cv_slider_curve_max_length;
extern ConVar cv_slider_curve_max_points;
extern ConVar cv_slider_curve_points_separation;
extern ConVar cv_slider_debug_draw;
extern ConVar cv_slider_debug_draw_square_vao;
extern ConVar cv_slider_debug_wireframe;
extern ConVar cv_slider_draw_body;
extern ConVar cv_slider_draw_endcircle;
extern ConVar cv_slider_end_inside_check_offset;
extern ConVar cv_slider_end_miss_breaks_combo;
extern ConVar cv_slider_followcircle_fadein_fade_time;
extern ConVar cv_slider_followcircle_fadein_scale;
extern ConVar cv_slider_followcircle_fadein_scale_time;
extern ConVar cv_slider_followcircle_fadeout_fade_time;
extern ConVar cv_slider_followcircle_fadeout_scale;
extern ConVar cv_slider_followcircle_fadeout_scale_time;
extern ConVar cv_slider_followcircle_tick_pulse_scale;
extern ConVar cv_slider_followcircle_tick_pulse_time;
extern ConVar cv_slider_legacy_use_baked_vao;
extern ConVar cv_slider_max_repeats;
extern ConVar cv_slider_max_ticks;
extern ConVar cv_slider_osu_next_style;
extern ConVar cv_slider_rainbow;
extern ConVar cv_slider_reverse_arrow_alpha_multiplier;
extern ConVar cv_slider_reverse_arrow_animated;
extern ConVar cv_slider_reverse_arrow_black_threshold;
extern ConVar cv_slider_reverse_arrow_fadein_duration;
extern ConVar cv_slider_shrink;
extern ConVar cv_slider_sliderhead_fadeout;
extern ConVar cv_slider_snake_duration_multiplier;
extern ConVar cv_slider_use_gradient_image;
extern ConVar cv_snaking_sliders;
extern ConVar cv_snd_async_buffer;
extern ConVar cv_snd_change_check_interval;
extern ConVar cv_snd_dev_buffer;
extern ConVar cv_snd_dev_period;
extern ConVar cv_snd_freq;
extern ConVar cv_snd_output_device;
extern ConVar cv_snd_pitch_hitsounds;
extern ConVar cv_snd_pitch_hitsounds_factor;
extern ConVar cv_snd_play_interp_duration;
extern ConVar cv_snd_play_interp_ratio;
extern ConVar cv_snd_ready_delay;
extern ConVar cv_snd_restart;
extern ConVar cv_snd_restrict_play_frame;
extern ConVar cv_snd_updateperiod;
extern ConVar cv_snd_wav_file_min_size;
extern ConVar cv_songbrowser_background_fade_in_duration;
extern ConVar cv_songbrowser_bottombar_percent;
extern ConVar cv_songbrowser_button_active_color_a;
extern ConVar cv_songbrowser_button_active_color_b;
extern ConVar cv_songbrowser_button_active_color_g;
extern ConVar cv_songbrowser_button_active_color_r;
extern ConVar cv_songbrowser_button_collection_active_color_a;
extern ConVar cv_songbrowser_button_collection_active_color_b;
extern ConVar cv_songbrowser_button_collection_active_color_g;
extern ConVar cv_songbrowser_button_collection_active_color_r;
extern ConVar cv_songbrowser_button_collection_inactive_color_a;
extern ConVar cv_songbrowser_button_collection_inactive_color_b;
extern ConVar cv_songbrowser_button_collection_inactive_color_g;
extern ConVar cv_songbrowser_button_collection_inactive_color_r;
extern ConVar cv_songbrowser_button_difficulty_inactive_color_a;
extern ConVar cv_songbrowser_button_difficulty_inactive_color_b;
extern ConVar cv_songbrowser_button_difficulty_inactive_color_g;
extern ConVar cv_songbrowser_button_difficulty_inactive_color_r;
extern ConVar cv_songbrowser_button_inactive_color_a;
extern ConVar cv_songbrowser_button_inactive_color_b;
extern ConVar cv_songbrowser_button_inactive_color_g;
extern ConVar cv_songbrowser_button_inactive_color_r;
extern ConVar cv_songbrowser_scorebrowser_enabled;
extern ConVar cv_songbrowser_scores_sortingtype;
extern ConVar cv_songbrowser_search_delay;
extern ConVar cv_songbrowser_search_hardcoded_filter;
extern ConVar cv_songbrowser_sortingtype;
extern ConVar cv_songbrowser_thumbnail_delay;
extern ConVar cv_songbrowser_thumbnail_fade_in_duration;
extern ConVar cv_songbrowser_topbar_left_percent;
extern ConVar cv_songbrowser_topbar_left_width_percent;
extern ConVar cv_songbrowser_topbar_middle_width_percent;
extern ConVar cv_songbrowser_topbar_right_height_percent;
extern ConVar cv_songbrowser_topbar_right_percent;
extern ConVar cv_sort_skins_by_name;
extern ConVar cv_sound_panning;
extern ConVar cv_sound_panning_multiplier;
extern ConVar cv_speed_override;
extern ConVar cv_spinner_fade_out_time_multiplier;
extern ConVar cv_spinner_use_ar_fadein;
extern ConVar cv_stars_slider_curve_points_separation;
extern ConVar cv_stars_stacking;
extern ConVar cv_start_first_main_menu_song_at_preview_point;
extern ConVar cv_submit_after_pause;
extern ConVar cv_submit_scores;
extern ConVar cv_tablet_sensitivity_ignore;
extern ConVar cv_timingpoints_force;
extern ConVar cv_timingpoints_offset;
extern ConVar cv_tooltip_anim_duration;
extern ConVar cv_ui_scale;
extern ConVar cv_ui_scale_to_dpi;
extern ConVar cv_ui_scale_to_dpi_minimum_height;
extern ConVar cv_ui_scale_to_dpi_minimum_width;
extern ConVar cv_ui_scrollview_kinetic_approach_time;
extern ConVar cv_ui_scrollview_kinetic_energy_multiplier;
extern ConVar cv_ui_scrollview_mousewheel_multiplier;
extern ConVar cv_ui_scrollview_mousewheel_overscrollbounce;
extern ConVar cv_ui_scrollview_resistance;
extern ConVar cv_ui_scrollview_scrollbarwidth;
extern ConVar cv_ui_textbox_caret_blink_time;
extern ConVar cv_ui_textbox_text_offset_x;
extern ConVar cv_ui_window_animspeed;
extern ConVar cv_ui_window_shadow_radius;
extern ConVar cv_universal_offset;
extern ConVar cv_universal_offset_hardcoded;
extern ConVar cv_use_https;
extern ConVar cv_use_ppv3;
extern ConVar cv_user_draw_accuracy;
extern ConVar cv_user_draw_level;
extern ConVar cv_user_draw_level_bar;
extern ConVar cv_user_draw_pp;
extern ConVar cv_user_include_relax_and_autopilot_for_stats;
extern ConVar cv_version;
extern ConVar cv_volume;
extern ConVar cv_volume_change_interval;
extern ConVar cv_volume_effects;
extern ConVar cv_volume_master;
extern ConVar cv_volume_master_inactive;
extern ConVar cv_volume_music;
extern ConVar cv_vprof;
extern ConVar cv_vprof_display_mode;
extern ConVar cv_vprof_graph;
extern ConVar cv_vprof_graph_alpha;
extern ConVar cv_vprof_graph_draw_overhead;
extern ConVar cv_vprof_graph_height;
extern ConVar cv_vprof_graph_margin;
extern ConVar cv_vprof_graph_range_max;
extern ConVar cv_vprof_graph_width;
extern ConVar cv_vprof_spike;
extern ConVar cv_vs_browser_animspeed;
extern ConVar cv_vs_percent;
extern ConVar cv_vs_repeat;
extern ConVar cv_vs_shuffle;
extern ConVar cv_vs_volume;
extern ConVar cv_vsync;
extern ConVar cv_win_disable_windows_key;
extern ConVar cv_win_disable_windows_key_while_playing;
extern ConVar cv_win_ink_workaround;
extern ConVar cv_win_mouse_raw_input_buffer;
extern ConVar cv_win_processpriority;
extern ConVar cv_win_realtimestylus;
extern ConVar cv_win_snd_wasapi_buffer_size;
extern ConVar cv_win_snd_wasapi_exclusive;
extern ConVar cv_win_snd_wasapi_period_size;
