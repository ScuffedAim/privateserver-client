#pragma once
#include "CBaseUIButton.h"

class Image;

class UIPauseMenuButton : public CBaseUIButton {
   public:
    UIPauseMenuButton(std::function<Image *()> getImageFunc, float xPos, float yPos, float xSize, float ySize,
                      UString name);

    virtual void draw(Graphics *g);

    virtual void onMouseInside();
    virtual void onMouseOutside();

    void setBaseScale(float xScale, float yScale);
    void setAlpha(float alpha) { this->fAlpha = alpha; }

    Image *getImage() { return this->getImageFunc != NULL ? this->getImageFunc() : NULL; }

   private:
    Vector2 vScale;
    Vector2 vBaseScale;
    float fScaleMultiplier;

    float fAlpha;

    std::function<Image *()> getImageFunc;
};
