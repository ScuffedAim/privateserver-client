#pragma once
#include "CBaseUIElement.h"

class McFont;

class CBaseUIButton : public CBaseUIElement {
   public:
    CBaseUIButton(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = "",
                  UString text = "");
    virtual ~CBaseUIButton() { ; }

    virtual void draw(Graphics *g);

    void click() { this->onClicked(); }

    // callbacks, either void or with ourself as the argument
    typedef fastdelegate::FastDelegate0<> ButtonClickVoidCallback;
    CBaseUIButton *setClickCallback(ButtonClickVoidCallback clickCallback) {
        this->clickVoidCallback = clickCallback;
        return this;
    }
    typedef fastdelegate::FastDelegate1<CBaseUIButton *> ButtonClickCallback;
    CBaseUIButton *setClickCallback(ButtonClickCallback clickCallback) {
        this->clickCallback = clickCallback;
        return this;
    }

    // set
    CBaseUIButton *setDrawFrame(bool drawFrame) {
        this->bDrawFrame = drawFrame;
        return this;
    }
    CBaseUIButton *setDrawBackground(bool drawBackground) {
        this->bDrawBackground = drawBackground;
        return this;
    }
    CBaseUIButton *setTextLeft(bool textLeft) {
        this->bTextLeft = textLeft;
        this->updateStringMetrics();
        return this;
    }

    CBaseUIButton *setFrameColor(Color frameColor) {
        this->frameColor = frameColor;
        return this;
    }
    CBaseUIButton *setBackgroundColor(Color backgroundColor) {
        this->backgroundColor = backgroundColor;
        return this;
    }
    CBaseUIButton *setTextColor(Color textColor) {
        this->textColor = textColor;
        this->textBrightColor = this->textDarkColor = 0;
        return this;
    }
    CBaseUIButton *setTextBrightColor(Color textBrightColor) {
        this->textBrightColor = textBrightColor;
        return this;
    }
    CBaseUIButton *setTextDarkColor(Color textDarkColor) {
        this->textDarkColor = textDarkColor;
        return this;
    }

    CBaseUIButton *setText(UString text) {
        this->sText = text;
        this->updateStringMetrics();
        return this;
    }
    CBaseUIButton *setFont(McFont *font) {
        this->font = font;
        this->updateStringMetrics();
        return this;
    }

    CBaseUIButton *setSizeToContent(int horizontalBorderSize = 1, int verticalBorderSize = 1) {
        this->setSize(this->fStringWidth + 2 * horizontalBorderSize, this->fStringHeight + 2 * verticalBorderSize);
        return this;
    }
    CBaseUIButton *setWidthToContent(int horizontalBorderSize = 1) {
        this->setSizeX(this->fStringWidth + 2 * horizontalBorderSize);
        return this;
    }

    // get
    inline Color getFrameColor() const { return this->frameColor; }
    inline Color getBackgroundColor() const { return this->backgroundColor; }
    inline Color getTextColor() const { return this->textColor; }
    inline UString getText() const { return this->sText; }
    inline McFont *getFont() const { return this->font; }
    inline ButtonClickCallback getClickCallback() const { return this->clickCallback; }
    inline bool isTextLeft() const { return this->bTextLeft; }

    // events
    virtual void onMouseUpInside();
    virtual void onResized() { this->updateStringMetrics(); }

   protected:
    virtual void onClicked();

    virtual void drawText(Graphics *g);

    void drawHoverRect(Graphics *g, int distance);

    void updateStringMetrics();

    bool bDrawFrame;
    bool bDrawBackground;
    bool bTextLeft;

    Color frameColor;
    Color backgroundColor;
    Color textColor;
    Color textBrightColor;
    Color textDarkColor;

    McFont *font;
    UString sText;
    float fStringWidth;
    float fStringHeight;

    ButtonClickVoidCallback clickVoidCallback;
    ButtonClickCallback clickCallback;
};
