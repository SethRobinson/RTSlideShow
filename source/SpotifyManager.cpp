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

    if (IsInStringCaseInsensitive(title, "Google Chrome") || IsInStringCaseInsensitive(title, "Brave"))
    {
        if (IsInStringCaseInsensitive(title, "Spotify") || (IsInStringCaseInsensitive(title, "�E"))
            || (IsInStringCaseInsensitive(title, "?")))  //on windows 10, the dot becomes a ?, regional settings issue?
        {
            LogMsg("Spotify: Found browser+Spotify window: %s", title);
            HWND* pHwnd = reinterpret_cast<HWND*>(lParam);
            *pHwnd = hwnd;
            return FALSE; // Stop enumeration
        }
        else
        {
            LogMsg("Spotify: Browser window skipped (no Spotify match): %s", title);
        }
    }

    return TRUE;
}

void extractSongAndArtist(const std::string& title, std::string& song, std::string& artist)
{
    std::size_t pos = title.find(" �E");
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
        // Handle the case where neither " �E" nor " ? " are found.
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

void SpotifyManager::NextSong()
{
    INPUT input[2] = {};
    // Key down
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = VK_MEDIA_NEXT_TRACK;
    // Key up
    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = VK_MEDIA_NEXT_TRACK;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
    // Send the input events
    SendInput(2, input, sizeof(INPUT));
}

void SpotifyManager::PreviousSong()
{
    INPUT input[2] = {};
    // Key down
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = VK_MEDIA_PREV_TRACK;
    // Key up
    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = VK_MEDIA_PREV_TRACK;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
    // Send the input events
    SendInput(2, input, sizeof(INPUT));
}

void SpotifyManager::GetActiveSong(string &songPlayingOut, string &artistPlayingOut, bool &windowFoundOut, bool &isPlayingOut, HWND &spotifyBrowserWindowHWNDOut)
{
    // If we have a cached handle, verify it's still valid
    if (m_chromeSpotifyWindow != NULL)
    {
        if (!IsWindow(m_chromeSpotifyWindow))
        {
            LogMsg("Spotify: Cached window handle is no longer valid, will re-enumerate");
            m_chromeSpotifyWindow = NULL;
            m_bCheckedForChromeWindow = false;
        }
        else
        {
            char checkTitle[256];
            GetWindowText(m_chromeSpotifyWindow, checkTitle, sizeof(checkTitle));
            if (strlen(checkTitle) == 0)
            {
                LogMsg("Spotify: Cached window title is empty, will re-enumerate");
                m_chromeSpotifyWindow = NULL;
                m_bCheckedForChromeWindow = false;
            }
        }
    }

    if (m_chromeSpotifyWindow == NULL && m_bCheckedForChromeWindow == false)
    {
        m_bCheckedForChromeWindow = true;
       // LogMsg("Spotify: Enumerating windows to find browser+Spotify...");
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&m_chromeSpotifyWindow));

        if (m_chromeSpotifyWindow == NULL)
        {
          //  LogMsg("Spotify: No browser+Spotify window found after enumeration");
        }
    }

    spotifyBrowserWindowHWNDOut = NULL;
    windowFoundOut = false;
    isPlayingOut = false;
    artistPlayingOut = "";
    songPlayingOut = "";

    if (m_chromeSpotifyWindow)
    {
        spotifyBrowserWindowHWNDOut = m_chromeSpotifyWindow;

        char title[256];
        GetWindowText(m_chromeSpotifyWindow, title, sizeof(title));
        string sTitle(title);

        //LogMsg("Spotify: Window title: %s", sTitle.c_str());

        windowFoundOut = true;

        if (IsInStringCaseInsensitive(sTitle, "Spotify - Web Player"))
        {
         //   LogMsg("Spotify: Not playing - idle Web Player page");
            return;
        }

        if (IsInStringCaseInsensitive(sTitle, "Spotify Playlist"))
        {
         //   LogMsg("Spotify: Not playing - Spotify Playlist page");
            return;
        }
        if (IsInStringCaseInsensitive(sTitle, "playlist by"))
        {
         //   LogMsg("Spotify: Not playing - playlist by page");
            return;
        }

        isPlayingOut = true;

        StringReplace(" - Google Chrome", "", sTitle);
        StringReplace(" - Brave", "", sTitle);

        extractSongAndArtist(sTitle, songPlayingOut, artistPlayingOut);
       // LogMsg("Spotify: Playing - Song: %s, Artist: %s", songPlayingOut.c_str(), artistPlayingOut.c_str());
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

    /*
    if (!isSpotifyWindowActive)
    {
        return;
    }
    */

    //isPlaying = true; //hack because the window thing is unreliable
    GetApp()->m_spotifyManager.Pause();
    LogMsg("Toggling spotify music in SetSpotifyPauseVariant");
    return;

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

  /* if (!isSpotifyWindowActive)
   {
       return;
   }*/

   LogMsg("Scheduling to call SetSpotifyPauseVariant in %d ms", delayMS);

   VariantList vList;
   vList.Get(0).Set((int32)bPause);
   GetMessageManager()->CallStaticFunction(SetSpotifyPauseVariant, delayMS, &vList);

}





