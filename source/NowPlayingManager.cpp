#include "PlatformPrecomp.h"
#include "NowPlayingManager.h"
#include "App.h"
#include "util/TextScanner.h"
#include "Entity/EntityUtils.h"

NowPlayingManager::NowPlayingManager()
{
	
}

NowPlayingManager::~NowPlayingManager()
{
}

void NowPlayingManager::Init()
{
	m_updateTimer = GetTick() + 2000;
	if (GetApp()->m_nowPlayingTextFile.empty()) return; //don't care
}

void NowPlayingManager::KillActiveSong()
{

	vector<Entity*> ents;
	
	GetEntityRoot()->GetEntitiesByName(&ents, "Song");

	//LogMsg("Found %d ents", ents.size());

	for (int i = 0; i < ents.size(); i++)
	{
		
			ZoomToPositionOffsetEntity(ents[i], CL_Vec2f(-((float)GetScreenSizeXf()*1.5f), 0), m_zoomSpeed);
			KillEntity(ents[i], ((int) (m_zoomSpeed*1.5f)));
		
	}
} 

void NowPlayingManager::PushSongLabelToTop()
{
	vector<Entity*> ents;

	GetEntityRoot()->GetEntitiesByName(&ents, "Song");

	for (int i = 0; i < ents.size(); i++)
	{
		ents[i]->GetParent()->MoveEntityToTopByAddress(ents[i]);

	}

}


bool NowPlayingManager::SetNewSong(string songName, string artist)
{
	m_song = songName;
	m_artist = artist;

	string song = "`$Now playing: `w" + m_song + " `7by`` " + m_artist;

	if (artist.length() < 2)
	{
		song = "`$Now playing: `w" + m_song;
	}
	
	Entity *pParent = GetEntityRoot()->GetEntityByName("GUI");

	//GetApp()->GetEntityRoot()->PrintTreeAsText(2);
	if (!pParent) return false;
	
	
	AddFocusIfNeeded(pParent);

	//kill the old one
	KillActiveSong();

	auto pEnt = CreateTextLabelEntity(pParent, "Song", GetScreenSizeXf()/2, GetScreenSizeYf()-64, song);
	SetupTextEntity(pEnt, FONT_LARGE);
	TextRenderComponent* pTextComp = (TextRenderComponent*)pEnt->GetComponentByName("TextRender");
	pTextComp->GetVar("backgroundColor")->Set(MAKE_RGBA(0, 0, 0, 150));

	SetAlignmentEntity(pEnt, ALIGNMENT_CENTER);
	
	auto pEntIcon = CreateOverlayEntity(pEnt, "IconOverlay", "interface/music_icon.png", -90, -40);
	SetAlignmentEntity(pEntIcon, ALIGNMENT_CENTER);
	BobEntity(pEntIcon, 10, 0, 1000);
	SetSize2DEntity(pEntIcon, CL_Vec2f(0.6f, 0.16f));
	
	//again for other size of text

	pEntIcon = CreateOverlayEntity(pEnt, "IconOverlay", "interface/music_icon.png", GetSize2DEntity(pEnt).x-35, -40);
	SetAlignmentEntity(pEntIcon, ALIGNMENT_CENTER);
	BobEntity(pEntIcon, 10, 0, 1000);
	SetSize2DEntity(pEntIcon, CL_Vec2f(0.6f, 0.16f));

	ZoomToPositionFromThisOffsetEntity(pEnt, CL_Vec2f(GetScreenSizeXf(), 0), m_zoomSpeed);
	
	 
	pEnt->GetParent()->MoveEntityToTopByAddress(pEnt);

	return true;

}

void NowPlayingManager::UpdateFoobar()
{

	if (FileExists(GetApp()->m_nowPlayingTextFile))
	{
		TextScanner ts(GetApp()->m_nowPlayingTextFile, false);
		if (ts.GetLine(0).find("not running") != string::npos || ts.GetLine(0) == "" || ToUpperCaseString(ts.GetLine(0)) == "STOPPED")
		{
			if (m_bIsPlayingSong)
			{
				m_bIsPlayingSong = false;
				KillActiveSong();
			}
		}
		else
		{
			m_bIsPlayingSong = true;
			//looks like real song data here
			string tempSong = ts.GetLine(0);
			string tempArtist = ts.GetLine(1);

			if (tempSong != m_song || tempArtist != m_artist)
			{
				LogMsg("Song changed, it's now %s by %s", tempSong.c_str(), tempArtist.c_str());

				SetNewSong(tempSong, tempArtist);
			}
			else
			{
				PushSongLabelToTop();
			}
		}
	}
	else
	{
		if (m_bIsPlayingSong)
		{
			m_bIsPlayingSong = false;
			KillActiveSong();
		}

	}
}

bool NowPlayingManager::UpdateSpotifyWebWindow()
{

	
	bool isSpotifyWindowActive;
	bool isPlaying;
	string songName, artist;
	HWND spotifyBrowserWindowHWND;
	GetApp()->m_spotifyManager.GetActiveSong(songName, artist, isSpotifyWindowActive, isPlaying, spotifyBrowserWindowHWND);

	if (!isSpotifyWindowActive)
	{
		return false;
	}

	if (!isPlaying)
	{
		if (m_bIsPlayingSong)
		{
			m_bIsPlayingSong = false;
			KillActiveSong();
		}
		return false;
	}

	if (isPlaying)
	{
		m_bIsPlayingSong = true;

		if (songName != m_song || artist != m_artist)
		{
			SetNewSong(songName, artist);
		}
		else
		{
			//Song Hasn't changed, but push it to the top if needed
			PushSongLabelToTop();
		}

	}

	return true;
}


void NowPlayingManager::Update()
{

	
//	if (GetApp()->m_nowPlayingTextFile.empty()) return; //don't care

	if (m_updateTimer < GetTick())
	{
		m_updateTimer = GetTick() + m_updateSpeedMS;


		if (UpdateSpotifyWebWindow())
		{

		}
		else
		{
			//we'll, maybe we're using foobar then
			UpdateFoobar();
		}

		
		if (!m_bIsPlayingSong)
		{
			//KillActiveSong();
			m_song.clear();
			m_artist.clear();

		}

	}
}
