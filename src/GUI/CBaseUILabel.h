#pragma once
#include "CBaseUIElement.h"

class CBaseUILabel : public CBaseUIElement {
   public:
    enum TEXT_JUSTIFICATION { TEXT_JUSTIFICATION_LEFT, TEXT_JUSTIFICATION_CENTERED, TEXT_JUSTIFICATION_RIGHT };

   public:
    CBaseUILabel(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = "",
                 UString text = "");
    virtual ~CBaseUILabel() { ; }

    virtual void draw(Graphics *g);
    virtual void mouse_update(bool *propagate_clicks);

    // cancer
    void setRelSizeX(float x) { this->vmSize.x = x; }

    // set
    CBaseUILabel *setDrawFrame(bool drawFrame) {
        this->bDrawFrame = drawFrame;
        return this;
    }
    CBaseUILabel *setDrawBackground(bool drawBackground) {
        this->bDrawBackground = drawBackground;
        return this;
    }

    CBaseUILabel *setFrameColor(Color frameColor) {
        this->frameColor = frameColor;
        return this;
    }
    CBaseUILabel *setBackgroundColor(Color backgroundColor) {
        this->backgroundColor = backgroundColor;
        return this;
    }
    CBaseUILabel *setTextColor(Color textColor) {
        this->textColor = textColor;
        return this;
    }

    CBaseUILabel *setText(UString text) {
        this->sText = text;
        this->updateStringMetrics();
        return this;
    }
    CBaseUILabel *setFont(McFont *font) {
        this->font = font;
        this->updateStringMetrics();
        return this;
    }

    CBaseUILabel *setSizeToContent(int horizontalBorderSize = 1, int verticalBorderSize = 1) {
        this->setSize(this->fStringWidth + 2 * horizontalBorderSize, this->fStringHeight + 2 * verticalBorderSize);
        return this;
    }
    CBaseUILabel *setWidthToContent(int horizontalBorderSize = 1) {
        this->setSizeX(this->fStringWidth + 2 * horizontalBorderSize);
        return this;
    }
    CBaseUILabel *setTextJustification(TEXT_JUSTIFICATION textJustification) {
        this->textJustification = textJustification;
        return this;
    }

    // DEPRECATED! use setTextJustification() instead
    void setCenterText(bool centerText) { this->bCenterText = centerText; }

    // get
    inline Color getFrameColor() const { return this->frameColor; }
    inline Color getBackgroundColor() const { return this->backgroundColor; }
    inline Color getTextColor() const { return this->textColor; }
    inline McFont *getFont() const { return this->font; }
    inline UString getText() const { return this->sText; }

    virtual void onResized() { this->updateStringMetrics(); }

   protected:
    virtual void drawText(Graphics *g);

    void updateStringMetrics();

    McFont *font;
    UString sText;
    float fStringWidth;
    float fStringHeight;

    bool bDrawFrame;
    bool bDrawBackground;
    bool bCenterText;

    Color frameColor;
    Color backgroundColor;
    Color textColor;

    TEXT_JUSTIFICATION textJustification;
};
