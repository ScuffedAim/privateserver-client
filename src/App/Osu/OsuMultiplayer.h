//================ Copyright (c) 2018, PG, All rights reserved. =================//
//
// Purpose:		network play
//
// $NoKeywords: $osump
//===============================================================================//

#ifndef OSUMULTIPLAYER_H
#define OSUMULTIPLAYER_H

#include "cbase.h"

class Osu;
class OsuBeatmap;

class OsuMultiplayer
{
public:
	enum STATE
	{
		SELECT,
		START,
		SEEK,
		SKIP,
		PAUSE,
		UNPAUSE,
		RESTART,
		STOP
	};

	struct PLAYER
	{
		unsigned int id;
		UString name;
		bool missingBeatmap;
		int combo;
		float accuracy;
		unsigned long long score;
		bool dead;
		unsigned long long sortHack;
	};

public:
	OsuMultiplayer(Osu *osu);
	~OsuMultiplayer();

	// receive
	bool onClientReceive(unsigned int id, void *data, size_t size);
	bool onClientReceiveInt(unsigned int id, void *data, size_t size, bool forceAcceptOnServer = false);
	bool onServerReceive(unsigned int id, void *data, size_t size);

	// client events
	void onClientConnectedToServer();
	void onClientDisconnectedFromServer();

	// server events
	void onServerClientChange(unsigned int id, UString name, bool connected);
	void onLocalServerStarted();
	void onLocalServerStopped();

	// clientside game events
	void onClientStatusUpdate(bool missingBeatmap);
	void onClientScoreChange(int combo, float accuracy, unsigned long long score, bool dead, bool reliable = false);
	bool onClientPlayStateChangeRequestBeatmap(OsuBeatmap *beatmap);

	// serverside game events
	void onServerModUpdate();
	void onServerPlayStateChange(STATE state, unsigned long seekMS = 0, bool quickRestart = false, OsuBeatmap *beatmap = NULL);

	bool isServer();
	bool isInMultiplayer();

	inline std::vector<PLAYER> &getPlayers() {return m_clientPlayers;}

private:
	static unsigned long long sortHackCounter;

	void onClientCommand(UString command);
	void onClientCommandInt(UString string, bool fromModUpdate);

	enum PACKET_TYPE
	{
		PLAYER_CHANGE_TYPE,
		PLAYER_STATE_TYPE,
		CONVAR_TYPE,
		STATE_TYPE,
		SCORE_TYPE
	};

	struct PLAYER_CHANGE_PACKET
	{
		unsigned int id;
		bool connected;
		size_t size;
		wchar_t name[255];
	};

	struct PLAYER_STATE_PACKET
	{
		unsigned int id;
		bool missingBeatmap;
	};

	struct CONVAR_PACKET
	{
		wchar_t str[1024];
		size_t len;
	};

	struct GAME_STATE_PACKET
	{
		STATE state;
		unsigned long seekMS;
		bool quickRestart;
		unsigned char beatmapUUID[16];
		long beatmapId;
	};

	struct SCORE_PACKET
	{
		unsigned int id;
		int combo;
		float accuracy;
		unsigned long long score;
		bool dead;
	};

	Osu *m_osu;
	std::vector<PLAYER> m_serverPlayers;
	std::vector<PLAYER> m_clientPlayers;
};

#endif
