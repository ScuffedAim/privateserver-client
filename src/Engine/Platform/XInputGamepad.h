//================ Copyright (c) 2014, PG, All rights reserved. =================//
//
// Purpose:		xinput gamepad wrapper
//
// $NoKeywords: $xi
//===============================================================================//

#ifndef XINPUTGAMEPAD_H
#define XINPUTGAMEPAD_H

#include "Gamepad.h"

#ifdef _WIN32
#include <xinput.h>
#endif

class XInputGamepad : public Gamepad
{
public:
	XInputGamepad();
	virtual ~XInputGamepad() {;}

	void update();

	void setVibration(float leftMotorSpeedPercent, float rightMotorSpeedPercent);

	inline Vector2 getLeftStick() {return m_vLeftStick;}
	inline Vector2 getRightStick() {return m_vRightStick;}

	inline float getLeftTrigger() {return m_fLeftTrigger;}
	inline float getRightTrigger() {return m_fRightTrigger;}

	bool isButtonPressed(GAMEPADBUTTON button);

	inline bool isConnected() {return m_iPort != -1;}
	inline int getPort() {return m_iPort+1;}

private:
	void updateConnection();
	void updateButtonStates();
	void checkHandleButtonChangeEvents(bool &previous, bool current, GAMEPADBUTTON b);

#ifdef _WIN32
	XINPUT_STATE m_state;
#endif

	int m_iPort;

	Vector2 m_vLeftStick;
	Vector2 m_vRightStick;

	float m_fLeftTrigger;
	float m_fRightTrigger;

	bool m_bDpadUpPressed;
	bool m_bDpadDownPressed;
	bool m_bDpadLeftPressed;
	bool m_bDpadRightPressed;
	bool m_bStartPressed;
	bool m_bBackPressed;
	bool m_bLeftThumbPressed;
	bool m_bRightThumbPressed;
	bool m_bLeftShoulderPressed;
	bool m_bRightShoulderPressed;
	bool m_bAPressed;
	bool m_bBPressed;
	bool m_bXPressed;
	bool m_bYPressed;
};

#endif
