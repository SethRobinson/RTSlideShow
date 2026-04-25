//  ***************************************************************
//  SlideManager - Creation date: 01/23/2016
//  -------------------------------------------------------------
//  Robinson Technologies Copyright (C) 2016 - All Rights Reserved
//
//  ***************************************************************
//  Programmer(s):  Seth A. Robinson (seth@rtsoft.com)
//  ***************************************************************

#ifndef SlideManager_h__
#define SlideManager_h__

#include <boost/signals2.hpp>

class SlideManager
{
public:
	SlideManager();
	virtual ~SlideManager();

	bool IsThingWeCanShow(string fName);
	bool IsThingThatPlaysAudioOnly(string fName);

	bool Init(Entity* pParent, string dirName);
	Entity * ShowSlide(int slideNum, bool bDoTransitions = true);

	//Per-frame tick: drives the "showCoords" var on every TouchDragMove and ScrollToZoom
	//component anywhere in the entity tree based on App::IsDebugOverlayHotkeyHeld(), so
	//the move/scale debug readouts only appear while the modifier key is held - uniformly
	//across slide images, video/audio widgets, script-created text/stream widgets, etc.,
	//without us having to touch the proton SDK components.
	void Update();

	void NextSlide();
	void ModScale(float mod, bool bApplyMovementToo);
	void ModPos(CL_Vec2f vMod);
	void PlaySlideSFX();
	void PreviousSlide();
	Entity* CreateMediaFromFileName(string fileName, string entName, CL_Vec2f vPos, bool bAddBasePath);

	//Reposition all current slide/markup entities proportionally so they stay in the same
	//relative spot when the virtual screen size changes (e.g. user maximizes the window).
	void OnVirtualScreenResized(CL_Vec2f vOldSize, CL_Vec2f vNewSize);

	//Tie pEnt to the currently active slide. On the next slide change, pEnt
	//will slide off in the same direction as the slide and then be killed.
	//No-op if there's no active slide. Safe to call multiple times for the
	//same entity (later calls are ignored).
	void RegisterEntityForActiveSlideKill(Entity* pEnt);

private:

	bool IsImageFile(string fileExtension);
	bool IsMediaFile(string fileExtension);
	bool IsAudioFile(string fileExtension);
	void GetRidOfActiveSlide(bool bBackwards);
	void OnTiedEntityRemoved(Entity* pEnt);

	Entity *m_pParentEnt;
	vector<string> m_files;
	string m_slideDir;
	int m_activeSlide;
	unsigned int m_slideTimer;
	int m_timeBetweenSlidesMS = 0;
	const int m_slideSpeedMS = 200;

	//entity -> sig_onRemoved connection (so the registry self-prunes when the
	//entity dies for any other reason: user Delete, scripted kill, etc.)
	std::map<Entity*, boost::signals2::connection> m_tiedToActiveSlide;

};

#endif // SlideManager_h__