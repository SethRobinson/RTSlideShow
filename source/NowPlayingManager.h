//  ***************************************************************
//  NowPlayingManager - Creation date: 11/26/2021
//  -------------------------------------------------------------
//  Robinson Technologies Copyright (C) 2021 - All Rights Reserved
//
//  ***************************************************************
//  Programmer(s):  Seth A. Robinson (seth@rtsoft.com)
//  ***************************************************************

#ifndef NowPlayingManager_h__
#define NowPlayingManager_h__

class NowPlayingManager
{
public:
	NowPlayingManager();
	virtual ~NowPlayingManager();

	void Init();
	void UpdateFoobar();
	bool UpdateSpotifyWebWindow();
	void Update();


protected:
	
	bool SetNewSong(string songName, string artist);
	void KillActiveSong();

	void PushSongLabelToTop();

	unsigned int m_updateTimer = 0;
	int m_updateSpeedMS = 500;
	bool m_bIsPlayingSong = false;
	string m_song, m_artist;
	int m_zoomSpeed = 3000;

private:
};

#endif // NowPlayingManager_h__
