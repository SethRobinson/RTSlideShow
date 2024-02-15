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

int StringAndVarToInt(string in)
{

	return atoi(GetApp()->m_varMan.ReplaceVars(in).c_str());
}

float StringAndVarToFloat(string in)
{
	return (float)atof(GetApp()->m_varMan.ReplaceVars(in).c_str());
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
			
			GetApp()->m_foobarManager.SetPause(bPaused, timeMS);
			GetApp()->m_spotifyManager.SetPause(bPaused, timeMS);
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

		//process "add_stream|name|stream1|source|%stream1_source%|"
		if (words[0] == "add_stream")
		{
			//add the stream
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			string source = GetApp()->m_varMan.ReplaceVars(words[4]);
			int cacheMS = StringAndVarToInt(words[6]);
			Entity* pGUIEnt = GetBaseApp()->GetEntityRoot()->GetEntityByName("GUI");
			AddNewStream(name, source, cacheMS, pGUIEnt);
		}

		//process set_stream_vol|name|stream1|value (0 to 1)|%stream_vol%|
		if (words[0] == "set_stream_vol")
		{
			string name = GetApp()->m_varMan.ReplaceVars(words[2]);
			float vol = StringAndVarToFloat(words[4]);
			SetStreamVolumeByName(name, vol);
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
				//SetScale2DEntity(pEnt, CL_Vec2f(scaleX, scaleY));
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
				// pEnt->RemoveComponentByName("ic_scale");
				CL_Vec2f vDestSize = CL_Vec2f((float)targetX, (float)targetY);
				CL_Vec2f vFinalScale = EntityGetScaleBySizeAndAspectMode(pEnt, vDestSize, ASPECT_HEIGHT_CONTROLS_WIDTH);
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
				//SetPos2DEntity(pEnt, CL_Vec2f(x, y));
				//MoveEntity(pEnt, GetPos2DEntity(pEnt), CL_Vec2f(x, y), durationMS, delayMS);
				//move the entity
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
	}
}

void LaunchScriptVariant(VariantList* pVList)
{
	LaunchScript(pVList->Get(0).GetString());
}

void LaunchScript(string command)
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
	ShowTextMessageSimple("Running script " + command, 1000);
#endif

	Script script;
	if (script.Load(path + command + ".txt"))
	{
		script.Run();
	}
}
