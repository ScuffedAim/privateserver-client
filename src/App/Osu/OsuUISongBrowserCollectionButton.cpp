//================ Copyright (c) 2016, PG, All rights reserved. =================//
//
// Purpose:		collection button (also used for grouping)
//
// $NoKeywords: $osusbcb
//===============================================================================//

#include "OsuUISongBrowserCollectionButton.h"

#include "Collections.h"
#include "ConVar.h"
#include "Engine.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Osu.h"
#include "OsuDatabase.h"
#include "OsuNotificationOverlay.h"
#include "OsuSkin.h"
#include "OsuSongBrowser.h"
#include "OsuUIContextMenu.h"
#include "ResourceManager.h"

ConVar osu_songbrowser_button_collection_active_color_a("osu_songbrowser_button_collection_active_color_a", 255,
                                                        FCVAR_NONE);
ConVar osu_songbrowser_button_collection_active_color_r("osu_songbrowser_button_collection_active_color_r", 163,
                                                        FCVAR_NONE);
ConVar osu_songbrowser_button_collection_active_color_g("osu_songbrowser_button_collection_active_color_g", 240,
                                                        FCVAR_NONE);
ConVar osu_songbrowser_button_collection_active_color_b("osu_songbrowser_button_collection_active_color_b", 44,
                                                        FCVAR_NONE);

ConVar osu_songbrowser_button_collection_inactive_color_a("osu_songbrowser_button_collection_inactive_color_a", 255,
                                                          FCVAR_NONE);
ConVar osu_songbrowser_button_collection_inactive_color_r("osu_songbrowser_button_collection_inactive_color_r", 35,
                                                          FCVAR_NONE);
ConVar osu_songbrowser_button_collection_inactive_color_g("osu_songbrowser_button_collection_inactive_color_g", 50,
                                                          FCVAR_NONE);
ConVar osu_songbrowser_button_collection_inactive_color_b("osu_songbrowser_button_collection_inactive_color_b", 143,
                                                          FCVAR_NONE);

OsuUISongBrowserCollectionButton::OsuUISongBrowserCollectionButton(Osu *osu, OsuSongBrowser *songBrowser,
                                                                   CBaseUIScrollView *view,
                                                                   OsuUIContextMenu *contextMenu, float xPos,
                                                                   float yPos, float xSize, float ySize, UString name,
                                                                   UString collectionName,
                                                                   std::vector<OsuUISongBrowserButton *> children)
    : OsuUISongBrowserButton(osu, songBrowser, view, contextMenu, xPos, yPos, xSize, ySize, name) {
    m_sCollectionName = collectionName;
    m_children = children;

    m_fTitleScale = 0.35f;

    // settings
    setOffsetPercent(0.075f * 0.5f);
}

void OsuUISongBrowserCollectionButton::draw(Graphics *g) {
    OsuUISongBrowserButton::draw(g);
    if(!m_bVisible) return;

    OsuSkin *skin = m_osu->getSkin();

    // scaling
    const Vector2 pos = getActualPos();
    const Vector2 size = getActualSize();

    // draw title
    UString titleString = buildTitleString();
    int textXOffset = size.x * 0.02f;
    float titleScale = (size.y * m_fTitleScale) / m_font->getHeight();
    g->setColor(m_bSelected ? skin->getSongSelectActiveText() : skin->getSongSelectInactiveText());
    g->pushTransform();
    {
        g->scale(titleScale, titleScale);
        g->translate(pos.x + textXOffset, pos.y + size.y / 2 + (m_font->getHeight() * titleScale) / 2);
        g->drawString(m_font, titleString);
    }
    g->popTransform();
}

void OsuUISongBrowserCollectionButton::onSelected(bool wasSelected, bool autoSelectBottomMostChild,
                                                  bool wasParentSelected) {
    OsuUISongBrowserButton::onSelected(wasSelected, autoSelectBottomMostChild, wasParentSelected);

    m_songBrowser->onSelectionChange(this, true);
    m_songBrowser->scrollToSongButton(this, true);
}

void OsuUISongBrowserCollectionButton::onRightMouseUpInside() { triggerContextMenu(engine->getMouse()->getPos()); }

void OsuUISongBrowserCollectionButton::triggerContextMenu(Vector2 pos) {
    if(m_osu->getSongBrowser()->getGroupingMode() != OsuSongBrowser::GROUP::GROUP_COLLECTIONS) return;

    if(m_contextMenu != NULL) {
        m_contextMenu->setPos(pos);
        m_contextMenu->setRelPos(pos);
        m_contextMenu->begin(0, true);
        {
            m_contextMenu->addButton("[...]      Rename Collection", 1);

            CBaseUIButton *spacer = m_contextMenu->addButton("---");
            spacer->setTextLeft(false);
            spacer->setEnabled(false);
            spacer->setTextColor(0xff888888);
            spacer->setTextDarkColor(0xff000000);

            m_contextMenu->addButton("[-]         Delete Collection", 2);
        }
        m_contextMenu->end(false, false);
        m_contextMenu->setClickCallback(
            fastdelegate::MakeDelegate(this, &OsuUISongBrowserCollectionButton::onContextMenu));
        OsuUIContextMenu::clampToRightScreenEdge(m_contextMenu);
        OsuUIContextMenu::clampToBottomScreenEdge(m_contextMenu);
    }
}

void OsuUISongBrowserCollectionButton::onContextMenu(UString text, int id) {
    if(id == 1) {
        m_contextMenu->begin(0, true);
        {
            CBaseUIButton *label = m_contextMenu->addButton("Enter Collection Name:");
            label->setTextLeft(false);
            label->setEnabled(false);

            CBaseUIButton *spacer = m_contextMenu->addButton("---");
            spacer->setTextLeft(false);
            spacer->setEnabled(false);
            spacer->setTextColor(0xff888888);
            spacer->setTextDarkColor(0xff000000);

            m_contextMenu->addTextbox(m_sCollectionName, id)->setCursorPosRight();

            spacer = m_contextMenu->addButton("---");
            spacer->setTextLeft(false);
            spacer->setEnabled(false);
            spacer->setTextColor(0xff888888);
            spacer->setTextDarkColor(0xff000000);

            label = m_contextMenu->addButton("(Press ENTER to confirm.)", id);
            label->setTextLeft(false);
            label->setTextColor(0xff555555);
            label->setTextDarkColor(0xff000000);
        }
        m_contextMenu->end(false, false);
        m_contextMenu->setClickCallback(
            fastdelegate::MakeDelegate(this, &OsuUISongBrowserCollectionButton::onRenameCollectionConfirmed));
        OsuUIContextMenu::clampToRightScreenEdge(m_contextMenu);
        OsuUIContextMenu::clampToBottomScreenEdge(m_contextMenu);
    } else if(id == 2) {
        if(engine->getKeyboard()->isShiftDown())
            onDeleteCollectionConfirmed(text, id);
        else {
            m_contextMenu->begin(0, true);
            {
                m_contextMenu->addButton("Really delete collection?")->setEnabled(false);
                CBaseUIButton *spacer = m_contextMenu->addButton("---");
                spacer->setTextLeft(false);
                spacer->setEnabled(false);
                spacer->setTextColor(0xff888888);
                spacer->setTextDarkColor(0xff000000);
                m_contextMenu->addButton("Yes", 2)->setTextLeft(false);
                m_contextMenu->addButton("No")->setTextLeft(false);
            }
            m_contextMenu->end(false, false);
            m_contextMenu->setClickCallback(
                fastdelegate::MakeDelegate(this, &OsuUISongBrowserCollectionButton::onDeleteCollectionConfirmed));
            OsuUIContextMenu::clampToRightScreenEdge(m_contextMenu);
            OsuUIContextMenu::clampToBottomScreenEdge(m_contextMenu);
        }
    }
}

void OsuUISongBrowserCollectionButton::onRenameCollectionConfirmed(UString text, int id) {
    if(text.length() > 0) {
        std::string old_name = m_sCollectionName.toUtf8();
        std::string new_name = text.toUtf8();
        auto collection = get_or_create_collection(old_name);
        collection->rename_to(new_name);
        save_collections();

        // (trigger re-sorting of collection buttons)
        m_osu->getSongBrowser()->onCollectionButtonContextMenu(this, m_sCollectionName, 3);
    }
}

void OsuUISongBrowserCollectionButton::onDeleteCollectionConfirmed(UString text, int id) {
    if(id != 2) return;

    // just forward it
    m_osu->getSongBrowser()->onCollectionButtonContextMenu(this, m_sCollectionName, id);
}

UString OsuUISongBrowserCollectionButton::buildTitleString() {
    UString titleString = m_sCollectionName;

    // count children (also with support for search results in collections)
    int numChildren = 0;
    {
        for(size_t c = 0; c < m_children.size(); c++) {
            const std::vector<OsuUISongBrowserButton *> &childrenChildren = m_children[c]->getChildren();
            if(childrenChildren.size() > 0) {
                for(size_t cc = 0; cc < childrenChildren.size(); cc++) {
                    if(childrenChildren[cc]->isSearchMatch()) numChildren++;
                }
            } else if(m_children[c]->isSearchMatch())
                numChildren++;
        }
    }

    titleString.append(UString::format((numChildren == 1 ? " (%i map)" : " (%i maps)"), numChildren));

    return titleString;
}

Color OsuUISongBrowserCollectionButton::getActiveBackgroundColor() const {
    return COLOR(clamp<int>(osu_songbrowser_button_collection_active_color_a.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_collection_active_color_r.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_collection_active_color_g.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_collection_active_color_b.getInt(), 0, 255));
}

Color OsuUISongBrowserCollectionButton::getInactiveBackgroundColor() const {
    return COLOR(clamp<int>(osu_songbrowser_button_collection_inactive_color_a.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_collection_inactive_color_r.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_collection_inactive_color_g.getInt(), 0, 255),
                 clamp<int>(osu_songbrowser_button_collection_inactive_color_b.getInt(), 0, 255));
}
