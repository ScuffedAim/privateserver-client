#include "UIRankingScreenRankingPanel.h"

#include "ConVar.h"
#include "Engine.h"
#include "HUD.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SkinImage.h"
#include "score.h"

UIRankingScreenRankingPanel::UIRankingScreenRankingPanel() : CBaseUIImage("", 0, 0, 0, 0, "") {
    setImage(osu->getSkin()->getRankingPanel());
    setDrawFrame(true);

    m_iScore = 0;
    m_iNum300s = 0;
    m_iNum300gs = 0;
    m_iNum100s = 0;
    m_iNum100ks = 0;
    m_iNum50s = 0;
    m_iNumMisses = 0;
    m_iCombo = 0;
    m_fAccuracy = 0.0f;
    m_bPerfect = false;
}

void UIRankingScreenRankingPanel::draw(Graphics *g) {
    CBaseUIImage::draw(g);
    if(!m_bVisible) return;

    const float uiScale = /*cv_ui_scale.getFloat()*/ 1.0f;  // NOTE: commented for now, doesn't really work due to
                                                            // legacy layout expectations

    const float globalScoreScale = (osu->getSkin()->getVersion() > 1.0f ? 1.3f : 1.05f) * uiScale;

    const int globalYOffsetRaw = -1;
    const int globalYOffset = osu->getUIScale(globalYOffsetRaw);

    // draw score
    g->setColor(0xffffffff);
    float scale = osu->getImageScale(osu->getSkin()->getScore0(), 20.0f) * globalScoreScale;
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(m_vPos.x + osu->getUIScale(111.0f) * uiScale,
                     m_vPos.y + (osu->getSkin()->getScore0()->getHeight() / 2) * scale +
                         (osu->getUIScale(11.0f) + globalYOffset) * uiScale);
        osu->getHUD()->drawScoreNumber(g, m_iScore, scale);
    }
    g->popTransform();

    // draw hit images
    const Vector2 hitImageStartPos = Vector2(40, 100 + globalYOffsetRaw);
    const Vector2 hitGridOffsetX = Vector2(200, 0);
    const Vector2 hitGridOffsetY = Vector2(0, 60);

    drawHitImage(g, osu->getSkin()->getHit300(), scale, hitImageStartPos);
    drawHitImage(g, osu->getSkin()->getHit100(), scale, hitImageStartPos + hitGridOffsetY);
    drawHitImage(g, osu->getSkin()->getHit50(), scale, hitImageStartPos + hitGridOffsetY * 2);
    drawHitImage(g, osu->getSkin()->getHit300g(), scale, hitImageStartPos + hitGridOffsetX);
    drawHitImage(g, osu->getSkin()->getHit100k(), scale, hitImageStartPos + hitGridOffsetX + hitGridOffsetY);
    drawHitImage(g, osu->getSkin()->getHit0(), scale, hitImageStartPos + hitGridOffsetX + hitGridOffsetY * 2);

    // draw numHits
    const Vector2 numHitStartPos = hitImageStartPos + Vector2(40, osu->getSkin()->getVersion() > 1.0f ? -16 : -25);
    scale = osu->getImageScale(osu->getSkin()->getScore0(), 17.0f) * globalScoreScale;

    drawNumHits(g, m_iNum300s, scale, numHitStartPos);
    drawNumHits(g, m_iNum100s, scale, numHitStartPos + hitGridOffsetY);
    drawNumHits(g, m_iNum50s, scale, numHitStartPos + hitGridOffsetY * 2);

    drawNumHits(g, m_iNum300gs, scale, numHitStartPos + hitGridOffsetX);
    drawNumHits(g, m_iNum100ks, scale, numHitStartPos + hitGridOffsetX + hitGridOffsetY);
    drawNumHits(g, m_iNumMisses, scale, numHitStartPos + hitGridOffsetX + hitGridOffsetY * 2);

    const int row4 = 260;
    const int row4ImageOffset = (osu->getSkin()->getVersion() > 1.0f ? 20 : 8) - 20;

    // draw combo
    scale = osu->getImageScale(osu->getSkin()->getScore0(), 17.0f) * globalScoreScale;
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(m_vPos.x + osu->getUIScale(15.0f) * uiScale,
                     m_vPos.y + (osu->getSkin()->getScore0()->getHeight() / 2) * scale +
                         (osu->getUIScale(row4 + 10) + globalYOffset) * uiScale);
        osu->getHUD()->drawComboSimple(g, m_iCombo, scale);
    }
    g->popTransform();

    // draw maxcombo label
    Vector2 hardcodedOsuRankingMaxComboImageSize =
        Vector2(162, 50) * (osu->getSkin()->isRankingMaxCombo2x() ? 2.0f : 1.0f);
    scale = osu->getImageScale(hardcodedOsuRankingMaxComboImageSize, 32.0f) * uiScale;
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(m_vPos.x + osu->getSkin()->getRankingMaxCombo()->getWidth() * scale * 0.5f +
                         osu->getUIScale(4.0f) * uiScale,
                     m_vPos.y + (osu->getUIScale(row4 - 5 - row4ImageOffset) + globalYOffset) * uiScale);
        g->drawImage(osu->getSkin()->getRankingMaxCombo());
    }
    g->popTransform();

    // draw accuracy
    scale = osu->getImageScale(osu->getSkin()->getScore0(), 17.0f) * globalScoreScale;
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(m_vPos.x + osu->getUIScale(195.0f) * uiScale,
                     m_vPos.y + (osu->getSkin()->getScore0()->getHeight() / 2) * scale +
                         (osu->getUIScale(row4 + 10) + globalYOffset) * uiScale);
        osu->getHUD()->drawAccuracySimple(g, m_fAccuracy * 100.0f, scale);
    }
    g->popTransform();

    // draw accuracy label
    Vector2 hardcodedOsuRankingAccuracyImageSize =
        Vector2(192, 58) * (osu->getSkin()->isRankingAccuracy2x() ? 2.0f : 1.0f);
    scale = osu->getImageScale(hardcodedOsuRankingAccuracyImageSize, 36.0f) * uiScale;
    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(m_vPos.x + osu->getSkin()->getRankingAccuracy()->getWidth() * scale * 0.5f +
                         osu->getUIScale(183.0f) * uiScale,
                     m_vPos.y + (osu->getUIScale(row4 - 3 - row4ImageOffset) + globalYOffset) * uiScale);
        g->drawImage(osu->getSkin()->getRankingAccuracy());
    }
    g->popTransform();

    // draw perfect
    if(m_bPerfect) {
        scale = osu->getImageScale(osu->getSkin()->getRankingPerfect()->getSizeBaseRaw(), 94.0f) * uiScale;
        osu->getSkin()->getRankingPerfect()->drawRaw(
            g,
            m_vPos +
                Vector2(osu->getUIScale(osu->getSkin()->getVersion() > 1.0f ? 260 : 200),
                        osu->getUIScale(430.0f) + globalYOffset) *
                    Vector2(1.0f, 0.97f) * uiScale -
                Vector2(0, osu->getSkin()->getRankingPerfect()->getSizeBaseRaw().y) * scale * 0.5f,
            scale);
    }
}

void UIRankingScreenRankingPanel::drawHitImage(Graphics *g, SkinImage *img, float scale, Vector2 pos) {
    const float uiScale = /*cv_ui_scale.getFloat()*/ 1.0f;  // NOTE: commented for now, doesn't really work due to
                                                            // legacy layout expectations

    /// img->setAnimationFrameForce(0);
    img->draw(g, Vector2(m_vPos.x + osu->getUIScale(pos.x) * uiScale, m_vPos.y + osu->getUIScale(pos.y) * uiScale),
              uiScale);
}

void UIRankingScreenRankingPanel::drawNumHits(Graphics *g, int numHits, float scale, Vector2 pos) {
    const float uiScale = /*cv_ui_scale.getFloat()*/ 1.0f;  // NOTE: commented for now, doesn't really work due to
                                                            // legacy layout expectations

    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(
            m_vPos.x + osu->getUIScale(pos.x) * uiScale,
            m_vPos.y + (osu->getSkin()->getScore0()->getHeight() / 2) * scale + osu->getUIScale(pos.y) * uiScale);
        osu->getHUD()->drawComboSimple(g, numHits, scale);
    }
    g->popTransform();
}

void UIRankingScreenRankingPanel::setScore(LiveScore *score) {
    m_iScore = score->getScore();
    m_iNum300s = score->getNum300s();
    m_iNum300gs = score->getNum300gs();
    m_iNum100s = score->getNum100s();
    m_iNum100ks = score->getNum100ks();
    m_iNum50s = score->getNum50s();
    m_iNumMisses = score->getNumMisses();
    m_iCombo = score->getComboMax();
    m_fAccuracy = score->getAccuracy();
    m_bPerfect = (score->getComboFull() > 0 && m_iCombo >= score->getComboFull());
}

void UIRankingScreenRankingPanel::setScore(FinishedScore score) {
    m_iScore = score.score;
    m_iNum300s = score.num300s;
    m_iNum300gs = score.numGekis;
    m_iNum100s = score.num100s;
    m_iNum100ks = score.numKatus;
    m_iNum50s = score.num50s;
    m_iNumMisses = score.numMisses;
    m_iCombo = score.comboMax;
    m_fAccuracy = LiveScore::calculateAccuracy(score.num300s, score.num100s, score.num50s, score.numMisses);
    m_bPerfect = score.perfect;

    // round acc up from two decimal places
    if(m_fAccuracy > 0.0f) m_fAccuracy = std::round(m_fAccuracy * 10000.0f) / 10000.0f;
}
