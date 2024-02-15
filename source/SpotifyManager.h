//  ***************************************************************
//  SpotifyManager - Creation date: 10/21/2023 4:31:17 PM
//  -------------------------------------------------------------
//  License: Uh, check for license.txt or license.md for that?
//
//  ***************************************************************
//  Programmer(s):  Seth A. Robinson (seth@rtsoft.com)
//  ***************************************************************

#pragma once

class SpotifyManager
{
public:
	SpotifyManager();
	virtual ~SpotifyManager();

	void TogglePlayback(HWND hwnd);
	void Pause();

	void GetActiveSong(string& songPlayingOut, string& artistPlayingOut, bool& windowFoundOut, bool& isPlayingOut, HWND & spotifyBrowserWindowHWNDOut);
	void SetPause(bool bPause, int delayMS);

	HWND m_chromeSpotifyWindow = NULL;

	bool m_bCheckedForChromeWindow = false;

protected:

};
