#pragma once
#include "App.h"
#include "BanchoNetworking.h"
#include "ModSelector.h"
#include "MouseListener.h"
#include "Replay.h"
#include "score.h"

class CWindowManager;

class VolumeOverlay;
class Chat;
class Lobby;
class RoomScreen;
class PromptScreen;
class UIUserContextMenuScreen;
class MainMenu;
class PauseMenu;
class OptionsMenu;
class SongBrowser;
class SpectatorScreen;
class BackgroundImageHandler;
class RankingScreen;
class UpdateHandler;
class NotificationOverlay;
class TooltipOverlay;
class Beatmap;
class OsuScreen;
class Skin;
class HUD;
class Changelog;
class ModFPoSu;

class ConVar;
class Image;
class McFont;
class RenderTarget;

class Osu : public App, public MouseListener {
   public:
    static bool autoUpdater;

    static Vector2 osuBaseResolution;

    static float getImageScaleToFitResolution(Image *img, Vector2 resolution);
    static float getImageScaleToFitResolution(Vector2 size, Vector2 resolution);
    static float getImageScaleToFillResolution(Vector2 size, Vector2 resolution);
    static float getImageScaleToFillResolution(Image *img, Vector2 resolution);
    static float getImageScale(Vector2 size, float osuSize);
    static float getImageScale(Image *img, float osuSize);
    static float getUIScale(float osuResolutionRatio);
    static float getUIScale();  // NOTE: includes premultiplied dpi scale!

    static bool findIgnoreCase(const std::string &haystack, const std::string &needle);

    Osu();
    virtual ~Osu();

    virtual void draw(Graphics *g);
    virtual void update();

    virtual void onKeyDown(KeyboardEvent &e);
    virtual void onKeyUp(KeyboardEvent &e);
    virtual void onChar(KeyboardEvent &e);
    virtual void stealFocus();

    void onLeftChange(bool down);
    void onMiddleChange(bool down) { (void)down; }
    void onRightChange(bool down);

    void onWheelVertical(int delta) { (void)delta; }
    void onWheelHorizontal(int delta) { (void)delta; }

    virtual void onResolutionChanged(Vector2 newResolution);
    virtual void onDPIChanged();

    virtual void onFocusGained();
    virtual void onFocusLost();
    virtual void onMinimized();
    virtual bool onShutdown();

    void onPlayEnd(FinishedScore score, bool quit = true, bool aborted = false);

    void toggleModSelection(bool waitForF1KeyUp = false);
    void toggleSongBrowser();
    void toggleOptionsMenu();
    void toggleChangelog();
    void toggleEditor();

    void saveScreenshot();

    void reloadSkin() { onSkinReload(); }

    inline Vector2 getScreenSize() const { return g_vInternalResolution; }
    inline int getScreenWidth() const { return (int)g_vInternalResolution.x; }
    inline int getScreenHeight() const { return (int)g_vInternalResolution.y; }

    Beatmap *getSelectedBeatmap();

    inline OptionsMenu *getOptionsMenu() const { return m_optionsMenu; }
    inline SongBrowser *getSongBrowser() const { return m_songBrowser2; }
    inline BackgroundImageHandler *getBackgroundImageHandler() const { return m_backgroundImageHandler; }
    inline Skin *getSkin() const { return m_skin; }
    inline HUD *getHUD() const { return m_hud; }
    inline NotificationOverlay *getNotificationOverlay() const { return m_notificationOverlay; }
    inline TooltipOverlay *getTooltipOverlay() const { return m_tooltipOverlay; }
    inline ModSelector *getModSelector() const { return m_modSelector; }
    inline ModFPoSu *getFPoSu() const { return m_fposu; }
    inline PauseMenu *getPauseMenu() const { return m_pauseMenu; }
    inline MainMenu *getMainMenu() const { return m_mainMenu; }
    inline RankingScreen *getRankingScreen() const { return m_rankingScreen; }
    inline LiveScore *getScore() const { return m_score; }
    inline UpdateHandler *getUpdateHandler() const { return m_updateHandler; }

    inline RenderTarget *getPlayfieldBuffer() const { return m_playfieldBuffer; }
    inline RenderTarget *getSliderFrameBuffer() const { return m_sliderFrameBuffer; }
    inline RenderTarget *getFrameBuffer() const { return m_frameBuffer; }
    inline RenderTarget *getFrameBuffer2() const { return m_frameBuffer2; }
    inline McFont *getTitleFont() const { return m_titleFont; }
    inline McFont *getSubTitleFont() const { return m_subTitleFont; }
    inline McFont *getSongBrowserFont() const { return m_songBrowserFont; }
    inline McFont *getSongBrowserFontBold() const { return m_songBrowserFontBold; }
    inline McFont *getFontIcons() const { return m_fontIcons; }

    float getDifficultyMultiplier();
    float getCSDifficultyMultiplier();
    float getScoreMultiplier();
    float getAnimationSpeedMultiplier();

    inline bool getModAuto() const { return cv_mod_autoplay.getBool(); }
    inline bool getModAutopilot() const { return cv_mod_autopilot.getBool(); }
    inline bool getModRelax() const { return cv_mod_relax.getBool(); }
    inline bool getModSpunout() const { return cv_mod_spunout.getBool(); }
    inline bool getModTarget() const { return cv_mod_target.getBool(); }
    inline bool getModScorev2() const { return cv_mod_scorev2.getBool(); }
    inline bool getModFlashlight() const { return cv_mod_flashlight.getBool(); }
    inline bool getModNF() const { return cv_mod_nofail.getBool(); }
    inline bool getModHD() const { return cv_mod_hidden.getBool(); }
    inline bool getModHR() const { return cv_mod_hardrock.getBool(); }
    inline bool getModEZ() const { return cv_mod_easy.getBool(); }
    inline bool getModSD() const { return cv_mod_suddendeath.getBool(); }
    inline bool getModSS() const { return cv_mod_perfect.getBool(); }
    inline bool getModNightmare() const { return cv_mod_nightmare.getBool(); }
    inline bool getModTD() const { return cv_mod_touchdevice.getBool() || cv_mod_touchdevice_always.getBool(); }

    inline std::vector<ConVar *> getExperimentalMods() const { return m_experimentalMods; }

    bool isInPlayMode();
    bool isNotInPlayModeOrPaused();
    inline bool isSkinLoading() const { return m_bSkinLoadScheduled; }

    inline bool isSkipScheduled() const { return m_bSkipScheduled; }
    inline bool isSeeking() const { return m_bSeeking; }
    inline float getQuickSaveTime() const { return m_fQuickSaveTime; }

    bool shouldFallBackToLegacySliderRenderer();  // certain mods or actions require Sliders to render dynamically
                                                  // (e.g. wobble or the CS override slider)

    void useMods(FinishedScore *score);
    void updateMods();
    void updateConfineCursor();
    void updateMouseSettings();
    void updateWindowsKeyDisable();

    static Vector2 g_vInternalResolution;

    void updateModsForConVarTemplate(UString oldValue, UString newValue) {
        (void)oldValue;
        (void)newValue;
        updateMods();
    }

    void rebuildRenderTargets();
    void reloadFonts();
    void fireResolutionChanged();

    // callbacks
    void onInternalResolutionChanged(UString oldValue, UString args);

    void onSkinReload();
    void onSkinChange(UString oldValue, UString newValue);
    void onAnimationSpeedChange(UString oldValue, UString newValue);
    void updateAnimationSpeed();

    void onSpeedChange(UString oldValue, UString newValue);
    void onDTPresetChange(UString oldValue, UString newValue);
    void onHTPresetChange(UString oldValue, UString newValue);

    void onPlayfieldChange(UString oldValue, UString newValue);

    void onUIScaleChange(UString oldValue, UString newValue);
    void onUIScaleToDPIChange(UString oldValue, UString newValue);
    void onLetterboxingChange(UString oldValue, UString newValue);

    void onConfineCursorWindowedChange(UString oldValue, UString newValue);
    void onConfineCursorFullscreenChange(UString oldValue, UString newValue);

    void onKey1Change(bool pressed, bool mouse);
    void onKey2Change(bool pressed, bool mouse);

    void onModMafhamChange(UString oldValue, UString newValue);
    void onModFPoSuChange(UString oldValue, UString newValue);
    void onModFPoSu3DChange(UString oldValue, UString newValue);
    void onModFPoSu3DSpheresChange(UString oldValue, UString newValue);
    void onModFPoSu3DSpheresAAChange(UString oldValue, UString newValue);

    void onLetterboxingOffsetChange(UString oldValue, UString newValue);

    // interfaces
    VolumeOverlay *m_volumeOverlay = NULL;
    MainMenu *m_mainMenu = NULL;
    OptionsMenu *m_optionsMenu = NULL;
    Chat *m_chat = NULL;
    Lobby *m_lobby = NULL;
    RoomScreen *m_room = NULL;
    PromptScreen *m_prompt = NULL;
    UIUserContextMenuScreen *m_user_actions = NULL;
    SongBrowser *m_songBrowser2 = NULL;
    BackgroundImageHandler *m_backgroundImageHandler = NULL;
    ModSelector *m_modSelector = NULL;
    RankingScreen *m_rankingScreen = NULL;
    PauseMenu *m_pauseMenu = NULL;
    Skin *m_skin = NULL;
    HUD *m_hud = NULL;
    TooltipOverlay *m_tooltipOverlay = NULL;
    NotificationOverlay *m_notificationOverlay = NULL;
    LiveScore *m_score = NULL;
    Changelog *m_changelog = NULL;
    UpdateHandler *m_updateHandler = NULL;
    ModFPoSu *m_fposu = NULL;
    SpectatorScreen *m_spectatorScreen = NULL;

    std::vector<OsuScreen *> m_screens;

    // rendering
    RenderTarget *m_backBuffer;
    RenderTarget *m_playfieldBuffer;
    RenderTarget *m_sliderFrameBuffer;
    RenderTarget *m_frameBuffer;
    RenderTarget *m_frameBuffer2;
    Vector2 m_vInternalResolution;
    Vector2 flashlight_position;

    // mods
    Replay::Mods previous_mods;
    bool m_bModAutoTemp = false;  // when ctrl+clicking a map, the auto mod should disable itself after the map finishes

    std::vector<ConVar *> m_experimentalMods;

    // keys
    bool m_bF1;
    bool m_bUIToggleCheck;
    bool m_bScoreboardToggleCheck;
    bool m_bEscape;
    bool m_bKeyboardKey1Down;
    bool m_bKeyboardKey12Down;
    bool m_bKeyboardKey2Down;
    bool m_bKeyboardKey22Down;
    bool m_bMouseKey1Down;
    bool m_bMouseKey2Down;
    bool m_bSkipScheduled;
    bool m_bQuickRetryDown;
    float m_fQuickRetryTime;
    bool m_bSeekKey;
    bool m_bSeeking;
    bool m_bClickedSkipButton = false;
    float m_fPrevSeekMousePosX;
    float m_fQuickSaveTime;

    // async toggles
    // TODO: this way of doing things is bullshit
    bool m_bToggleModSelectionScheduled;
    bool m_bToggleSongBrowserScheduled;
    bool m_bToggleOptionsMenuScheduled;
    bool m_bOptionsMenuFullscreen;
    bool m_bToggleChangelogScheduled;
    bool m_bToggleEditorScheduled;

    // cursor
    bool m_bShouldCursorBeVisible;

    // global resources
    std::vector<McFont *> m_fonts;
    McFont *m_titleFont;
    McFont *m_subTitleFont;
    McFont *m_songBrowserFont;
    McFont *m_songBrowserFontBold;
    McFont *m_fontIcons;

    // debugging
    CWindowManager *m_windowManager;

    // replay
    UString watched_user_name;
    u32 watched_user_id = 0;

    // custom
    bool music_unpause_scheduled = false;
    bool m_bScheduleEndlessModNextBeatmap;
    int m_iMultiplayerClientNumEscPresses;
    bool m_bWasBossKeyPaused;
    bool m_bSkinLoadScheduled;
    bool m_bSkinLoadWasReload;
    Skin *m_skinScheduledToLoad;
    bool m_bFontReloadScheduled;
    bool m_bFireResolutionChangedScheduled;
    bool m_bFireDelayedFontReloadAndResolutionChangeToFixDesyncedUIScaleScheduled;
    std::atomic<bool> should_pause_background_threads = false;
};

extern Osu *osu;
