#include "PauseMenu.h"

#include "AnimationHandler.h"
#include "Bancho.h"
#include "Beatmap.h"
#include "CBaseUIContainer.h"
#include "Chat.h"
#include "ConVar.h"
#include "Engine.h"
#include "Environment.h"
#include "HUD.h"
#include "KeyBindings.h"
#include "Keyboard.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "RichPresence.h"
#include "Skin.h"
#include "SoundEngine.h"
#include "UIPauseMenuButton.h"

PauseMenu::PauseMenu() : OsuScreen() {
    this->bScheduledVisibility = false;
    this->bScheduledVisibilityChange = false;

    this->selectedButton = NULL;
    this->fWarningArrowsAnimStartTime = 0.0f;
    this->fWarningArrowsAnimAlpha = 0.0f;
    this->fWarningArrowsAnimX = 0.0f;
    this->fWarningArrowsAnimY = 0.0f;
    this->bInitialWarningArrowFlyIn = true;

    this->bContinueEnabled = true;
    this->bClick1Down = false;
    this->bClick2Down = false;

    this->fDimAnim = 0.0f;

    this->setSize(osu->getScreenWidth(), osu->getScreenHeight());

    UIPauseMenuButton *continueButton =
        this->addButton([]() -> Image * { return osu->getSkin()->getPauseContinue(); }, "Resume");
    UIPauseMenuButton *retryButton =
        this->addButton([]() -> Image * { return osu->getSkin()->getPauseRetry(); }, "Retry");
    UIPauseMenuButton *backButton = this->addButton([]() -> Image * { return osu->getSkin()->getPauseBack(); }, "Quit");

    continueButton->setClickCallback(fastdelegate::MakeDelegate(this, &PauseMenu::onContinueClicked));
    retryButton->setClickCallback(fastdelegate::MakeDelegate(this, &PauseMenu::onRetryClicked));
    backButton->setClickCallback(fastdelegate::MakeDelegate(this, &PauseMenu::onBackClicked));

    this->updateLayout();
}

void PauseMenu::draw(Graphics *g) {
    const bool isAnimating = anim->isAnimating(&this->fDimAnim);
    if(!this->bVisible && !isAnimating) return;

    // draw dim
    if(cv_pause_dim_background.getBool()) {
        g->setColor(COLORf(this->fDimAnim * cv_pause_dim_alpha.getFloat(), 0.078f, 0.078f, 0.078f));
        g->fillRect(0, 0, osu->getScreenWidth(), osu->getScreenHeight());
    }

    // draw background image
    if((this->bVisible || isAnimating)) {
        Image *image = NULL;
        if(this->bContinueEnabled)
            image = osu->getSkin()->getPauseOverlay();
        else
            image = osu->getSkin()->getFailBackground();

        if(image != osu->getSkin()->getMissingTexture()) {
            const float scale = Osu::getImageScaleToFillResolution(image, osu->getScreenSize());
            const Vector2 centerTrans = (osu->getScreenSize() / 2);

            g->setColor(COLORf(this->fDimAnim, 1.0f, 1.0f, 1.0f));
            g->pushTransform();
            {
                g->scale(scale, scale);
                g->translate((int)centerTrans.x, (int)centerTrans.y);
                g->drawImage(image);
            }
            g->popTransform();
        }
    }

    // draw buttons
    for(int i = 0; i < this->buttons.size(); i++) {
        this->buttons[i]->setAlpha(1.0f - (1.0f - this->fDimAnim) * (1.0f - this->fDimAnim) * (1.0f - this->fDimAnim));
    }
    OsuScreen::draw(g);

    // draw selection arrows
    if(this->selectedButton != NULL) {
        const Color arrowColor = COLOR(255, 0, 114, 255);
        float animation = fmod((float)(engine->getTime() - this->fWarningArrowsAnimStartTime) * 3.2f, 2.0f);
        if(animation > 1.0f) animation = 2.0f - animation;

        animation = -animation * (animation - 2);  // quad out
        const float offset = osu->getUIScale(20.0f + 45.0f * animation);

        g->setColor(arrowColor);
        g->setAlpha(this->fWarningArrowsAnimAlpha * this->fDimAnim);
        osu->getHUD()->drawWarningArrow(g,
                                        Vector2(this->fWarningArrowsAnimX, this->fWarningArrowsAnimY) +
                                            Vector2(0, this->selectedButton->getSize().y / 2) - Vector2(offset, 0),
                                        false, false);
        osu->getHUD()->drawWarningArrow(
            g,
            Vector2(osu->getScreenWidth() - this->fWarningArrowsAnimX, this->fWarningArrowsAnimY) +
                Vector2(0, this->selectedButton->getSize().y / 2) + Vector2(offset, 0),
            true, false);
    }
}

void PauseMenu::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;

    // update and focus handling
    OsuScreen::mouse_update(propagate_clicks);

    if(this->bScheduledVisibilityChange) {
        this->bScheduledVisibilityChange = false;
        this->setVisible(this->bScheduledVisibility);
    }

    if(anim->isAnimating(&this->fWarningArrowsAnimX)) this->fWarningArrowsAnimStartTime = engine->getTime();
}

void PauseMenu::onContinueClicked() {
    if(!this->bContinueEnabled) return;
    if(anim->isAnimating(&this->fDimAnim)) return;

    engine->getSound()->play(osu->getSkin()->clickPauseContinue);
    osu->getSelectedBeatmap()->pause();

    this->scheduleVisibilityChange(false);
}

void PauseMenu::onRetryClicked() {
    if(anim->isAnimating(&this->fDimAnim)) return;

    engine->getSound()->play(osu->getSkin()->clickPauseRetry);
    osu->getSelectedBeatmap()->restart();

    this->scheduleVisibilityChange(false);
}

void PauseMenu::onBackClicked() {
    if(anim->isAnimating(&this->fDimAnim)) return;

    engine->getSound()->play(osu->getSkin()->clickPauseBack);
    osu->getSelectedBeatmap()->stop();

    this->scheduleVisibilityChange(false);
}

void PauseMenu::onSelectionChange() {
    if(this->selectedButton != NULL) {
        if(this->bInitialWarningArrowFlyIn) {
            this->bInitialWarningArrowFlyIn = false;

            this->fWarningArrowsAnimY = this->selectedButton->getPos().y;
            this->fWarningArrowsAnimX = this->selectedButton->getPos().x - osu->getUIScale(170.0f);

            anim->moveLinear(&this->fWarningArrowsAnimAlpha, 1.0f, 0.3f);
            anim->moveQuadIn(&this->fWarningArrowsAnimX, this->selectedButton->getPos().x, 0.3f);
        } else
            this->fWarningArrowsAnimX = this->selectedButton->getPos().x;

        anim->moveQuadOut(&this->fWarningArrowsAnimY, this->selectedButton->getPos().y, 0.1f);
    }
}

void PauseMenu::onKeyDown(KeyboardEvent &e) {
    OsuScreen::onKeyDown(e);  // only used for options menu
    if(!this->bVisible || e.isConsumed()) return;

    if(e == (KEYCODE)cv_LEFT_CLICK.getInt() || e == (KEYCODE)cv_RIGHT_CLICK.getInt() ||
       e == (KEYCODE)cv_LEFT_CLICK_2.getInt() || e == (KEYCODE)cv_RIGHT_CLICK_2.getInt()) {
        bool fireButtonClick = false;
        if((e == (KEYCODE)cv_LEFT_CLICK.getInt() || e == (KEYCODE)cv_LEFT_CLICK_2.getInt()) && !this->bClick1Down) {
            this->bClick1Down = true;
            fireButtonClick = true;
        }
        if((e == (KEYCODE)cv_RIGHT_CLICK.getInt() || e == (KEYCODE)cv_RIGHT_CLICK_2.getInt()) && !this->bClick2Down) {
            this->bClick2Down = true;
            fireButtonClick = true;
        }
        if(fireButtonClick) {
            for(int i = 0; i < this->buttons.size(); i++) {
                if(this->buttons[i]->isMouseInside()) {
                    this->buttons[i]->click();
                    break;
                }
            }
        }
    }

    // handle arrow keys selection
    if(this->buttons.size() > 0) {
        if(!engine->getKeyboard()->isAltDown() && e == KEY_DOWN) {
            UIPauseMenuButton *nextSelectedButton = this->buttons[0];

            // get first visible button
            for(int i = 0; i < this->buttons.size(); i++) {
                if(!this->buttons[i]->isVisible()) continue;

                nextSelectedButton = this->buttons[i];
                break;
            }

            // next selection logic
            bool next = false;
            for(int i = 0; i < this->buttons.size(); i++) {
                if(!this->buttons[i]->isVisible()) continue;

                if(next) {
                    nextSelectedButton = this->buttons[i];
                    break;
                }
                if(this->selectedButton == this->buttons[i]) next = true;
            }
            this->selectedButton = nextSelectedButton;
            this->onSelectionChange();
        }

        if(!engine->getKeyboard()->isAltDown() && e == KEY_UP) {
            UIPauseMenuButton *nextSelectedButton = this->buttons[this->buttons.size() - 1];

            // get first visible button
            for(int i = this->buttons.size() - 1; i >= 0; i--) {
                if(!this->buttons[i]->isVisible()) continue;

                nextSelectedButton = this->buttons[i];
                break;
            }

            // next selection logic
            bool next = false;
            for(int i = this->buttons.size() - 1; i >= 0; i--) {
                if(!this->buttons[i]->isVisible()) continue;

                if(next) {
                    nextSelectedButton = this->buttons[i];
                    break;
                }
                if(this->selectedButton == this->buttons[i]) next = true;
            }
            this->selectedButton = nextSelectedButton;
            this->onSelectionChange();
        }

        if(this->selectedButton != NULL && e == KEY_ENTER) this->selectedButton->click();
    }

    // consume ALL events, except for a few special binds which are allowed through (e.g. for unpause or changing the
    // local offset in Osu.cpp)
    if(e != KEY_ESCAPE && e != (KEYCODE)cv_GAME_PAUSE.getInt() && e != (KEYCODE)cv_INCREASE_LOCAL_OFFSET.getInt() &&
       e != (KEYCODE)cv_DECREASE_LOCAL_OFFSET.getInt())
        e.consume();
}

void PauseMenu::onKeyUp(KeyboardEvent &e) {
    if(e == (KEYCODE)cv_LEFT_CLICK.getInt() || e == (KEYCODE)cv_LEFT_CLICK_2.getInt()) this->bClick1Down = false;

    if(e == (KEYCODE)cv_RIGHT_CLICK.getInt() || e == (KEYCODE)cv_RIGHT_CLICK_2.getInt()) this->bClick2Down = false;
}

void PauseMenu::onChar(KeyboardEvent &e) {
    if(!this->bVisible) return;

    e.consume();
}

void PauseMenu::scheduleVisibilityChange(bool visible) {
    if(visible != this->bVisible) {
        this->bScheduledVisibilityChange = true;
        this->bScheduledVisibility = visible;
    }

    // HACKHACK:
    if(!visible) this->setContinueEnabled(true);
}

void PauseMenu::updateLayout() {
    const float height = (osu->getScreenHeight() / (float)this->buttons.size());
    const float half = (this->buttons.size() - 1) / 2.0f;

    float maxWidth = 0.0f;
    float maxHeight = 0.0f;
    for(int i = 0; i < this->buttons.size(); i++) {
        Image *img = this->buttons[i]->getImage();
        if(img == NULL) img = osu->getSkin()->getMissingTexture();

        const float scale = osu->getUIScale(256) / (411.0f * (osu->getSkin()->isPauseContinue2x() ? 2.0f : 1.0f));

        this->buttons[i]->setBaseScale(scale, scale);
        this->buttons[i]->setSize(img->getWidth() * scale, img->getHeight() * scale);

        if(this->buttons[i]->getSize().x > maxWidth) maxWidth = this->buttons[i]->getSize().x;
        if(this->buttons[i]->getSize().y > maxHeight) maxHeight = this->buttons[i]->getSize().y;
    }

    for(int i = 0; i < this->buttons.size(); i++) {
        Vector2 newPos =
            Vector2(osu->getScreenWidth() / 2.0f - maxWidth / 2, (i + 1) * height - height / 2.0f - maxHeight / 2.0f);

        const float pinch = max(0.0f, (height / 2.0f - maxHeight / 2.0f));
        if((float)i < half)
            newPos.y += pinch;
        else if((float)i > half)
            newPos.y -= pinch;

        this->buttons[i]->setPos(newPos);
        this->buttons[i]->setSize(maxWidth, maxHeight);
    }

    this->onSelectionChange();
}

void PauseMenu::onResolutionChange(Vector2 newResolution) {
    this->setSize(newResolution);
    this->updateLayout();
}

CBaseUIContainer *PauseMenu::setVisible(bool visible) {
    this->bVisible = visible;

    if(osu->isInPlayMode()) {
        this->setContinueEnabled(!osu->getSelectedBeatmap()->hasFailed());
    } else {
        this->setContinueEnabled(true);
    }

    if(visible) {
        engine->getSound()->play(osu->getSkin()->pauseLoop);

        if(this->bContinueEnabled) {
            RichPresence::setBanchoStatus("Taking a break", PAUSED);

            if(!bancho.spectators.empty()) {
                Packet packet;
                packet.id = OUT_SPECTATE_FRAMES;
                write<i32>(&packet, 0);
                write<u16>(&packet, 0);
                write<u8>(&packet, LiveReplayBundle::Action::PAUSE);
                write<ScoreFrame>(&packet, ScoreFrame::get());
                write<u16>(&packet, osu->getSelectedBeatmap()->spectator_sequence++);
                send_packet(packet);
            }
        } else {
            RichPresence::setBanchoStatus("Failed", SUBMITTING);
        }
    } else {
        engine->getSound()->stop(osu->getSkin()->pauseLoop);

        RichPresence::onPlayStart();

        if(!bancho.spectators.empty()) {
            Packet packet;
            packet.id = OUT_SPECTATE_FRAMES;
            write<i32>(&packet, 0);
            write<u16>(&packet, 0);
            write<u8>(&packet, LiveReplayBundle::Action::UNPAUSE);
            write<ScoreFrame>(&packet, ScoreFrame::get());
            write<u16>(&packet, osu->getSelectedBeatmap()->spectator_sequence++);
            send_packet(packet);
        }
    }

    // HACKHACK: force disable mod selection screen in case it was open and the beatmap ended/failed
    osu->getModSelector()->setVisible(false);

    // reset
    this->selectedButton = NULL;
    this->bInitialWarningArrowFlyIn = true;
    this->fWarningArrowsAnimAlpha = 0.0f;
    this->bScheduledVisibility = visible;
    this->bScheduledVisibilityChange = false;

    if(this->bVisible) this->updateLayout();

    osu->updateConfineCursor();
    osu->updateWindowsKeyDisable();

    anim->moveQuadOut(&this->fDimAnim, (this->bVisible ? 1.0f : 0.0f),
                      cv_pause_anim_duration.getFloat() * (this->bVisible ? 1.0f - this->fDimAnim : this->fDimAnim),
                      true);
    osu->chat->updateVisibility();
    return this;
}

void PauseMenu::setContinueEnabled(bool continueEnabled) {
    this->bContinueEnabled = continueEnabled;
    if(this->buttons.size() > 0) this->buttons[0]->setVisible(this->bContinueEnabled);
}

UIPauseMenuButton *PauseMenu::addButton(std::function<Image *()> getImageFunc, UString btn_name) {
    UIPauseMenuButton *button = new UIPauseMenuButton(getImageFunc, 0, 0, 0, 0, btn_name);
    this->addBaseUIElement(button);
    this->buttons.push_back(button);
    return button;
}
