//================ Copyright (c) 2018, PG, All rights reserved. =================//
//
// Purpose:		slider used for the volume overlay HUD
//
// $NoKeywords: $osuvolsl
//===============================================================================//

#include "UIVolumeSlider.h"

#include "AnimationHandler.h"
#include "Engine.h"
#include "Osu.h"
#include "ResourceManager.h"

UIVolumeSlider::UIVolumeSlider(float xPos, float yPos, float xSize, float ySize, UString name)
    : CBaseUISlider(xPos, yPos, xSize, ySize, name) {
    m_type = TYPE::MASTER;
    m_bSelected = false;

    m_bWentMouseInside = false;
    m_fSelectionAnim = 0.0f;

    m_font = engine->getResourceManager()->getFont("FONT_DEFAULT");

    engine->getResourceManager()->loadImage("ic_volume_mute_white_48dp.png", "OSU_UI_VOLUME_SLIDER_BLOCK_0");
    engine->getResourceManager()->loadImage("ic_volume_up_white_48dp.png", "OSU_UI_VOLUME_SLIDER_BLOCK_1");
    engine->getResourceManager()->loadImage("ic_music_off_48dp.png", "OSU_UI_VOLUME_SLIDER_MUSIC_0");
    engine->getResourceManager()->loadImage("ic_music_48dp.png", "OSU_UI_VOLUME_SLIDER_MUSIC_1");
    engine->getResourceManager()->loadImage("ic_effects_off_48dp.png", "OSU_UI_VOLUME_SLIDER_EFFECTS_0");
    engine->getResourceManager()->loadImage("ic_effects_48dp.png", "OSU_UI_VOLUME_SLIDER_EFFECTS_1");

    setFrameColor(0xff7f7f7f);
}

void UIVolumeSlider::drawBlock(Graphics *g) {
    // draw icon
    Image *img = NULL;
    if(getFloat() < 0.01f)
        img = engine->getResourceManager()->getImage(
            m_type == TYPE::MASTER
                ? "OSU_UI_VOLUME_SLIDER_BLOCK_0"
                : (m_type == TYPE::MUSIC ? "OSU_UI_VOLUME_SLIDER_MUSIC_0" : "OSU_UI_VOLUME_SLIDER_EFFECTS_0"));
    else
        img = engine->getResourceManager()->getImage(
            m_type == TYPE::MASTER
                ? "OSU_UI_VOLUME_SLIDER_BLOCK_1"
                : (m_type == TYPE::MUSIC ? "OSU_UI_VOLUME_SLIDER_MUSIC_1" : "OSU_UI_VOLUME_SLIDER_EFFECTS_1"));

    g->pushTransform();
    {
        const float scaleMultiplier = 0.95f + 0.2f * m_fSelectionAnim;

        g->scale((m_vBlockSize.y / img->getSize().x) * scaleMultiplier,
                 (m_vBlockSize.y / img->getSize().y) * scaleMultiplier);
        g->translate(m_vPos.x + m_vBlockPos.x + m_vBlockSize.x / 2.0f,
                     m_vPos.y + m_vBlockPos.y + m_vBlockSize.y / 2.0f + 1);
        g->setColor(0xffffffff);
        g->drawImage(img);
    }
    g->popTransform();

    // draw percentage
    g->pushTransform();
    {
        g->translate((int)(m_vPos.x + m_vSize.x + m_vSize.x * 0.0335f),
                     (int)(m_vPos.y + m_vSize.y / 2 + m_font->getHeight() / 2));
        g->setColor(0xff000000);
        g->translate(1, 1);
        g->drawString(m_font, UString::format("%i%%", (int)(std::round(getFloat() * 100.0f))));
        g->setColor(0xffffffff);
        g->translate(-1, -1);
        g->drawString(m_font, UString::format("%i%%", (int)(std::round(getFloat() * 100.0f))));
    }
    g->popTransform();
}

void UIVolumeSlider::onMouseInside() {
    CBaseUISlider::onMouseInside();

    m_bWentMouseInside = true;
}

void UIVolumeSlider::setSelected(bool selected) {
    m_bSelected = selected;

    if(m_bSelected) {
        setFrameColor(0xffffffff);
        anim->moveQuadOut(&m_fSelectionAnim, 1.0f, 0.1f, true);
        anim->moveQuadIn(&m_fSelectionAnim, 0.0f, 0.15f, 0.1f, false);
    } else {
        setFrameColor(0xff7f7f7f);
        anim->moveLinear(&m_fSelectionAnim, 0.0f, 0.15f * m_fSelectionAnim, true);
    }
}

bool UIVolumeSlider::checkWentMouseInside() {
    const bool temp = m_bWentMouseInside;
    m_bWentMouseInside = false;
    return temp;
}

float UIVolumeSlider::getMinimumExtraTextWidth() { return m_vSize.x * 0.0335f * 2.0f + m_font->getStringWidth("100%"); }
