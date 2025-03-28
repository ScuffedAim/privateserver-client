#pragma once
#include <mutex>

#include "CBaseUIElement.h"

class CBaseUITextbox;
class CBaseUIButton;
class CBaseUIScrollView;
class CBaseUIBoxShadow;

class ConsoleBoxTextbox;

class ConsoleBox : public CBaseUIElement {
   public:
    ConsoleBox();
    virtual ~ConsoleBox();

    void draw(Graphics *g);
    void drawLogOverlay(Graphics *g);
    void mouse_update(bool *propagate_clicks);

    void onKeyDown(KeyboardEvent &e);
    void onChar(KeyboardEvent &e);

    void onResolutionChange(Vector2 newResolution);

    void processCommand(UString command);
    void execConfigFile(std::string filename);

    void log(UString text, Color textColor = 0xffffffff);

    // set
    void setRequireShiftToActivate(bool requireShiftToActivate) {
        this->bRequireShiftToActivate = requireShiftToActivate;
    }

    // get
    bool isBusy();
    bool isActive();

    // ILLEGAL:
    inline ConsoleBoxTextbox *getTextbox() const { return this->textbox; }

   private:
    struct LOG_ENTRY {
        UString text;
        Color textColor;
    };

   private:
    void onSuggestionClicked(CBaseUIButton *suggestion);

    void addSuggestion(const UString &text, const UString &helpText, const UString &command);
    void clearSuggestions();

    void show();
    void toggle(KeyboardEvent &e);

    float getAnimTargetY();

    float getDPIScale();

    int iSuggestionCount;
    int iSelectedSuggestion;  // for up/down buttons

    ConsoleBoxTextbox *textbox;
    CBaseUIScrollView *suggestion;
    std::vector<CBaseUIButton *> vSuggestionButtons;
    float fSuggestionY;

    bool bRequireShiftToActivate;
    bool bConsoleAnimateOnce;
    float fConsoleDelay;
    float fConsoleAnimation;
    bool bConsoleAnimateIn;
    bool bConsoleAnimateOut;

    bool bSuggestionAnimateIn;
    bool bSuggestionAnimateOut;
    float fSuggestionAnimation;

    float fLogTime;
    float fLogYPos;
    std::vector<LOG_ENTRY> log_entries;
    McFont *logFont;

    std::vector<UString> commandHistory;
    int iSelectedHistory;

    std::mutex logMutex;
};
