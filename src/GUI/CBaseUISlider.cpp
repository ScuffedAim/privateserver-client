#include "CBaseUISlider.h"

#include "AnimationHandler.h"
#include "Engine.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Osu.h"
#include "Skin.h"
#include "SoundEngine.h"

using namespace std;

CBaseUISlider::CBaseUISlider(float xPos, float yPos, float xSize, float ySize, UString name)
    : CBaseUIElement(xPos, yPos, xSize, ySize, name) {
    this->grabs_clicks = true;

    this->bDrawFrame = true;
    this->bDrawBackground = true;
    this->bHorizontal = false;
    this->bHasChanged = false;
    this->bAnimated = true;
    this->bLiveUpdate = false;
    this->bAllowMouseWheel = true;

    this->backgroundColor = COLOR(255, 0, 0, 0);
    this->frameColor = COLOR(255, 255, 255, 255);

    this->fCurValue = 0.0f;
    this->fCurPercent = 0.0f;
    this->fMinValue = 0.0f;
    this->fMaxValue = 1.0f;
    this->fKeyDelta = 0.1f;

    this->vBlockSize = Vector2(xSize < ySize ? xSize : ySize, xSize < ySize ? xSize : ySize);

    this->sliderChangeCallback = NULL;

    this->setOrientation(xSize > ySize);
}

void CBaseUISlider::draw(Graphics *g) {
    if(!this->bVisible) return;

    // draw background
    if(this->bDrawBackground) {
        g->setColor(this->backgroundColor);
        g->fillRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y + 1);
    }

    // draw frame
    g->setColor(this->frameColor);
    if(this->bDrawFrame) g->drawRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y + 1);

    // draw sliding line
    if(!this->bHorizontal)
        g->drawLine(this->vPos.x + this->vSize.x / 2.0f, this->vPos.y + this->vBlockSize.y / 2.0,
                    this->vPos.x + this->vSize.x / 2.0f, this->vPos.y + this->vSize.y - this->vBlockSize.y / 2.0f);
    else
        g->drawLine(this->vPos.x + (this->vBlockSize.x - 1) / 2 + 1, this->vPos.y + this->vSize.y / 2.0f + 1,
                    this->vPos.x + this->vSize.x - (this->vBlockSize.x - 1) / 2,
                    this->vPos.y + this->vSize.y / 2.0f + 1);

    this->drawBlock(g);
}

void CBaseUISlider::drawBlock(Graphics *g) {
    // draw block
    Vector2 center =
        this->vPos +
        Vector2(this->vBlockSize.x / 2 + (this->vSize.x - this->vBlockSize.x) * this->getPercent(), this->vSize.y / 2);
    Vector2 topLeft = center - this->vBlockSize / 2;
    Vector2 topRight = center + Vector2(this->vBlockSize.x / 2 + 1, -this->vBlockSize.y / 2);
    Vector2 halfLeft = center + Vector2(-this->vBlockSize.x / 2, 1);
    Vector2 halfRight = center + Vector2(this->vBlockSize.x / 2 + 1, 1);
    Vector2 bottomLeft = center + Vector2(-this->vBlockSize.x / 2, this->vBlockSize.y / 2 + 1);
    Vector2 bottomRight = center + Vector2(this->vBlockSize.x / 2 + 1, this->vBlockSize.y / 2 + 1);

    g->drawQuad(topLeft, topRight, halfRight + Vector2(0, 1), halfLeft + Vector2(0, 1), COLOR(255, 255, 255, 255),
                COLOR(255, 255, 255, 255), COLOR(255, 241, 241, 241), COLOR(255, 241, 241, 241));

    g->drawQuad(halfLeft, halfRight, bottomRight, bottomLeft, COLOR(255, 225, 225, 225), COLOR(255, 225, 225, 225),
                COLOR(255, 255, 255, 255), COLOR(255, 255, 255, 255));
}

void CBaseUISlider::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    CBaseUIElement::mouse_update(propagate_clicks);

    Vector2 mousepos = engine->getMouse()->getPos();

    // handle moving
    if(this->bActive) {
        // calculate new values
        if(!this->bHorizontal) {
            if(this->bAnimated) {
                anim->moveQuadOut(
                    &this->vBlockPos.y,
                    clamp<float>(mousepos.y - this->vGrabBackup.y, 0.0f, this->vSize.y - this->vBlockSize.y), 0.10f, 0,
                    true);
            } else {
                this->vBlockPos.y =
                    clamp<float>(mousepos.y - this->vGrabBackup.y, 0.0f, this->vSize.y - this->vBlockSize.y);
            }

            this->fCurPercent =
                clamp<float>(1.0f - (std::round(this->vBlockPos.y) / (this->vSize.y - this->vBlockSize.y)), 0.0f, 1.0f);
        } else {
            if(this->bAnimated) {
                anim->moveQuadOut(
                    &this->vBlockPos.x,
                    clamp<float>(mousepos.x - this->vGrabBackup.x, 0.0f, this->vSize.x - this->vBlockSize.x), 0.10f, 0,
                    true);
            } else {
                this->vBlockPos.x =
                    clamp<float>(mousepos.x - this->vGrabBackup.x, 0.0f, this->vSize.x - this->vBlockSize.x);
            }

            this->fCurPercent =
                clamp<float>(std::round(this->vBlockPos.x) / (this->vSize.x - this->vBlockSize.x), 0.0f, 1.0f);
        }

        // set new value
        if(this->bAnimated) {
            if(this->bLiveUpdate) {
                this->setValue(lerp<float>(this->fMinValue, this->fMaxValue, this->fCurPercent), false);
            } else {
                this->fCurValue = lerp<float>(this->fMinValue, this->fMaxValue, this->fCurPercent);
            }
        } else {
            this->setValue(lerp<float>(this->fMinValue, this->fMaxValue, this->fCurPercent), false);
        }

        this->bHasChanged = true;
    } else {
        // handle mouse wheel
        if(this->bMouseInside && this->bAllowMouseWheel) {
            int wheelDelta = engine->getMouse()->getWheelDeltaVertical();
            if(wheelDelta != 0) {
                const int multiplier = max(1, std::abs(wheelDelta) / 120);

                if(wheelDelta > 0) {
                    this->setValue(this->fCurValue + this->fKeyDelta * multiplier, this->bAnimated);
                } else {
                    this->setValue(this->fCurValue - this->fKeyDelta * multiplier, this->bAnimated);
                }
            }
        }
    }

    // handle animation value settings after mouse release
    if(!this->bActive) {
        if(anim->isAnimating(&this->vBlockPos.x)) {
            this->fCurPercent =
                clamp<float>(std::round(this->vBlockPos.x) / (this->vSize.x - this->vBlockSize.x), 0.0f, 1.0f);

            if(this->bLiveUpdate)
                this->setValue(lerp<float>(this->fMinValue, this->fMaxValue, this->fCurPercent), false);
            else
                this->fCurValue = lerp<float>(this->fMinValue, this->fMaxValue, this->fCurPercent);
        }

        if(anim->isAnimating(&this->vBlockPos.y)) {
            this->fCurPercent =
                clamp<float>(1.0f - (std::round(this->vBlockPos.y) / (this->vSize.y - this->vBlockSize.y)), 0.0f, 1.0f);

            if(this->bLiveUpdate)
                this->setValue(lerp<float>(this->fMinValue, this->fMaxValue, this->fCurPercent), false);
            else
                this->fCurValue = lerp<float>(this->fMinValue, this->fMaxValue, this->fCurPercent);
        }
    }
}

void CBaseUISlider::onKeyDown(KeyboardEvent &e) {
    if(!this->bVisible) return;

    if(this->isMouseInside()) {
        if(e == KEY_LEFT) {
            this->setValue(this->getFloat() - this->fKeyDelta, false);
            e.consume();
        } else if(e == KEY_RIGHT) {
            this->setValue(this->getFloat() + this->fKeyDelta, false);
            e.consume();
        }
    }
}

void CBaseUISlider::fireChangeCallback() {
    if(this->sliderChangeCallback != NULL) this->sliderChangeCallback(this);
}

void CBaseUISlider::updateBlockPos() {
    if(!this->bHorizontal)
        this->vBlockPos.x = this->vSize.x / 2.0f - this->vBlockSize.x / 2.0f;
    else
        this->vBlockPos.y = this->vSize.y / 2.0f - this->vBlockSize.y / 2.0f;
}

CBaseUISlider *CBaseUISlider::setBounds(float minValue, float maxValue) {
    this->fMinValue = minValue;
    this->fMaxValue = maxValue;

    this->fKeyDelta = (this->fMaxValue - this->fMinValue) / 10.0f;

    return this;
}

CBaseUISlider *CBaseUISlider::setValue(float value, bool animate, bool call_callback) {
    bool changeCallbackCheck = false;
    if(value != this->fCurValue) {
        changeCallbackCheck = true;
        this->bHasChanged = true;
    }

    this->fCurValue = clamp<float>(value, this->fMinValue, this->fMaxValue);
    float percent = this->getPercent();

    if(!this->bHorizontal) {
        if(animate)
            anim->moveQuadOut(&this->vBlockPos.y, (this->vSize.y - this->vBlockSize.y) * (1.0f - percent), 0.2f, 0,
                              true);
        else
            this->vBlockPos.y = (this->vSize.y - this->vBlockSize.y) * (1.0f - percent);
    } else {
        if(animate)
            anim->moveQuadOut(&this->vBlockPos.x, (this->vSize.x - this->vBlockSize.x) * percent, 0.2f, 0, true);
        else
            this->vBlockPos.x = (this->vSize.x - this->vBlockSize.x) * percent;
    }

    if(call_callback && changeCallbackCheck && this->sliderChangeCallback != NULL) {
        this->sliderChangeCallback(this);

        if(this->bHasChanged) {
            if(this->fLastSoundPlayTime + 0.05f < engine->getTime()) {
                engine->getSound()->play(osu->getSkin()->sliderbar, 0.f, 1.f + 0.05f * percent);
                this->fLastSoundPlayTime = engine->getTime();
            }
        }
    }

    this->updateBlockPos();

    return this;
}

CBaseUISlider *CBaseUISlider::setInitialValue(float value) {
    this->fCurValue = clamp<float>(value, this->fMinValue, this->fMaxValue);
    float percent = this->getPercent();

    if(this->fCurValue == this->fMaxValue) percent = 1.0f;

    if(!this->bHorizontal)
        this->vBlockPos.y = (this->vSize.y - this->vBlockSize.y) * (1.0f - percent);
    else
        this->vBlockPos.x = (this->vSize.x - this->vBlockSize.x) * percent;

    this->updateBlockPos();

    return this;
}

void CBaseUISlider::setBlockSize(float xSize, float ySize) { this->vBlockSize = Vector2(xSize, ySize); }

float CBaseUISlider::getPercent() {
    return clamp<float>((this->fCurValue - this->fMinValue) / (std::abs(this->fMaxValue - this->fMinValue)), 0.0f,
                        1.0f);
}

bool CBaseUISlider::hasChanged() {
    if(anim->isAnimating(&this->vBlockPos.x)) return true;
    if(this->bHasChanged) {
        this->bHasChanged = false;
        return true;
    }
    return false;
}

void CBaseUISlider::onFocusStolen() { this->bBusy = false; }

void CBaseUISlider::onMouseUpInside() {
    this->bBusy = false;

    if(this->fCurValue != this->fPrevValue && this->sliderChangeCallback != NULL) this->sliderChangeCallback(this);
}

void CBaseUISlider::onMouseUpOutside() {
    this->bBusy = false;

    if(this->fCurValue != this->fPrevValue && this->sliderChangeCallback != NULL) this->sliderChangeCallback(this);
}

void CBaseUISlider::onMouseDownInside() {
    this->fPrevValue = this->fCurValue;

    if(McRect(this->vPos.x + this->vBlockPos.x, this->vPos.y + this->vBlockPos.y, this->vBlockSize.x,
              this->vBlockSize.y)
           .contains(engine->getMouse()->getPos()))
        this->vGrabBackup = engine->getMouse()->getPos() - this->vBlockPos;
    else
        this->vGrabBackup = this->vPos + this->vBlockSize / 2;

    this->bBusy = true;
}

void CBaseUISlider::onResized() { this->setValue(this->getFloat(), false); }
