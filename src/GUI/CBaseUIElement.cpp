#include "CBaseUIElement.h"

#include "Engine.h"
#include "Mouse.h"

void CBaseUIElement::mouse_update(bool *propagate_clicks) {
    // check if mouse is inside element
    McRect temp = McRect(m_vPos.x + 1, m_vPos.y + 1, m_vSize.x - 1, m_vSize.y - 1);
    if(temp.contains(engine->getMouse()->getPos())) {
        if(!m_bMouseInside) {
            m_bMouseInside = true;
            if(m_bVisible && m_bEnabled) onMouseInside();
        }
    } else {
        if(m_bMouseInside) {
            m_bMouseInside = false;
            if(m_bVisible && m_bEnabled) onMouseOutside();
        }
    }

    if(!m_bVisible || !m_bEnabled) return;

    if(engine->getMouse()->isLeftDown() && *propagate_clicks) {
        m_bMouseUpCheck = true;
        if(m_bMouseInside) {
            *propagate_clicks = !grabs_clicks;
        }

        // onMouseDownOutside
        if(!m_bMouseInside && !m_bMouseInsideCheck) {
            m_bMouseInsideCheck = true;
            onMouseDownOutside();
        }

        // onMouseDownInside
        if(m_bMouseInside && !m_bMouseInsideCheck) {
            m_bActive = true;
            m_bMouseInsideCheck = true;
            onMouseDownInside();
        }
    } else {
        if(m_bMouseUpCheck) {
            if(m_bActive) {
                if(m_bMouseInside)
                    onMouseUpInside();
                else
                    onMouseUpOutside();

                if(!m_bKeepActive) m_bActive = false;
            }
        }

        m_bMouseInsideCheck = false;
        m_bMouseUpCheck = false;
    }
}
