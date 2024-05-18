#include "NotificationOverlay.h"

#include "AnimationHandler.h"
#include "ConVar.h"
#include "Engine.h"
#include "Keyboard.h"
#include "Osu.h"
#include "ResourceManager.h"

ConVar osu_notification_duration("osu_notification_duration", 1.25f, FCVAR_DEFAULT);

NotificationOverlay::NotificationOverlay() : OsuScreen() {
    m_bWaitForKey = false;
    m_bWaitForKeyDisallowsLeftClick = false;
    m_bConsumeNextChar = false;
    m_keyListener = NULL;
}

void NotificationOverlay::draw(Graphics *g) {
    if(!isVisible()) return;

    if(m_bWaitForKey) {
        g->setColor(0x22ffffff);
        g->setAlpha((m_notification1.backgroundAnim / 0.5f) * 0.13f);
        g->fillRect(0, 0, osu->getScreenWidth(), osu->getScreenHeight());
    }

    drawNotificationBackground(g, m_notification2);
    drawNotificationBackground(g, m_notification1);
    drawNotificationText(g, m_notification2);
    drawNotificationText(g, m_notification1);
}

void NotificationOverlay::drawNotificationText(Graphics *g, NotificationOverlay::NOTIFICATION &n) {
    McFont *font = osu->getSubTitleFont();
    int height = font->getHeight() * 2;
    int stringWidth = font->getStringWidth(n.text);

    g->pushTransform();
    {
        g->setColor(0xff000000);
        g->setAlpha(n.alpha);
        g->translate((int)(osu->getScreenWidth() / 2 - stringWidth / 2 + 1),
                     (int)(osu->getScreenHeight() / 2 + font->getHeight() / 2 + n.fallAnim * height * 0.15f + 1));
        g->drawString(font, n.text);

        g->setColor(n.textColor);
        g->setAlpha(n.alpha);
        g->translate(-1, -1);
        g->drawString(font, n.text);
    }
    g->popTransform();
}

void NotificationOverlay::drawNotificationBackground(Graphics *g, NotificationOverlay::NOTIFICATION &n) {
    McFont *font = osu->getSubTitleFont();
    int height = font->getHeight() * 2 * n.backgroundAnim;

    g->setColor(0xff000000);
    g->setAlpha(n.alpha * 0.75f);
    g->fillRect(0, osu->getScreenHeight() / 2 - height / 2, osu->getScreenWidth(), height);
}

void NotificationOverlay::onKeyDown(KeyboardEvent &e) {
    if(!isVisible()) return;

    // escape always stops waiting for a key
    if(e.getKeyCode() == KEY_ESCAPE) {
        if(m_bWaitForKey) e.consume();

        stopWaitingForKey();
    }

    // key binding logic
    if(m_bWaitForKey) {
        /*
        float prevDuration = osu_notification_duration.getFloat();
        osu_notification_duration.setValue(0.85f);
        addNotification(UString::format("The new key is (ASCII Keycode): %lu", e.getKeyCode()));
        osu_notification_duration.setValue(prevDuration); // restore convar
        */

        // HACKHACK: prevent left mouse click bindings if relevant
        if(env->getOS() == Environment::OS::OS_WINDOWS && m_bWaitForKeyDisallowsLeftClick &&
           e.getKeyCode() == 0x01)  // 0x01 == VK_LBUTTON
            stopWaitingForKey();
        else {
            stopWaitingForKey(true);

            debugLog("keyCode = %lu\n", e.getKeyCode());

            if(m_keyListener != NULL) m_keyListener->onKey(e);
        }

        e.consume();
    }

    if(m_bWaitForKey) e.consume();
}

void NotificationOverlay::onKeyUp(KeyboardEvent &e) {
    if(!isVisible()) return;

    if(m_bWaitForKey) e.consume();
}

void NotificationOverlay::onChar(KeyboardEvent &e) {
    if(m_bWaitForKey || m_bConsumeNextChar) e.consume();

    m_bConsumeNextChar = false;
}

void NotificationOverlay::addNotification(UString text, Color textColor, bool waitForKey, float duration) {
    const float notificationDuration = (duration < 0.0f ? osu_notification_duration.getFloat() : duration);

    // swap effect
    if(isVisible()) {
        m_notification2.text = m_notification1.text;
        m_notification2.textColor = 0xffffffff;

        m_notification2.time = 0.0f;
        m_notification2.alpha = 0.5f;
        m_notification2.backgroundAnim = 1.0f;
        m_notification2.fallAnim = 0.0f;

        anim->deleteExistingAnimation(&m_notification1.alpha);

        anim->moveQuadIn(&m_notification2.fallAnim, 1.0f, 0.2f, 0.0f, true);
        anim->moveQuadIn(&m_notification2.alpha, 0.0f, 0.2f, 0.0f, true);
    }

    // build new notification
    m_bWaitForKey = waitForKey;
    m_bConsumeNextChar = m_bWaitForKey;

    float fadeOutTime = 0.4f;

    m_notification1.text = text;
    m_notification1.textColor = textColor;

    if(!waitForKey)
        m_notification1.time = engine->getTime() + notificationDuration + fadeOutTime;
    else
        m_notification1.time = 0.0f;

    m_notification1.alpha = 0.0f;
    m_notification1.backgroundAnim = 0.5f;
    m_notification1.fallAnim = 0.0f;

    // animations
    if(isVisible())
        m_notification1.alpha = 1.0f;
    else
        anim->moveLinear(&m_notification1.alpha, 1.0f, 0.075f, true);

    if(!waitForKey) anim->moveQuadOut(&m_notification1.alpha, 0.0f, fadeOutTime, notificationDuration, false);

    anim->moveQuadOut(&m_notification1.backgroundAnim, 1.0f, 0.15f, 0.0f, true);
}

void NotificationOverlay::stopWaitingForKey(bool stillConsumeNextChar) {
    m_bWaitForKey = false;
    m_bWaitForKeyDisallowsLeftClick = false;
    m_bConsumeNextChar = stillConsumeNextChar;
}

bool NotificationOverlay::isVisible() {
    return engine->getTime() < m_notification1.time || engine->getTime() < m_notification2.time || m_bWaitForKey;
}
