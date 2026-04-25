#include "PlatformPrecomp.h"
#include "NowPlayingManager.h"
#include "App.h"
#include "util/TextScanner.h"
#include "util/utf8.h"
#include "Entity/EntityUtils.h"
#include "GUI/FreeTypeManager.h"
#include "Renderer/SurfaceAnim.h"

static const float C_NOW_PLAYING_FONT_PIXEL_HEIGHT = 38.0f;
static const float C_NOW_PLAYING_MIN_FONT_PIXEL_HEIGHT = 24.0f;
static const float C_NOW_PLAYING_PADDING_X = 18.0f;
static const float C_NOW_PLAYING_PADDING_Y = 10.0f;
static const float C_NOW_PLAYING_ICON_VISIBLE_CENTER = 59.0f;
static const float C_NOW_PLAYING_ICON_GAP = 32.0f;
static const float C_NOW_PLAYING_SCREEN_EDGE_PADDING = 20.0f;

static string LocateNowPlayingAsset(string pathAndFile)
{
	if (FileExists(GetApp()->m_slideDir + "/" + pathAndFile))
	{
		return GetApp()->m_slideDir + "/" + pathAndFile;
	}

	return pathAndFile;
}

static wstring UTF8ToWideString(const string& text)
{
	vector<unsigned short> utf16line;
	try
	{
		utf8::utf8to16(text.begin(), text.end(), back_inserter(utf16line));
	}
	catch (...)
	{
		int wideCharsNeeded = MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), NULL, 0);
		if (wideCharsNeeded <= 0) return L"";

		wstring fallback(wideCharsNeeded, 0);
		MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), &fallback[0], wideCharsNeeded);
		return fallback;
	}

	wstring wideText;
	wideText.reserve(utf16line.size());
	for (unsigned short c : utf16line)
	{
		wideText += (WCHAR)c;
	}

	return wideText;
}

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

	string song = "Now playing: " + m_song + " by " + m_artist;

	if (artist.length() < 2)
	{
		song = "Now playing: " + m_song;
	}
	
	Entity *pParent = GetEntityRoot()->GetEntityByName("GUI");

	//GetApp()->GetEntityRoot()->PrintTreeAsText(2);
	if (!pParent) return false;
	
	
	AddFocusIfNeeded(pParent);

	//kill the old one
	KillActiveSong();
	CL_Vec2f vDrawPos;
	if (GetApp()->m_songPos.x != 0.0f || GetApp()->m_songPos.y != 0.0f) {
		vDrawPos = GetApp()->m_songPos;
	}
	else {
		// Use the default position you had before
		vDrawPos = CL_Vec2f(GetScreenSizeXf() / 2, GetScreenSizeYf() - 50); // Example default
	}

	FreeTypeManager freeTypeManager;
	freeTypeManager.SetFontName(LocateNowPlayingAsset("fonts/SourceHanSerif-Medium.ttc"));
	if (!freeTypeManager.Init())
	{
		return false;
	}

	wstring wideSong = UTF8ToWideString(song);
	if (wideSong.empty())
	{
		return false;
	}

	vector<unsigned short> utf16Song;
	utf16Song.reserve(wideSong.size());
	for (WCHAR c : wideSong)
	{
		utf16Song.push_back((unsigned short)c);
	}

	rtRectf textRect(0, 0, 0, 0);
	float pixelHeight = C_NOW_PLAYING_FONT_PIXEL_HEIGHT;
	float maxSurfaceWidth = GetScreenSizeXf() - ((C_NOW_PLAYING_ICON_VISIBLE_CENTER + C_NOW_PLAYING_ICON_GAP + C_NOW_PLAYING_SCREEN_EDGE_PADDING) * 2.0f);
	if (maxSurfaceWidth < 320.0f) maxSurfaceWidth = GetScreenSizeXf() - (C_NOW_PLAYING_SCREEN_EDGE_PADDING * 2.0f);
	float maxContentWidth = maxSurfaceWidth - (C_NOW_PLAYING_PADDING_X * 2.0f);
	if (maxContentWidth < 100.0f) maxContentWidth = 100.0f;

	freeTypeManager.MeasureText(&textRect, (WCHAR*)&wideSong[0], (int)wideSong.size(), pixelHeight, false);
	if (textRect.GetWidth() > maxContentWidth)
	{
		pixelHeight *= maxContentWidth / textRect.GetWidth();
		if (pixelHeight < C_NOW_PLAYING_MIN_FONT_PIXEL_HEIGHT) pixelHeight = C_NOW_PLAYING_MIN_FONT_PIXEL_HEIGHT;
		freeTypeManager.MeasureText(&textRect, (WCHAR*)&wideSong[0], (int)wideSong.size(), pixelHeight, false);
	}

	float surfaceWidth = textRect.GetWidth() + (C_NOW_PLAYING_PADDING_X * 2.0f);
	if (surfaceWidth > maxSurfaceWidth) surfaceWidth = maxSurfaceWidth;
	if (surfaceWidth < 320.0f) surfaceWidth = 320.0f;
	float contentWidth = surfaceWidth - (C_NOW_PLAYING_PADDING_X * 2.0f);
	if (contentWidth < 100.0f) contentWidth = surfaceWidth;
	float surfaceHeight = textRect.GetHeight() + (C_NOW_PLAYING_PADDING_Y * 2.0f);
	if (surfaceHeight < 64.0f) surfaceHeight = 64.0f;

	vector<CL_Vec2f> lineStarts;
	lineStarts.push_back(CL_Vec2f(C_NOW_PLAYING_PADDING_X, (surfaceHeight - textRect.GetHeight()) * 0.5f));

	SurfaceAnim* pTextSurface = freeTypeManager.TextToSurfaceAnim(CL_Vec2f(surfaceWidth, surfaceHeight), utf16Song, pixelHeight,
		glColorBytes(0, 0, 0, 150), glColorBytes(255, 255, 255, 255), false, &lineStarts, 0);
	if (!pTextSurface)
	{
		return false;
	}
	pTextSurface->SetSmoothing(true);

	Entity* pEnt = pParent->AddEntity(new Entity("Song"));
	pEnt->GetVar("pos2d")->Set(vDrawPos);
	pEnt->GetVar("size2d")->Set(CL_Vec2f(surfaceWidth, surfaceHeight));

	Entity* pTextEnt = pEnt->AddEntity(new Entity("SongText"));
	pTextEnt->GetVar("pos2d")->Set(CL_Vec2f(-surfaceWidth / 2.0f, -surfaceHeight / 2.0f));
	OverlayRenderComponent* pOverlayComp = (OverlayRenderComponent*)pTextEnt->AddComponent(new OverlayRenderComponent());
	pOverlayComp->SetSurface(pTextSurface, true);
	
	float iconY = -C_NOW_PLAYING_ICON_VISIBLE_CENTER;
	auto pEntIcon = CreateOverlayEntity(pEnt, "IconOverlay", "interface/music_icon.png",
		-surfaceWidth / 2.0f - C_NOW_PLAYING_ICON_GAP - C_NOW_PLAYING_ICON_VISIBLE_CENTER, iconY);
	SetAlignmentEntity(pEntIcon, ALIGNMENT_CENTER);
	BobEntity(pEntIcon, 10, 0, 1000);
	SetSize2DEntity(pEntIcon, CL_Vec2f(0.6f, 0.16f));
	
	//again for other size of text

	pEntIcon = CreateOverlayEntity(pEnt, "IconOverlay", "interface/music_icon.png",
		surfaceWidth / 2.0f + C_NOW_PLAYING_ICON_GAP - C_NOW_PLAYING_ICON_VISIBLE_CENTER, iconY);
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
