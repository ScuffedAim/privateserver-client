#pragma once
#include "Beatmap.h"

class ConVar;

class ModFPoSu;
class Beatmap;

class HitObject {
   public:
    static void drawHitResult(Graphics *g, Beatmap *beatmap, Vector2 rawPos, LiveScore::HIT result,
                              float animPercentInv, float hitDeltaRangePercent);
    static void drawHitResult(Graphics *g, Skin *skin, float hitcircleDiameter, float rawHitcircleDiameter,
                              Vector2 rawPos, LiveScore::HIT result, float animPercentInv, float hitDeltaRangePercent);

    static ConVar *m_osu_approach_scale_multiplier_ref;
    static ConVar *m_osu_timingpoints_force;
    static ConVar *m_osu_relax_offset_ref;

    static ConVar *m_osu_mod_mafham_ref;

   public:
    HitObject(long time, int sampleType, int comboNumber, bool isEndOfCombo, int colorCounter, int colorOffset,
              Beatmap *beatmap);
    virtual ~HitObject() { ; }

    virtual void draw(Graphics *g) { ; }
    virtual void draw2(Graphics *g);
    virtual void update(long curPos);

    virtual void updateStackPosition(float stackOffset) = 0;
    virtual void miss(long curPos) = 0;  // only used by notelock

    inline Beatmap *getBeatmap() const { return m_beatmap; }

    virtual int getCombo() { return 1; }  // how much combo this hitobject is "worth"
    virtual bool isCircle() { return false; }
    virtual bool isSlider() { return false; }
    virtual bool isSpinner() { return false; }
    void addHitResult(LiveScore::HIT result, long delta, bool isEndOfCombo, Vector2 posRaw, float targetDelta = 0.0f,
                      float targetAngle = 0.0f, bool ignoreOnHitErrorBar = false, bool ignoreCombo = false,
                      bool ignoreHealth = false, bool addObjectDurationToSkinAnimationTimeStartOffset = true);
    void misAimed() { m_bMisAim = true; }

    void setIsEndOfCombo(bool isEndOfCombo) { m_bIsEndOfCombo = isEndOfCombo; }
    void setStack(int stack) { m_iStack = stack; }
    void setForceDrawApproachCircle(bool firstNote) { m_bOverrideHDApproachCircle = firstNote; }
    void setAutopilotDelta(long delta) { m_iAutopilotDelta = delta; }
    void setBlocked(bool blocked) { m_bBlocked = blocked; }
    void setComboNumber(int comboNumber) { m_iComboNumber = comboNumber; }

    virtual Vector2 getRawPosAt(long pos) = 0;          // with stack calculation modifications
    virtual Vector2 getOriginalRawPosAt(long pos) = 0;  // without stack calculations
    virtual Vector2 getAutoCursorPos(long curPos) = 0;

    inline long getTime() const { return m_iTime; }
    inline long getDuration() const { return m_iObjectDuration; }
    inline int getStack() const { return m_iStack; }
    inline int getComboNumber() const { return m_iComboNumber; }
    inline bool isEndOfCombo() const { return m_bIsEndOfCombo; }
    inline int getColorCounter() const { return m_iColorCounter; }
    inline int getColorOffset() const { return m_iColorOffset; }
    inline float getApproachScale() const { return m_fApproachScale; }
    inline long getDelta() const { return m_iDelta; }
    inline long getApproachTime() const { return m_iApproachTime; }
    inline long getAutopilotDelta() const { return m_iAutopilotDelta; }
    inline unsigned long long getSortHack() const { return m_iSortHack; }

    inline bool isVisible() const { return m_bVisible; }
    inline bool isFinished() const { return m_bFinished; }
    inline bool isBlocked() const { return m_bBlocked; }
    inline bool hasMisAimed() const { return m_bMisAim; }

    virtual void onClickEvent(std::vector<Click> &clicks) { ; }
    virtual void onReset(long curPos);

   protected:
    Beatmap *m_beatmap;

    bool m_bVisible;
    bool m_bFinished;

    long m_iTime;  // the time at which this object must be clicked
    int m_iSampleType;
    int m_iComboNumber;
    bool m_bIsEndOfCombo;
    int m_iColorCounter;
    int m_iColorOffset;

    float m_fAlpha;
    float m_fAlphaWithoutHidden;
    float m_fAlphaForApproachCircle;
    float m_fApproachScale;
    float m_fHittableDimRGBColorMultiplierPercent;
    long m_iDelta;  // this must be signed
    long m_iApproachTime;
    long m_iFadeInTime;      // extra time added before the approachTime to let the object smoothly become visible
    long m_iObjectDuration;  // how long this object takes to click (circle = 0, slider = sliderTime, spinner =
                             // spinnerTime etc.), the object will stay visible this long extra after m_iTime; it should
                             // be set by the actual object inheriting from this class

    int m_iStack;

    bool m_bBlocked;
    bool m_bOverrideHDApproachCircle;
    bool m_bMisAim;
    long m_iAutopilotDelta;
    bool m_bUseFadeInTimeAsApproachTime;

   private:
    static unsigned long long sortHackCounter;

    static float lerp3f(float a, float b, float c, float percent);

    struct HITRESULTANIM {
        float time;
        Vector2 rawPos;
        LiveScore::HIT result;
        long delta;
        bool addObjectDurationToSkinAnimationTimeStartOffset;
    };

    void drawHitResultAnim(Graphics *g, const HITRESULTANIM &hitresultanim);

    HITRESULTANIM m_hitresultanim1;
    HITRESULTANIM m_hitresultanim2;

    unsigned long long m_iSortHack;
};
