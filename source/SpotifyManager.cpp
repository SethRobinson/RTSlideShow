#include "PlatformPrecomp.h"
#include "SpotifyManager.h"
#include "App.h"

SpotifyManager::SpotifyManager()
{
	
}

SpotifyManager::~SpotifyManager()
{
}
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    char title[256];
    char className[256];

    GetWindowText(hwnd, title, sizeof(title));
    GetClassName(hwnd, className, sizeof(className));

    //std::cout << "Window Title: " << title << ", Class Name: " << className << std::endl;
    // Assume LogMsg is some logging function you have
   // LogMsg("Window Title: %s, Class Name: %s", title, className);

    if (IsInStringCaseInsensitive(title, "Google Chrome"))
    {
        if (IsInStringCaseInsensitive(title, "Spotify") || (IsInStringCaseInsensitive(title, "•")) 
            || (IsInStringCaseInsensitive(title, "?")))  //on windows 10, the dot becomes a ?, regional settings issue?
        {
            //LogMsg("Found %s", title);
            HWND* pHwnd = reinterpret_cast<HWND*>(lParam);
            *pHwnd = hwnd;
            return FALSE; // Stop enumeration
        }
    }

    return TRUE;
}

void extractSongAndArtist(const std::string& title, std::string& song, std::string& artist)
{
    std::size_t pos = title.find(" • ");
    if (pos == std::string::npos)
    {
        pos = title.find(" ? ");
    }

    if (pos != std::string::npos)
    {
        song = title.substr(0, pos);
        artist = title.substr(pos + 3);
    }
    else
    {
        // Handle the case where neither " • " nor " ? " are found.
        // Here, we are being error-tolerant and assuming the whole title is the song name.
        song = title;
        artist = "Unknown Artist";
    }
}



void SpotifyManager::TogglePlayback(HWND hwnd)
{
    // Simulate a key press
    PostMessage(hwnd, WM_KEYDOWN, VK_SPACE, 0);

    // Simulate a key release
    PostMessage(hwnd, WM_KEYUP, VK_SPACE, 0);
}


void SpotifyManager::Pause()
{
    INPUT input[2] = {};

    // Key down
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = VK_MEDIA_PLAY_PAUSE;

    // Key up
    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = VK_MEDIA_PLAY_PAUSE;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;

    // Send the input events
    SendInput(2, input, sizeof(INPUT));
}

void SpotifyManager::GetActiveSong(string &songPlayingOut, string &artistPlayingOut, bool &windowFoundOut, bool &isPlayingOut, HWND &spotifyBrowserWindowHWNDOut)
{
   
    if (m_chromeSpotifyWindow == NULL && m_bCheckedForChromeWindow == false)
    {
        m_bCheckedForChromeWindow = true;
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&m_chromeSpotifyWindow));
    }
    spotifyBrowserWindowHWNDOut = NULL;

    windowFoundOut = false;
    isPlayingOut = false;
    artistPlayingOut = "";
    songPlayingOut = "";

    if (m_chromeSpotifyWindow)
    {
        spotifyBrowserWindowHWNDOut = m_chromeSpotifyWindow;

        // Now you can send key press messages to this window
        char title[256];
        GetWindowText(m_chromeSpotifyWindow, title, sizeof(title));
        string sTitle(title);

        //LogMsg("Found a window: %s", sTitle.c_str());

        windowFoundOut = true;

        if (IsInStringCaseInsensitive(sTitle, "Spotify - Web Player"))
        {
            //found spotify, but it's not playing anything
            return;
        }

        if (IsInStringCaseInsensitive(sTitle, "Spotify Playlist"))
        {
            //found spotify, but it's not playing anything
            return;
        }
        if (IsInStringCaseInsensitive(sTitle, "playlist by"))
        {
            //found spotify, but it's not playing anything
            return;
        }


        isPlayingOut = true;

        StringReplace(" - Google Chrome", "", sTitle);

        extractSongAndArtist(sTitle, songPlayingOut, artistPlayingOut);
        return;
    }

}

void SetSpotifyPauseVariant(VariantList* pVList)
{
    bool bPause = pVList->Get(0).GetINT32() != 0;


    bool isSpotifyWindowActive;
    bool isPlaying;
    string songName, artist;
    HWND spotifyBrowserWindowHWND;

    GetApp()->m_spotifyManager.GetActiveSong(songName, artist, isSpotifyWindowActive, isPlaying, spotifyBrowserWindowHWND);

    if (!isSpotifyWindowActive)
    {
        return;
    }
  
    if (isPlaying)
    {
       // LogMsg("Spotify is active, song: %s by %s", songName.c_str(), artist.c_str());
        if (bPause == true)
        {
            GetApp()->m_spotifyManager.Pause();
        }
    }
    else
    {
        //LogMsg("Spotify is active, but not playing");
        if (bPause == false)
        {
            GetApp()->m_spotifyManager.Pause();
        }
    }

 

}


void SpotifyManager::SetPause(bool bPause, int delayMS)
{

    bool isSpotifyWindowActive;
    bool isPlaying;
    string songName, artist;
    HWND spotifyBrowserWindowHWND;

   GetActiveSong(songName, artist, isSpotifyWindowActive, isPlaying, spotifyBrowserWindowHWND);

   if (!isSpotifyWindowActive)
   {
       return;
   }
   VariantList vList;
   vList.Get(0).Set((int32)bPause);
   GetMessageManager()->CallStaticFunction(SetSpotifyPauseVariant, delayMS, &vList);

}





