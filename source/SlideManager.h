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

class SlideManager
{
public:
	SlideManager();
	virtual ~SlideManager();

	bool IsThingWeCanShow(string fName);
	bool IsThingThatPlaysAudioOnly(string fName);

	bool Init(Entity* pParent, string dirName);
	Entity * ShowSlide(int slideNum, bool bDoTransitions = true);

	void NextSlide();
	void ModScale(float mod, bool bApplyMovementToo);
	void ModPos(CL_Vec2f vMod);
	void PlaySlideSFX();
	void PreviousSlide();
	Entity* CreateMediaFromFileName(string fileName, string entName, CL_Vec2f vPos, bool bAddBasePath);

private:

	bool IsImageFile(string fileExtension);
	bool IsMediaFile(string fileExtension);
	bool IsAudioFile(string fileExtension);
	void GetRidOfActiveSlide(bool bBackwards);

	Entity *m_pParentEnt;
	vector<string> m_files;
	string m_slideDir;
	int m_activeSlide;
	unsigned int m_slideTimer;
	int m_timeBetweenSlidesMS = 0;
	const int m_slideSpeedMS = 200;

};

#endif // SlideManager_h__