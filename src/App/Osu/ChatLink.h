#pragma once

#include "CBaseUILabel.h"
#include "UString.h"

class ChatLink : public CBaseUILabel {
   public:
    ChatLink(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString link = "", UString label = "");

    virtual void mouse_update(bool *propagate_clicks);
    virtual void onMouseUpInside();

    void open_beatmap_link(i32 map_id, i32 set_id);

    UString m_link;
};
