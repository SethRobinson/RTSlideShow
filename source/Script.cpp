#include "PlatformPrecomp.h"
#include "Script.h"
#include "Audio/AudioManager.h"
#include "util/MiscUtils.h"
#include "Manager/HueManager.h"
#include "Entity/HTTPComponent.h"
#include "Entity/EntityUtils.h"
#include "App.h"
#include "Entity/TouchDragComponent.h"
#include "Entity/TouchDragMoveComponent.h"
#include "Entity/ScrollToZoomComponent.h"
#include "Entity/LibVlcStreamComponent.h"
#include "WindowsFunctions.h"

//Tracks active per-frame "keep_on_top" hooks so we can disconnect on enabled|0.
//Entry is removed automatically (via sig_onRemoved) when the entity dies.
static std::map<Entity*, boost::signals2::connection> g_keepOnTopConnections;

static void KeepOnTopTick(Entity* pEnt, VariantList* /*pVList*/)
{
	if (pEnt && pEnt->GetParent())
	{
		pEnt->GetParent()->MoveEntityToTopByAddress(pEnt);
	}
}

static void KeepOnTopOnRemoved(Entity* pEnt)
{
	g_keepOnTopConnections.erase(pEnt);
}

int StringAndVarToInt(string in)
{
	return atoi(GetApp()->m_varMan.ReplaceVars(in).c_str());
}


float StringAndVarToFloat(string in)
{
	return (float)atof(GetApp()->m_varMan.ReplaceVars(in).c_str());
}

void PlayMusicOnceWithResume(VariantList* pVList)
{
	string fName = pVList->Get(0).GetString();
	LogMsg("PlayMusicOnceWithResume: starting playback of %s", fName.c_str());
	GetAudioManager()->Play(fName, false, true);
	GetApp()->m_bMusicOncePlaybackStarted = true;
	LogMsg("PlayMusicOnceWithResume: IsPlayingMusic: %d", (int)GetAudioManager()->IsPlayingMusic());
}

//Helper for the "set_stream_pause" script command.  Routed through MessageManager
//so the script can specify a delayMS, which is useful at startup: libVLC's
//SetPause is a no-op until the player has actually started playing, so we want to
//wait a few hundred ms after add_stream before pausing.  Calls OnSetPause on the
//component so restart_on_play behavior is honored on later unpause clicks.
void SetStreamPauseHelper(VariantList* pVList)
{
	string name = pVList->Get(0).GetString();
	bool bPaused = pVList->Get(1).GetUINT32() != 0;

	LibVlcStreamComponent* pVlcComp = GetStreamEntityByName(name);
	if (!pVlcComp)
	{
		LogMsg("set_stream_pause: can't find stream %s", name.c_str());
		return;
	}

	VariantList vList;
	vList.Get(0).Set(uint32(bPaused ? 1 : 0));
	pVlcComp->OnSetPause(&vList);
}

void StopMusicAndPlayBGIfNeeded(VariantList* pVList)
{

	if (GetAudioManager()->IsPlayingMusic())
	{
		StopMusic(pVList);

		if (!GetApp()->GetBGMusicIsPlaying() && GetApp()->GetBGMusicMode() && GetApp()->m_bBGMusicWasPlayingBeforePlayMusic)
		{
			GetApp()->m_foobarManager.SetPause(false, 0);
			GetApp()->m_spotifyManager.SetPause(false, 0);
			GetApp()->SetBGMusicIsPlaying(true);
			LogMsg("StopMusicAndPlayBGIfNeeded: resumed BG music (was playing before)");
		}
	}

}


void ToggleMusic(VariantList* pVList)
{

	if (GetAudioManager()->IsPlayingMusic())
	{
		StopMusic(pVList);

		if (!GetApp()->GetBGMusicIsPlaying() && GetApp()->GetBGMusicMode() && GetApp()->m_bBGMusicWasPlayingBeforePlayMusic)
		{
			GetApp()->m_foobarManager.SetPause(false, 0);
			GetApp()->m_spotifyManager.SetPause(false, 0);
			LogMsg("ToggleMusic: resumed BG music (was playing before)");
			GetApp()->SetBGMusicIsPlaying(true);
		}
	}
	else
	{

		if (GetApp()->GetBGMusicMode())
		{

			if (!GetApp()->GetBGMusicIsPlaying())
			{
				//play bg music
				GetApp()->m_foobarManager.SetPause(false, 0);
				GetApp()->m_spotifyManager.SetPause(false, 0);
				GetApp()->SetBGMusicIsPlaying(true);
				LogMsg("We think we started playing BG music, with GetApp()->GetBGMusicMode() true");

			}
			else
			{
				//stop the bg music!
				GetApp()->m_foobarManager.SetPause(true, 0);
				GetApp()->m_spotifyManager.SetPause(true, 0);
				GetApp()->SetBGMusicIsPlaying(false);

				LogMsg("We stopped the BG music that was playing.  GetApp()->GetBGMusicMode() = true ");

			}
		}

	}


}

Script::Script()
{

}

Script::~Script()
{
}

bool Script::Load(string fname)
{


	//see how many scripts there are that start with this name, first one exists, so we know at least one is valid
	vector<string> fileVec;
	fileVec.push_back(fname);

	//remove the .txt at the end
	string fileNameWithoutExtension = fname.substr(0, fname.length() - 4);

	int counter = 1;
	while (1)
	{

		string nameToTry = fileNameWithoutExtension + "_" + toString(counter) + ".txt";
		if (FileExists(nameToTry))
		{
			fileVec.push_back(nameToTry);
			counter++;
		}
		else
		{
			break;
		}

	}


	//choose which fName to use
	fname = fileVec[RandomRange(0, (int)fileVec.size())];

	if (!m_scanner.LoadFile(fname, false))
	{
		LogMsg("Error: Script %s not found.", fname.c_str());
		return false;
	}

	return true;
}

void ShowImage(VariantList* pVList)
{

	//Now let's pull the vars out of the PVList into normal variables
	string filename = pVList->Get(0).GetString();
	uint32 durationMS = pVList->Get(1).GetUINT32();
	int32 x = pVList->Get(2).GetINT32();
	int32 y = pVList->Get(3).GetINT32();
	float scale = pVList->Get(4).GetFloat();
	int32 smoothing = pVList->Get(5).GetINT32();
	string transition = pVList->Get(6).GetString();

	//create the image entity
	Entity* pParent = GetBaseApp()->GetEntityRoot()->GetEntityByName("GUI");

	Entity* pImage = CreateOverlayEntity(pParent, "ShowImage", filename, (float)x, (float)y);
	SetScale2DEntity(pImage, CL_Vec2f(scale, scale));
	SetSmoothingEntity(pImage, smoothing == 1);

	if (transition == "bob")
	{
		//PulsateColorEntity(pImage, false, 0, 900);
		FadeInEntity(pImage, true, 1000);
		BobEntity(pImage, 100);
		
		if (durationMS > 0)
		{
			FadeOutAndKillEntity(pImage, true, 500, durationMS + 500);
		}
	}
	else if (transition == "fade")
	{
		//PulsateColorEntity(pImage, false, 0, 900);
		
		FadeInEntity(pImage, true, 1000);
		if (durationMS > 0)
		{
			FadeOutAndKillEntity(pImage, true, 1000, durationMS + 1000);
		}
	}
	else
	{
		//default
		SlideScreen(pImage, true);
		
		if (durationMS > 0)
		{
			FadeOutEntity(pImage, true, 500, durationMS + 500);
			KillEntity(pImage, durationMS + 1000); //kill it a little after the slide is done
		}

	}

	//pImage->GetParent()->MoveEntityToTopByAddress(pImage);
#ifdef _DEBUG
	GetApp()->GetEntityRoot()->PrintTreeAsText();
#endif
}


string Script::LocateFile(string pathAndFile)
{
	
	if (FileExists(GetApp()->m_slideDir + "/" + pathAndFile))
	{
		//found it
		return GetApp()->m_slideDir + "/" + pathAndFile;
	}

	if (!FileExists(pathAndFile))
	{
		LogMsg("Can't find %s", pathAndFile.c_str());
	}

	return pathAndFile;
}

string Script::ReplaceLocalVariables(string in)
{

	StringReplace( "%parm1%", m_parm1, in);
	return in;
}

void Script::Run()
{
	for (int i = 0; i < m_scanner.GetLineCount(); i++)
	{
		string line = m_scanner.GetLine(i);

		line = StripWhiteSpace(line);

		//ignore lines starting with // or # or blank lines
		if (line == "" || line[0] == '/' || line[0] == '#')
		{
			continue;
		}

		auto words = StringTokenize(line, "|");

		//process something like "play_sfx|delayMS|0|filename|audio/keypad_hit.wav"

		if (words[0] == "play_sfx")
		{
			int timeMS = StringAndVarToInt(words[2]);
			string sfx = GetApp()->m_varMan.ReplaceVars(words[4]);

			VariantList vList;
			vList.Get(0).Set(sfx);
			GetMessageManager()->CallStaticFunction(PlaySound, timeMS, &vList);
		}

		if (words[0] == "set_system_volume")
		{

			string parm = ReplaceLocalVariables(words[1]);
			int volume = StringAndVarToInt(parm);
			
			LogMsg("Setting system volume to %d", volume);
			
			SetWindowsSystemVolume(volume); //this function should take between 0 and 100 and set the volume in
			//a way that makes sense (50 is half volume, etc)

			
			/*VariantList vList;
			vList.Get(0).Set(sfx);
			GetMessageManager()->CallStaticFunction(PlaySound, timeMS, &vList);*/
		}

		if (words[0] == "play_music")
		{
			if (GetApp()->m_bWaitingForMusicOnceToFinish)
			{
				LogMsg("play_music: ignoring because play_music_once is in progress");
				continue;
			}

			int timeMS = StringAndVarToInt(words[2]);
			string sfx = GetApp()->m_varMan.ReplaceVars(words[4]);

			LogMsg("play_music: %s (delay %d ms, BGMusicPlaying: %d)", sfx.c_str(), timeMS, (int)GetApp()->GetBGMusicIsPlaying());

			VariantList vList;
			vList.Get(0).Set(sfx);

			GetApp()->m_bBGMusicWasPlayingBeforePlayMusic = GetApp()->GetBGMusicIsPlaying();

			if (GetApp()->GetBGMusicIsPlaying())
			{
				LogMsg("play_music: pausing BG music");
				GetApp()->m_foobarManager.SetPause(true, 0);
				GetApp()->m_spotifyManager.SetPause(true, 0);
				GetApp()->SetBGMusicIsPlaying(false);
			}


			GetMessageManager()->CallStaticFunction(PlayMusic, timeMS, &vList);
		
		}

		if (words[0] == "play_music_once")
		{
			if (GetApp()->m_bWaitingForMusicOnceToFinish)
			{
				LogMsg("play_music_once: ignoring because another play_music_once is already in progress");
				continue;
			}

			int timeMS = StringAndVarToInt(words[2]);
			string sfx = GetApp()->m_varMan.ReplaceVars(words[4]);

			LogMsg("play_music_once: %s (delay %d ms, BGMusicPlaying: %d)", sfx.c_str(), timeMS, (int)GetApp()->GetBGMusicIsPlaying());

			VariantList vList;
			vList.Get(0).Set(sfx);

			GetApp()->m_bBGMusicWasPlayingBeforeMusicOnce = GetApp()->GetBGMusicIsPlaying();
			GetApp()->m_bWaitingForMusicOnceToFinish = true;
			GetApp()->m_bMusicOncePlaybackStarted = false;

			if (GetApp()->GetBGMusicIsPlaying())
			{
				LogMsg("play_music_once: pausing BG music");
				GetApp()->m_foobarManager.SetPause(true, 0);
				GetApp()->m_spotifyManager.SetPause(true, 0);
				GetApp()->SetBGMusicIsPlaying(false);
			}

			GetMessageManager()->CallStaticFunction(PlayMusicOnceWithResume, timeMS, &vList);
		}
		 
		if (words[0] == "next_song")
		{
			//GetApp()->m_foobarManager.NextSong();
			if (GetApp()->GetBGMusicIsPlaying())
			{
				GetApp()->m_spotifyManager.NextSong();
			}
		}

		if (words[0] == "previous_song")
		{
			//GetApp()->m_foobarManager.NextSong();
			if (GetApp()->GetBGMusicIsPlaying())
			{
				GetApp()->m_spotifyManager.PreviousSong();
			}
		}

		if (words[0] == "stop_music")
		{
			int timeMS = StringAndVarToInt(words[2]);
			VariantList vList;
			GetMessageManager()->CallStaticFunction(StopMusicAndPlayBGIfNeeded, timeMS, &vList);
		}
		if (words[0] == "toggle_music")
		{
			int timeMS = StringAndVarToInt(words[2]);
			VariantList vList;
			GetMessageManager()->CallStaticFunction(ToggleMusic, timeMS, &vList);
		}

		if (words[0] == "toggle_bg_music_mode")
		{
			GetApp()->ToggleBGMusicMode();

			GetApp()->m_foobarManager.SetPause(false, 0);
			GetApp()->m_spotifyManager.SetPause(false, 0);
			GetApp()->SetBGMusicIsPlaying(true);


//			GetApp()->SetBGMusicIsPlaying(false);

			/*
			if (!GetApp()->GetBGMusicMode())
			{
				LogMsg("Turning off BG music mode");
				if (GetApp()->GetBGMusicIsPlaying())
				{
					//stop this
					GetApp()->m_foobarManager.SetPause(true, 0);
					GetApp()->m_spotifyManager.SetPause(true, 0);
					GetApp()->SetBGMusicIsPlaying(false);
				}
				GetApp()->ToggleBGMusicMode();
			}
			else
			{
				LogMsg("Turning on BG music mode");
				GetApp()->ToggleBGMusicMode();
				//play bg music
				GetApp()->m_foobarManager.SetPause(false, 0);
				GetApp()->m_spotifyManager.SetPause(false, 0);
				GetApp()->SetBGMusicIsPlaying(true);

				//oh, if normal music was playing, kill that
				if (GetAudioManager()->IsPlayingMusic())
				{
					StopMusic(NULL);
				}

			}
			*/
		}

		//process something like "set_hue_light_rgb|delayMS|0|name|Hue play 1|rgb|255,0,0|allow_partial|1"
		if (words[0] == "set_hue_light_rgb")
		{
			int timeMS = StringAndVarToInt(words[2]);
			string lightName = GetApp()->m_varMan.ReplaceVars(words[4]);
			string rgb = GetApp()->m_varMan.ReplaceVars(words[6]);
			bool bAllowPartialName = StringToBool(words[8]);

			VariantList vList;
			vList.Get(0).Set(lightName);
			vList.Get(1).Set(rgb);
			//Set doesn't have bool, so for bAllowPartialName we'll convert it to an int
			vList.Get(2).Set((int32)bAllowPartialName);

			GetMessageManager()->CallStaticFunction(SetHueLightRGB, timeMS, &vList);
		}

		//process "set_foobar_pause|delayMS|0|paused|1|"
		if (words[0] == "set_foobar_pause")
		{
			//get the spotify active song and info
			int timeMS = StringAndVarToInt(words[2]);
			bool bPaused = StringToBool(words[4]);

			if (bPaused)
			{
				//we are pausing, so if bg music mode is on, turn it off
				if (GetApp()->GetBGMusicIsPlaying())
				{
					GetApp()->SetBGMusicIsPlaying(false);
					GetApp()->m_foobarManager.SetPause(bPaused, timeMS);
					GetApp()->m_spotifyManager.SetPause(bPaused, timeMS);
				}
			}
			else
			{

				if (!GetApp()->GetBGMusicMode())
				{
					//if bg music mode is off, don't unpause
					LogMsg("Not unpausing foobar because BG music mode is off");
					
				}
				else
				{
					//we are unpausing, so if bg music mode is off, turn it on
					if (!GetApp()->GetBGMusicIsPlaying())
					{
						GetApp()->SetBGMusicIsPlaying(true);

						GetApp()->m_foobarManager.SetPause(bPaused, timeMS);
						GetApp()->m_spotifyManager.SetPause(bPaused, timeMS);

					}
				}

				
			}
			
			
		}



		if (words[0] == "run_script")
		{
			string finalScriptName = GetApp()->m_varMan.ReplaceVars(words[2]);
			LogMsg("Launching external script %s (%s)", words[2].c_str(), finalScriptName.c_str());
			LaunchScript(finalScriptName);
		}

		if (words[0] == "queue_script")
		{
			int delayMS = StringAndVarToInt(words[2]);

			string finalScriptName = GetApp()->m_varMan.ReplaceVars(words[4]);

			LogMsg("Launching external script %s (%s)", words[2].c_str(), finalScriptName.c_str());
			
			VariantList vList;
			vList.Get(0).Set(finalScriptName);

			GetMessageManager()->CallStaticFunction(LaunchScriptVariant, delayMS, &vList);
		}

		//process "show_image|delayMS|0|filename|no.png|durationMS|1000|x|100|y|100|scale|3.0|smoothing|0|"
		if (words[0] == "show_image") //or you can use add_image, which doesn't do any fancy transitions
		{
			int timeMS = StringAndVarToInt(words[2]);
			string filename = GetApp()->m_varMan.ReplaceVars(words[4]);
			uint32 durationMS = StringAndVarToInt(words[6]);
			int32 x = StringAndVarToInt(words[8]);
			int32 y = StringAndVarToInt(words[10]);
			float scale = StringAndVarToFloat(words[12]);
			int32 smoothing = (int32)StringToBool(words[14]);
			string transition = words[16];

			VariantList vList;
			vList.Get(0).Set(filename);
			vList.Get(1).Set(durationMS);
			vList.Get(2).Set(x);
			vList.Get(3).Set(y);
			vList.Get(4).Set(scale);
			vList.Get(5).Set(smoothing);
			vList.Get(6).Set(transition);

			GetMessageManager()->CallStaticFunction(ShowImage, timeMS, &vList);
		}

		//process "move_to_top|name|topbar|"
		if (words[0] == "move_to_top")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (pEnt)
			{
				pEnt->GetParent()->MoveEntityToTopByAddress(pEnt);
			}
			else
			{
				LogMsg("Can't find %s (move_to_top)", name.c_str());
			}
		}
	
		//process "set_var|stream1_source|rtsp://192.168.68.137/0|"
		if (words[0] == "set_var")
		{
			string varName = GetApp()->m_varMan.ReplaceVars(words[2]);
			string varValue = GetApp()->m_varMan.ReplaceVars(words[4]);
			GetApp()->m_varMan.SetVar(varName, varValue);
		}

		//process "add_stream|name|stream1|source|%stream1_source%|cache_ms|333|"
		if (words[0] == "add_stream")
		{
			//add the stream
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			string source = GetApp()->m_varMan.ReplaceVars(words[4]);
			int cacheMS = StringAndVarToInt(words[6]);
			Entity* pGUIEnt = GetBaseApp()->GetEntityRoot()->GetEntityByName("GUI");

			//For local audio files, derive a friendly title from the filename so the
			//widget shows e.g. "Bad Girls Club" instead of being unlabeled.  Skip URLs
			//(rtsp://, http://, webcam:) so the existing video-stream callers don't
			//start showing ugly URL fragments as labels - those scripts already manage
			//their own titles via add_text.
			string title = "";
			string ext = ToLowerCaseString(GetFileExtension(source));
			bool bIsLocalFile = source.find("://") == string::npos && source.find("webcam:") != 0;
			if (bIsLocalFile && (ext == "mp3" || ext == "wav" || ext == "ogg" || ext == "mid"))
			{
				title = GetFileNameWithoutExtension(source);
			}

			AddNewStream(name, source, cacheMS, pGUIEnt, true, title);
		}

		//process set_stream_vol|name|stream1|value (0 to 1)|%stream_vol%|
		if (words[0] == "set_stream_vol")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			float vol = StringAndVarToFloat(words[4]);
			SetStreamVolumeByName(name, vol);
		}

		//process "set_stream_pause|delayMS|0|name|drama|paused|1|"
		//Pauses (or unpauses) a libVLC stream entity created by add_stream (or by
		//slide auto-creation).  delayMS is useful at startup: libVLC's SetPause is a
		//no-op if the player isn't yet in the "playing" state, so pair add_stream
		//with a few-hundred-ms delayed pause to make the widget appear paused.
		if (words[0] == "set_stream_pause")
		{
			int delayMS = StringAndVarToInt(words[2]);
			string name = GetApp()->m_varMan.ReplaceVars(words[4]);
			bool bPaused = StringToBool(words[6]);

			VariantList vList;
			vList.Get(0).Set(name);
			vList.Get(1).Set(uint32(bPaused ? 1 : 0));
			GetMessageManager()->CallStaticFunction(SetStreamPauseHelper, delayMS, &vList);
		}

		//process set_scale|delayMS|100|name|stream1|scale_x|1.5|scale_y|1.5|durationMS|1000|

		if (words[0] == "set_scale")
		{
			int delayMS = StringAndVarToInt(words[2]);
			string name = GetApp()->m_varMan.ReplaceVars(words[4]);
			float scaleX = StringAndVarToFloat(words[6]);
			float scaleY = StringAndVarToFloat(words[8]);
			int durationMS = StringAndVarToInt(words[10]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (pEnt)
			{
				//InterpolateComponent silently no-ops when duration_ms is 0, AND a direct snap
				//can be clobbered by other in-flight scale2d interpolators (e.g. the slide-in
				//transition). A 1ms interpolation wins the per-frame last-write-wins battle
				//and snaps to target on the very next frame.
				if (durationMS <= 0) durationMS = 1;
				ScaleEntity(pEnt, GetScale2DEntity(pEnt), CL_Vec2f(scaleX, scaleY), durationMS, delayMS);
			}
			else
			{
				LogMsg("Can't find %s (set_scale)", name.c_str());
			}
		}

		//process set_scale_by_target_size|delayMS|0|name|stream1|x|960|y|540|durationMS|1000|

		if (words[0] == "set_scale_by_target_size")
		{
			int delayMS = StringAndVarToInt(words[2]);
			string name = GetApp()->m_varMan.ReplaceVars(words[4]);
			int targetX = StringAndVarToInt(words[6]);
			int targetY = StringAndVarToInt(words[8]);
			int durationMS = StringAndVarToInt(words[10]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (pEnt)
			{
				CL_Vec2f vDestSize = CL_Vec2f((float)targetX, (float)targetY);
				CL_Vec2f vFinalScale = EntityGetScaleBySizeAndAspectMode(pEnt, vDestSize, ASPECT_HEIGHT_CONTROLS_WIDTH);
				//See set_scale comment: 0ms gets clamped to 1ms so it actually fires.
				if (durationMS <= 0) durationMS = 1;
				ScaleEntity(pEnt, GetScale2DEntity(pEnt), vFinalScale, durationMS, delayMS);
			}
			else
			{
				LogMsg("Can't find %s (set_scale_by_target_size)", name.c_str());
			}
		}

		//process set_pos|delay|0|name|stream1|x|52|y|467|durationMS|1000
		if (words[0] == "set_pos")
		{
			int delayMS = StringAndVarToInt(words[2]);
			string name = GetApp()->m_varMan.ReplaceVars(words[4]);
			int x = StringAndVarToInt(words[6]);
			int y = StringAndVarToInt(words[8]);
			int durationMS = StringAndVarToInt(words[10]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			
			if (pEnt)
			{

				if (words[6] == "unchanged")
				{
					x = (int)GetPos2DEntity(pEnt).x;
				}
				if (words[8] == "unchanged")
				{
					y = (int)GetPos2DEntity(pEnt).y;
				}

				//InterpolateComponent silently no-ops when duration_ms is 0, AND a direct snap
				//can be clobbered by other in-flight pos2d interpolators (e.g. the slide-in
				//transition still ticking on this entity). A 1ms interpolation wins the
				//per-frame last-write-wins battle and snaps to target on the very next frame.
				if (durationMS <= 0) durationMS = 1;
				ZoomToPositionEntity(pEnt, CL_Vec2f((float)x, (float)y), durationMS, INTERPOLATE_SMOOTHSTEP, delayMS);
			}
			else
			{
				LogMsg("Can't find %s (set_pos)", name.c_str());
			}
		}

		if (words[0] == "set_pos_offset")
		{
			int delayMS = StringAndVarToInt(words[2]);
			string name = GetApp()->m_varMan.ReplaceVars(words[4]);
			int x = StringAndVarToInt(words[6]);
			int y = StringAndVarToInt(words[8]);
			int durationMS = StringAndVarToInt(words[10]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (pEnt)
			{
				//See set_pos comment: 0ms gets clamped to 1ms so it actually fires and wins
				//against any other pos2d interpolators that may still be in flight.
				if (durationMS <= 0) durationMS = 1;
				ZoomToPositionOffsetEntity(pEnt, CL_Vec2f((float)x, (float)y), durationMS, INTERPOLATE_SMOOTHSTEP, delayMS);
			}
			else
			{
				LogMsg("Can't find %s (set_pos)", name.c_str());
			}
		}

		//process mod_var|name|cur_scene|operation|add|mod|1|special operation|wrap|special value|1| #add 1 to our variable, forcing it to wrap to 0 if we go over 1
		if (words[0] == "mod_var")
		{
			string varName = GetApp()->m_varMan.ReplaceVars(words[2]);
			string operation = GetApp()->m_varMan.ReplaceVars(words[4]);
			string mod = GetApp()->m_varMan.ReplaceVars(words[6]);
			string specialOperation = GetApp()->m_varMan.ReplaceVars(words[8]);
			string specialValue = GetApp()->m_varMan.ReplaceVars(words[10]);
			//find the current value
			int curVal = StringAndVarToInt(GetApp()->m_varMan.ReplaceVars("%"+varName+"%"));
			
			//do the operation
			if (operation == "add")
			{
				curVal += StringAndVarToInt(mod);
			}
			else if (operation == "subtract")
			{
				curVal -= StringAndVarToInt(mod);
			}
			else if (operation == "multiply")
			{
				curVal *= StringAndVarToInt(mod);
			}
			else if (operation == "divide")
			{
				curVal /= StringAndVarToInt(mod);
			}
			else
			{
				LogMsg("Unknown operation %s in mod_var", operation.c_str());
			}
			//do the special operation
			if (specialOperation == "wrap")
			{
				int specialVal = StringAndVarToInt(specialValue);
				if (curVal > specialVal)
				{
					curVal = 0;
				}
			}
			else if (specialOperation == "clamp")
			{
				int specialVal = StringAndVarToInt(specialValue);
				if (curVal > specialVal)
				{
					curVal = specialVal;
				}
			}
			else
			{
				//LogMsg("Unknown special operation %s in mod_var", specialOperation.c_str());
			}
			//set the new value
			GetApp()->m_varMan.SetVar(varName, toString(curVal));
			LogMsg("Variable %s is now %d", varName.c_str(), curVal);
		}

		if (words[0] == "mod_var_float")
		{
			string varName = GetApp()->m_varMan.ReplaceVars(words[2]);
			string operation = GetApp()->m_varMan.ReplaceVars(words[4]);
			string mod = GetApp()->m_varMan.ReplaceVars(words[6]);
			string specialOperation = GetApp()->m_varMan.ReplaceVars(words[8]);
			string specialValue = GetApp()->m_varMan.ReplaceVars(words[10]);
			//find the current value
			float curVal = StringAndVarToFloat(GetApp()->m_varMan.ReplaceVars("%" + varName + "%"));

			//do the operation
			if (operation == "add")
			{
				curVal += StringAndVarToFloat(mod);
			}
			else if (operation == "subtract")
			{
				curVal -= StringAndVarToFloat(mod);
			}
			else if (operation == "multiply")
			{
				curVal *= StringAndVarToFloat(mod);
			}
			else if (operation == "divide")
			{
				curVal /= StringAndVarToFloat(mod);
			}
			else
			{
				LogMsg("Unknown operation %s in mod_var", operation.c_str());
			}
			//do the special operation
			if (specialOperation == "wrap")
			{
				int specialVal = (int)StringAndVarToFloat(specialValue);
				if (curVal > specialVal)
				{
					curVal = 0;
				}
			}
			else if (specialOperation == "clamp")
			{
				int specialVal = (int)StringAndVarToFloat(specialValue);
				if (curVal > specialVal)
				{
					curVal = (float)specialVal;
				}
			}
			else
			{
				//LogMsg("Unknown special operation %s in mod_var", specialOperation.c_str());
			}
			//set the new value
			GetApp()->m_varMan.SetVar(varName, toString(curVal));
			LogMsg("Variable %s is now %d", varName.c_str(), curVal);
		}

		//process add_image|name|topbar|source|media/topbar.png|x|0|y|0|

		if (words[0] == "add_image") //a more fancy version is called show_image, which does bobbing, fading and delayed showing
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			string source = GetApp()->m_varMan.ReplaceVars(words[4]);
			int x = StringAndVarToInt(words[6]);
			int y = StringAndVarToInt(words[8]);
			Entity* pParent = GetBaseApp()->GetEntityRoot()->GetEntityByName("GUI");
			Entity* pEnt = CreateOverlayEntity(pParent, name, LocateFile(source), (float) x, (float) y);

			EntityComponent* pDragComp = pEnt->AddComponent(new TouchDragComponent);
			EntityComponent* pDragMoveComp = pEnt->AddComponent(new TouchDragMoveComponent);
			EntityComponent* pMouseWheelZoom = pEnt->AddComponent(new ScrollToZoomComponent);
		}

		//process set_border_pixels|name|topbar|border|0|0|0|0|
		if (words[0] == "set_border_pixels")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			int left = StringAndVarToInt(words[4]);
			int top = StringAndVarToInt(words[5]);
			int right = StringAndVarToInt(words[6]);
			int bottom = StringAndVarToInt(words[7]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (pEnt)
			{
				//get the OverlayRenderComponent if it exists
				OverlayRenderComponent* pOverlayComp = (OverlayRenderComponent*)pEnt->GetComponentByName("OverlayRender");
				if (pOverlayComp)
				{
					pOverlayComp->GetVar("borderPaddingPixels")->Set(CL_Rectf((float)left, (float)top, (float)right, (float)bottom));
				}
				else
				{
					LogMsg("%s doesn't have an OverlayRenderComponent (set_border_pixels)", name.c_str());
				}
			}
			else
			{
				LogMsg("Can't find %s (set_border_pixels)", name.c_str());
			}
		}
		
		
		//process log|log message|
		if (words[0] == "log")
		{
			string msg = GetApp()->m_varMan.ReplaceVars(words[1]);
			LogMsg(msg.c_str());
		}

		//process add_text|name|text|msg|Hello, this is text!|x|500|y|200|alignment|upper_left|
		if (words[0] == "add_text")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			string msg = GetApp()->m_varMan.ReplaceVars(words[4]);
			int x = StringAndVarToInt(words[6]);
			int y = StringAndVarToInt(words[8]);
			string alignment = GetApp()->m_varMan.ReplaceVars(words[10]);
			Entity* pParent = GetBaseApp()->GetEntityRoot()->GetEntityByName("GUI");
			
			Entity* pEnt = CreateTextLabelEntity(pParent, name, (float)x, (float)y, msg);
			
			//get pointer directly to the text component
			TextRenderComponent* pTextComp = (TextRenderComponent*)pEnt->GetComponentByName("TextRender");
			pTextComp->GetVar("backgroundColor")->Set(MAKE_RGBA(0, 0, 0, 150));
			
			SetupTextEntity(pEnt, FONT_LARGE, 1.0f);
			SetAlignmentEntity(pEnt, GetAlignmentFromString(alignment));
		
			EntityComponent* pDragComp = pEnt->AddComponent(new TouchDragComponent);
			EntityComponent* pDragMoveComp = pEnt->AddComponent(new TouchDragMoveComponent);
			EntityComponent* pMouseWheelZoom = pEnt->AddComponent(new ScrollToZoomComponent);

			//add a rect background
			//Entity* pRect = CreateOverlayRectEntity(pParent, CL_Vec2f(0, 0), CL_Vec2f(30, 10), MAKE_RGBA(0, 0, 0, 255));
			//pRect->SetName(name + "highlight");

		}

		//process set_alignment|name|topbar|alignment|center|
		if (words[0] == "set_alignment")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			string alignment = GetApp()->m_varMan.ReplaceVars(words[4]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (pEnt)
			{
				SetAlignmentEntity(pEnt, GetAlignmentFromString(alignment));
			}
			else
			{
				LogMsg("Can't find %s (set_alignment)", name.c_str());
			}
		}
		
		//process attach|name|text|target|stream1|
		if (words[0] == "attach")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			string target = GetApp()->m_varMan.ReplaceVars(words[4]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			Entity* pTarget = GetEntityRoot()->GetEntityByName(target);
			if (pEnt && pTarget)
			{
				pEnt->GetParent()->RemoveEntityByAddress(pEnt, false);
				pTarget->AddEntity(pEnt);
			}
			else
			{
				LogMsg("Can't find %s or %s (attach)", name.c_str(), target.c_str());
			}
		}


		//process kill|name|single_screen|delayMS|0|fadeMS|0|
		if (words[0] == "kill_image" || words[0] == "kill")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			int delayMS = StringAndVarToInt(words[4]);
			int fadeMS = StringAndVarToInt(words[6]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (pEnt)
			{
				if (fadeMS > 0)
				{
					FadeOutAndKillEntity(pEnt, true, fadeMS, delayMS);
				}
				else
				{
					KillEntity(pEnt, delayMS);
				}
			}
			else
			{
				LogMsg("Can't find %s (kill_image)", name.c_str());
			}
		}

		//process keep|name|slide0|new_name|drama|
		//Renames an entity (and its companion MarkupEntity) so the slide auto-cleanup
		//can't find it by the old "slideN" name and it survives slide changes.
		//To remove it later, use kill|name|<new_name>|delayMS|0|fadeMS|0|
		if (words[0] == "keep")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			string newName = words.size() > 4 ? GetApp()->m_varMan.ReplaceVars(words[4]) : "";
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (!pEnt)
			{
				LogMsg("Can't find %s (keep)", name.c_str());
			}
			else if (newName.empty())
			{
				LogMsg("keep: no new_name supplied for %s, nothing renamed", name.c_str());
			}
			else
			{
				//also rename the companion MarkupEntity sibling, if present
				if (pEnt->GetParent())
				{
					Entity* pMarkup = pEnt->GetParent()->GetEntityByName("MarkupEntity" + name);
					if (pMarkup)
					{
						pMarkup->SetName("MarkupEntity" + newName);
					}
				}
				pEnt->SetName(newName);
				LogMsg("keep: renamed %s -> %s (will not auto-delete on slide change)", name.c_str(), newName.c_str());
			}
		}

		//process set_controls_locked|name|slide0|locked|1|
		//For audio/video stream entities, stops the play/volume controls from
		//fading out after 2 seconds of no input. locked=0 reverts to default fade behavior.
		if (words[0] == "set_controls_locked")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			bool bLocked = StringToBool(words[4]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (pEnt)
			{
				LibVlcStreamComponent* pVlc = (LibVlcStreamComponent*)pEnt->GetComponentByName("LibVlcStream");
				if (pVlc)
				{
					pVlc->SetControlsAlwaysVisible(bLocked);
				}
				else
				{
					LogMsg("%s has no LibVlcStream component (set_controls_locked)", name.c_str());
				}
			}
			else
			{
				LogMsg("Can't find %s (set_controls_locked)", name.c_str());
			}
		}

		//process set_restart_on_play|name|slide0|enabled|1|
		//For audio/video stream entities, makes the play button (and script-driven
		//set_pause-style toggles) seek back to position 0 every time playback resumes
		//from a paused state, instead of continuing where it was paused.
		//enabled=0 reverts to default continue-from-pause behavior.
		if (words[0] == "set_restart_on_play")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			bool bEnabled = StringToBool(words[4]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (pEnt)
			{
				LibVlcStreamComponent* pVlc = (LibVlcStreamComponent*)pEnt->GetComponentByName("LibVlcStream");
				if (pVlc)
				{
					pVlc->SetRestartOnPlay(bEnabled);
				}
				else
				{
					LogMsg("%s has no LibVlcStream component (set_restart_on_play)", name.c_str());
				}
			}
			else
			{
				LogMsg("Can't find %s (set_restart_on_play)", name.c_str());
			}
		}

		//process keep_on_top|name|drama|enabled|1|
		//Hooks the entity's per-frame OnUpdate signal; each frame it re-inserts itself
		//at the end of its parent's child list so it always draws on top of siblings.
		//Handler disconnects automatically when the entity dies (sig_onRemoved).
		//enabled|0 detaches the hook (entity returns to normal z-order behavior).
		if (words[0] == "keep_on_top")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			bool bEnabled = StringToBool(words[4]);
			Entity* pEnt = GetEntityRoot()->GetEntityByName(name);
			if (!pEnt)
			{
				LogMsg("Can't find %s (keep_on_top)", name.c_str());
			}
			else
			{
				auto it = g_keepOnTopConnections.find(pEnt);
				if (it != g_keepOnTopConnections.end())
				{
					it->second.disconnect();
					g_keepOnTopConnections.erase(it);
				}
				if (bEnabled)
				{
					boost::signals2::connection conn = pEnt->GetFunction("OnUpdate")->sig_function.connect(
						99, boost::bind(&KeepOnTopTick, pEnt, _1));
					g_keepOnTopConnections[pEnt] = conn;
					pEnt->sig_onRemoved.connect(boost::bind(&KeepOnTopOnRemoved, _1));
					LogMsg("keep_on_top: pinned %s to top of its parent each frame", name.c_str());
				}
				else
				{
					LogMsg("keep_on_top: released %s from per-frame top pinning", name.c_str());
				}
			}
		}
	}
}

void Script::SetParms(string parm1)
{
	m_parm1 = parm1;
}

void LaunchScriptVariant(VariantList* pVList)
{
	//Get(1) optionally carries an entity name to restore into the %active% var
	//just-in-time before the script runs. This protects against fast nav
	//overwriting %active% between when the script was queued and when it actually fires.
	string activeOverride = pVList->Get(1).GetString();
	if (!activeOverride.empty())
	{
		GetApp()->m_varMan.SetVar("active", activeOverride);
	}

	LaunchScript(pVList->Get(0).GetString());
}

void LaunchScript(string command, string parm1)
{

	//for safetly, let's not allow any ../ in the command
	if (command.find("../") != string::npos)
	{
		LogMsg("Error: LaunchScript: command contains ../");
		return;
	}

	if (command.find("..\\") != string::npos)
	{
		LogMsg("Error: LaunchScript: command contains ..\\");
		return;

	}


	string path = "";
	string extension = ".txt";

	if (FileExists(GetApp()->m_slideDir + "/" + command + extension))
	{
		//found it
		path = GetApp()->m_slideDir + "/";
	}

	LogMsg("Running script %s%s%s", path.c_str(), command.c_str(), extension.c_str());

#ifdef _DEBUG
	ShowTextMessageSimple("Running script " + command+" "+parm1, 1000);
#endif

	Script script;
	script.SetParms(parm1);
	if (script.Load(path + command + ".txt"))
	{
		script.Run();
	}
}
