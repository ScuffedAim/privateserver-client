#include "UIRankingScreenInfoLabel.h"

#include <chrono>

#include "Beatmap.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "Osu.h"
#include "ResourceManager.h"

UIRankingScreenInfoLabel::UIRankingScreenInfoLabel(float xPos, float yPos, float xSize, float ySize, UString name)
    : CBaseUIElement(xPos, yPos, xSize, ySize, name) {
    this->font = osu->getSubTitleFont();

    this->iMargin = 10;

    float globalScaler = 1.3f;
    this->fSubTitleScale = 0.6f * globalScaler;

    this->sArtist = "Artist";
    this->sTitle = "Title";
    this->sDiff = "Difficulty";
    this->sMapper = "Mapper";
    this->sPlayer = "Guest";
    this->sDate = "?";
}

void UIRankingScreenInfoLabel::draw(Graphics *g) {
    // build strings
    UString titleText = this->sArtist.c_str();
    titleText.append(" - ");
    titleText.append(this->sTitle.c_str());
    titleText.append(" [");
    titleText.append(this->sDiff.c_str());
    titleText.append("]");
    UString subTitleText = "Beatmap by ";
    subTitleText.append(this->sMapper.c_str());
    const UString playerText = this->buildPlayerString();

    const float globalScale = max((this->vSize.y / this->getMinimumHeight()) * 0.741f, 1.0f);

    // draw title
    g->setColor(0xffffffff);
    g->pushTransform();
    {
        const float scale = globalScale;

        g->scale(scale, scale);
        g->translate(this->vPos.x, this->vPos.y + this->font->getHeight() * scale);
        g->drawString(this->font, titleText);
    }
    g->popTransform();

    // draw subtitle
    g->setColor(0xffffffff);
    g->pushTransform();
    {
        const float scale = this->fSubTitleScale * globalScale;

        const float subTitleStringWidth = this->font->getStringWidth(subTitleText);

        g->translate((int)(-subTitleStringWidth / 2), (int)(this->font->getHeight() / 2));
        g->scale(scale, scale);
        g->translate(
            (int)(this->vPos.x + (subTitleStringWidth / 2) * scale),
            (int)(this->vPos.y + this->font->getHeight() * globalScale + (this->font->getHeight() / 2) * scale + this->iMargin));
        g->drawString(this->font, subTitleText);
    }
    g->popTransform();

    // draw subsubtitle (player text + datetime)
    g->setColor(0xffffffff);
    g->pushTransform();
    {
        const float scale = this->fSubTitleScale * globalScale;

        const float playerStringWidth = this->font->getStringWidth(playerText);

        g->translate((int)(-playerStringWidth / 2), (int)(this->font->getHeight() / 2));
        g->scale(scale, scale);
        g->translate((int)(this->vPos.x + (playerStringWidth / 2) * scale),
                     (int)(this->vPos.y + this->font->getHeight() * globalScale + this->font->getHeight() * scale +
                           (this->font->getHeight() / 2) * scale + this->iMargin * 2));
        g->drawString(this->font, playerText);
    }
    g->popTransform();
}

void UIRankingScreenInfoLabel::setFromBeatmap(Beatmap *beatmap, DatabaseBeatmap *diff2) {
    this->setArtist(diff2->getArtist());
    this->setTitle(diff2->getTitle());
    this->setDiff(diff2->getDifficultyName());
    this->setMapper(diff2->getCreator());

    std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    this->sDate = std::ctime(&now_c);
    trim(&this->sDate);
}

UString UIRankingScreenInfoLabel::buildPlayerString() {
    UString playerString = "Played by ";
    playerString.append(this->sPlayer.c_str());
    playerString.append(" on ");
    playerString.append(this->sDate.c_str());

    return playerString;
}

float UIRankingScreenInfoLabel::getMinimumWidth() {
    float titleWidth = 0;
    float subTitleWidth = 0;
    float playerWidth = this->font->getStringWidth(this->buildPlayerString()) * this->fSubTitleScale;

    return max(max(titleWidth, subTitleWidth), playerWidth);
}

float UIRankingScreenInfoLabel::getMinimumHeight() {
    float titleHeight = this->font->getHeight();
    float subTitleHeight = this->font->getHeight() * this->fSubTitleScale;
    float playerHeight = this->font->getHeight() * this->fSubTitleScale;

    return titleHeight + subTitleHeight + playerHeight + this->iMargin * 2;
}
