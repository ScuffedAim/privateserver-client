#include "Osu.h"

#include <sstream>

#include "AnimationHandler.h"
#include "BackgroundImageHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "Beatmap.h"
#include "CBaseUIScrollView.h"
#include "CBaseUISlider.h"
#include "CBaseUITextbox.h"
#include "CWindowManager.h"
#include "Changelog.h"
#include "Chat.h"
#include "ConVar.h"
#include "Console.h"
#include "ConsoleBox.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Downloader.h"
#include "Engine.h"
#include "Environment.h"
#include "GameRules.h"
#include "HUD.h"
#include "HitObject.h"
#include "Icons.h"
#include "KeyBindings.h"
#include "Keyboard.h"
#include "LegacyReplay.h"
#include "Lobby.h"
#include "MainMenu.h"
#include "ModFPoSu.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "PauseMenu.h"
#include "PeppyImporter.h"
#include "PromptScreen.h"
#include "RankingScreen.h"
#include "RenderTarget.h"
#include "ResourceManager.h"
#include "RichPresence.h"
#include "RoomScreen.h"
#include "Shader.h"
#include "Skin.h"
#include "SongBrowser/LeaderboardPPCalcThread.h"
#include "SongBrowser/LoudnessCalcThread.h"
#include "SongBrowser/MapCalcThread.h"
#include "SongBrowser/ScoreConverterThread.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "SpectatorScreen.h"
#include "TooltipOverlay.h"
#include "UIModSelectorModButton.h"
#include "UIUserContextMenu.h"
#include "UpdateHandler.h"
#include "UserCard.h"
#include "VolumeOverlay.h"
#include "score.h"

#ifdef _WIN32
#include "WinEnvironment.h"
#endif

using namespace std;

Osu *osu = NULL;

Vector2 Osu::g_vInternalResolution;
Vector2 Osu::osuBaseResolution = Vector2(640.0f, 480.0f);

Shader *actual_flashlight_shader = NULL;
Shader *flashlight_shader = NULL;

Osu::Osu() {
    osu = this;

    srand(time(NULL));

    bancho.neosu_version = UString::format("%.2f-" NEOSU_STREAM, cv_version.getFloat());
    bancho.user_agent = UString::format("Mozilla/5.0 (compatible; neosu/%s; +https://" NEOSU_DOMAIN "/)",
                                        bancho.neosu_version.toUtf8());

    // experimental mods list
    this->experimentalMods.push_back(&cv_fposu_mod_strafing);
    this->experimentalMods.push_back(&cv_mod_wobble);
    this->experimentalMods.push_back(&cv_mod_arwobble);
    this->experimentalMods.push_back(&cv_mod_timewarp);
    this->experimentalMods.push_back(&cv_mod_artimewarp);
    this->experimentalMods.push_back(&cv_mod_minimize);
    this->experimentalMods.push_back(&cv_mod_fadingcursor);
    this->experimentalMods.push_back(&cv_mod_fps);
    this->experimentalMods.push_back(&cv_mod_jigsaw1);
    this->experimentalMods.push_back(&cv_mod_jigsaw2);
    this->experimentalMods.push_back(&cv_mod_fullalternate);
    this->experimentalMods.push_back(&cv_mod_reverse_sliders);
    this->experimentalMods.push_back(&cv_mod_no50s);
    this->experimentalMods.push_back(&cv_mod_no100s);
    this->experimentalMods.push_back(&cv_mod_ming3012);
    this->experimentalMods.push_back(&cv_mod_halfwindow);
    this->experimentalMods.push_back(&cv_mod_millhioref);
    this->experimentalMods.push_back(&cv_mod_mafham);
    this->experimentalMods.push_back(&cv_mod_strict_tracking);
    this->experimentalMods.push_back(&cv_playfield_mirror_horizontal);
    this->experimentalMods.push_back(&cv_playfield_mirror_vertical);
    this->experimentalMods.push_back(&cv_mod_wobble2);
    this->experimentalMods.push_back(&cv_mod_shirone);
    this->experimentalMods.push_back(&cv_mod_approach_different);

    env->setWindowTitle("ScuffedAim");
    env->setCursorVisible(false);

    engine->getConsoleBox()->setRequireShiftToActivate(true);
    engine->getMouse()->addListener(this);

    cv_vsync.setValue(0.0f);
    cv_fps_max.setValue(420.0f);
    cv_fps_max.setDefaultFloat(420.0f);

    cv_snd_change_check_interval.setDefaultFloat(0.5f);
    cv_snd_change_check_interval.setValue(cv_snd_change_check_interval.getDefaultFloat());

    env->setWindowResizable(false);

    // convar callbacks
    cv_resolution.setValue(UString::format("%ix%i", engine->getScreenWidth(), engine->getScreenHeight()));
    cv_resolution.setCallback(fastdelegate::MakeDelegate(this, &Osu::onInternalResolutionChanged));
    cv_windowed_resolution.setCallback(fastdelegate::MakeDelegate(this, &Osu::onWindowedResolutionChanged));
    cv_animation_speed_override.setCallback(fastdelegate::MakeDelegate(this, &Osu::onAnimationSpeedChange));
    cv_ui_scale.setCallback(fastdelegate::MakeDelegate(this, &Osu::onUIScaleChange));
    cv_ui_scale_to_dpi.setCallback(fastdelegate::MakeDelegate(this, &Osu::onUIScaleToDPIChange));
    cv_letterboxing.setCallback(fastdelegate::MakeDelegate(this, &Osu::onLetterboxingChange));
    cv_letterboxing_offset_x.setCallback(fastdelegate::MakeDelegate(this, &Osu::onLetterboxingOffsetChange));
    cv_letterboxing_offset_y.setCallback(fastdelegate::MakeDelegate(this, &Osu::onLetterboxingOffsetChange));
    cv_confine_cursor_windowed.setCallback(fastdelegate::MakeDelegate(this, &Osu::onConfineCursorWindowedChange));
    cv_confine_cursor_fullscreen.setCallback(fastdelegate::MakeDelegate(this, &Osu::onConfineCursorFullscreenChange));

    // vars
    this->skin = NULL;
    this->backgroundImageHandler = NULL;
    this->modSelector = NULL;
    this->updateHandler = NULL;

    this->bF1 = false;
    this->bUIToggleCheck = false;
    this->bScoreboardToggleCheck = false;
    this->bEscape = false;
    this->bKeyboardKey1Down = false;
    this->bKeyboardKey12Down = false;
    this->bKeyboardKey2Down = false;
    this->bKeyboardKey22Down = false;
    this->bMouseKey1Down = false;
    this->bMouseKey2Down = false;
    this->bSkipScheduled = false;
    this->bQuickRetryDown = false;
    this->fQuickRetryTime = 0.0f;
    this->bSeeking = false;
    this->bSeekKey = false;
    this->fPrevSeekMousePosX = -1.0f;
    this->fQuickSaveTime = 0.0f;

    this->bToggleModSelectionScheduled = false;
    this->bToggleSongBrowserScheduled = false;
    this->bToggleOptionsMenuScheduled = false;
    this->bOptionsMenuFullscreen = true;
    this->bToggleChangelogScheduled = false;
    this->bToggleEditorScheduled = false;

    this->bShouldCursorBeVisible = false;

    this->bScheduleEndlessModNextBeatmap = false;
    this->iMultiplayerClientNumEscPresses = 0;
    this->bWasBossKeyPaused = false;
    this->bSkinLoadScheduled = false;
    this->bSkinLoadWasReload = false;
    this->skinScheduledToLoad = NULL;
    this->bFontReloadScheduled = false;
    this->bFireResolutionChangedScheduled = false;
    this->bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled = false;

    // debug
    this->windowManager = new CWindowManager();

    // renderer
    g_vInternalResolution = engine->getScreenSize();

    this->backBuffer =
        engine->getResourceManager()->createRenderTarget(0, 0, this->getScreenWidth(), this->getScreenHeight());
    this->playfieldBuffer = engine->getResourceManager()->createRenderTarget(0, 0, 64, 64);
    this->sliderFrameBuffer =
        engine->getResourceManager()->createRenderTarget(0, 0, this->getScreenWidth(), this->getScreenHeight());
    this->frameBuffer = engine->getResourceManager()->createRenderTarget(0, 0, 64, 64);
    this->frameBuffer2 = engine->getResourceManager()->createRenderTarget(0, 0, 64, 64);

    // load a few select subsystems very early
    this->notificationOverlay = new NotificationOverlay();
    this->score = new LiveScore(false);
    this->updateHandler = new UpdateHandler();

    // load main menu icon before skin
    engine->getResourceManager()->loadImage("neosu.png", "NEOSU_LOGO");

    // exec the main config file (this must be right here!)
    Console::execConfigFile("underride");  // same as override, but for defaults
    Console::execConfigFile("osu");
    Console::execConfigFile("override");  // used for quickfixing live builds without redeploying/recompiling

    // clear screen in case cfg switched to fullscreen mode
    // (loading the rest of the app can take a bit of time)
    engine->onPaint();

    std::ifstream osuCfgFile(MCENGINE_DATA_DIR "cfg/osu.cfg");
    if(!osuCfgFile.good()) {
        import_settings_from_osu_stable();
    }

    // Initialize sound here so we can load the preferred device from config
    // Avoids initializing the sound device twice, which can take a while depending on the driver
    auto sound_engine = engine->getSound();
    sound_engine->updateOutputDevices(true);
    sound_engine->initializeOutputDevice(sound_engine->getWantedDevice());
    cv_snd_output_device.setValue(sound_engine->getOutputDeviceName());
    cv_snd_freq.setCallback(fastdelegate::MakeDelegate(sound_engine, &SoundEngine::onFreqChanged));
    cv_snd_restart.setCallback(fastdelegate::MakeDelegate(sound_engine, &SoundEngine::restart));
    cv_win_snd_wasapi_exclusive.setCallback(_RESTART_SOUND_ENGINE_ON_CHANGE);
    cv_win_snd_wasapi_buffer_size.setCallback(_RESTART_SOUND_ENGINE_ON_CHANGE);
    cv_win_snd_wasapi_period_size.setCallback(_RESTART_SOUND_ENGINE_ON_CHANGE);
    cv_asio_buffer_size.setCallback(_RESTART_SOUND_ENGINE_ON_CHANGE);

    // Initialize skin after sound engine has started, or else sounds won't load properly
    cv_skin.setCallback(fastdelegate::MakeDelegate(this, &Osu::onSkinChange));
    cv_skin_reload.setCallback(fastdelegate::MakeDelegate(this, &Osu::onSkinReload));
    this->onSkinChange("", cv_skin.getString());

    // Convar callbacks that should be set after loading the config
    cv_mod_mafham.setCallback(fastdelegate::MakeDelegate(this, &Osu::onModMafhamChange));
    cv_mod_fposu.setCallback(fastdelegate::MakeDelegate(this, &Osu::onModFPoSuChange));
    cv_playfield_mirror_horizontal.setCallback(fastdelegate::MakeDelegate(this, &Osu::updateModsForConVarTemplate));
    cv_playfield_mirror_vertical.setCallback(fastdelegate::MakeDelegate(this, &Osu::updateModsForConVarTemplate));
    cv_playfield_rotation.setCallback(fastdelegate::MakeDelegate(this, &Osu::onPlayfieldChange));
    cv_speed_override.setCallback(fastdelegate::MakeDelegate(this, &Osu::onSpeedChange));
    cv_mod_doubletime_dummy.setCallback(fastdelegate::MakeDelegate(this, &Osu::onDTPresetChange));
    cv_mod_halftime_dummy.setCallback(fastdelegate::MakeDelegate(this, &Osu::onHTPresetChange));
    cv_draw_songbrowser_thumbnails.setCallback(fastdelegate::MakeDelegate(this, &Osu::onThumbnailsToggle));

    // load global resources
    const int baseDPI = 96;
    const int newDPI = Osu::getUIScale() * baseDPI;

    McFont *defaultFont =
        engine->getResourceManager()->loadFont("weblysleekuisb.ttf", "FONT_DEFAULT", 15, true, newDPI);
    this->titleFont =
        engine->getResourceManager()->loadFont("SourceSansPro-Semibold.otf", "FONT_OSU_TITLE", 60, true, newDPI);
    this->subTitleFont =
        engine->getResourceManager()->loadFont("SourceSansPro-Semibold.otf", "FONT_OSU_SUBTITLE", 21, true, newDPI);
    this->songBrowserFont =
        engine->getResourceManager()->loadFont("SourceSansPro-Regular.otf", "FONT_OSU_SONGBROWSER", 35, true, newDPI);
    this->songBrowserFontBold =
        engine->getResourceManager()->loadFont("SourceSansPro-Bold.otf", "FONT_OSU_SONGBROWSER_BOLD", 30, true, newDPI);
    this->fontIcons = engine->getResourceManager()->loadFont("fontawesome-webfont.ttf", "FONT_OSU_ICONS", Icons::icons,
                                                             26, true, newDPI);
    this->fonts.push_back(defaultFont);
    this->fonts.push_back(this->titleFont);
    this->fonts.push_back(this->subTitleFont);
    this->fonts.push_back(this->songBrowserFont);
    this->fonts.push_back(this->songBrowserFontBold);
    this->fonts.push_back(this->fontIcons);

    float averageIconHeight = 0.0f;
    for(int i = 0; i < Icons::icons.size(); i++) {
        UString iconString;
        iconString.insert(0, Icons::icons[i]);
        const float height = this->fontIcons->getStringHeight(iconString);
        if(height > averageIconHeight) averageIconHeight = height;
    }
    this->fontIcons->setHeight(averageIconHeight);

    if(defaultFont->getDPI() != newDPI) {
        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }

    // load skin
    {
        UString skinFolder = cv_osu_folder.getString();
        skinFolder.append("/");
        skinFolder.append(cv_osu_folder_sub_skins.getString());
        skinFolder.append(cv_skin.getString());
        skinFolder.append("/");
        if(this->skin == NULL)  // the skin may already be loaded by Console::execConfigFile() above
            this->onSkinChange("", cv_skin.getString());

        // enable async skin loading for user-action skin changes (but not during startup)
        cv_skin_async.setValue(1.0f);
    }

    // load subsystems, add them to the screens array
    this->songBrowser2 = new SongBrowser();
    this->volumeOverlay = new VolumeOverlay();
    this->tooltipOverlay = new TooltipOverlay();
    this->optionsMenu = new OptionsMenu();
    this->mainMenu = new MainMenu();  // has to be after options menu
    this->backgroundImageHandler = new BackgroundImageHandler();
    this->modSelector = new ModSelector();
    this->rankingScreen = new RankingScreen();
    this->pauseMenu = new PauseMenu();
    this->hud = new HUD();
    this->changelog = new Changelog();
    this->fposu = new ModFPoSu();
    this->chat = new Chat();
    this->lobby = new Lobby();
    this->room = new RoomScreen();
    this->prompt = new PromptScreen();
    this->user_actions = new UIUserContextMenuScreen();
    this->spectatorScreen = new SpectatorScreen();

    this->userButton = new UserCard(bancho.user_id);

    // the order in this vector will define in which order events are handled/consumed
    this->screens.push_back(this->volumeOverlay);
    this->screens.push_back(this->prompt);
    this->screens.push_back(this->modSelector);
    this->screens.push_back(this->user_actions);
    this->screens.push_back(this->room);
    this->screens.push_back(this->chat);
    this->screens.push_back(this->notificationOverlay);
    this->screens.push_back(this->optionsMenu);
    this->screens.push_back(this->rankingScreen);
    this->screens.push_back(this->spectatorScreen);
    this->screens.push_back(this->pauseMenu);
    this->screens.push_back(this->hud);
    this->screens.push_back(this->songBrowser2);
    this->screens.push_back(this->lobby);
    this->screens.push_back(this->changelog);
    this->screens.push_back(this->mainMenu);
    this->screens.push_back(this->tooltipOverlay);
    this->mainMenu->setVisible(true);

    // update mod settings
    this->updateMods();

    // Init online functionality (multiplayer/leaderboards/etc)
    init_networking_thread();
    if(cv_mp_autologin.getBool()) {
        reconnect();
    }

    this->updateHandler->checkForUpdates();

#ifdef _WIN32
    // Process cmdline args now, after everything has been initialized
    for(i32 i = 0; i < engine->iArgc; i++) {
        handle_cmdline_args(engine->sArgv[i]);
    }
#endif

    // Not the type of shader you want players to tweak or delete, so loading from string
    actual_flashlight_shader = engine->getGraphics()->createShaderFromSource(
        "#version 110\n"
        "varying vec2 tex_coord;\n"
        "void main() {\n"
        "    gl_Position = gl_ModelViewProjectionMatrix * vec4(gl_Vertex.x, gl_Vertex.y, 0.0, 1.0);\n"
        "    gl_FrontColor = gl_Color;\n"
        "    tex_coord = gl_MultiTexCoord0.xy;\n"
        "}",
        "#version 110\n"
        "uniform float max_opacity;\n"
        "uniform float flashlight_radius;\n"
        "uniform vec2 flashlight_center;\n"
        "void main(void) {\n"
        "    float dist = distance(flashlight_center, gl_FragCoord.xy);\n"
        "    float opacity = smoothstep(flashlight_radius, flashlight_radius * 1.4, dist);\n"
        "    opacity = 1.0 - min(opacity, max_opacity);\n"
        "    gl_FragColor = vec4(1.0, 1.0, 0.9, opacity);\n"
        "}");
    flashlight_shader = engine->getGraphics()->createShaderFromSource(
        "#version 110\n"
        "varying vec2 tex_coord;\n"
        "void main() {\n"
        "    gl_Position = gl_ModelViewProjectionMatrix * vec4(gl_Vertex.x, gl_Vertex.y, 0.0, 1.0);\n"
        "    gl_FrontColor = gl_Color;\n"
        "    tex_coord = gl_MultiTexCoord0.xy;\n"
        "}",
        "#version 110\n"
        "uniform float max_opacity;\n"
        "uniform float flashlight_radius;\n"
        "uniform vec2 flashlight_center;\n"
        "void main(void) {\n"
        "    float dist = distance(flashlight_center, gl_FragCoord.xy);\n"
        "    float opacity = 1.0 - smoothstep(flashlight_radius, flashlight_radius * 1.4, dist);\n"
        "    opacity = 1.0 - min(opacity, max_opacity);\n"
        "    gl_FragColor = vec4(0.0, 0.0, 0.0, opacity);\n"
        "}");
    engine->getResourceManager()->loadResource(actual_flashlight_shader);
    engine->getResourceManager()->loadResource(flashlight_shader);
}

Osu::~Osu() {
    sct_abort();
    lct_set_map(NULL);
    loct_abort();
    mct_abort();
    kill_networking_thread();

    // "leak" UpdateHandler object, but not relevant since shutdown:
    // this is the only way of handling instant user shutdown requests properly, there is no solution for active working
    // threads besides letting the OS kill them when the main threads exits. we must not delete the update handler
    // object, because the thread is potentially still accessing members during shutdown
    this->updateHandler->stop();  // tell it to stop at the next cancellation point, depending on the OS/runtime and
                                  // engine shutdown time it may get killed before that

    SAFE_DELETE(this->windowManager);

    for(int i = 0; i < this->screens.size(); i++) {
        SAFE_DELETE(this->screens[i]);
    }

    SAFE_DELETE(this->fposu);
    SAFE_DELETE(this->score);
    SAFE_DELETE(this->skin);
    SAFE_DELETE(this->backgroundImageHandler);

    osu = NULL;
}

void Osu::draw(Graphics *g) {
    if(this->skin == NULL || flashlight_shader == NULL)  // sanity check
    {
        g->setColor(0xff000000);
        g->fillRect(0, 0, this->getScreenWidth(), this->getScreenHeight());
        return;
    }

    // if we are not using the native window resolution, draw into the buffer
    const bool isBufferedDraw = cv_resolution_enabled.getBool();

    if(isBufferedDraw) this->backBuffer->enable();

    f32 fadingCursorAlpha = 1.f;

    // draw everything in the correct order
    if(this->isInPlayMode()) {  // if we are playing a beatmap
        Beatmap *beatmap = this->getSelectedBeatmap();
        const bool isFPoSu = (cv_mod_fposu.getBool());

        if(isFPoSu) this->playfieldBuffer->enable();

        this->getSelectedBeatmap()->draw(g);

        auto actual_flashlight_enabled = cv_mod_actual_flashlight.getBool();
        if(cv_mod_flashlight.getBool() || actual_flashlight_enabled) {
            // Convert screen mouse -> osu mouse pos
            Vector2 cursorPos = beatmap->getCursorPos();
            Vector2 mouse_position = cursorPos - GameRules::getPlayfieldOffset();
            mouse_position /= GameRules::getPlayfieldScaleFactor();

            // Update flashlight position
            double follow_delay = cv_flashlight_follow_delay.getFloat();
            double frame_time = min(engine->getFrameTime(), follow_delay);
            double t = frame_time / follow_delay;
            t = t * (2.f - t);
            this->flashlight_position += t * (mouse_position - this->flashlight_position);
            Vector2 flashlightPos =
                this->flashlight_position * GameRules::getPlayfieldScaleFactor() + GameRules::getPlayfieldOffset();

            float base_fl_radius = cv_flashlight_radius.getFloat() * GameRules::getPlayfieldScaleFactor();
            float anti_fl_radius = base_fl_radius * 0.625f;
            float fl_radius = base_fl_radius;
            if(this->getScore()->getCombo() >= 200 || cv_flashlight_always_hard.getBool()) {
                anti_fl_radius = base_fl_radius;
                fl_radius *= 0.625f;
            } else if(this->getScore()->getCombo() >= 100) {
                anti_fl_radius = base_fl_radius * 0.8125f;
                fl_radius *= 0.8125f;
            }

            if(cv_mod_flashlight.getBool()) {
                // Dim screen when holding a slider
                float opacity = 1.f;
                if(this->getSelectedBeatmap()->holding_slider && !cv_avoid_flashes.getBool()) {
                    opacity = 0.2f;
                }

                flashlight_shader->enable();
                flashlight_shader->setUniform1f("max_opacity", opacity);
                flashlight_shader->setUniform1f("flashlight_radius", fl_radius);
                flashlight_shader->setUniform2f("flashlight_center", flashlightPos.x,
                                                this->getScreenSize().y - flashlightPos.y);

                g->setColor(COLOR(255, 0, 0, 0));
                g->fillRect(0, 0, this->getScreenWidth(), this->getScreenHeight());

                flashlight_shader->disable();
            }
            if(actual_flashlight_enabled) {
                // Brighten screen when holding a slider
                float opacity = 1.f;
                if(this->getSelectedBeatmap()->holding_slider && !cv_avoid_flashes.getBool()) {
                    opacity = 0.8f;
                }

                actual_flashlight_shader->enable();
                actual_flashlight_shader->setUniform1f("max_opacity", opacity);
                actual_flashlight_shader->setUniform1f("flashlight_radius", anti_fl_radius);
                actual_flashlight_shader->setUniform2f("flashlight_center", flashlightPos.x,
                                                       this->getScreenSize().y - flashlightPos.y);

                g->setColor(COLOR(255, 0, 0, 0));
                g->fillRect(0, 0, this->getScreenWidth(), this->getScreenHeight());

                actual_flashlight_shader->disable();
            }
        }

        if(!isFPoSu) this->hud->draw(g);

        // quick retry fadeout overlay
        if(this->fQuickRetryTime != 0.0f && this->bQuickRetryDown) {
            float alphaPercent = 1.0f - (this->fQuickRetryTime - engine->getTime()) / cv_quick_retry_delay.getFloat();
            if(engine->getTime() > this->fQuickRetryTime) alphaPercent = 1.0f;

            g->setColor(COLOR((int)(255 * alphaPercent), 0, 0, 0));
            g->fillRect(0, 0, this->getScreenWidth(), this->getScreenHeight());
        }

        this->pauseMenu->draw(g);
        this->modSelector->draw(g);
        this->chat->draw(g);
        this->optionsMenu->draw(g);

        if(cv_draw_fps.getBool() && (!isFPoSu)) this->hud->drawFps(g);

        this->windowManager->draw(g);

        if(isFPoSu && cv_draw_cursor_ripples.getBool()) this->hud->drawCursorRipples(g);

        // draw FPoSu cursor trail
        fadingCursorAlpha =
            1.0f - clamp<float>((float)this->score->getCombo() / cv_mod_fadingcursor_combo.getFloat(), 0.0f, 1.0f);
        if(this->pauseMenu->isVisible() || this->getSelectedBeatmap()->isContinueScheduled() ||
           !cv_mod_fadingcursor.getBool())
            fadingCursorAlpha = 1.0f;
        if(isFPoSu && cv_fposu_draw_cursor_trail.getBool())
            this->hud->drawCursorTrail(g, beatmap->getCursorPos(), fadingCursorAlpha);

        if(isFPoSu) {
            this->playfieldBuffer->disable();
            this->fposu->draw(g);
            this->hud->draw(g);

            if(cv_draw_fps.getBool()) this->hud->drawFps(g);
        }
    } else {  // if we are not playing
        this->spectatorScreen->draw(g);

        this->lobby->draw(g);
        this->room->draw(g);

        if(this->songBrowser2 != NULL) this->songBrowser2->draw(g);

        this->mainMenu->draw(g);
        this->changelog->draw(g);
        this->rankingScreen->draw(g);
        this->chat->draw(g);
        this->user_actions->draw(g);
        this->optionsMenu->draw(g);
        this->modSelector->draw(g);
        this->prompt->draw(g);

        if(cv_draw_fps.getBool()) this->hud->drawFps(g);

        this->windowManager->draw(g);
    }

    this->tooltipOverlay->draw(g);
    this->notificationOverlay->draw(g);
    this->volumeOverlay->draw(g);

    // loading spinner for some async tasks
    if((this->bSkinLoadScheduled && this->skin != this->skinScheduledToLoad)) {
        this->hud->drawLoadingSmall(g, "");
    }

    // draw cursor
    if(this->isInPlayMode()) {
        Beatmap *beatmap = this->getSelectedBeatmap();
        Vector2 cursorPos = beatmap->getCursorPos();
        bool drawSecondTrail =
            (cv_mod_autoplay.getBool() || cv_mod_autopilot.getBool() || beatmap->is_watching || beatmap->is_spectating);
        bool updateAndDrawTrail = true;
        if(cv_mod_fposu.getBool()) {
            cursorPos = this->getScreenSize() / 2.0f;
            updateAndDrawTrail = false;
        }
        this->hud->drawCursor(g, cursorPos, fadingCursorAlpha, drawSecondTrail, updateAndDrawTrail);
    } else {
        this->hud->drawCursor(g, engine->getMouse()->getPos());
    }

    // if we are not using the native window resolution
    if(isBufferedDraw) {
        // draw a scaled version from the buffer to the screen
        this->backBuffer->disable();

        Vector2 offset = Vector2(engine->getGraphics()->getResolution().x / 2 - g_vInternalResolution.x / 2,
                                 engine->getGraphics()->getResolution().y / 2 - g_vInternalResolution.y / 2);
        g->setBlending(false);
        if(cv_letterboxing.getBool()) {
            this->backBuffer->draw(g, offset.x * (1.0f + cv_letterboxing_offset_x.getFloat()),
                                   offset.y * (1.0f + cv_letterboxing_offset_y.getFloat()), g_vInternalResolution.x,
                                   g_vInternalResolution.y);
        } else {
            if(cv_resolution_keep_aspect_ratio.getBool()) {
                const float scale =
                    getImageScaleToFitResolution(this->backBuffer->getSize(), engine->getGraphics()->getResolution());
                const float scaledWidth = this->backBuffer->getWidth() * scale;
                const float scaledHeight = this->backBuffer->getHeight() * scale;
                this->backBuffer->draw(
                    g,
                    max(0.0f, engine->getGraphics()->getResolution().x / 2.0f - scaledWidth / 2.0f) *
                        (1.0f + cv_letterboxing_offset_x.getFloat()),
                    max(0.0f, engine->getGraphics()->getResolution().y / 2.0f - scaledHeight / 2.0f) *
                        (1.0f + cv_letterboxing_offset_y.getFloat()),
                    scaledWidth, scaledHeight);
            } else {
                this->backBuffer->draw(g, 0, 0, engine->getGraphics()->getResolution().x,
                                       engine->getGraphics()->getResolution().y);
            }
        }
        g->setBlending(true);
    }
}

void Osu::update() {
    if(this->skin != NULL) this->skin->update();

    if(this->isInPlayMode() && cv_mod_fposu.getBool()) this->fposu->update();

    bool propagate_clicks = true;
    for(int i = 0; i < this->screens.size() && propagate_clicks; i++) {
        this->screens[i]->mouse_update(&propagate_clicks);
    }

    if(this->music_unpause_scheduled && engine->getSound()->isReady()) {
        if(this->getSelectedBeatmap()->getMusic() != NULL) {
            engine->getSound()->play(this->getSelectedBeatmap()->getMusic());
        }
        this->music_unpause_scheduled = false;
    }

    // main beatmap update
    this->bSeeking = false;
    if(this->isInPlayMode()) {
        this->getSelectedBeatmap()->update();

        // NOTE: force keep loaded background images while playing
        this->backgroundImageHandler->scheduleFreezeCache();

        // skip button clicking
        bool can_skip = this->getSelectedBeatmap()->isInSkippableSection() && !this->bClickedSkipButton;
        can_skip &= !this->getSelectedBeatmap()->isPaused() && !this->volumeOverlay->isBusy();
        if(can_skip) {
            const bool isAnyOsuKeyDown =
                (this->bKeyboardKey1Down || this->bKeyboardKey12Down || this->bKeyboardKey2Down ||
                 this->bKeyboardKey22Down || this->bMouseKey1Down || this->bMouseKey2Down);
            const bool isAnyKeyDown = (isAnyOsuKeyDown || engine->getMouse()->isLeftDown());

            if(isAnyKeyDown) {
                if(this->hud->getSkipClickRect().contains(engine->getMouse()->getPos())) {
                    if(!this->bSkipScheduled) {
                        this->bSkipScheduled = true;
                        this->bClickedSkipButton = true;

                        if(bancho.is_playing_a_multi_map()) {
                            Packet packet;
                            packet.id = MATCH_SKIP_REQUEST;
                            send_packet(packet);
                        }
                    }
                }
            }
        }

        // skipping
        if(this->bSkipScheduled) {
            const bool isLoading = this->getSelectedBeatmap()->isLoading();

            if(this->getSelectedBeatmap()->isInSkippableSection() && !this->getSelectedBeatmap()->isPaused() &&
               !isLoading) {
                bool can_skip_intro =
                    (cv_skip_intro_enabled.getBool() && this->getSelectedBeatmap()->iCurrentHitObjectIndex < 1);
                bool can_skip_break =
                    (cv_skip_breaks_enabled.getBool() && this->getSelectedBeatmap()->iCurrentHitObjectIndex > 0);
                if(bancho.is_playing_a_multi_map()) {
                    can_skip_intro = bancho.room.all_players_skipped;
                    can_skip_break = false;
                }

                if(can_skip_intro || can_skip_break) {
                    this->getSelectedBeatmap()->skipEmptySection();
                }
            }

            if(!isLoading) this->bSkipScheduled = false;
        }

        // Reset m_bClickedSkipButton on mouse up
        // We only use m_bClickedSkipButton to prevent seeking when clicking the skip button
        if(this->bClickedSkipButton && !this->getSelectedBeatmap()->isInSkippableSection()) {
            if(!engine->getMouse()->isLeftDown()) {
                this->bClickedSkipButton = false;
            }
        }

        // scrubbing/seeking
        this->bSeeking = (this->bSeekKey || this->getSelectedBeatmap()->is_watching);
        this->bSeeking &= !this->getSelectedBeatmap()->isPaused() && !this->volumeOverlay->isBusy();
        this->bSeeking &= !bancho.is_playing_a_multi_map() && !this->bClickedSkipButton;
        this->bSeeking &= !this->getSelectedBeatmap()->is_spectating;
        if(this->bSeeking) {
            const float mousePosX = (int)engine->getMouse()->getPos().x;
            const float percent = clamp<float>(mousePosX / (float)this->getScreenWidth(), 0.0f, 1.0f);

            if(engine->getMouse()->isLeftDown()) {
                if(mousePosX != this->fPrevSeekMousePosX || !cv_scrubbing_smooth.getBool()) {
                    this->fPrevSeekMousePosX = mousePosX;

                    // special case: allow cancelling the failing animation here
                    if(this->getSelectedBeatmap()->hasFailed()) this->getSelectedBeatmap()->cancelFailing();

                    this->getSelectedBeatmap()->seekPercentPlayable(percent);
                } else {
                    // special case: keep player invulnerable even if scrubbing position does not change
                    this->getSelectedBeatmap()->resetScore();
                }
            } else {
                this->fPrevSeekMousePosX = -1.0f;
            }

            if(engine->getMouse()->isRightDown()) {
                this->fQuickSaveTime = clamp<float>((float)((this->getSelectedBeatmap()->getStartTimePlayable() +
                                                             this->getSelectedBeatmap()->getLengthPlayable()) *
                                                            percent) /
                                                        (float)this->getSelectedBeatmap()->getLength(),
                                                    0.0f, 1.0f);
            }
        }

        // quick retry timer
        if(this->bQuickRetryDown && this->fQuickRetryTime != 0.0f && engine->getTime() > this->fQuickRetryTime) {
            this->fQuickRetryTime = 0.0f;

            if(!bancho.is_playing_a_multi_map()) {
                this->getSelectedBeatmap()->restart(true);
                this->getSelectedBeatmap()->update();
                this->pauseMenu->setVisible(false);
            }
        }
    }

    // background image cache tick
    this->backgroundImageHandler->update(
        this->songBrowser2->isVisible());  // NOTE: must be before the asynchronous ui toggles due to potential 1-frame
                                           // unloads after invisible songbrowser

    // asynchronous ui toggles
    // TODO: this is cancer, why did I even write this section
    if(this->bToggleModSelectionScheduled) {
        this->bToggleModSelectionScheduled = false;
        this->modSelector->setVisible(!this->modSelector->isVisible());

        if(bancho.is_in_a_multi_room()) {
            this->room->setVisible(!this->modSelector->isVisible());
        } else if(!this->isInPlayMode() && this->songBrowser2 != NULL) {
            this->songBrowser2->setVisible(!this->modSelector->isVisible());
        }
    }
    if(this->bToggleSongBrowserScheduled) {
        this->bToggleSongBrowserScheduled = false;

        if(this->mainMenu->isVisible() && this->optionsMenu->isVisible()) this->optionsMenu->setVisible(false);

        if(this->songBrowser2 != NULL) this->songBrowser2->setVisible(!this->songBrowser2->isVisible());

        if(bancho.is_in_a_multi_room()) {
            // We didn't select a map; revert to previously selected one
            auto diff2 = this->songBrowser2->lastSelectedBeatmap;
            if(diff2 != NULL) {
                bancho.room.map_name = UString::format("%s - %s [%s]", diff2->getArtist().c_str(),
                                                       diff2->getTitle().c_str(), diff2->getDifficultyName().c_str());
                bancho.room.map_md5 = diff2->getMD5Hash();
                bancho.room.map_id = diff2->getID();

                Packet packet;
                packet.id = MATCH_CHANGE_SETTINGS;
                bancho.room.pack(&packet);
                send_packet(packet);

                this->room->on_map_change();
            }
        } else {
            this->mainMenu->setVisible(!(this->songBrowser2 != NULL && this->songBrowser2->isVisible()));
        }

        this->updateConfineCursor();
    }
    if(this->bToggleOptionsMenuScheduled) {
        this->bToggleOptionsMenuScheduled = false;

        const bool wasFullscreen = this->optionsMenu->isFullscreen();
        this->optionsMenu->setFullscreen(false);
        this->optionsMenu->setVisible(!this->optionsMenu->isVisible());
        if(wasFullscreen) this->mainMenu->setVisible(!this->optionsMenu->isVisible());
    }
    if(this->bToggleChangelogScheduled) {
        this->bToggleChangelogScheduled = false;

        this->mainMenu->setVisible(!this->mainMenu->isVisible());
        this->changelog->setVisible(!this->mainMenu->isVisible());
    }
    if(this->bToggleEditorScheduled) {
        this->bToggleEditorScheduled = false;

        this->mainMenu->setVisible(!this->mainMenu->isVisible());
    }

    // handle cursor visibility if outside of internal resolution
    // TODO: not a critical bug, but the real cursor gets visible way too early if sensitivity is > 1.0f, due to this
    // using scaled/offset getMouse()->getPos()
    if(cv_resolution_enabled.getBool()) {
        McRect internalWindow = McRect(0, 0, g_vInternalResolution.x, g_vInternalResolution.y);
        bool cursorVisible = env->isCursorVisible();
        if(!internalWindow.contains(engine->getMouse()->getPos())) {
            if(!cursorVisible) env->setCursorVisible(true);
        } else {
            if(cursorVisible != this->bShouldCursorBeVisible) env->setCursorVisible(this->bShouldCursorBeVisible);
        }
    }

    // endless mod
    if(this->bScheduleEndlessModNextBeatmap) {
        this->bScheduleEndlessModNextBeatmap = false;
        this->songBrowser2->playNextRandomBeatmap();
    }

    // multiplayer update
    receive_api_responses();
    receive_bancho_packets();

    // skin async loading
    if(this->bSkinLoadScheduled) {
        if(this->skinScheduledToLoad != NULL && this->skinScheduledToLoad->isReady()) {
            this->bSkinLoadScheduled = false;

            if(this->skin != this->skinScheduledToLoad) SAFE_DELETE(this->skin);

            this->skin = this->skinScheduledToLoad;

            this->skinScheduledToLoad = NULL;

            // force layout update after all skin elements have been loaded
            this->fireResolutionChanged();

            // notify if done after reload
            if(this->bSkinLoadWasReload) {
                this->bSkinLoadWasReload = false;

                this->notificationOverlay->addNotification(
                    this->skin->getName().length() > 0
                        ? UString::format("Skin reloaded! (%s)", this->skin->getName().c_str())
                        : UString("Skin reloaded!"),
                    0xffffffff, false, 0.75f);
            }
        }
    }

    // (must be before m_bFontReloadScheduled and m_bFireResolutionChangedScheduled are handled!)
    if(this->bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled) {
        this->bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled = false;

        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }

    // delayed font reloads (must be before layout updates!)
    if(this->bFontReloadScheduled) {
        this->bFontReloadScheduled = false;
        this->reloadFonts();
    }

    // delayed layout updates
    if(this->bFireResolutionChangedScheduled) {
        this->bFireResolutionChangedScheduled = false;
        this->fireResolutionChanged();
    }
}

void Osu::useMods(FinishedScore *score) { Replay::Mods::use(score->mods); }

void Osu::updateMods() {
    if(bancho.is_in_a_multi_room()) {
        if(bancho.room.mods & (LegacyFlags::DoubleTime | LegacyFlags::Nightcore)) {
            cv_speed_override.setValue(1.5f);
        } else if(bancho.room.mods & (LegacyFlags::HalfTime)) {
            cv_speed_override.setValue(0.75f);
        } else {
            cv_speed_override.setValue(-1.f);
        }

        cv_mod_nofail.setValue(bancho.room.mods & LegacyFlags::NoFail);
        cv_mod_easy.setValue(bancho.room.mods & LegacyFlags::Easy);
        cv_mod_touchdevice.setValue(bancho.room.mods & LegacyFlags::TouchDevice);
        cv_mod_hidden.setValue(bancho.room.mods & LegacyFlags::Hidden);
        cv_mod_hardrock.setValue(bancho.room.mods & LegacyFlags::HardRock);
        cv_mod_suddendeath.setValue(bancho.room.mods & LegacyFlags::SuddenDeath);
        cv_mod_relax.setValue(bancho.room.mods & LegacyFlags::Relax);
        cv_mod_autoplay.setValue(bancho.room.mods & LegacyFlags::Autoplay);
        cv_mod_spunout.setValue(bancho.room.mods & LegacyFlags::SpunOut);
        cv_mod_autopilot.setValue(bancho.room.mods & LegacyFlags::Autopilot);
        cv_mod_perfect.setValue(bancho.room.mods & LegacyFlags::Perfect);
        cv_mod_target.setValue(bancho.room.mods & LegacyFlags::Target);
        cv_mod_scorev2.setValue(bancho.room.win_condition == SCOREV2);
        cv_mod_flashlight.setValue(bancho.room.mods & LegacyFlags::Flashlight);
        cv_mod_nightmare.setValue(false);
        cv_mod_actual_flashlight.setValue(false);

        if(bancho.room.freemods) {
            for(int i = 0; i < 16; i++) {
                if(bancho.room.slots[i].player_id != bancho.user_id) continue;

                cv_mod_nofail.setValue(bancho.room.slots[i].mods & LegacyFlags::NoFail);
                cv_mod_easy.setValue(bancho.room.slots[i].mods & LegacyFlags::Easy);
                cv_mod_touchdevice.setValue(bancho.room.slots[i].mods & LegacyFlags::TouchDevice);
                cv_mod_hidden.setValue(bancho.room.slots[i].mods & LegacyFlags::Hidden);
                cv_mod_hardrock.setValue(bancho.room.slots[i].mods & LegacyFlags::HardRock);
                cv_mod_suddendeath.setValue(bancho.room.slots[i].mods & LegacyFlags::SuddenDeath);
                cv_mod_relax.setValue(bancho.room.slots[i].mods & LegacyFlags::Relax);
                cv_mod_autoplay.setValue(bancho.room.slots[i].mods & LegacyFlags::Autoplay);
                cv_mod_spunout.setValue(bancho.room.slots[i].mods & LegacyFlags::SpunOut);
                cv_mod_autopilot.setValue(bancho.room.slots[i].mods & LegacyFlags::Autopilot);
                cv_mod_perfect.setValue(bancho.room.slots[i].mods & LegacyFlags::Perfect);
                cv_mod_target.setValue(bancho.room.slots[i].mods & LegacyFlags::Target);
            }
        }
    }

    osu->getScore()->mods = Replay::Mods::from_cvars();
    osu->getScore()->setCheated();

    if(this->songBrowser2 != NULL) {
        // Update pp/stars display for current map
        this->songBrowser2->recalculateStarsForSelectedBeatmap(true);
    }

    if(this->isInPlayMode()) {
        // handle auto/pilot cursor visibility
        this->bShouldCursorBeVisible = cv_mod_autoplay.getBool() || cv_mod_autopilot.getBool() ||
                                       this->getSelectedBeatmap()->is_watching ||
                                       this->getSelectedBeatmap()->is_spectating;
        env->setCursorVisible(this->bShouldCursorBeVisible);

        // notify the possibly running beatmap of mod changes
        // e.g. recalculating stacks dynamically if HR is toggled
        this->getSelectedBeatmap()->onModUpdate();
    }

    // handle windows key disable/enable
    this->updateWindowsKeyDisable();
}

void Osu::onKeyDown(KeyboardEvent &key) {
    // global hotkeys

    // global hotkey
    if(key == KEY_O && engine->getKeyboard()->isControlDown()) {
        this->toggleOptionsMenu();
        key.consume();
        return;
    }

    // special hotkeys
    // reload & recompile shaders
    if(engine->getKeyboard()->isAltDown() && engine->getKeyboard()->isControlDown() && key == KEY_R) {
        Shader *sliderShader = engine->getResourceManager()->getShader("slider");
        Shader *cursorTrailShader = engine->getResourceManager()->getShader("cursortrail");
        Shader *hitcircle3DShader = engine->getResourceManager()->getShader("hitcircle3D");

        if(sliderShader != NULL) sliderShader->reload();
        if(cursorTrailShader != NULL) cursorTrailShader->reload();
        if(hitcircle3DShader != NULL) hitcircle3DShader->reload();

        key.consume();
    }

    // reload skin (alt)
    if(engine->getKeyboard()->isAltDown() && engine->getKeyboard()->isControlDown() && key == KEY_S) {
        this->onSkinReload();
        key.consume();
    }

    if(key == (KEYCODE)cv_OPEN_SKIN_SELECT_MENU.getInt()) {
        this->optionsMenu->onSkinSelect();
        key.consume();
        return;
    }

    // disable mouse buttons hotkey
    if(key == (KEYCODE)cv_DISABLE_MOUSE_BUTTONS.getInt()) {
        if(cv_disable_mousebuttons.getBool()) {
            cv_disable_mousebuttons.setValue(0.0f);
            this->notificationOverlay->addNotification("Mouse buttons are enabled.");
        } else {
            cv_disable_mousebuttons.setValue(1.0f);
            this->notificationOverlay->addNotification("Mouse buttons are disabled.");
        }
    }

    if(key == (KEYCODE)cv_TOGGLE_MAP_BACKGROUND.getInt()) {
        auto diff = this->getSelectedBeatmap()->getSelectedDifficulty2();
        if(!diff) {
            this->notificationOverlay->addNotification("No beatmap is currently selected.");
        } else {
            diff->draw_background = !diff->draw_background;
            diff->update_overrides();
        }
        key.consume();
        return;
    }

    // F8 toggle chat
    if(key == (KEYCODE)cv_TOGGLE_CHAT.getInt()) {
        // When options menu is open, instead of toggling chat, close options menu and open chat
        if(bancho.is_online() && this->optionsMenu->isVisible()) {
            this->optionsMenu->setVisible(false);
            this->chat->user_wants_chat = true;
            this->chat->updateVisibility();
        } else {
            this->chat->user_wants_chat = !this->chat->user_wants_chat;
            this->chat->updateVisibility();
        }
    }

    // screenshots
    if(key == (KEYCODE)cv_SAVE_SCREENSHOT.getInt()) this->saveScreenshot();

    // boss key (minimize + mute)
    if(key == (KEYCODE)cv_BOSS_KEY.getInt()) {
        engine->getEnvironment()->minimize();
        this->bWasBossKeyPaused = this->getSelectedBeatmap()->isPreviewMusicPlaying();
        this->getSelectedBeatmap()->pausePreviewMusic(false);
    }

    // local hotkeys (and gameplay keys)

    // while playing (and not in options)
    if(this->isInPlayMode() && !this->optionsMenu->isVisible() && !this->chat->isVisible()) {
        auto beatmap = this->getSelectedBeatmap();

        // instant replay
        if((beatmap->isPaused() || beatmap->hasFailed())) {
            if(!key.isConsumed() && key == (KEYCODE)cv_INSTANT_REPLAY.getInt()) {
                if(!beatmap->is_watching && !beatmap->is_spectating) {
                    FinishedScore score;
                    score.replay = beatmap->live_replay;
                    score.beatmap_hash = beatmap->getSelectedDifficulty2()->getMD5Hash();
                    score.mods = this->getScore()->mods;

                    if(bancho.is_online()) {
                        score.player_id = bancho.user_id;
                        score.playerName = bancho.username.toUtf8();
                    } else {
                        auto local_name = cv_name.getString();
                        score.player_id = 0;
                        score.playerName = local_name.toUtf8();
                    }

                    double percentFinished = beatmap->getPercentFinished();
                    double duration = cv_instant_replay_duration.getFloat() * 1000.f;
                    double offsetPercent = duration / beatmap->getLength();
                    double seekPoint = fmax(0.f, percentFinished - offsetPercent);
                    beatmap->cancelFailing();
                    beatmap->watch(score, seekPoint);
                    return;
                }
            }
        }

        // while playing and not paused
        if(!this->getSelectedBeatmap()->isPaused()) {
            this->getSelectedBeatmap()->onKeyDown(key);

            // K1
            {
                const bool isKeyLeftClick = (key == (KEYCODE)cv_LEFT_CLICK.getInt());
                const bool isKeyLeftClick2 = (key == (KEYCODE)cv_LEFT_CLICK_2.getInt());
                if((!this->bKeyboardKey1Down && isKeyLeftClick) || (!this->bKeyboardKey12Down && isKeyLeftClick2)) {
                    if(isKeyLeftClick2)
                        this->bKeyboardKey12Down = true;
                    else
                        this->bKeyboardKey1Down = true;

                    this->onKey1Change(true, false);

                    if(!this->getSelectedBeatmap()->hasFailed()) key.consume();
                } else if(isKeyLeftClick || isKeyLeftClick2) {
                    if(!this->getSelectedBeatmap()->hasFailed()) key.consume();
                }
            }

            // K2
            {
                const bool isKeyRightClick = (key == (KEYCODE)cv_RIGHT_CLICK.getInt());
                const bool isKeyRightClick2 = (key == (KEYCODE)cv_RIGHT_CLICK_2.getInt());
                if((!this->bKeyboardKey2Down && isKeyRightClick) || (!this->bKeyboardKey22Down && isKeyRightClick2)) {
                    if(isKeyRightClick2)
                        this->bKeyboardKey22Down = true;
                    else
                        this->bKeyboardKey2Down = true;

                    this->onKey2Change(true, false);

                    if(!this->getSelectedBeatmap()->hasFailed()) key.consume();
                } else if(isKeyRightClick || isKeyRightClick2) {
                    if(!this->getSelectedBeatmap()->hasFailed()) key.consume();
                }
            }

            // handle skipping
            if(key == KEY_ENTER || key == (KEYCODE)cv_SKIP_CUTSCENE.getInt()) this->bSkipScheduled = true;

            // toggle ui
            if(!key.isConsumed() && key == (KEYCODE)cv_TOGGLE_SCOREBOARD.getInt() && !this->bScoreboardToggleCheck) {
                this->bScoreboardToggleCheck = true;

                if(engine->getKeyboard()->isShiftDown()) {
                    if(!this->bUIToggleCheck) {
                        this->bUIToggleCheck = true;
                        cv_draw_hud.setValue(!cv_draw_hud.getBool());
                        this->notificationOverlay->addNotification(cv_draw_hud.getBool()
                                                                       ? "In-game interface has been enabled."
                                                                       : "In-game interface has been disabled.",
                                                                   0xffffffff, false, 0.1f);

                        key.consume();
                    }
                } else {
                    if(bancho.is_playing_a_multi_map()) {
                        cv_draw_scoreboard_mp.setValue(!cv_draw_scoreboard_mp.getBool());
                        this->notificationOverlay->addNotification(
                            cv_draw_scoreboard_mp.getBool() ? "Scoreboard is shown." : "Scoreboard is hidden.",
                            0xffffffff, false, 0.1f);
                    } else {
                        cv_draw_scoreboard.setValue(!cv_draw_scoreboard.getBool());
                        this->notificationOverlay->addNotification(
                            cv_draw_scoreboard.getBool() ? "Scoreboard is shown." : "Scoreboard is hidden.", 0xffffffff,
                            false, 0.1f);
                    }

                    key.consume();
                }
            }

            // allow live mod changing while playing
            if(!key.isConsumed() && (key == KEY_F1 || key == (KEYCODE)cv_TOGGLE_MODSELECT.getInt()) &&
               ((KEY_F1 != (KEYCODE)cv_LEFT_CLICK.getInt() && KEY_F1 != (KEYCODE)cv_LEFT_CLICK_2.getInt()) ||
                (!this->bKeyboardKey1Down && !this->bKeyboardKey12Down)) &&
               ((KEY_F1 != (KEYCODE)cv_RIGHT_CLICK.getInt() && KEY_F1 != (KEYCODE)cv_RIGHT_CLICK_2.getInt()) ||
                (!this->bKeyboardKey2Down && !this->bKeyboardKey22Down)) &&
               !this->bF1 && !this->getSelectedBeatmap()->hasFailed() &&
               !bancho.is_playing_a_multi_map())  // only if not failed though
            {
                this->bF1 = true;
                this->toggleModSelection(true);
            }

            // quick save/load
            if(!bancho.is_playing_a_multi_map()) {
                if(key == (KEYCODE)cv_QUICK_SAVE.getInt())
                    this->fQuickSaveTime = this->getSelectedBeatmap()->getPercentFinished();

                if(key == (KEYCODE)cv_QUICK_LOAD.getInt()) {
                    // special case: allow cancelling the failing animation here
                    if(this->getSelectedBeatmap()->hasFailed()) this->getSelectedBeatmap()->cancelFailing();

                    this->getSelectedBeatmap()->seekPercent(this->fQuickSaveTime);
                }
            }

            // quick seek
            if(!bancho.is_playing_a_multi_map()) {
                const bool backward = (key == (KEYCODE)cv_SEEK_TIME_BACKWARD.getInt());
                const bool forward = (key == (KEYCODE)cv_SEEK_TIME_FORWARD.getInt());

                if(backward || forward) {
                    const unsigned long lengthMS = this->getSelectedBeatmap()->getLength();
                    const float percentFinished = this->getSelectedBeatmap()->getPercentFinished();

                    if(lengthMS > 0) {
                        double seekedPercent = 0.0;
                        if(backward)
                            seekedPercent -= (double)cv_seek_delta.getInt() * (1.0 / (double)lengthMS) * 1000.0;
                        else if(forward)
                            seekedPercent += (double)cv_seek_delta.getInt() * (1.0 / (double)lengthMS) * 1000.0;

                        if(seekedPercent != 0.0f) {
                            // special case: allow cancelling the failing animation here
                            if(this->getSelectedBeatmap()->hasFailed()) this->getSelectedBeatmap()->cancelFailing();

                            this->getSelectedBeatmap()->seekPercent(percentFinished + seekedPercent);
                        }
                    }
                }
            }
        }

        // while paused or maybe not paused

        // handle quick restart
        if(((key == (KEYCODE)cv_QUICK_RETRY.getInt() ||
             (engine->getKeyboard()->isControlDown() && !engine->getKeyboard()->isAltDown() && key == KEY_R)) &&
            !this->bQuickRetryDown)) {
            this->bQuickRetryDown = true;
            this->fQuickRetryTime = engine->getTime() + cv_quick_retry_delay.getFloat();
        }

        // handle seeking
        if(key == (KEYCODE)cv_SEEK_TIME.getInt()) this->bSeekKey = true;

        // handle fposu key handling
        this->fposu->onKeyDown(key);
    }

    // forward to all subsystem, if not already consumed
    for(int i = 0; i < this->screens.size(); i++) {
        if(key.isConsumed()) break;

        this->screens[i]->onKeyDown(key);
    }

    // special handling, after subsystems, if still not consumed
    if(!key.isConsumed()) {
        // if playing
        if(this->isInPlayMode()) {
            // toggle pause menu
            if((key == (KEYCODE)cv_GAME_PAUSE.getInt() || key == KEY_ESCAPE) && !this->bEscape) {
                if(!bancho.is_playing_a_multi_map() || this->iMultiplayerClientNumEscPresses > 1) {
                    if(this->iMultiplayerClientNumEscPresses > 1) {
                        this->getSelectedBeatmap()->stop(true);
                        this->room->ragequit();
                    } else {
                        // you can open the pause menu while the failing animation is
                        // happening, but never close it then
                        if(!this->getSelectedBeatmap()->hasFailed() || !this->pauseMenu->isVisible()) {
                            this->bEscape = true;
                            this->getSelectedBeatmap()->pause();
                            this->pauseMenu->setVisible(this->getSelectedBeatmap()->isPaused());

                            key.consume();
                        } else  // pressing escape while in failed pause menu
                        {
                            this->getSelectedBeatmap()->stop(true);
                        }
                    }
                } else {
                    this->iMultiplayerClientNumEscPresses++;
                    if(this->iMultiplayerClientNumEscPresses == 2)
                        this->notificationOverlay->addNotification(
                            "Hit 'Escape' once more to exit this multiplayer match.", 0xffffffff, false, 0.75f);
                }
            }

            // local offset
            if(key == (KEYCODE)cv_INCREASE_LOCAL_OFFSET.getInt()) {
                long offsetAdd = engine->getKeyboard()->isAltDown() ? 1 : 5;
                this->getSelectedBeatmap()->getSelectedDifficulty2()->setLocalOffset(
                    this->getSelectedBeatmap()->getSelectedDifficulty2()->getLocalOffset() + offsetAdd);
                this->notificationOverlay->addNotification(
                    UString::format("Local beatmap offset set to %ld ms",
                                    this->getSelectedBeatmap()->getSelectedDifficulty2()->getLocalOffset()));
            }
            if(key == (KEYCODE)cv_DECREASE_LOCAL_OFFSET.getInt()) {
                long offsetAdd = -(engine->getKeyboard()->isAltDown() ? 1 : 5);
                this->getSelectedBeatmap()->getSelectedDifficulty2()->setLocalOffset(
                    this->getSelectedBeatmap()->getSelectedDifficulty2()->getLocalOffset() + offsetAdd);
                this->notificationOverlay->addNotification(
                    UString::format("Local beatmap offset set to %ld ms",
                                    this->getSelectedBeatmap()->getSelectedDifficulty2()->getLocalOffset()));
            }
        }
    }
}

void Osu::onKeyUp(KeyboardEvent &key) {
    // clicks
    {
        // K1
        {
            const bool isKeyLeftClick = (key == (KEYCODE)cv_LEFT_CLICK.getInt());
            const bool isKeyLeftClick2 = (key == (KEYCODE)cv_LEFT_CLICK_2.getInt());
            if((isKeyLeftClick && this->bKeyboardKey1Down) || (isKeyLeftClick2 && this->bKeyboardKey12Down)) {
                if(isKeyLeftClick2)
                    this->bKeyboardKey12Down = false;
                else
                    this->bKeyboardKey1Down = false;

                if(this->isInPlayMode()) this->onKey1Change(false, false);
            }
        }

        // K2
        {
            const bool isKeyRightClick = (key == (KEYCODE)cv_RIGHT_CLICK.getInt());
            const bool isKeyRightClick2 = (key == (KEYCODE)cv_RIGHT_CLICK_2.getInt());
            if((isKeyRightClick && this->bKeyboardKey2Down) || (isKeyRightClick2 && this->bKeyboardKey22Down)) {
                if(isKeyRightClick2)
                    this->bKeyboardKey22Down = false;
                else
                    this->bKeyboardKey2Down = false;

                if(this->isInPlayMode()) this->onKey2Change(false, false);
            }
        }
    }

    // forward to all subsystems, if not consumed
    for(int i = 0; i < this->screens.size(); i++) {
        if(key.isConsumed()) break;

        this->screens[i]->onKeyUp(key);
    }

    // misc hotkeys release
    if(key == KEY_F1 || key == (KEYCODE)cv_TOGGLE_MODSELECT.getInt()) this->bF1 = false;
    if(key == (KEYCODE)cv_GAME_PAUSE.getInt() || key == KEY_ESCAPE) this->bEscape = false;
    if(key == KEY_SHIFT) this->bUIToggleCheck = false;
    if(key == (KEYCODE)cv_TOGGLE_SCOREBOARD.getInt()) {
        this->bScoreboardToggleCheck = false;
        this->bUIToggleCheck = false;
    }
    if(key == (KEYCODE)cv_QUICK_RETRY.getInt() || key == KEY_R) this->bQuickRetryDown = false;
    if(key == (KEYCODE)cv_SEEK_TIME.getInt()) this->bSeekKey = false;

    // handle fposu key handling
    this->fposu->onKeyUp(key);
}

void Osu::stealFocus() {
    for(auto screen : this->screens) {
        screen->stealFocus();
    }
}

void Osu::onChar(KeyboardEvent &e) {
    for(int i = 0; i < this->screens.size(); i++) {
        if(e.isConsumed()) break;

        this->screens[i]->onChar(e);
    }
}

void Osu::onLeftChange(bool down) {
    if(this->isInPlayMode() && !this->getSelectedBeatmap()->isPaused() && cv_disable_mousebuttons.getBool()) return;

    if(!this->bMouseKey1Down && down) {
        this->bMouseKey1Down = true;
        this->onKey1Change(true, true);
    } else if(this->bMouseKey1Down) {
        this->bMouseKey1Down = false;
        this->onKey1Change(false, true);
    }
}

void Osu::onRightChange(bool down) {
    if(this->isInPlayMode() && !this->getSelectedBeatmap()->isPaused() && cv_disable_mousebuttons.getBool()) return;

    if(!this->bMouseKey2Down && down) {
        this->bMouseKey2Down = true;
        this->onKey2Change(true, true);
    } else if(this->bMouseKey2Down) {
        this->bMouseKey2Down = false;
        this->onKey2Change(false, true);
    }
}

void Osu::toggleModSelection(bool waitForF1KeyUp) {
    this->bToggleModSelectionScheduled = true;
    this->modSelector->setWaitForF1KeyUp(waitForF1KeyUp);
}

void Osu::toggleSongBrowser() { this->bToggleSongBrowserScheduled = true; }

void Osu::toggleOptionsMenu() {
    this->bToggleOptionsMenuScheduled = true;
    this->bOptionsMenuFullscreen = this->mainMenu->isVisible();
}

void Osu::toggleChangelog() { this->bToggleChangelogScheduled = true; }

void Osu::toggleEditor() { this->bToggleEditorScheduled = true; }

void Osu::saveScreenshot() {
    engine->getSound()->play(this->skin->getShutter());
    int screenshotNumber = 0;
    UString screenshot_path;
    do {
        screenshot_path = UString::format("screenshots/screenshot%d.png", screenshotNumber);
        screenshotNumber++;
    } while(env->fileExists(screenshot_path.toUtf8()));

    std::vector<unsigned char> pixels = engine->getGraphics()->getScreenshot();
    Image::saveToImage(&pixels[0], engine->getGraphics()->getResolution().x, engine->getGraphics()->getResolution().y,
                       screenshot_path.toUtf8());
}

void Osu::onPlayEnd(FinishedScore score, bool quit, bool aborted) {
    cv_snd_change_check_interval.setValue(cv_snd_change_check_interval.getDefaultFloat());

    if(!quit) {
        if(!cv_mod_endless.getBool()) {
            // NOTE: the order of these two calls matters
            this->rankingScreen->setScore(score);
            this->rankingScreen->setBeatmapInfo(this->getSelectedBeatmap(),
                                                this->getSelectedBeatmap()->getSelectedDifficulty2());

            engine->getSound()->play(this->skin->getApplause());
        } else {
            this->bScheduleEndlessModNextBeatmap = true;
            return;  // nothing more to do here
        }
    }

    this->mainMenu->setVisible(false);
    this->modSelector->setVisible(false);
    this->pauseMenu->setVisible(false);

    env->setCursorVisible(false);
    this->bShouldCursorBeVisible = false;

    this->iMultiplayerClientNumEscPresses = 0;

    if(this->songBrowser2 != NULL) this->songBrowser2->onPlayEnd(quit);

    // When playing in multiplayer, screens are toggled in Room
    if(!bancho.is_playing_a_multi_map()) {
        if(quit) {
            // When spectating, we don't want to enable the song browser after song ends
            if(bancho.spectated_player_id == 0) {
                this->toggleSongBrowser();
            }
        } else {
            this->rankingScreen->setVisible(true);
        }
    }

    this->updateConfineCursor();
    this->updateWindowsKeyDisable();
}

Beatmap *Osu::getSelectedBeatmap() {
    if(this->songBrowser2 != NULL) return this->songBrowser2->getSelectedBeatmap();

    return NULL;
}

float Osu::getDifficultyMultiplier() {
    float difficultyMultiplier = 1.0f;

    if(cv_mod_hardrock.getBool()) difficultyMultiplier = 1.4f;
    if(cv_mod_easy.getBool()) difficultyMultiplier = 0.5f;

    return difficultyMultiplier;
}

float Osu::getCSDifficultyMultiplier() {
    float difficultyMultiplier = 1.0f;

    if(cv_mod_hardrock.getBool()) difficultyMultiplier = 1.3f;  // different!
    if(cv_mod_easy.getBool()) difficultyMultiplier = 0.5f;

    return difficultyMultiplier;
}

float Osu::getScoreMultiplier() {
    float multiplier = 1.0f;

    // Dumb formula, but the values for HT/DT were dumb to begin with
    f32 s = this->getSelectedBeatmap()->getSpeedMultiplier();
    if(s > 1.f) {
        multiplier *= (0.24 * s) + 0.76;
    } else if(s < 1.f) {
        multiplier *= 0.008 * std::exp(4.81588 * s);
    }

    if(cv_mod_easy.getBool() || (cv_mod_nofail.getBool() && !cv_mod_scorev2.getBool())) multiplier *= 0.50f;
    if(cv_mod_hardrock.getBool()) {
        if(cv_mod_scorev2.getBool())
            multiplier *= 1.10f;
        else
            multiplier *= 1.06f;
    }
    if(cv_mod_flashlight.getBool()) multiplier *= 1.12f;
    if(cv_mod_hidden.getBool()) multiplier *= 1.06f;
    if(cv_mod_spunout.getBool()) multiplier *= 0.90f;

    if(cv_mod_relax.getBool() || cv_mod_autopilot.getBool()) multiplier *= 0.f;

    return multiplier;
}

float Osu::getAnimationSpeedMultiplier() {
    float animationSpeedMultiplier = this->getSelectedBeatmap()->getSpeedMultiplier();

    if(cv_animation_speed_override.getFloat() >= 0.0f) return max(cv_animation_speed_override.getFloat(), 0.05f);

    return animationSpeedMultiplier;
}

bool Osu::isInPlayMode() { return (this->songBrowser2 != NULL && this->songBrowser2->hasSelectedAndIsPlaying()); }

bool Osu::isNotInPlayModeOrPaused() { return !this->isInPlayMode() || this->getSelectedBeatmap()->isPaused(); }

bool Osu::shouldFallBackToLegacySliderRenderer() {
    return cv_force_legacy_slider_renderer.getBool() || cv_mod_wobble.getBool() || cv_mod_wobble2.getBool() ||
           cv_mod_minimize.getBool() || this->modSelector->isCSOverrideSliderActive()
        /* || (this->osu_playfield_rotation->getFloat() < -0.01f || m_osu_playfield_rotation->getFloat() > 0.01f)*/;
}

void Osu::onResolutionChanged(Vector2 newResolution) {
    debugLog("Osu::onResolutionChanged(%i, %i), minimized = %i\n", (int)newResolution.x, (int)newResolution.y,
             (int)engine->isMinimized());

    if(engine->isMinimized()) return;  // ignore if minimized

    const float prevUIScale = getUIScale();

    if(!cv_resolution_enabled.getBool()) {
        g_vInternalResolution = newResolution;
    } else if(!engine->isMinimized()) {  // if we just got minimized, ignore the resolution change (for the internal
                                         // stuff)
        // clamp upwards to internal resolution (osu_resolution)
        if(g_vInternalResolution.x < this->vInternalResolution.x) g_vInternalResolution.x = this->vInternalResolution.x;
        if(g_vInternalResolution.y < this->vInternalResolution.y) g_vInternalResolution.y = this->vInternalResolution.y;

        // clamp downwards to engine resolution
        if(newResolution.x < g_vInternalResolution.x) g_vInternalResolution.x = newResolution.x;
        if(newResolution.y < g_vInternalResolution.y) g_vInternalResolution.y = newResolution.y;

        // disable internal resolution on specific conditions
        bool windowsBorderlessHackCondition =
            (env->getOS() == Environment::OS::WINDOWS && env->isFullscreen() && env->isFullscreenWindowedBorderless() &&
             (int)g_vInternalResolution.y == (int)env->getNativeScreenSize().y);  // HACKHACK
        if(((int)g_vInternalResolution.x == engine->getScreenWidth() &&
            (int)g_vInternalResolution.y == engine->getScreenHeight()) ||
           !env->isFullscreen() || windowsBorderlessHackCondition) {
            debugLog("Internal resolution == Engine resolution || !Fullscreen, disabling resampler (%i, %i)\n",
                     (int)(g_vInternalResolution == engine->getScreenSize()), (int)(!env->isFullscreen()));
            cv_resolution_enabled.setValue(0.0f);
            g_vInternalResolution = engine->getScreenSize();
        }
    }

    // update dpi specific engine globals
    cv_ui_scrollview_scrollbarwidth.setValue(15.0f * Osu::getUIScale());  // not happy with this as a convar

    // interfaces
    for(int i = 0; i < this->screens.size(); i++) {
        this->screens[i]->onResolutionChange(g_vInternalResolution);
    }

    // rendertargets
    this->rebuildRenderTargets();

    // mouse scale/offset
    this->updateMouseSettings();

    // cursor clipping
    this->updateConfineCursor();

    // see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=323
    struct LossyComparisonToFixExcessFPUPrecisionBugBecauseFuckYou {
        static bool equalEpsilon(float f1, float f2) { return std::abs(f1 - f2) < 0.00001f; }
    };

    // a bit hacky, but detect resolution-specific-dpi-scaling changes and force a font and layout reload after a 1
    // frame delay (1/2)
    if(!LossyComparisonToFixExcessFPUPrecisionBugBecauseFuckYou::equalEpsilon(getUIScale(), prevUIScale))
        this->bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled = true;
}

void Osu::onDPIChanged() {
    // delay
    this->bFontReloadScheduled = true;
    this->bFireResolutionChangedScheduled = true;
}

void Osu::rebuildRenderTargets() {
    debugLog("Osu::rebuildRenderTargets: %fx%f\n", g_vInternalResolution.x, g_vInternalResolution.y);

    this->backBuffer->rebuild(0, 0, g_vInternalResolution.x, g_vInternalResolution.y);

    if(cv_mod_fposu.getBool())
        this->playfieldBuffer->rebuild(0, 0, g_vInternalResolution.x, g_vInternalResolution.y);
    else
        this->playfieldBuffer->rebuild(0, 0, 64, 64);

    this->sliderFrameBuffer->rebuild(0, 0, g_vInternalResolution.x, g_vInternalResolution.y,
                                     Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X);

    if(cv_mod_mafham.getBool()) {
        this->frameBuffer->rebuild(0, 0, g_vInternalResolution.x, g_vInternalResolution.y);
        this->frameBuffer2->rebuild(0, 0, g_vInternalResolution.x, g_vInternalResolution.y);
    } else {
        this->frameBuffer->rebuild(0, 0, 64, 64);
        this->frameBuffer2->rebuild(0, 0, 64, 64);
    }
}

void Osu::reloadFonts() {
    const int baseDPI = 96;
    const int newDPI = Osu::getUIScale() * baseDPI;

    for(McFont *font : this->fonts) {
        if(font->getDPI() != newDPI) {
            font->setDPI(newDPI);
            font->reload();
        }
    }
}

void Osu::updateMouseSettings() {
    // mouse scaling & offset
    Vector2 offset = Vector2(0, 0);
    Vector2 scale = Vector2(1, 1);
    if(cv_resolution_enabled.getBool()) {
        if(cv_letterboxing.getBool()) {
            // special case for osu: since letterboxed raw input absolute to window should mean the 'game' window, and
            // not the 'engine' window, no offset scaling is necessary
            if(cv_mouse_raw_input_absolute_to_window.getBool())
                offset = -Vector2((engine->getScreenWidth() / 2 - g_vInternalResolution.x / 2),
                                  (engine->getScreenHeight() / 2 - g_vInternalResolution.y / 2));
            else
                offset = -Vector2((engine->getScreenWidth() / 2 - g_vInternalResolution.x / 2) *
                                      (1.0f + cv_letterboxing_offset_x.getFloat()),
                                  (engine->getScreenHeight() / 2 - g_vInternalResolution.y / 2) *
                                      (1.0f + cv_letterboxing_offset_y.getFloat()));

            scale = Vector2(g_vInternalResolution.x / engine->getScreenWidth(),
                            g_vInternalResolution.y / engine->getScreenHeight());
        }
    }

    engine->getMouse()->setOffset(offset);
    engine->getMouse()->setScale(scale);
}

void Osu::updateWindowsKeyDisable() {
    if(cv_debug.getBool()) debugLog("Osu::updateWindowsKeyDisable()\n");

    if(cv_win_disable_windows_key_while_playing.getBool()) {
        const bool isPlayerPlaying =
            engine->hasFocus() && this->isInPlayMode() &&
            (!this->getSelectedBeatmap()->isPaused() || this->getSelectedBeatmap()->isRestartScheduled()) &&
            !cv_mod_autoplay.getBool();
        cv_win_disable_windows_key.setValue(isPlayerPlaying ? 1.0f : 0.0f);
    }
}

void Osu::fireResolutionChanged() { this->onResolutionChanged(g_vInternalResolution); }

void Osu::onWindowedResolutionChanged(UString oldValue, UString args) {
    if(env->isFullscreen()) return;
    if(args.length() < 7) return;

    std::vector<UString> resolution = args.split("x");
    if(resolution.size() != 2) {
        debugLog(
            "Error: Invalid parameter count for command 'osu_resolution'! (Usage: e.g. \"osu_resolution 1280x720\")");
        return;
    }

    int width = resolution[0].toFloat();
    int height = resolution[1].toFloat();
    if(width < 300 || height < 240) {
        debugLog("Error: Invalid values for resolution for command 'osu_resolution'!");
        return;
    }

    env->setWindowSize(width, height);
    env->center();
}

void Osu::onInternalResolutionChanged(UString oldValue, UString args) {
    if(!env->isFullscreen()) return;
    if(args.length() < 7) return;

    std::vector<UString> resolution = args.split("x");
    if(resolution.size() != 2) {
        debugLog(
            "Error: Invalid parameter count for command 'osu_resolution'! (Usage: e.g. \"osu_resolution 1280x720\")");
        return;
    }

    int width = resolution[0].toFloat();
    int height = resolution[1].toFloat();
    if(width < 300 || height < 240) {
        debugLog("Error: Invalid values for resolution for command 'osu_resolution'!");
        return;
    }

    const float prevUIScale = getUIScale();
    Vector2 newInternalResolution = Vector2(width, height);

    // clamp requested internal resolution to current renderer resolution
    // however, this could happen while we are transitioning into fullscreen. therefore only clamp when not in
    // fullscreen or not in fullscreen transition
    bool isTransitioningIntoFullscreenHack = engine->getGraphics()->getResolution().x < env->getNativeScreenSize().x ||
                                             engine->getGraphics()->getResolution().y < env->getNativeScreenSize().y;
    if(!env->isFullscreen() || !isTransitioningIntoFullscreenHack) {
        if(newInternalResolution.x > engine->getGraphics()->getResolution().x)
            newInternalResolution.x = engine->getGraphics()->getResolution().x;
        if(newInternalResolution.y > engine->getGraphics()->getResolution().y)
            newInternalResolution.y = engine->getGraphics()->getResolution().y;
    }

    // enable and store, then force onResolutionChanged()
    cv_resolution_enabled.setValue(1.0f);
    g_vInternalResolution = newInternalResolution;
    this->vInternalResolution = newInternalResolution;
    this->fireResolutionChanged();

    // a bit hacky, but detect resolution-specific-dpi-scaling changes and force a font and layout reload after a 1
    // frame delay (2/2)
    if(getUIScale() != prevUIScale) this->bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled = true;
}

void Osu::onFocusGained() {
    // cursor clipping
    this->updateConfineCursor();

    if(this->bWasBossKeyPaused) {
        this->bWasBossKeyPaused = false;
        this->getSelectedBeatmap()->pausePreviewMusic();
    }

    this->updateWindowsKeyDisable();
    this->volumeOverlay->gainFocus();
}

void Osu::onFocusLost() {
    if(this->isInPlayMode() && !this->getSelectedBeatmap()->isPaused() && cv_pause_on_focus_loss.getBool()) {
        if(!bancho.is_playing_a_multi_map() && !this->getSelectedBeatmap()->is_watching &&
           !this->getSelectedBeatmap()->is_spectating) {
            this->getSelectedBeatmap()->pause(false);
            this->pauseMenu->setVisible(true);
            this->modSelector->setVisible(false);
        }
    }

    this->updateWindowsKeyDisable();
    this->volumeOverlay->loseFocus();

    // release cursor clip
    env->setCursorClip(false, McRect());
}

void Osu::onMinimized() { this->volumeOverlay->loseFocus(); }

bool Osu::onShutdown() {
    debugLog("Osu::onShutdown()\n");

    if(!cv_alt_f4_quits_even_while_playing.getBool() && this->isInPlayMode()) {
        this->getSelectedBeatmap()->stop();
        return false;
    }

    // save everything
    this->optionsMenu->save();
    db->save();

    disconnect();

    // the only time where a shutdown could be problematic is while an update is being installed, so we block it here
    return this->updateHandler == NULL ||
           this->updateHandler->getStatus() != UpdateHandler::STATUS::STATUS_INSTALLING_UPDATE;
}

void Osu::onSkinReload() {
    this->bSkinLoadWasReload = true;
    this->onSkinChange("", cv_skin.getString());
}

void Osu::onSkinChange(UString oldValue, UString newValue) {
    if(this->skin != NULL) {
        if(this->bSkinLoadScheduled || this->skinScheduledToLoad != NULL) return;
        if(newValue.length() < 1) return;
    }

    if(newValue == UString("default")) {
        this->skinScheduledToLoad = new Skin(newValue, MCENGINE_DATA_DIR "materials/default/", true);
        if(this->skin == NULL) this->skin = this->skinScheduledToLoad;
        this->bSkinLoadScheduled = true;
        return;
    }

    std::string neosuSkinFolder = MCENGINE_DATA_DIR "skins/";
    neosuSkinFolder.append(newValue.toUtf8());
    neosuSkinFolder.append("/");
    if(env->directoryExists(neosuSkinFolder)) {
        this->skinScheduledToLoad = new Skin(newValue, neosuSkinFolder, false);
    } else {
        UString ppySkinFolder = cv_osu_folder.getString();
        ppySkinFolder.append("/");
        ppySkinFolder.append(cv_osu_folder_sub_skins.getString());
        ppySkinFolder.append(newValue);
        ppySkinFolder.append("/");
        std::string sf = ppySkinFolder.toUtf8();
        this->skinScheduledToLoad = new Skin(newValue, sf, false);
    }

    // initial load
    if(this->skin == NULL) this->skin = this->skinScheduledToLoad;

    this->bSkinLoadScheduled = true;
}

void Osu::updateAnimationSpeed() {
    if(this->getSkin() != NULL) {
        float speed = this->getAnimationSpeedMultiplier() / this->getSelectedBeatmap()->getSpeedMultiplier();
        this->getSkin()->setAnimationSpeed(speed >= 0.0f ? speed : 0.0f);
    }
}

void Osu::onAnimationSpeedChange(UString oldValue, UString newValue) { this->updateAnimationSpeed(); }

void Osu::onSpeedChange(UString oldValue, UString newValue) {
    float speed = newValue.toFloat();
    this->getSelectedBeatmap()->setSpeed(speed >= 0.0f ? speed : this->getSelectedBeatmap()->getSpeedMultiplier());
    this->updateAnimationSpeed();

    // HACKHACK: Update DT/HT buttons, speed slider etc
    cv_mod_doubletime_dummy.setValue(speed == 1.5f, false);
    osu->getModSelector()->modButtonDoubletime->setOn(speed == 1.5f, true);
    cv_mod_halftime_dummy.setValue(speed == 0.75f, false);
    osu->getModSelector()->modButtonHalftime->setOn(speed == 0.75f, true);
    osu->getModSelector()->updateButtons(true);
    osu->getModSelector()->updateScoreMultiplierLabelText();
    osu->getModSelector()->updateOverrideSliderLabels();
}

void Osu::onDTPresetChange(UString oldValue, UString newValue) {
    cv_speed_override.setValue(cv_mod_doubletime_dummy.getBool() ? 1.5f : -1.f);
    osu->getModSelector()->speedSlider->setValue(cv_speed_override.getFloat(), false, false);
}

void Osu::onHTPresetChange(UString oldValue, UString newValue) {
    cv_speed_override.setValue(cv_mod_halftime_dummy.getBool() ? 0.75f : -1.f);
    osu->getModSelector()->speedSlider->setValue(cv_speed_override.getFloat(), false, false);
}

void Osu::onThumbnailsToggle(UString oldValue, UString newValue) {
    osu->getSongBrowser()->thumbnailYRatio = cv_draw_songbrowser_thumbnails.getBool() ? 1.333333f : 0.f;
}

void Osu::onPlayfieldChange(UString oldValue, UString newValue) { this->getSelectedBeatmap()->onModUpdate(); }

void Osu::onUIScaleChange(UString oldValue, UString newValue) {
    const float oldVal = oldValue.toFloat();
    const float newVal = newValue.toFloat();

    if(oldVal != newVal) {
        // delay
        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }
}

void Osu::onUIScaleToDPIChange(UString oldValue, UString newValue) {
    const bool oldVal = oldValue.toFloat() > 0.0f;
    const bool newVal = newValue.toFloat() > 0.0f;

    if(oldVal != newVal) {
        // delay
        this->bFontReloadScheduled = true;
        this->bFireResolutionChangedScheduled = true;
    }
}

void Osu::onLetterboxingChange(UString oldValue, UString newValue) {
    if(cv_resolution_enabled.getBool()) {
        bool oldVal = oldValue.toFloat() > 0.0f;
        bool newVal = newValue.toFloat() > 0.0f;

        if(oldVal != newVal) this->bFireResolutionChangedScheduled = true;  // delay
    }
}

void Osu::updateConfineCursor() {
    if(cv_debug.getBool()) debugLog("Osu::updateConfineCursor()\n");

    if((cv_confine_cursor_fullscreen.getBool() && env->isFullscreen()) ||
       (cv_confine_cursor_windowed.getBool() && !env->isFullscreen()) ||
       (this->isInPlayMode() && !this->pauseMenu->isVisible() && !this->getModAuto() && !this->getModAutopilot() &&
        !this->getSelectedBeatmap()->is_watching && !this->getSelectedBeatmap()->is_spectating))
        env->setCursorClip(true, McRect());
    else
        env->setCursorClip(false, McRect());
}

void Osu::onConfineCursorWindowedChange(UString oldValue, UString newValue) { this->updateConfineCursor(); }

void Osu::onConfineCursorFullscreenChange(UString oldValue, UString newValue) { this->updateConfineCursor(); }

void Osu::onKey1Change(bool pressed, bool mouse) {
    int numKeys1Down = 0;
    if(this->bKeyboardKey1Down) numKeys1Down++;
    if(this->bKeyboardKey12Down) numKeys1Down++;
    if(this->bMouseKey1Down) numKeys1Down++;

    const bool isKeyPressed1Allowed =
        (numKeys1Down == 1);  // all key1 keys (incl. multiple bindings) act as one single key with state handover

    // WARNING: if paused, keyReleased*() will be called out of sequence every time due to the fix. do not put actions
    // in it
    if(this->isInPlayMode() /* && !getSelectedBeatmap()->isPaused()*/)  // NOTE: allow keyup even while beatmap is
                                                                        // paused, to correctly not-continue immediately
                                                                        // due to pressed keys
    {
        if(!(mouse && cv_disable_mousebuttons.getBool())) {
            // quickfix
            if(cv_disable_mousebuttons.getBool()) this->bMouseKey1Down = false;

            if(pressed && isKeyPressed1Allowed && !this->getSelectedBeatmap()->isPaused())  // see above note
                this->getSelectedBeatmap()->keyPressed1(mouse);
            else if(!this->bKeyboardKey1Down && !this->bKeyboardKey12Down && !this->bMouseKey1Down)
                this->getSelectedBeatmap()->keyReleased1(mouse);
        }
    }

    // cursor anim + ripples
    const bool doAnimate = !(this->isInPlayMode() && !this->getSelectedBeatmap()->isPaused() && mouse &&
                             cv_disable_mousebuttons.getBool());
    if(doAnimate) {
        if(pressed && isKeyPressed1Allowed) {
            this->hud->animateCursorExpand();
            this->hud->addCursorRipple(engine->getMouse()->getPos());
        } else if(!this->bKeyboardKey1Down && !this->bKeyboardKey12Down && !this->bMouseKey1Down &&
                  !this->bKeyboardKey2Down && !this->bKeyboardKey22Down && !this->bMouseKey2Down)
            this->hud->animateCursorShrink();
    }
}

void Osu::onKey2Change(bool pressed, bool mouse) {
    int numKeys2Down = 0;
    if(this->bKeyboardKey2Down) numKeys2Down++;
    if(this->bKeyboardKey22Down) numKeys2Down++;
    if(this->bMouseKey2Down) numKeys2Down++;

    const bool isKeyPressed2Allowed =
        (numKeys2Down == 1);  // all key2 keys (incl. multiple bindings) act as one single key with state handover

    // WARNING: if paused, keyReleased*() will be called out of sequence every time due to the fix. do not put actions
    // in it
    if(this->isInPlayMode() /* && !getSelectedBeatmap()->isPaused()*/)  // NOTE: allow keyup even while beatmap is
                                                                        // paused, to correctly not-continue immediately
                                                                        // due to pressed keys
    {
        if(!(mouse && cv_disable_mousebuttons.getBool())) {
            // quickfix
            if(cv_disable_mousebuttons.getBool()) this->bMouseKey2Down = false;

            if(pressed && isKeyPressed2Allowed && !this->getSelectedBeatmap()->isPaused())  // see above note
                this->getSelectedBeatmap()->keyPressed2(mouse);
            else if(!this->bKeyboardKey2Down && !this->bKeyboardKey22Down && !this->bMouseKey2Down)
                this->getSelectedBeatmap()->keyReleased2(mouse);
        }
    }

    // cursor anim + ripples
    const bool doAnimate = !(this->isInPlayMode() && !this->getSelectedBeatmap()->isPaused() && mouse &&
                             cv_disable_mousebuttons.getBool());
    if(doAnimate) {
        if(pressed && isKeyPressed2Allowed) {
            this->hud->animateCursorExpand();
            this->hud->addCursorRipple(engine->getMouse()->getPos());
        } else if(!this->bKeyboardKey2Down && !this->bKeyboardKey22Down && !this->bMouseKey2Down &&
                  !this->bKeyboardKey1Down && !this->bKeyboardKey12Down && !this->bMouseKey1Down)
            this->hud->animateCursorShrink();
    }
}

void Osu::onModMafhamChange(UString oldValue, UString newValue) { this->rebuildRenderTargets(); }

void Osu::onModFPoSuChange(UString oldValue, UString newValue) { this->rebuildRenderTargets(); }

void Osu::onModFPoSu3DChange(UString oldValue, UString newValue) { this->rebuildRenderTargets(); }

void Osu::onModFPoSu3DSpheresChange(UString oldValue, UString newValue) { this->rebuildRenderTargets(); }

void Osu::onModFPoSu3DSpheresAAChange(UString oldValue, UString newValue) { this->rebuildRenderTargets(); }

void Osu::onLetterboxingOffsetChange(UString oldValue, UString newValue) { this->updateMouseSettings(); }

void Osu::onUserCardChange(UString new_username) {
    // NOTE: force update options textbox to avoid shutdown inconsistency
    this->getOptionsMenu()->setUsername(new_username);
    this->userButton->setID(bancho.user_id);
}

float Osu::getImageScaleToFitResolution(Vector2 size, Vector2 resolution) {
    return resolution.x / size.x > resolution.y / size.y ? resolution.y / size.y : resolution.x / size.x;
}

float Osu::getImageScaleToFitResolution(Image *img, Vector2 resolution) {
    return getImageScaleToFitResolution(Vector2(img->getWidth(), img->getHeight()), resolution);
}

float Osu::getImageScaleToFillResolution(Vector2 size, Vector2 resolution) {
    return resolution.x / size.x < resolution.y / size.y ? resolution.y / size.y : resolution.x / size.x;
}

float Osu::getImageScaleToFillResolution(Image *img, Vector2 resolution) {
    return getImageScaleToFillResolution(Vector2(img->getWidth(), img->getHeight()), resolution);
}

float Osu::getImageScale(Vector2 size, float osuSize) {
    int swidth = osu->getScreenWidth();
    int sheight = osu->getScreenHeight();

    if(swidth * 3 > sheight * 4)
        swidth = sheight * 4 / 3;
    else
        sheight = swidth * 3 / 4;

    const float xMultiplier = swidth / osuBaseResolution.x;
    const float yMultiplier = sheight / osuBaseResolution.y;

    const float xDiameter = osuSize * xMultiplier;
    const float yDiameter = osuSize * yMultiplier;

    return xDiameter / size.x > yDiameter / size.y ? xDiameter / size.x : yDiameter / size.y;
}

float Osu::getImageScale(Image *img, float osuSize) {
    return getImageScale(Vector2(img->getWidth(), img->getHeight()), osuSize);
}

float Osu::getUIScale(float osuResolutionRatio) {
    int swidth = osu->getScreenWidth();
    int sheight = osu->getScreenHeight();

    if(swidth * 3 > sheight * 4)
        swidth = sheight * 4 / 3;
    else
        sheight = swidth * 3 / 4;

    const float xMultiplier = swidth / osuBaseResolution.x;
    const float yMultiplier = sheight / osuBaseResolution.y;

    const float xDiameter = osuResolutionRatio * xMultiplier;
    const float yDiameter = osuResolutionRatio * yMultiplier;

    return xDiameter > yDiameter ? xDiameter : yDiameter;
}

float Osu::getUIScale() {
    if(osu != NULL) {
        if(osu->getScreenWidth() < cv_ui_scale_to_dpi_minimum_width.getInt() ||
           osu->getScreenHeight() < cv_ui_scale_to_dpi_minimum_height.getInt())
            return cv_ui_scale.getFloat();
    } else if(engine->getScreenWidth() < cv_ui_scale_to_dpi_minimum_width.getInt() ||
              engine->getScreenHeight() < cv_ui_scale_to_dpi_minimum_height.getInt())
        return cv_ui_scale.getFloat();

    return ((cv_ui_scale_to_dpi.getBool() ? env->getDPIScale() : 1.0f) * cv_ui_scale.getFloat());
}

bool Osu::findIgnoreCase(const std::string &haystack, const std::string &needle) {
    auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                          [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); });

    return (it != haystack.end());
}
