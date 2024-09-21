#pragma once
#include "HitObject.h"

class SliderCurve;

class VertexArrayObject;

class Slider : public HitObject {
   public:
    struct SLIDERCLICK {
        long time;
        bool finished;
        bool successful;
        bool sliderend;
        int type;
        int tickIndex;
    };

   public:
    Slider(char stype, int repeat, float pixelLength, std::vector<Vector2> points, std::vector<int> hitSounds,
           std::vector<float> ticks, float sliderTime, float sliderTimeWithoutRepeats, long time, int sampleType,
           int comboNumber, bool isEndOfCombo, int colorCounter, int colorOffset, BeatmapInterface *beatmap);
    virtual ~Slider();

    virtual void draw(Graphics *g);
    virtual void draw2(Graphics *g);
    void draw2(Graphics *g, bool drawApproachCircle, bool drawOnlyApproachCircle);
    virtual void update(long curPos, f64 frame_time);

    virtual void updateStackPosition(float stackOffset);
    virtual void miss(long curPos);
    virtual int getCombo() { return 2 + max((m_iRepeat - 1), 0) + (max((m_iRepeat - 1), 0) + 1) * m_ticks.size(); }

    Vector2 getRawPosAt(long pos);
    Vector2 getOriginalRawPosAt(long pos);
    inline Vector2 getAutoCursorPos(long curPos) { return m_vCurPoint; }

    virtual void onClickEvent(std::vector<Click> &clicks);
    virtual void onReset(long curPos);

    void rebuildVertexBuffer(bool useRawCoords = false);

    inline bool isStartCircleFinished() const { return m_bStartFinished; }
    inline int getRepeat() const { return m_iRepeat; }
    inline std::vector<Vector2> getRawPoints() const { return m_points; }
    inline float getPixelLength() const { return m_fPixelLength; }
    inline const std::vector<SLIDERCLICK> &getClicks() const { return m_clicks; }

    // ILLEGAL:
    inline VertexArrayObject *getVAO() const { return m_vao; }
    inline SliderCurve *getCurve() const { return m_curve; }

   private:
    void drawStartCircle(Graphics *g, float alpha);
    void drawEndCircle(Graphics *g, float alpha, float sliderSnake);
    void drawBody(Graphics *g, float alpha, float from, float to);

    void updateAnimations(long curPos);

    void onHit(LiveScore::HIT result, long delta, bool startOrEnd, float targetDelta = 0.0f, float targetAngle = 0.0f,
               bool isEndResultFromStrictTrackingMod = false);
    void onRepeatHit(bool successful, bool sliderend);
    void onTickHit(bool successful, int tickIndex);
    void onSliderBreak();

    float getT(long pos, bool raw);

    bool isClickHeldSlider();  // special logic to disallow hold tapping

    SliderCurve *m_curve;

    char m_cType;
    int m_iRepeat;
    float m_fPixelLength;
    std::vector<Vector2> m_points;
    std::vector<int> m_hitSounds;
    float m_fSliderTime;
    float m_fSliderTimeWithoutRepeats;

    struct SLIDERTICK {
        float percent;
        bool finished;
    };
    std::vector<SLIDERTICK> m_ticks;  // ticks (drawing)

    // TEMP: auto cursordance
    std::vector<SLIDERCLICK> m_clicks;  // repeats (type 0) + ticks (type 1)

    float m_fSlidePercent;        // 0.0f - 1.0f - 0.0f - 1.0f - etc.
    float m_fActualSlidePercent;  // 0.0f - 1.0f
    float m_fSliderSnakePercent;
    float m_fReverseArrowAlpha;
    float m_fBodyAlpha;

    Vector2 m_vCurPoint;
    Vector2 m_vCurPointRaw;

    LiveScore::HIT m_startResult;
    LiveScore::HIT m_endResult;
    bool m_bStartFinished;
    float m_fStartHitAnimation;
    bool m_bEndFinished;
    float m_fEndHitAnimation;
    float m_fEndSliderBodyFadeAnimation;
    long m_iStrictTrackingModLastClickHeldTime;
    int m_iFatFingerKey;
    int m_iPrevSliderSlideSoundSampleSet;
    bool m_bCursorLeft;
    bool m_bCursorInside;
    bool m_bHeldTillEnd;
    bool m_bHeldTillEndForLenienceHack;
    bool m_bHeldTillEndForLenienceHackCheck;
    float m_fFollowCircleTickAnimationScale;
    float m_fFollowCircleAnimationScale;
    float m_fFollowCircleAnimationAlpha;

    int m_iReverseArrowPos;
    int m_iCurRepeat;
    int m_iCurRepeatCounterForHitSounds;
    bool m_bInReverse;
    bool m_bHideNumberAfterFirstRepeatHit;

    VertexArrayObject *m_vao;
};
