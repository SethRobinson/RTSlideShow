/*
 *  App.h
 *  Created by Seth Robinson on 3/6/09.
 *  For license info, check the license.txt file that should have come with this.
 *

//Some sound effects from: https://on-jin.com/sound/ta.php

 */

#pragma once

#include "BaseApp.h"
#include "RestManager.h"
#include "Manager/HueManager.h"
#include "FoobarManager.h"
#include "VariableManager.h"
#include "SpotifyManager.h"

class libVLC_RTSP;

//this define will cause windows builds to ignore the settings in main.cpp and force 1024X768
//#define C_FORCE_BIG_SCREEN_SIZE_FOR_WINDOWS_BUILDS


class App: public BaseApp
{
public:
	
	virtual void OnMessage(Message &message);

	void EarlyInit();

	App();
	virtual ~App();
	
	virtual bool Init();
	virtual void Kill();
	virtual void Draw();
	virtual void OnScreenSizeChange();
	virtual void Update();
	virtual bool OnPreInitVideo();

	string GetVersionString();
	float GetVersion();
	int GetBuild();
	void GetServerInfo(string &server, uint32 &port);
	VariantDB * GetShared() {return &m_varDB;}
	Variant * GetVar(const string &keyName );
	Variant * GetVarWithDefault(const string &varName, const Variant &var) {return m_varDB.GetVarWithDefault(varName, var);}
	int GetSpecial();
	void OnExitApp(VariantList *pVarList);

	//yeah I'm just using this for globals because I don't want to do it right.  fite me

	string m_slideDir = "slides"; //overwritten by config.txt
	int m_autoSlideTimeBetweenMS = 0; //overwritten by config.txt
	string m_nowPlayingTextFile; //blank for none, otherwise it reads the now playing simple .txt file that foobar rights to show the current song
	float m_slide_sfx_vol = 1.0f;
	
	CL_Vec2f m_clockPos = CL_Vec2f(0.0f, 0.0f); //where to show the clock, if enabled
	CL_Vec2f m_songPos = CL_Vec2f(0.0f, 0.0f);
	bool m_bShowClock = false;
	bool m_showCoords = true;
	bool m_disableNowPlaying = false;
	string m_configFile = "config.txt";
	string m_showEndMessage = ""; //this disabled it
	string m_endMessageTime;
	RestManager m_restManager; //opens a socket to listen for control requests
	HueManager m_hueManager;
	FoobarManager m_foobarManager;
	SpotifyManager m_spotifyManager;
	uint32 m_slideSpeedMS = 1000 * 60;

	bool m_bWindowedMode = true;
	VariableManager m_varMan;

	string m_hueUserName;
	string m_hueBridgeIP;

private:

	void ReadConfigFile();
	void UpdateVariables();

	bool m_bDidPostInit;
	VariantDB m_varDB; //holds all data we want to save/load
	int m_special;
	
};


extern App g_App;

App * GetApp();
const char * GetAppName();
const char * GetBundleName();
const char * GetBundlePrefix();