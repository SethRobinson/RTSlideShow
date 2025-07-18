
#include "PlatformPrecomp.h"
#include "App.h"
#include "Renderer/LinearParticle.h"
#include "Entity/EntityUtils.h"//create the classes that our globally library expects to exist somewhere.
#include "Renderer/SoftSurface.h"
#include "Entity/ArcadeInputComponent.h" 
#include "Gamepad/GamepadManager.h"
#include "Gamepad/GamepadProvideriCade.h"
#include "GUI/IntroMenu.h"
#include "util/TextScanner.h"
#include "NowPlayingManager.h"
#include "Entity/LibVlcStreamComponent.h"
#include "Script.h"
#include "SlideManager.h"

#ifdef RT_MOGA_ENABLED
#include "Gamepad/GamepadProviderMoga.h"
#endif
 
SlideManager g_slideManager;

MessageManager g_messageManager;
MessageManager * GetMessageManager() {return &g_messageManager;}

FileManager g_fileManager;
FileManager * GetFileManager() {return &g_fileManager;}

GamepadManager g_gamepadManager;
GamepadManager * GetGamepadManager() {return &g_gamepadManager;}

#ifdef __APPLE__

#if TARGET_OS_IPHONE == 1
	//it's an iPhone or iPad
	#include "Audio/AudioManagerOS.h"
	AudioManagerOS g_audioManager;

	#ifdef RT_IOS_60BEAT_GAMEPAD_SUPPORT
	#include "Gamepad/GamepadProvider60Beat.h"
	#endif
#else
	//it's being compiled as a native OSX app
   #include "Audio/AudioManagerFMOD.h"

   AudioManagerFMOD g_audioManager; //dummy with no sound
#endif
	
#else
#include "Audio/AudioManagerSDL.h"
#include "Audio/AudioManagerAndroid.h"

#if defined RT_WEBOS || defined RT_USE_SDL_AUDIO
AudioManagerSDL g_audioManager; //sound in windows and WebOS
//AudioManager g_audioManager; //to disable sound
#elif defined ANDROID_NDK
//AudioManager g_audioManager; //to disable sound
AudioManagerAndroid g_audioManager; //sound for android
#else
//in windows

#include "Gamepad/GamepadProviderDirectX.h"
#include "Gamepad/GamepadProviderXInput.h"


#include "Audio/AudioManagerAudiere.h"
//#include "Audio/AudioManagerFMOD.h"

AudioManagerAudiere g_audioManager;  //Use Audiere for audio
//AudioManagerFMOD g_audioManager; //if we wanted FMOD sound in windows
//AudioManager g_audioManager; //to disable sound

#endif
#endif


NowPlayingManager g_nowPlayingManager;

AudioManager * GetAudioManager(){return &g_audioManager;}

App *g_pApp = NULL;
BaseApp * GetBaseApp() 
{
	if (!g_pApp)
	{
		g_pApp = new App;
	}
	return g_pApp;
}

App * GetApp() 
{
	return g_pApp;
}

void App::OnMessage(Message &message)
{

	if (message.GetType() == MESSAGE_TYPE_FILE_DROPPED)
	{
		LogMsg("File dropped: %s at %s", message.GetVariantList().Get(0).GetString().c_str(),
			PrintVector2(message.GetVariantList().Get(1).GetVector2()).c_str());
	
		//open the media file if it's a media type we recognize
		string file = message.GetVariantList().Get(0).GetString();
		
		if (g_slideManager.IsThingWeCanShow(file))
		{
			g_slideManager.CreateMediaFromFileName(file, "DroppedMedia", message.GetVariantList().Get(1).GetVector2(), false);
		}
		else
		{
			LogMsg("Don't know how to handle filetype of %s", file.c_str());
		}
		

	
	}
	BaseApp::OnMessage(message);
}
void App::EarlyInit()
{
	auto parms = GetBaseApp()->GetCommandLineParms();

	
	//check each string in parms
	for (auto it = parms.begin(); it != parms.end(); ++it)
	{
		//separate the string into words
		std::vector<std::string> words = StringTokenize(*it, "=");

		if (words.size() < 1) continue;

		if (ToUpperCaseString(words[0]) == "CONFIG" || ToUpperCaseString(words[0]) == "-CONFIG")
		{
			m_configFile = words[1];
			LogMsg("Config set to %s", words[1].c_str());
		} else

		if (ToUpperCaseString(words[0]) == "-WINDOWED" || ToUpperCaseString(words[0]) == "-WINDOW")
		{
			LogMsg("Setting windowed mode");
			m_bWindowedMode = true;
		}
		else
		if(ToUpperCaseString(words[0]) == "-FULLSCREEN")
		{
			LogMsg("Setting fullscreen mode");
			m_bWindowedMode = false;
		}
		else

		{
			//what is it then?
			LogMsg("Config set to %s", words[0].c_str());
			m_configFile = words[0];
		}

	}
}

App::App()
{
	m_bDidPostInit = false;
	OneTimeReleaseOnAppClose();
}

App::~App()
{
	L_ParticleSystem::deinit();
}

void App::OnExitApp(VariantList *pVarList)
{
	LogMsg("Exiting the app");
	OSMessage o;
	o.m_type = OSMessage::MESSAGE_FINISH_APP;
	GetBaseApp()->AddOSMessage(o);
}

bool App::Init()
{
	//SetDefaultAudioClickSound("audio/enter.wav");
	SetDefaultButtonStyle(Button2DComponent::BUTTON_STYLE_CLICK_ON_TOUCH_RELEASE);
	SetManualRotationMode(true);
	
	//to test at various FPS
	SetFPSLimit(0);

	/*
	bool isSpotifyWindowActive;
	bool isPlaying;
	string songName, artist;
	HWND spotifyBrowserWindowHWND;

	m_spotifyManager.GetActiveSong(songName, artist, isSpotifyWindowActive, isPlaying, spotifyBrowserWindowHWND);
	
	if (isSpotifyWindowActive)
	{
		if (isPlaying)
		{
			LogMsg("Spotify is active, song: %s by %s", songName.c_str(), artist.c_str());
		}
		else
		{
			LogMsg("Spotify is active, but not playing");
		}
		//SetForegroundWindow(spotifyBrowserWindowHWND);
		//m_spotifyManager.TogglePlayback(spotifyBrowserWindowHWND);

		m_spotifyManager.Pause();
	} else
	{
		LogMsg("Spotify is not active");
	}

	*/
	
	//we'll use a virtual screen size of this, and it will be scaled to any device
	int scaleToX = 1920;
	int scaleToY = 1080;

	L_ParticleSystem::init(2000);

	SetupFakePrimaryScreenSize(scaleToX, scaleToY); //game will think it's this size, and will be scaled up

	//setup some variables

	//convert GetVersion() from a float to a string that has 2 decimal places like "1.00"
	string num = toString(GetVersion());
	if (num.find(".") == string::npos)
	{
		num += ".00";
	}
	else
	{
		//add a zero if it's only one decimal place
		if (num.length() - num.find(".") == 2)
		{
			num += "0";
		}
	}

	m_varMan.SetVar("version", num);


	if (m_bInitted)	
	{
		return true;
	}
	
	if (!BaseApp::Init()) return false;

	LogMsg("Save path is %s", GetSavePath().c_str());

	if (!GetFont(FONT_SMALL)->Load("interface/font_trajan.rtfont")) 
	{
		LogMsg("Can't load font 1");
		return false;
	}
	if (!GetFont(FONT_LARGE)->Load("interface/font_trajan_big.rtfont"))
	{
		LogMsg("Can't load font 2");
		return false;
	}
	//GetFont(FONT_SMALL)->SetSmoothing(false); //if we wanted to disable bilinear filtering on the font

#ifdef _DEBUG
	GetBaseApp()->SetFPSVisible(true);
#endif
	
	bool bFileExisted;
	m_varDB.Load("save.dat", &bFileExisted);
	m_varDB.GetVarWithDefault("level", Variant(uint32(1)));

	ReadConfigFile();

	//preload audio
	GetAudioManager()->Preload("audio/click.wav");

#ifdef PLATFORM_WINDOWS
	//If you don't have directx, just comment out this and remove the dx lib dependency, directx is only used for the
	//gamepad input on windows
	GamepadProviderXInput* pTemp = new GamepadProviderXInput();
	pTemp->PreallocateControllersEvenIfMissing(true); 
	GetGamepadManager()->AddProvider(pTemp); //use XInput joysticks

	//do another scan for directx devices
	GamepadProviderDirectX* pTempDirectX = new GamepadProviderDirectX;
	pTempDirectX->SetIgnoreXInputCapableDevices(true);
	GetGamepadManager()->AddProvider(pTempDirectX); //use directx joysticks

#endif

	//GetGamepadManager()->AddProvider(new GamepadProvideriCade); //use iCade, this actually should work with any platform...
    //OnFullscreenToggleRequest();

	g_nowPlayingManager.Init();

	SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_CONTINUOUS);

	if (!m_restManager.Init())
	{
		//show a windows text box with an error
		MessageBox(NULL, "Can't init rest manager.  Port already in use?", "Error", MB_OK);
		return false;
	}
	 
	if (!GetApp()->m_hueBridgeIP.empty())
	{
		if (!m_hueManager.Init(GetApp()->m_hueBridgeIP, GetApp()->m_hueUserName))
		{
			//show a windows text box with an error
			MessageBox(NULL, "Can't init hue manager.  Ignoring hue stuff then", "Error", MB_OK);
		}
	}



	return true;
}

void App::Kill()
{
	m_varDB.Save("save.dat");
	
	BaseApp::Kill();
	g_pApp = NULL;
}


void App::ReadConfigFile()
{
	const int buffSize = 256;
	//char buff[buffSize];
	TextScanner ts;

	//set m_clockPos to 0,0
	m_clockPos = CL_Vec2f(0.0f, 0.0f);

	if (ts.LoadFile(m_configFile))
	{
		m_autoSlideTimeBetweenMS = StringToInt(ts.GetParmString("autoSlideTimeBetweenMS", 1));
		m_slideDir = ts.GetParmString("slideDir", 1);
		m_nowPlayingTextFile = ts.GetParmString("nowPlayingTextFile", 1);

		//check config file to see if show_cursor is set to 1
		bool bShowCursor = StringToInt(ts.GetParmString("showCursor", 1));
		ShowCursor(bShowCursor);
		m_slide_sfx_vol = StringToFloat(ts.GetParmString("slideSFXVol", 1));

		m_bShowClock = StringToInt(ts.GetParmString("showClock", 1));
		
		//let's also set m_clockPos with "setClockPos" parm, if it exists
		string clockPos = ts.GetParmString("setClockPos", 1);
		if (clockPos != "")
		{
			vector<string> coords = StringTokenize(clockPos, ",");
			if (coords.size() == 2)
			{
				m_clockPos.x = StringToFloat(coords[0]);
				m_clockPos.y = StringToFloat(coords[1]);
			}
			else
			{
				LogMsg("Error in config.txt, setClockPos should be like: setClockPos=100,200");
			}
		}


		
		m_showCoords = StringToInt(ts.GetParmString("showCoords", 1));
		m_disableNowPlaying = StringToInt(ts.GetParmString("disableNowPlaying", 1));

		m_showEndMessage = ts.GetParmString("showEndMessage", 2);

		if (m_showEndMessage != "")
		{
			//oh, it actually was set?  wow, ok
			m_endMessageTime =ts.GetParmString("showEndMessage", 1);
		}
		m_hueUserName = ts.GetParmString("hueUserName", 1);
		m_hueBridgeIP = ts.GetParmString("hueBridgeIP", 1);
	}

}

void App::UpdateVariables()
{
	m_varMan.SetVar("screen_width", toString(GetScreenSizeX()));
	m_varMan.SetVar("screen_height", toString(GetScreenSizeY()));
	m_varMan.SetVar("screen_width_half", toString(GetScreenSizeX()/2));
	m_varMan.SetVar("screen_height_half", toString(GetScreenSizeY()/2));

}

void App::Update()
{
	BaseApp::Update();
	m_restManager.Update();
	m_hueManager.Update();
	
	UpdateVariables();

	g_gamepadManager.Update();
	if (!m_disableNowPlaying)
	{
		g_nowPlayingManager.Update();
	}

	if (!m_bDidPostInit)
	{
		m_bDidPostInit = true;
	
		//build a dummy entity called "GUI" to put our GUI menu entities under
		Entity *pGUIEnt = GetEntityRoot()->AddEntity(new Entity("GUI"));
		AddFocusIfNeeded(pGUIEnt);

#ifdef _DEBUG
		IntroMenuCreate(pGUIEnt);
//		MainMenuCreate(pGUIEnt);
//		GameMenuCreate(pGUIEnt);
#else
		IntroMenuCreate(pGUIEnt);
#endif
		LaunchScript("Startup");
		//AddNewStream("StreamEon", "rtsp://192.168.68.114/0", 500, pGUIEnt);
		//AddNewStream("StreamMe", "rtsp://192.168.68.137/0", 333, pGUIEnt);
		//SetStreamVolumeByName("StreamMe", 0.3f);
	}
}

void App::Draw()
{
	PrepareForGL();
//	glClearColor(0.6,0.6,0.6,1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	BaseApp::Draw();

	uint32 backgroundColor = MAKE_RGBA(0, 0, 0, 150);
	rtRectf rt;
	
	if (GetApp()->m_bShowClock)
	{
        //LogMsg("Showing clock");
        string time = GetTimeAsString();
        CL_Vec2f vDrawPos;
        if (m_clockPos.x != 0.0f || m_clockPos.y != 0.0f) {
            vDrawPos = m_clockPos;
        } else {
            vDrawPos = CL_Vec2f(GetScreenSizeXf() / 2, GetScreenSizeYf() - 100);
        }
        GetFont(FONT_LARGE)->DrawAlignedBackground(vDrawPos.x, vDrawPos.y, time, ALIGNMENT_DOWN_CENTER, 1.0f, backgroundColor);
        GetFont(FONT_LARGE)->DrawAligned(vDrawPos.x, vDrawPos.y, time, ALIGNMENT_DOWN_CENTER);
	}
	
	if (m_showEndMessage != "")
	{
		if (IsCurrentTimeSameOrLaterThanThis(m_endMessageTime))
		{
			//show the end string
			CL_Vec2f vDrawPos = CL_Vec2f(GetScreenSizeXf() / 2, GetScreenSizeYf() - 140);
			GetFont(FONT_LARGE)->DrawAlignedBackground(vDrawPos.x, vDrawPos.y, m_showEndMessage, ALIGNMENT_DOWN_CENTER, 1.0f, backgroundColor);
			GetFont(FONT_LARGE)->DrawAligned(vDrawPos.x, vDrawPos.y, m_showEndMessage, ALIGNMENT_DOWN_CENTER);
		}
	}

	g_globalBatcher.Flush();
}

void App::OnScreenSizeChange()
{
	BaseApp::OnScreenSizeChange();
}

void App::GetServerInfo( string &server, uint32 &port )
{
#if defined (_DEBUG) && defined(WIN32)
	server = "localhost";
	port = 8080;

	//server = "www.rtsoft.com";
	//port = 80;
#else

	server = "rtsoft.com";
	port = 80;
#endif
}

int App::GetSpecial()
{
	return m_special; //1 means pirated copy
}

Variant * App::GetVar( const string &keyName )
{
	return GetShared()->GetVar(keyName);
}

std::string App::GetVersionString()
{
	return "V1.11";
}

float App::GetVersion()
{
	return 1.11f;
}

int App::GetBuild()
{
	return 1;
}

const char * GetAppName() {return "RTSlideShow by Seth A. Robinson, created with Proton SDK - rtsoft.com";}

//for palm webos and android
const char * GetBundlePrefix()
{
	const char * bundlePrefix = "com.rtsoft.";
	return bundlePrefix;
}

const char * GetBundleName()
{
	const char * bundleName = "RTSlideShow";
	return bundleName;
}

	//below is a sort of hack that allows "release" builds on windows to override the settings of whatever the shared main.cpp is telling
	//us for window sizes
	#ifdef _WINDOWS_
	#include "win/app/main.h"
	extern bool g_bIsFullScreen;
	#endif

	bool App::OnPreInitVideo()
	{
		g_pApp->EarlyInit();

		if (!BaseApp::OnPreInitVideo()) return false;

		if (!m_bWindowedMode)
		{

			//comment out this part if you really want to emulate other systems/phone sizes, from settings in main.cpp
			SetEmulatedPlatformID(PLATFORM_ID_WINDOWS);

			//go full screen
			g_bIsFullScreen = true;
			HWND        hDesktopWnd = GetDesktopWindow();
			HDC         hDesktopDC = GetDC(hDesktopWnd);
			g_winVideoScreenX = GetDeviceCaps(hDesktopDC, HORZRES);
			g_winVideoScreenY = GetDeviceCaps(hDesktopDC, VERTRES);
			ReleaseDC(hDesktopWnd, hDesktopDC);
		}
		else
		{
			//degug mode

			SetupScreenInfo(1920, 1080, ORIENTATION_DONT_CARE);

#if defined(WINAPI)
		g_winVideoScreenX = 1920;
		g_winVideoScreenY = 1080;
#endif
	}


	return true;
	}

