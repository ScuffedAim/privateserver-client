//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		changelog screen
//
// $NoKeywords: $osulog
//===============================================================================//

#ifndef OSUCHANGELOG_H
#define OSUCHANGELOG_H

#include "OsuScreenBackable.h"

class CBaseUIContainer;
class CBaseUIScrollView;
class CBaseUIImage;
class CBaseUILabel;

class OsuChangelog : public OsuScreenBackable {
   public:
    OsuChangelog(Osu *osu);
    virtual ~OsuChangelog();

    virtual void mouse_update(bool *propagate_clicks);

    virtual CBaseUIContainer *setVisible(bool visible);

   private:
    virtual void updateLayout();
    virtual void onBack();

    void onChangeClicked(CBaseUIButton *button);

    CBaseUIScrollView *m_scrollView;

    struct CHANGELOG {
        UString title;
        std::vector<UString> changes;
    };

    struct CHANGELOG_UI {
        CBaseUILabel *title;
        std::vector<CBaseUIButton *> changes;
    };

    std::vector<CHANGELOG_UI> m_changelogs;
};

#endif
