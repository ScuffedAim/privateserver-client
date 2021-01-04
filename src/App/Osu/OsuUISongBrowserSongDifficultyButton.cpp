//================ Copyright (c) 2016, PG, All rights reserved. =================//
//
// Purpose:		beatmap difficulty button (child of OsuUISongBrowserSongButton)
//
// $NoKeywords: $osusbsdb
//===============================================================================//

#include "OsuUISongBrowserSongDifficultyButton.h"

#include "Engine.h"
#include "ResourceManager.h"
#include "AnimationHandler.h"
#include "ConVar.h"

#include "Osu.h"
#include "OsuSkin.h"
#include "OsuDatabaseBeatmap.h"
#include "OsuSongBrowser2.h"
#include "OsuBackgroundImageHandler.h"
#include "OsuDatabase.h"
#include "OsuReplay.h"

#include "OsuUISongBrowserScoreButton.h"

#include "OpenGLHeaders.h"
#include "OpenGLLegacyInterface.h"
#include "OpenGL3Interface.h"
#include "OpenGLES2Interface.h"

ConVar *OsuUISongBrowserSongDifficultyButton::m_osu_scores_enabled = NULL;

OsuUISongBrowserSongDifficultyButton::OsuUISongBrowserSongDifficultyButton(Osu *osu, OsuSongBrowser2 *songBrowser, CBaseUIScrollView *view, OsuUIContextMenu *contextMenu, float xPos, float yPos, float xSize, float ySize, UString name, OsuDatabaseBeatmap *diff2, OsuUISongBrowserSongButton *parentSongButton) : OsuUISongBrowserSongButton(osu, songBrowser, view, contextMenu, xPos, yPos, xSize, ySize, name, NULL)
{
	m_databaseBeatmap = diff2; // NOTE: can't use parent constructor for passing this argument, as it would otherwise try to build a full button (and not just a diff button)
	m_parentSongButton = parentSongButton;

	if (m_osu_scores_enabled == NULL)
		m_osu_scores_enabled = convar->getConVarByName("osu_scores_enabled");

	/*
	m_sTitle = "Title";
	m_sArtist = "Artist";
	m_sMapper = "Mapper";
	m_sDiff = "Difficulty";
	*/

	m_sTitle = m_databaseBeatmap->getTitle();
	m_sArtist = m_databaseBeatmap->getArtist();
	m_sMapper = m_databaseBeatmap->getCreator();
	m_sDiff = m_databaseBeatmap->getDifficultyName();

	m_fDiffScale = 0.18f;
	m_fOffsetPercentAnim = (m_parentSongButton != NULL ? 1.0f : 0.0f);

	m_bUpdateGradeScheduled = true;
	m_bPrevOffsetPercentSelectionState = isIndependentDiffButton();

	// settings
	setHideIfSelected(false);

	updateLayoutEx();
}

OsuUISongBrowserSongDifficultyButton::~OsuUISongBrowserSongDifficultyButton()
{
	anim->deleteExistingAnimation(&m_fOffsetPercentAnim);
}

void OsuUISongBrowserSongDifficultyButton::draw(Graphics *g)
{
	OsuUISongBrowserButton::draw(g);
	if (!m_bVisible) return;

	const bool isIndependentDiff = isIndependentDiffButton();

	OsuSkin *skin = m_osu->getSkin();

	// scaling
	const Vector2 pos = getActualPos();
	const Vector2 size = getActualSize();

	// draw background image
	drawBeatmapBackgroundThumbnail(g, m_osu->getBackgroundImageHandler()->getLoadBackgroundImage(m_databaseBeatmap));

	if (m_bHasGrade)
		drawGrade(g);

	drawTitle(g, !isIndependentDiff ? 0.2f : 1.0f);
	drawSubTitle(g, !isIndependentDiff ? 0.2f : 1.0f);

	// draw diff name
	const float titleScale = (size.y*m_fTitleScale) / m_font->getHeight();
	const float subTitleScale = (size.y*m_fSubTitleScale) / m_font->getHeight();
	const float diffScale = (size.y*m_fDiffScale) / m_fontBold->getHeight();
	g->setColor(m_bSelected ? skin->getSongSelectActiveText() : skin->getSongSelectInactiveText());
	g->pushTransform();
	{
		g->scale(diffScale, diffScale);
		g->translate(pos.x + m_fTextOffset, pos.y + size.y*m_fTextMarginScale + m_font->getHeight()*titleScale + size.y*m_fTextSpacingScale + m_font->getHeight()*subTitleScale*0.85f + size.y*m_fTextSpacingScale + m_fontBold->getHeight()*diffScale*0.8f);
		g->drawString(m_fontBold, buildDiffString());
	}
	g->popTransform();

	// draw stars
	const float starsNoMod = m_databaseBeatmap->getStarsNomod();
	const bool areStarsInaccurate = (m_osu->getSongBrowser()->getBackgroundStarCalculator()->isDead() || !m_osu->getSongBrowser()->getBackgroundStarCalculator()->isAsyncReady());
	const float stars = (areStarsInaccurate || !m_bSelected ? starsNoMod : m_osu->getSongBrowser()->getBackgroundStarCalculator()->getTotalStars());
	if (stars > 0)
	{
		const float starOffsetY = (size.y*0.85);
		const float starWidth = (size.y*0.2);
		const float starScale = starWidth / skin->getStar()->getHeight();
		const int numFullStars = std::min((int)stars, 25);
		const float partialStarScale = std::max(0.5f, clamp<float>(stars - numFullStars, 0.0f, 1.0f)); // at least 0.5x

		g->setColor(m_bSelected ? skin->getSongSelectActiveText() : skin->getSongSelectInactiveText());

		// full stars
		for (int i=0; i<numFullStars; i++)
		{
			const float scale = std::min(starScale*1.175f, starScale + i*0.015f); // more stars = getting bigger, up to a limit

			g->pushTransform();
			{
				g->scale(scale, scale);
				g->translate(pos.x + m_fTextOffset + starWidth/2 + i*starWidth*1.75f, pos.y + starOffsetY);
				g->drawImage(skin->getStar());
			}
			g->popTransform();
		}

		// partial star
		g->pushTransform();
		{
			g->scale(starScale*partialStarScale, starScale*partialStarScale);
			g->translate(pos.x + m_fTextOffset + starWidth/2 + numFullStars*starWidth*1.75f, pos.y + starOffsetY);
			g->drawImage(skin->getStar());
		}
		g->popTransform();

		// fill leftover space up to 10 stars total (background stars)
		g->setColor(0x1effffff);
		const float backgroundStarScale = 0.6f;

#if defined(MCENGINE_FEATURE_OPENGL)

			const bool isOpenGLRendererHack = (dynamic_cast<OpenGLLegacyInterface*>(g) != NULL || dynamic_cast<OpenGL3Interface*>(g) != NULL);

#elif defined(MCENGINE_FEATURE_OPENGLES)

			const bool isOpenGLRendererHack = (dynamic_cast<OpenGLES2Interface*>(g) != NULL);

#endif

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_OPENGLES)

		if (isOpenGLRendererHack)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE); // HACKHACK: OpenGL hardcoded

#endif

		for (int i=(numFullStars + 1); i<10; i++)
		{
			g->pushTransform();
			{
				g->scale(starScale*backgroundStarScale, starScale*backgroundStarScale);
				g->translate(pos.x + m_fTextOffset + starWidth/2 + i*starWidth*1.75f, pos.y + starOffsetY);
				g->drawImage(skin->getStar());
			}
			g->popTransform();
		}

#if defined(MCENGINE_FEATURE_OPENGL) || defined(MCENGINE_FEATURE_OPENGLES)

		if (isOpenGLRendererHack)
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // HACKHACK: OpenGL hardcoded

#endif

	}
}

void OsuUISongBrowserSongDifficultyButton::update()
{
	OsuUISongBrowserSongButton::update();
	if (!m_bVisible) return;

	// dynamic settings (moved from constructor to here)
	const bool isIndependentDiff = isIndependentDiffButton();
	const bool newOffsetPercentSelectionState = (m_bSelected || !isIndependentDiff);
	if (newOffsetPercentSelectionState != m_bPrevOffsetPercentSelectionState)
	{
		m_bPrevOffsetPercentSelectionState = newOffsetPercentSelectionState;
		anim->moveQuadOut(&m_fOffsetPercentAnim, newOffsetPercentSelectionState ? 1.0f : 0.0f, 0.25f * (1.0f - m_fOffsetPercentAnim), true);
	}
	setOffsetPercent(lerp<float>(0.0f, 0.075f, m_fOffsetPercentAnim));
	setInactiveBackgroundColor(!isIndependentDiff ? COLOR(255, 0, 150, 236) : COLOR(240, 235, 73, 153));

	if (m_bUpdateGradeScheduled)
	{
		m_bUpdateGradeScheduled = false;
		updateGrade();
	}
}

void OsuUISongBrowserSongDifficultyButton::onSelected(bool wasSelected)
{
	OsuUISongBrowserButton::onSelected(wasSelected);

	updateGrade();

	m_songBrowser->onSelectionChange(this, true);
	m_songBrowser->onDifficultySelected(m_databaseBeatmap, wasSelected);
	m_songBrowser->scrollToSongButton(this);
}

void OsuUISongBrowserSongDifficultyButton::updateGrade()
{
	if (!m_osu_scores_enabled->getBool())
	{
		m_bHasGrade = false;
		return;
	}

	bool hasGrade = false;
	OsuScore::GRADE grade;

	m_osu->getSongBrowser()->getDatabase()->sortScores(m_databaseBeatmap->getMD5Hash());
	if ((*m_osu->getSongBrowser()->getDatabase()->getScores())[m_databaseBeatmap->getMD5Hash()].size() > 0)
	{
		const OsuDatabase::Score &score = (*m_osu->getSongBrowser()->getDatabase()->getScores())[m_databaseBeatmap->getMD5Hash()][0];
		hasGrade = true;
		grade = OsuScore::calculateGrade(score.num300s, score.num100s, score.num50s, score.numMisses, score.modsLegacy & OsuReplay::Mods::Hidden, score.modsLegacy & OsuReplay::Mods::Flashlight);
	}

	m_bHasGrade = hasGrade;
	if (m_bHasGrade)
		m_grade = grade;
}

bool OsuUISongBrowserSongDifficultyButton::isIndependentDiffButton() const
{
	return (m_parentSongButton == NULL || !m_parentSongButton->isSelected());
}
