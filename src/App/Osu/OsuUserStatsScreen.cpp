//================ Copyright (c) 2019, PG, All rights reserved. =================//
//
// Purpose:		top plays list for weighted pp/acc
//
// $NoKeywords: $
//===============================================================================//

#include "OsuUserStatsScreen.h"

#include "Engine.h"
#include "ConVar.h"
#include "SoundEngine.h"
#include "ResourceManager.h"
#include "Mouse.h"

#include "CBaseUIContainer.h"
#include "CBaseUIScrollView.h"

#include "Osu.h"
#include "OsuIcons.h"
#include "OsuSkin.h"
#include "OsuReplay.h"
#include "OsuOptionsMenu.h"
#include "OsuSongBrowser2.h"
#include "OsuMultiplayer.h"
#include "OsuDatabase.h"
#include "OsuDatabaseBeatmap.h"

#include "OsuUIContextMenu.h"
#include "OsuUISongBrowserScoreButton.h"
#include "OsuUISongBrowserUserButton.h"

ConVar osu_ui_top_ranks_max("osu_ui_top_ranks_max", 100, "maximum number of displayed scores, to keep the ui/scrollbar manageable");



class OsuUserStatsScreenMenuButton : public CBaseUIButton
{
public:
	OsuUserStatsScreenMenuButton(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = "", UString text = "") : CBaseUIButton(xPos, yPos, xSize, ySize, name, text)
	{
	}

	virtual void drawText(Graphics *g)
	{
		// HACKHACK: force update string height to non-average line height for icon
		m_fStringHeight = m_font->getStringHeight(m_sText);

		if (m_font != NULL && m_sText.length() > 0)
		{
			g->pushClipRect(McRect(m_vPos.x+1, m_vPos.y+1, m_vSize.x-1, m_vSize.y-1));
			{
				g->setColor(m_textColor);
				g->pushTransform();
				{
					g->translate((int)(m_vPos.x + m_vSize.x/2.0f - (m_fStringWidth/2.0f)), (int)(m_vPos.y + m_vSize.y/2.0f + (m_fStringHeight/2.0f)));
					g->drawString(m_font, m_sText);
				}
				g->popTransform();
			}
			g->popClipRect();
		}
	}
};



OsuUserStatsScreen::OsuUserStatsScreen(Osu *osu) : OsuScreenBackable(osu)
{
	m_name_ref = convar->getConVarByName("name");

	// TODO: (statistics panel with values (how many plays, average UR/offset+-/score/pp/etc.))

	m_container = new CBaseUIContainer();

	m_contextMenu = new OsuUIContextMenu(m_osu);
	m_contextMenu->setVisible(true);

	m_userButton = new OsuUISongBrowserUserButton(m_osu);
	m_userButton->addTooltipLine("Click to change [User]");
	m_userButton->setClickCallback( fastdelegate::MakeDelegate(this, &OsuUserStatsScreen::onUserClicked) );
	m_container->addBaseUIElement(m_userButton);

	m_scores = new CBaseUIScrollView();
	m_scores->setBackgroundColor(0xff222222);
	m_scores->setHorizontalScrolling(false);
	m_scores->setVerticalScrolling(true);
	m_container->addBaseUIElement(m_scores);

	m_menuButton = new OsuUserStatsScreenMenuButton();
	m_menuButton->setFont(m_osu->getFontIcons());
	{
		UString iconString; iconString.insert(0, OsuIcons::GEAR);
		m_menuButton->setText(iconString);
	}
	m_menuButton->setClickCallback( fastdelegate::MakeDelegate(this, &OsuUserStatsScreen::onMenuClicked) );
	m_container->addBaseUIElement(m_menuButton);

	// the context menu gets added last (drawn on top of everything)
	m_container->addBaseUIElement(m_contextMenu);

	///m_vInfoText.push_back("Old scores are not recalculated yet!");
	///m_vInfoText.push_back("Will be included in a future update.");
	///m_vInfoText.push_back("(Scores before 15.02.2019 = old pp)");
}

OsuUserStatsScreen::~OsuUserStatsScreen()
{
	SAFE_DELETE(m_container);
}

void OsuUserStatsScreen::draw(Graphics *g)
{
	if (!m_bVisible) return;

	/*
	if (m_vInfoText.size() > 0)
	{
		const float dpiScale = Osu::getUIScale();

		const float infoScale = 0.1f;
		const float paddingScale = 0.4f;
		McFont *infoFont = m_osu->getSubTitleFont();
		g->setColor(0x77888888);
		for (int i=0; i<m_vInfoText.size(); i++)
		{
			g->pushTransform();
			{
				const float scale = (m_scores->getPos().y / infoFont->getHeight())*infoScale;
				const float height = infoFont->getHeight()*scale;
				const float padding = height*paddingScale;

				g->scale(scale, scale);
				g->translate(m_userButton->getPos().x + m_userButton->getSize().x + 10 * dpiScale, m_scores->getPos().y/2 + height/2 - ((m_vInfoText.size()-1)*height + (m_vInfoText.size()-1)*padding)/2 + i*(height+padding));
				g->drawString(infoFont, m_vInfoText[i]);
			}
			g->popTransform();
		}
	}
	*/

	m_container->draw(g);

	OsuScreenBackable::draw(g);
}

void OsuUserStatsScreen::update()
{
	OsuScreenBackable::update();
	if (!m_bVisible) return;

	m_container->update();

	if (m_contextMenu->isMouseInside())
		m_scores->stealFocus();

	if (m_osu->getOptionsMenu()->isMouseInside())
	{
		stealFocus();
		m_contextMenu->stealFocus();
		m_container->stealFocus();
	}

	updateLayout();
}

void OsuUserStatsScreen::setVisible(bool visible)
{
	OsuScreenBackable::setVisible(visible);

	if (m_bVisible)
		rebuildScoreButtons(m_name_ref->getString());
	else
		m_contextMenu->setVisible2(false);
}

void OsuUserStatsScreen::onScoreContextMenu(OsuUISongBrowserScoreButton *scoreButton, UString text)
{
	if (text == "Delete Score")
	{
		m_osu->getSongBrowser()->getDatabase()->deleteScore(std::string(scoreButton->getName().toUtf8()), scoreButton->getScoreUnixTimestamp());

		rebuildScoreButtons(m_name_ref->getString());
		m_osu->getSongBrowser()->rebuildScoreButtons();
	}
}

void OsuUserStatsScreen::onBack()
{
	engine->getSound()->play(m_osu->getSkin()->getMenuClick());
	m_osu->toggleSongBrowser();
}

void OsuUserStatsScreen::rebuildScoreButtons(UString playerName)
{
	// since this score list can grow very large, UI elements are not cached, but rebuilt completely every time

	// hard reset (delete)
	m_scores->getContainer()->clear();
	m_scoreButtons.clear();

	OsuDatabase *db = m_osu->getSongBrowser()->getDatabase();
	std::vector<OsuDatabase::Score*> scores = db->getPlayerPPScores(playerName).ppScores;
	for (int i=scores.size()-1; i>=std::max(0, (int)scores.size() - osu_ui_top_ranks_max.getInt()); i--)
	{
		const float weight = OsuDatabase::getWeightForIndex(scores.size()-1-i);

		const OsuDatabaseBeatmap *diff = db->getBeatmapDifficulty(scores[i]->md5hash);

		UString title = "...";
		if (diff != NULL)
		{
			title = diff->getArtist();
			title.append(" - ");
			title.append(diff->getTitle());
			title.append(" [");
			title.append(diff->getDifficultyName());
			title.append("]");
		}

		OsuUISongBrowserScoreButton *button = new OsuUISongBrowserScoreButton(m_osu, m_contextMenu, 0, 0, 300, 100, UString(scores[i]->md5hash.c_str()), OsuUISongBrowserScoreButton::STYLE::TOP_RANKS);
		button->setScore(*scores[i], scores.size()-i, title, weight);
		button->setClickCallback( fastdelegate::MakeDelegate(this, &OsuUserStatsScreen::onScoreClicked) );

		m_scoreButtons.push_back(button);
		m_scores->getContainer()->addBaseUIElement(button);
	}

	m_userButton->setText(playerName);
	m_osu->getOptionsMenu()->setUsername(playerName); // NOTE: force update options textbox to avoid shutdown inconsistency
	m_userButton->updateUserStats();

	updateLayout();
}

void OsuUserStatsScreen::onUserClicked(CBaseUIButton *button)
{
	engine->getSound()->play(m_osu->getSkin()->getMenuClick());

	// NOTE: code duplication (see OsuSongbrowser2.cpp)
	std::vector<UString> names = m_osu->getSongBrowser()->getDatabase()->getPlayerNamesWithPPScores();
	if (names.size() > 0)
	{
		m_contextMenu->setPos(m_userButton->getPos() + Vector2(0, m_userButton->getSize().y));
		m_contextMenu->setRelPos(m_userButton->getPos() + Vector2(0, m_userButton->getSize().y));
		m_contextMenu->begin(m_userButton->getSize().x);
		m_contextMenu->addButton("Switch User:", 0)->setTextColor(0xff888888)->setTextDarkColor(0xff000000)->setTextLeft(false)->setEnabled(false);
		//m_contextMenu->addButton("", 0)->setEnabled(false);
		for (int i=0; i<names.size(); i++)
		{
			CBaseUIButton *button = m_contextMenu->addButton(names[i]);
			if (names[i] == m_name_ref->getString())
				button->setTextBrightColor(0xff00ff00);
		}
		m_contextMenu->end();
		m_contextMenu->setClickCallback( fastdelegate::MakeDelegate(this, &OsuUserStatsScreen::onUserButtonChange) );
	}
}

void OsuUserStatsScreen::onUserButtonChange(UString text, int id)
{
	if (id == 0) return;

	if (text != m_name_ref->getString())
	{
		m_name_ref->setValue(text);
		rebuildScoreButtons(m_name_ref->getString());
	}
}

void OsuUserStatsScreen::onScoreClicked(CBaseUIButton *button)
{
	m_osu->toggleSongBrowser();
	m_osu->getMultiplayer()->setBeatmap(((OsuUISongBrowserScoreButton*)button)->getScore().md5hash);
	m_osu->getSongBrowser()->highlightScore(((OsuUISongBrowserScoreButton*)button)->getScore().unixTimestamp);
}

void OsuUserStatsScreen::onMenuClicked(CBaseUIButton *button)
{
	m_contextMenu->setPos(m_menuButton->getPos() + Vector2(0, m_menuButton->getSize().y - 1));
	m_contextMenu->setRelPos(m_menuButton->getPos() + Vector2(0, m_menuButton->getSize().y - 1));
	m_contextMenu->begin(0, true);
	{
		m_contextMenu->addButton("Recalculate pp");
		m_contextMenu->addButton("---");
		m_contextMenu->addButton("Delete All Scores (!)");
	}
	m_contextMenu->end();
	m_contextMenu->setClickCallback( fastdelegate::MakeDelegate(this, &OsuUserStatsScreen::onMenuSelected) );
}

void OsuUserStatsScreen::onMenuSelected(UString text, int id)
{
	// TODO: implement "recalculate pp", "delete all scores", etc.
	debugLog("TODO ...\n");
}

void OsuUserStatsScreen::updateLayout()
{
	OsuScreenBackable::updateLayout();

	const float dpiScale = Osu::getUIScale();

	m_container->setSize(m_osu->getScreenSize());

	const int scoreListHeight = m_osu->getScreenHeight()*0.8f;
	m_scores->setSize(m_osu->getScreenWidth()*0.6f, scoreListHeight);
	m_scores->setPos(m_osu->getScreenWidth()/2 - m_scores->getSize().x/2, m_osu->getScreenHeight() - scoreListHeight);

	const int margin = 5 * dpiScale;
	const int padding = 5 * dpiScale;

	// NOTE: these can't really be uiScale'd, because of the fixed aspect ratio
	const int scoreButtonWidth = m_scores->getSize().x*0.92f - 2*margin;
	const int scoreButtonHeight = scoreButtonWidth*0.065f;
	for (int i=0; i<m_scoreButtons.size(); i++)
	{
		m_scoreButtons[i]->setSize(scoreButtonWidth - 2, scoreButtonHeight);
		m_scoreButtons[i]->setRelPos(margin, margin + i*(scoreButtonHeight + padding));
	}
	m_scores->getContainer()->update_pos();

	m_scores->setScrollSizeToContent();

	const int userButtonHeight = m_scores->getPos().y*0.6f;
	m_userButton->setSize(userButtonHeight*3.5f, userButtonHeight);
	m_userButton->setPos(m_osu->getScreenWidth()/2 - m_userButton->getSize().x/2, m_scores->getPos().y/2 - m_userButton->getSize().y/2);

	m_menuButton->setSize(userButtonHeight*0.7f, userButtonHeight*0.7f);
	m_menuButton->setPos(std::max(m_userButton->getPos().x + m_userButton->getSize().x, m_userButton->getPos().x + m_userButton->getSize().x + (m_userButton->getPos().x - m_scores->getPos().x)/2 - m_menuButton->getSize().x/2), m_userButton->getPos().y + m_userButton->getSize().y/2 - m_menuButton->getSize().y/2);
}
