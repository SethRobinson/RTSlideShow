#include "PlatformPrecomp.h"
#include "IntroMenu.h"
#include "Entity/EntityUtils.h"
#include "Entity/CustomInputComponent.h"
#include "SlideManager.h"
#include "Entity/ArcadeInputComponent.h"
#include "Gamepad/GamepadManager.h"
#include "Script.h"
#include "Entity/LibVlcStreamComponent.h"
#include "Entity/TouchDragMarkupComponent.h"


CL_Vec2f g_lastMousePos;

CL_Vec2f g_lastPointerClickPos[C_MAX_TOUCHES_AT_ONCE];
CL_Vec2f g_lastPointerClickPosMovement[C_MAX_TOUCHES_AT_ONCE];

void IntroMenuOnSelect(VariantList *pVList) //0=vec2 point of click, 1=entity sent from
{
	Entity *pEntClicked = pVList->m_variant[1].GetEntity();
	Entity *pMenu = GetEntityRoot()->GetEntityByName("Intro");

	LogMsg("Clicked %s entity at %s", pEntClicked->GetName().c_str(),pVList->m_variant[1].Print().c_str());

	DisableAllButtonsEntity(pMenu);
	SlideScreen(pEntClicked->GetParent(), false);
	//kill this menu entirely, but we wait half a second while the transition is happening before doing it
	GetMessageManager()->CallEntityFunction(pEntClicked->GetParent(), 500, "OnDelete", NULL); 

	if (pEntClicked->GetName() == "Test")
	{
		//ControllerTestMenuCreate(pMenu->GetParent());
		return;
	}

	//GetEntityRoot()->PrintTreeAsText(); //useful for debugging
}


void OnNextSlide(VariantList *pVList)
{
	g_slideManager.NextSlide();
}

void OnPreviousSlide(VariantList *pVList)
{
	g_slideManager.PreviousSlide();
}

void OnArcadeInput(VariantList *pVList)
{

	int vKey = pVList->Get(0).GetUINT32();
	bool bIsDown = pVList->Get(1).GetUINT32() != 0;

	if (!bIsDown) return; //don't care

#ifdef _DEBUG
	LogMsg("GameMenuArcade: Key %d, down is %d", vKey, int(bIsDown));
#endif

	switch(vKey)
	{
	case VIRTUAL_KEY_DIR_LEFT:
		LogMsg("left");
		OnPreviousSlide(pVList);
		break;

	case VIRTUAL_KEY_DIR_RIGHT:
		LogMsg("right");
		OnNextSlide(pVList);
		break;
	}

}


void AppInput(VariantList* pVList)
{

	//0 = message type, 1 = parent coordinate offset, 2 is fingerID
	eMessageType msgType = eMessageType(int(pVList->Get(0).GetFloat()));
	CL_Vec2f pt = pVList->Get(1).GetVector2();
	//pt += GetAlignmentOffset(*m_pSize2d, eAlignment(*m_pAlignment));

	uint32 modifiers = pVList->Get(3).GetUINT32();

	bool bControlDown = (modifiers & VIRTUAL_KEY_MODIFIER_CONTROL) != 0;
	bool bAltDown = (modifiers & VIRTUAL_KEY_MODIFIER_ALT) != 0;
	bool bShiftDown = (modifiers & VIRTUAL_KEY_MODIFIER_SHIFT) != 0;


	uint32 fingerID = 0;
	if (msgType != MESSAGE_TYPE_GUI_CHAR && pVList->Get(2).GetType() == Variant::TYPE_UINT32)
	{
		fingerID = pVList->Get(2).GetUINT32();
	}

	CL_Vec2f vLastTouchPt = GetBaseApp()->GetTouch(fingerID)->GetLastPos();

	switch (msgType)
	{

	case MESSAGE_TYPE_GUI_CLICK_START:

		g_lastPointerClickPos[fingerID] = pt;
		g_lastPointerClickPosMovement[fingerID] = pt;
   	    //LogMsg("Touch start: X: %.2f Y: %.2f (Finger %d)", pt.x, pt.y, fingerID);
		break;
	case MESSAGE_TYPE_GUI_MOUSEWHEEL:
		//LogMsg("Mouse wheel: Offet: %.2f (Finger %d)", pt.x, fingerID);
		//g_slideManager.ModScale(pt.x, true);
		break;

	case MESSAGE_TYPE_GUI_CLICK_MOVE_RAW:
		//LogMsg("Touch raw move: X: %.2f YL %.2f (Finger %d)", pt.x, pt.y, fingerID);
		g_lastMousePos = pt;

		/*
		if (fingerID == 0 && GetBaseApp()->GetTouch(fingerID)->IsDown())
		{
			//move image
			CL_Vec2f vOffset = pt - g_lastPointerClickPosMovement[fingerID];
			g_slideManager.ModPos(vOffset);
			g_lastPointerClickPosMovement[fingerID] = pt;
		}
		*/

		break;

	case MESSAGE_TYPE_GUI_CLICK_END:
	//	LogMsg("Touch end: X: %.2f Y: %.2f (Finger %d)", pt.x, pt.y, fingerID);
	{
		float distanceMoved = (pt - g_lastPointerClickPos[fingerID]).length();
		//LogMsg("Touch ended, moved %f dist", distanceMoved);


		/*

		const float minDistNeededToCountAsMove = 20.0f;
		if (distanceMoved < minDistNeededToCountAsMove)
		{
			if (fingerID == 0) OnNextSlide(NULL);
			if (fingerID == 1) OnPreviousSlide(NULL);

			//LogMsg("Touch ended, moved %f dist", distanceMoved);
			//OnSelect(pVList);
		}
		*/
		
	}
		break;

	case MESSAGE_TYPE_GUI_CHAR:
		char key = (char)pVList->Get(2).GetUINT32();
	//	LogMsg("MESSAGE_TYPE_GUI_CHAR sent key %c (%d)", key, (int)key);
		
	
		if (key == '1')
		{
			if (bAltDown && bControlDown && bShiftDown)
			{
				LaunchScript("ID0_ButtonG32");
				break;
			}

			if (bAltDown && bControlDown)
			{
				LaunchScript("ID0_ButtonA_Long");
				break;
			}

			if (bControlDown)
			{
				LaunchScript("ID0_ButtonG26");
				break;
			}

			if (bAltDown)
			{
				LaunchScript("ID0_ButtonB");
				break;
			}
			
			if (bShiftDown)
			{ 
				LaunchScript("ID0_ButtonA_Double");
				break;
			}

			//default
			LaunchScript("ID0_ButtonA");
			
		}

		
		if (key == '2')
		{

			if (bAltDown && bControlDown && bShiftDown)
			{
				LaunchScript("ID1_ButtonG32");
				break;
			}

			if (bAltDown && bControlDown)
			{
				LaunchScript("ID1_ButtonA_Long");
				break;
			}

			if (bControlDown)
			{
				LaunchScript("ID1_ButtonG26");
				break;
			}

			if (bAltDown)
			{
				LaunchScript("ID1_ButtonB");
				break;
			}

			if (bShiftDown)
			{
				LaunchScript("ID1_ButtonA_Double");
				break;
			}

			//default
			LaunchScript("ID1_ButtonA");

		}

		
		if (key == 'C')
		{

			
				//clear all active markups
				LogMsg("Clearing active markups");
				GetEntityRoot()->CallFunctionRecursively("ClearActiveMarkups", NULL);
			

		}

		

		if (key == 'f')
		{
			//toggle fps display
			GetBaseApp()->SetFPSVisible(!GetBaseApp()->GetFPSVisible());
		}

		
		if (key == 'n')
		{
			//new stream
			AddNewStream("StreamEon", "rtsp://192.168.68.114/0", 333, GetEntityRoot()->GetEntityByName("GUI"));

		}

		const int maxPenSize = 100;
		const int minPenSize = 1;

		if (key == ']')
		{
			//new stream
			int penSize = GetGlobalMarkupPenSize();
			penSize += 5;
			penSize = ranges::clamp(penSize, minPenSize, maxPenSize);
			SetGlobalMarkupPenSize(penSize);
		}

		if (key == '[')
		{
			//new stream
			int penSize = GetGlobalMarkupPenSize();
			penSize -= 5;
			penSize = ranges::clamp(penSize, minPenSize, maxPenSize);
			SetGlobalMarkupPenSize(penSize);
		}

		break;
	}
}


Entity * IntroMenuCreate(Entity *pParentEnt)
{

#ifdef WINAPI
//Sleep(700);
#endif
	//Entity *pBG = CreateOverlayEntity(pParentEnt, "IntroMenu", "interface/title.rttex", 0,0);
	Entity *pBG = CreateOverlayRectEntity(pParentEnt, GetScreenRect(), MAKE_RGBA(0,0,0,255));
	pBG->SetName("Intro"); //so we can find this entiry again later

	/*
	//add buttons on top of this entity
	Entity *pEnt;

	pEnt = CreateOverlayButtonEntity(pBG, "Test", "interface/intro_button_controller_test.rttex", 80, 250);
	pEnt->GetShared()->GetFunction("OnButtonSelected")->sig_function.connect(&IntroMenuOnSelect);
	BobEntity(pEnt);
	ZoomToPositionFromThisOffsetEntity(pEnt, CL_Vec2f(-500, 200), 2000);

	pEnt = CreateOverlayButtonEntity(pBG, "Looney", "interface/intro_button_looney.rttex", 550, 250);
	pEnt->GetShared()->GetFunction("OnButtonSelected")->sig_function.connect(&IntroMenuOnSelect);
	BobEntity(pEnt);
	ZoomToPositionFromThisOffsetEntity(pEnt, CL_Vec2f(500, 200), 2000);

	pEnt = CreateTextLabelEntity(pBG, "Text", GetScreenSizeXf()/2, 30, "Choose your path!");
	SetupTextEntity(pEnt, FONT_LARGE, 2.0f);m
	For LORD/TEOS BBS stuff:
	http://www.rtsoft.com/pages/lordfaq.htm
	SetAlignmentEntity(pEnt, ALIGNMENT_UPPER_CENTER);

	*/

	
	//for android, so the back key (or escape on windows) will quit out of the game
	EntityComponent *pComp = pBG->AddComponent(new CustomInputComponent);
	pComp->GetFunction("OnActivated")->sig_function.connect(1, boost::bind(&App::OnExitApp, GetApp(), _1));
	//tell the component which key has to be hit for it to be activated
	pComp->GetVar("keycode")->Set(uint32(VIRTUAL_KEY_BACK));
		//arcade input component is a way to tie keys/etc to send signals through GetBaseApp()->m_sig_arcade_input
	{
	ArcadeInputComponent *pComp = (ArcadeInputComponent*) pBG->AddComponent(new ArcadeInputComponent);
	/*
	//connect to a gamepad/joystick too if available
	*/

	//Just connect to all pads at once.. ok to do for a single player game.. otherwise, we should use
	//Gamepad *pPad = GetGamepadManager()->GetDefaultGamepad(); instead probably, or let the user choose.
	//Keep in mind pads can be removed/added on the fly
	for (int i=0; i < GetGamepadManager()->GetGamepadCount(); i++)
	{
		Gamepad *pPad = GetGamepadManager()->GetGamepad( (eGamepadID) i);
		pPad->ConnectToArcadeComponent(pComp, true, true);

		//if we cared about the analog sticks too, we'd do this:
		//pPad->m_sig_left_stick.connect(1, boost::bind(&OnGamepadStickUpdate, this, _1));	
		//pPad->m_sig_right_stick.connect(1, boost::bind(&OnGamepadStickUpdate, this, _1));	
	}

		//map buttons

		AddKeyBinding(pComp, "Left", VIRTUAL_KEY_DIR_LEFT, VIRTUAL_KEY_DIR_LEFT);
		AddKeyBinding(pComp, "Right", VIRTUAL_KEY_DIR_RIGHT, VIRTUAL_KEY_DIR_RIGHT);
		AddKeyBinding(pComp, "Up", VIRTUAL_KEY_DIR_UP, VIRTUAL_KEY_DIR_UP);
		AddKeyBinding(pComp, "Down", VIRTUAL_KEY_DIR_DOWN, VIRTUAL_KEY_DIR_DOWN);
		AddKeyBinding(pComp, "Dynamite", VIRTUAL_KEY_CONTROL, VIRTUAL_KEY_GAME_FIRE);
	//	AddKeyBinding(pComp, "RTrigger", VIRTUAL_DPAD_RTRIGGER, VIRTUAL_KEY_DIR_RIGHT, true);
	//	AddKeyBinding(pComp, "LTrigger", VIRTUAL_DPAD_LTRIGGER, VIRTUAL_KEY_DIR_LEFT, true);
		AddKeyBinding(pComp, "ForwardButton", VIRTUAL_DPAD_BUTTON_DOWN, VIRTUAL_KEY_DIR_RIGHT);
		AddKeyBinding(pComp, "BackButton", VIRTUAL_DPAD_BUTTON_RIGHT, VIRTUAL_KEY_DIR_LEFT);
	}

	GetBaseApp()->m_sig_arcade_input.connect(pBG->GetFunction("OnArcadeInput")->sig_function);
	pBG->GetShared()->GetFunction("OnArcadeInput")->sig_function.connect(&OnArcadeInput);
	g_slideManager.Init(pBG, GetApp()->m_slideDir);
	g_slideManager.ShowSlide(0);
	
	GetBaseApp()->m_sig_input.connect(&AppInput);
	

	return pBG;
}
