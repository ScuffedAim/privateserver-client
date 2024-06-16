#include "Button.h"

#include "SongBrowser.h"
// ---

#include "AnimationHandler.h"
#include "CBaseUIContainer.h"
#include "CBaseUIScrollView.h"
#include "ConVar.h"
#include "Engine.h"
#include "Mouse.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SoundEngine.h"

using namespace std;

ConVar osu_songbrowser_button_active_color_a("osu_songbrowser_button_active_color_a", 220 + 10, FCVAR_DEFAULT);
ConVar osu_songbrowser_button_active_color_r("osu_songbrowser_button_active_color_r", 255, FCVAR_DEFAULT);
ConVar osu_songbrowser_button_active_color_g("osu_songbrowser_button_active_color_g", 255, FCVAR_DEFAULT);
ConVar osu_songbrowser_button_active_color_b("osu_songbrowser_button_active_color_b", 255, FCVAR_DEFAULT);

ConVar osu_songbrowser_button_inactive_color_a("osu_songbrowser_button_inactive_color_a", 240, FCVAR_DEFAULT);
ConVar osu_songbrowser_button_inactive_color_r("osu_songbrowser_button_inactive_color_r", 235, FCVAR_DEFAULT);
ConVar osu_songbrowser_button_inactive_color_g("osu_songbrowser_button_inactive_color_g", 73, FCVAR_DEFAULT);
ConVar osu_songbrowser_button_inactive_color_b("osu_songbrowser_button_inactive_color_b", 153, FCVAR_DEFAULT);

int Button::marginPixelsX = 9;
int Button::marginPixelsY = 9;
float Button::lastHoverSoundTime = 0;
int Button::sortHackCounter = 0;

// Color Button::inactiveDifficultyBackgroundColor = COLOR(255, 0, 150, 236); // blue

Button::Button(SongBrowser *songBrowser, CBaseUIScrollView *view, UIContextMenu *contextMenu, float xPos, float yPos,
               float xSize, float ySize, UString name)
    : CBaseUIButton(xPos, yPos, xSize, ySize, name, "") {
    m_view = view;
    m_songBrowser = songBrowser;
    m_contextMenu = contextMenu;

    m_font = osu->getSongBrowserFont();
    m_fontBold = osu->getSongBrowserFontBold();

    m_bVisible = false;
    m_bSelected = false;
    m_bHideIfSelected = false;

    m_bRightClick = false;
    m_bRightClickCheck = false;

    m_fTargetRelPosY = yPos;
    m_fScale = 1.0f;
    m_fOffsetPercent = 0.0f;
    m_fHoverOffsetAnimation = 0.0f;
    m_fCenterOffsetAnimation = 0.0f;
    m_fCenterOffsetVelocityAnimation = 0.0f;

    m_iSortHack = sortHackCounter++;
    m_bIsSearchMatch = true;

    m_fHoverMoveAwayAnimation = 0.0f;
    m_moveAwayState = MOVE_AWAY_STATE::MOVE_CENTER;
}

Button::~Button() { deleteAnimations(); }

void Button::deleteAnimations() {
    anim->deleteExistingAnimation(&m_fCenterOffsetAnimation);
    anim->deleteExistingAnimation(&m_fCenterOffsetVelocityAnimation);
    anim->deleteExistingAnimation(&m_fHoverOffsetAnimation);
    anim->deleteExistingAnimation(&m_fHoverMoveAwayAnimation);
}

void Button::draw(Graphics *g) {
    if(!m_bVisible) return;

    drawMenuButtonBackground(g);

    // debug inner bounding box
    if(Osu::debug->getBool()) {
        // scaling
        const Vector2 pos = getActualPos();
        const Vector2 size = getActualSize();

        g->setColor(0xffff00ff);
        g->drawLine(pos.x, pos.y, pos.x + size.x, pos.y);
        g->drawLine(pos.x, pos.y, pos.x, pos.y + size.y);
        g->drawLine(pos.x, pos.y + size.y, pos.x + size.x, pos.y + size.y);
        g->drawLine(pos.x + size.x, pos.y, pos.x + size.x, pos.y + size.y);
    }

    // debug outer/actual bounding box
    if(Osu::debug->getBool()) {
        g->setColor(0xffff0000);
        g->drawLine(m_vPos.x, m_vPos.y, m_vPos.x + m_vSize.x, m_vPos.y);
        g->drawLine(m_vPos.x, m_vPos.y, m_vPos.x, m_vPos.y + m_vSize.y);
        g->drawLine(m_vPos.x, m_vPos.y + m_vSize.y, m_vPos.x + m_vSize.x, m_vPos.y + m_vSize.y);
        g->drawLine(m_vPos.x + m_vSize.x, m_vPos.y, m_vPos.x + m_vSize.x, m_vPos.y + m_vSize.y);
    }
}

void Button::drawMenuButtonBackground(Graphics *g) {
    g->setColor(m_bSelected ? getActiveBackgroundColor() : getInactiveBackgroundColor());
    g->pushTransform();
    {
        g->scale(m_fScale, m_fScale);
        g->translate(m_vPos.x + m_vSize.x / 2, m_vPos.y + m_vSize.y / 2);
        g->drawImage(osu->getSkin()->getMenuButtonBackground());
    }
    g->popTransform();
}

void Button::mouse_update(bool *propagate_clicks) {
    // Not correct, but clears most of the lag
    if(m_vPos.y + m_vSize.y < 0) return;
    if(m_vPos.y > engine->getScreenHeight()) return;

    // HACKHACK: absolutely disgusting
    // temporarily fool CBaseUIElement with modified position and size
    {
        Vector2 posBackup = m_vPos;
        Vector2 sizeBackup = m_vSize;

        m_vPos = getActualPos();
        m_vSize = getActualSize();
        { CBaseUIButton::mouse_update(propagate_clicks); }
        m_vPos = posBackup;
        m_vSize = sizeBackup;
    }

    if(!m_bVisible) return;

    // HACKHACK: this should really be part of the UI base
    // right click detection
    if(engine->getMouse()->isRightDown()) {
        if(!m_bRightClickCheck) {
            m_bRightClickCheck = true;
            m_bRightClick = isMouseInside();
        }
    } else {
        if(m_bRightClick) {
            if(isMouseInside()) onRightMouseUpInside();
        }

        m_bRightClickCheck = false;
        m_bRightClick = false;
    }

    // animations need constant layout updates while visible
    updateLayoutEx();
}

void Button::updateLayoutEx() {
    const float uiScale = Osu::ui_scale->getFloat();

    Image *menuButtonBackground = osu->getSkin()->getMenuButtonBackground();
    {
        const Vector2 minimumSize =
            Vector2(699.0f, 103.0f) * (osu->getSkin()->isMenuButtonBackground2x() ? 2.0f : 1.0f);
        const float minimumScale = Osu::getImageScaleToFitResolution(menuButtonBackground, minimumSize);
        m_fScale = Osu::getImageScale(menuButtonBackground->getSize() * minimumScale, 64.0f) * uiScale;
    }

    if(m_bVisible)  // lag prevention (animationHandler overflow)
    {
        const float centerOffsetAnimationTarget =
            1.0f - clamp<float>(std::abs((m_vPos.y + (m_vSize.y / 2) - m_view->getPos().y - m_view->getSize().y / 2) /
                                         (m_view->getSize().y / 2)),
                                0.0f, 1.0f);
        anim->moveQuadOut(&m_fCenterOffsetAnimation, centerOffsetAnimationTarget, 0.5f, true);

        float centerOffsetVelocityAnimationTarget =
            clamp<float>((std::abs(m_view->getVelocity().y)) / 3500.0f, 0.0f, 1.0f);

        if(m_songBrowser->isRightClickScrolling()) centerOffsetVelocityAnimationTarget = 0.0f;

        if(m_view->isScrolling())
            anim->moveQuadOut(&m_fCenterOffsetVelocityAnimation, 0.0f, 1.0f, true);
        else
            anim->moveQuadOut(&m_fCenterOffsetVelocityAnimation, centerOffsetVelocityAnimationTarget, 1.25f, true);
    }

    setSize((int)(menuButtonBackground->getWidth() * m_fScale), (int)(menuButtonBackground->getHeight() * m_fScale));

    const float percentCenterOffsetAnimation = 0.035f;
    const float percentVelocityOffsetAnimation = 0.35f;
    const float percentHoverOffsetAnimation = 0.075f;

    // this is the minimum offset necessary to not clip into the score scrollview (including all possible max animations
    // which can push us to the left, worst case)
    float minOffset = m_view->getSize().x * (percentCenterOffsetAnimation + percentHoverOffsetAnimation);
    {
        // also respect the width of the button image: push to the right until the edge of the button image can never be
        // visible even if all animations are fully active the 0.85f here heuristically pushes the buttons a bit further
        // to the right than would be necessary, to make animations work better on lower resolutions (would otherwise
        // hit the left edge too early)
        const float buttonWidthCompensation = max(m_view->getSize().x - getActualSize().x * 0.85f, 0.0f);
        minOffset += buttonWidthCompensation;
    }

    float offsetX =
        minOffset -
        m_view->getSize().x *
            (percentCenterOffsetAnimation * m_fCenterOffsetAnimation * (1.0f - m_fCenterOffsetVelocityAnimation) +
             percentHoverOffsetAnimation * m_fHoverOffsetAnimation -
             percentVelocityOffsetAnimation * m_fCenterOffsetVelocityAnimation + m_fOffsetPercent);
    offsetX = clamp<float>(
        offsetX, 0.0f,
        m_view->getSize().x -
            getActualSize().x * 0.15f);  // WARNING: hardcoded to match 0.85f above for buttonWidthCompensation

    setRelPosX(offsetX);
    setRelPosY(m_fTargetRelPosY + getSize().y * 0.125f * m_fHoverMoveAwayAnimation);
}

Button *Button::setVisible(bool visible) {
    CBaseUIButton::setVisible(visible);

    deleteAnimations();

    if(m_bVisible) {
        // scrolling pinch effect
        m_fCenterOffsetAnimation = 1.0f;
        m_fHoverOffsetAnimation = 0.0f;

        float centerOffsetVelocityAnimationTarget =
            clamp<float>((std::abs(m_view->getVelocity().y)) / 3500.0f, 0.0f, 1.0f);

        if(m_songBrowser->isRightClickScrolling()) centerOffsetVelocityAnimationTarget = 0.0f;

        m_fCenterOffsetVelocityAnimation = centerOffsetVelocityAnimationTarget;

        // force early layout update
        updateLayout();
        updateLayoutEx();
    }

    return this;
}

void Button::select(bool fireCallbacks, bool autoSelectBottomMostChild, bool wasParentSelected) {
    const bool wasSelected = m_bSelected;
    m_bSelected = true;

    // callback
    if(fireCallbacks) onSelected(wasSelected, autoSelectBottomMostChild, wasParentSelected);
}

void Button::deselect() { m_bSelected = false; }

void Button::resetAnimations() { setMoveAwayState(MOVE_AWAY_STATE::MOVE_CENTER, false); }

void Button::onClicked() {
    engine->getSound()->play(osu->getSkin()->m_selectDifficulty);

    CBaseUIButton::onClicked();

    select(true, true);
}

void Button::onMouseInside() {
    CBaseUIButton::onMouseInside();

    // hover sound
    if(engine->getTime() > lastHoverSoundTime + 0.05f)  // to avoid earraep
    {
        if(engine->hasFocus()) engine->getSound()->play(osu->getSkin()->getMenuHover());

        lastHoverSoundTime = engine->getTime();
    }

    // hover anim
    anim->moveQuadOut(&m_fHoverOffsetAnimation, 1.0f, 1.0f * (1.0f - m_fHoverOffsetAnimation), true);

    // move the rest of the buttons away from hovered-over one
    const std::vector<CBaseUIElement *> &elements = m_view->getContainer()->getElements();
    bool foundCenter = false;
    for(size_t i = 0; i < elements.size(); i++) {
        Button *b = dynamic_cast<Button *>(elements[i]);
        if(b != NULL)  // sanity
        {
            if(b == this) {
                foundCenter = true;
                b->setMoveAwayState(MOVE_AWAY_STATE::MOVE_CENTER);
            } else
                b->setMoveAwayState(foundCenter ? MOVE_AWAY_STATE::MOVE_DOWN : MOVE_AWAY_STATE::MOVE_UP);
        }
    }
}

void Button::onMouseOutside() {
    CBaseUIButton::onMouseOutside();

    // reverse hover anim
    anim->moveQuadOut(&m_fHoverOffsetAnimation, 0.0f, 1.0f * m_fHoverOffsetAnimation, true);

    // only reset all other elements' state if we still should do so (possible frame delay of onMouseOutside coming
    // together with the next element already getting onMouseInside!)
    if(m_moveAwayState == MOVE_AWAY_STATE::MOVE_CENTER) {
        const std::vector<CBaseUIElement *> &elements = m_view->getContainer()->getElements();
        for(size_t i = 0; i < elements.size(); i++) {
            Button *b = dynamic_cast<Button *>(elements[i]);
            if(b != NULL)  // sanity check
                b->setMoveAwayState(MOVE_AWAY_STATE::MOVE_CENTER);
        }
    }
}

void Button::setTargetRelPosY(float targetRelPosY) {
    m_fTargetRelPosY = targetRelPosY;
    setRelPosY(m_fTargetRelPosY);
}

Vector2 Button::getActualOffset() const {
    const float hd2xMultiplier = osu->getSkin()->isMenuButtonBackground2x() ? 2.0f : 1.0f;
    const float correctedMarginPixelsY =
        (2 * marginPixelsY + osu->getSkin()->getMenuButtonBackground()->getHeight() / hd2xMultiplier - 103.0f) / 2.0f;
    return Vector2((int)(marginPixelsX * m_fScale * hd2xMultiplier),
                   (int)(correctedMarginPixelsY * m_fScale * hd2xMultiplier));
}

void Button::setMoveAwayState(Button::MOVE_AWAY_STATE moveAwayState, bool animate) {
    m_moveAwayState = moveAwayState;

    // if we are not visible, destroy possibly existing animation
    if(!isVisible() || !animate) anim->deleteExistingAnimation(&m_fHoverMoveAwayAnimation);

    // only submit a new animation if we are visible, otherwise we would overwhelm the animationhandler with a shitload
    // of requests every time for every button (if we are not visible then we can just directly set the new value)
    switch(m_moveAwayState) {
        case MOVE_AWAY_STATE::MOVE_CENTER: {
            if(!isVisible() || !animate)
                m_fHoverMoveAwayAnimation = 0.0f;
            else
                anim->moveQuartOut(&m_fHoverMoveAwayAnimation, 0, 0.7f, isMouseInside() ? 0.0f : 0.05f,
                                   true);  // add a tiny bit of delay to avoid jerky movement if the cursor is briefly
                                           // between songbuttons while moving
        } break;

        case MOVE_AWAY_STATE::MOVE_UP: {
            if(!isVisible() || !animate)
                m_fHoverMoveAwayAnimation = -1.0f;
            else
                anim->moveQuartOut(&m_fHoverMoveAwayAnimation, -1.0f, 0.7f, true);
        } break;

        case MOVE_AWAY_STATE::MOVE_DOWN: {
            if(!isVisible() || !animate)
                m_fHoverMoveAwayAnimation = 1.0f;
            else
                anim->moveQuartOut(&m_fHoverMoveAwayAnimation, 1.0f, 0.7f, true);
        } break;
    }
}

Color Button::getActiveBackgroundColor() const {
    return COLOR(clamp<int>(osu_songbrowser_button_active_color_a.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_active_color_r.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_active_color_g.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_active_color_b.getInt(), 0, 255));
}

Color Button::getInactiveBackgroundColor() const {
    return COLOR(clamp<int>(osu_songbrowser_button_inactive_color_a.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_inactive_color_r.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_inactive_color_g.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_inactive_color_b.getInt(), 0, 255));
}
