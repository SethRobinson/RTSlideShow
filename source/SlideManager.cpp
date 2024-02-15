#include "PlatformPrecomp.h"
#include "SlideManager.h"
#include "App.h"
#include "Entity/EntityUtils.h"
#include "GUI/IntroMenu.h"
#include "Entity/TouchDragComponent.h"
#include "Entity/TouchDragMoveComponent.h"
#include "Entity/ScrollToZoomComponent.h"
#include "Entity/LibVlcStreamComponent.h"
#include "Entity/TouchDragMarkupComponent.h"
#include "GUI/IntroMenu.h"

bool is_not_digit(char c)
{
	return !isdigit(c);
}

bool numeric_string_compare(const std::string& s1, const std::string& s2)
{
	// handle empty strings...

	return StringToFloat(s1) < StringToFloat(s2);

}

SlideManager::SlideManager()
{
	m_pParentEnt = NULL;
	m_slideDir = "slides/";
	m_activeSlide = -1; //none
	m_slideTimer = 0;
}

SlideManager::~SlideManager()
{
}

bool SlideManager::IsImageFile(string fileExtension)
{
	if (fileExtension == "png") return true;
	if (fileExtension == "jpg") return true;
	return false;
}

bool SlideManager::IsMediaFile(string fileExtension)
{
	if (fileExtension == "mpg") return true;
	if (fileExtension == "mov") return true;
	if (fileExtension == "avi") return true;
	if (fileExtension == "mkv") return true;
	if (fileExtension == "mkv") return true;
	if (fileExtension == "flv") return true;
	if (fileExtension == "wav") return true;
	if (fileExtension == "mp3") return true;
	if (fileExtension == "mp4") return true;
	if (fileExtension == "wmv") return true;
	if (fileExtension == "ogg") return true;
	
	return false;
}

bool SlideManager::IsThingWeCanShow(string fName)
{
	string lowerCase = ToLowerCaseString(fName);
	
	string fileExtension = GetFileExtension(lowerCase);

	if (IsImageFile( fileExtension )) return true;

	if (IsMediaFile(fileExtension)) return true;
	
	return false;
}

bool SlideManager::Init(Entity *pParent, string dirName)
{
	m_pParentEnt = pParent;

	m_slideDir = dirName+"/";
	//first, let's grab a list of the files
	m_files.clear();
	m_files = GetFilesAtPath(m_slideDir);
	
	//remove non-images
	for (int i=0; i < (int)m_files.size(); i++)
	{
		//LogMsg("Slide %d - %s", i, files[i].c_str());
		if (!IsThingWeCanShow(m_files[i]))
		{
			//remove it
			m_files.erase(m_files.begin()+i);
			i = -1;
			//wastefully start again
			continue;
		}
	}

	std::sort(m_files.begin(), m_files.end(), numeric_string_compare);

	LogMsg("Slides in order: ");
	for (int i=0; i < (int)m_files.size(); i++)
	{
		LogMsg("Slide %d - %s", i, m_files[i].c_str());
	}
	return true;
}

void SlideManager::GetRidOfActiveSlide(bool bBackwards)
{
	if (m_activeSlide == -1) return; //nothing to get rid of

	//detect if either Shift key is currently being held down
	bool bShiftDown = GetAsyncKeyState(VK_LSHIFT) || GetAsyncKeyState(VK_RSHIFT);
	if (bShiftDown) return;

	Entity *pSlide = m_pParentEnt->GetEntityByName("slide"+toString(m_activeSlide));

	if (pSlide)
	{
		if (bBackwards)	
		{
			SlideScreen(pSlide, false, m_slideSpeedMS);
		} else
		{
			SlideScreenBackwards(pSlide, false, m_slideSpeedMS);
		}

		KillEntity(pSlide, m_slideSpeedMS+500);
		pSlide->SetName("TaggedForDeletion");
	}
	
	//the markup slide will automatically die as well, it knows because it listens for the dead of entity it's following
}

void OnNextSlide(VariantList* pVList);

void SetupProgressBar(Entity* pParent, int slideTimeMS)
{
	//make an entity that will be a smoothly moving color rect progress bar on the bottom of the screen

	int barHeight = 40;
	auto *pEnt = CreateOverlayRectEntity(pParent, CL_Rectf(0, (float) ( GetScreenSizeY()-barHeight), (float)GetScreenSizeX(), (float)GetScreenSizeY()), MAKE_RGBA(80, 80, 255, 160), RectRenderComponent::STYLE_NORMAL);

	MorphToVec2Entity(pEnt, "size2d", CL_Vec2f(0, (float)barHeight), GetApp()->m_autoSlideTimeBetweenMS, INTERPOLATE_LINEAR, 0);
	
	//create and connect a function inside the entity called "NextSlide" that links to our existing function that advances to the next slide
	pEnt->GetFunction("NextSlide")->sig_function.connect(&OnNextSlide);

	//have the entity call its function when the progress bar hits 0, doing it this way so if the entity is killed, it won't be called
	GetMessageManager()->CallEntityFunction(pEnt, slideTimeMS, "NextSlide");
}

void OnInputToSlide(VariantList* pVList)
{
	LogMsg("Got char: %d (%d)", (char)pVList->Get(2).GetUINT32(), pVList->Get(3).GetUINT32());

	Entity* pCallingEnt = pVList->Get(5).GetEntity();

	//if we get any input, advance to the next slide
	if (pVList->Get(2).GetUINT32() == 46) //del key
	{
		pCallingEnt->SetTaggedForDeletion();
	}

	if (pVList->Get(2).GetUINT32() == 'c') //let's clear markup of the image being dragged
	{
		//LogMsg("Clearing markup of %s", pCallingEnt->GetName().c_str());
		EntityComponent *pMarkupComp = pCallingEnt->GetVar("MarkupComponentFollowingUs")->GetComponent();
		if (pMarkupComp)
		{
			pMarkupComp->GetParent()->GetFunction("ClearActiveMarkups")->sig_function(NULL);
		}
	}


	if (pVList->Get(2).GetUINT32() == 's') //s for stamp
	{
		string stampName = pCallingEnt->GetName();
		LogMsg("Stamping %s", stampName.c_str());
		
		if (stampName.find("slide") == string::npos)
		{
			LogMsg("Can't stamp, not a slide");
			return;
		}
		StringReplace("_stamped", "", stampName);

		StringReplace("slide", "", stampName);
		//create a slide with the # we can get
		Entity *pSlideEnt = g_slideManager.ShowSlide(atoi(stampName.c_str()), false);

		pSlideEnt->SetName( pSlideEnt->GetName() + "_stamped" );
		if (pSlideEnt)
		{
			CL_Vec2f vScale = GetScale2DEntity(pCallingEnt);

			SetPos2DEntity(pSlideEnt, GetPos2DEntity(pCallingEnt));
			//SetScale2DEntity(pSlideEnt, vScale);
			pSlideEnt->GetParent()->MoveEntityToTopByAddress(pCallingEnt);
			ZoomToScaleEntity(pSlideEnt, vScale, 100,INTERPOLATE_SMOOTHSTEP, 1);
		
			//get access to the LibVlcStreamComponent if it exists
			LibVlcStreamComponent* pLibVlcStreamComp = (LibVlcStreamComponent*)pSlideEnt->GetComponentByName("LibVlcStream");
			if (pLibVlcStreamComp)
			{
				//set looping to be the same as it's set in the original
				LibVlcStreamComponent* pLibVlcStreamComp2 = (LibVlcStreamComponent*)pCallingEnt->GetComponentByName("LibVlcStream");
				if (pLibVlcStreamComp2)
				{
					pLibVlcStreamComp->GetVar("looping")->Set(pLibVlcStreamComp2->GetVar("looping")->GetUINT32());

					//also set the volume to be the same
					pLibVlcStreamComp->SetVolume(pLibVlcStreamComp2->GetVolume());
					pLibVlcStreamComp->SetMute(pLibVlcStreamComp2->GetMute());

					//also set the playback position to be the same
					//pLibVlcStreamComp->GetLibVlcRTSP()->SetPlaybackPosition(pLibVlcStreamComp2->GetLibVlcRTSP()->GetPlaybackPosition());
					//pLibVlcStreamComp->GetLibVlcRTSP()->SetPause(pLibVlcStreamComp2->GetLibVlcRTSP()->GetPause());

					//Call it virtually in 10 ms with MessageManager
					VariantList vList(uint32(pLibVlcStreamComp2->GetLibVlcRTSP()->GetPause()));
					GetMessageManager()->CallComponentFunction(pLibVlcStreamComp, 50, "SetPause", &vList);
	
					VariantList vList2(pLibVlcStreamComp2->GetLibVlcRTSP()->GetPlaybackPosition());
					GetMessageManager()->CallComponentFunction(pLibVlcStreamComp, 50, "SetPlaybackPosition", &vList2);

				}
			}
		}
	}


}

Entity* SlideManager::ShowSlide(int slideNum, bool bDoTransitions)
{
	bool bBackwards = false;

	if (bDoTransitions)
	{
	
		if (m_activeSlide > slideNum) bBackwards = true;

		if (slideNum >= (int)m_files.size() || slideNum < 0)
		{
			LogMsg("Can't show slide %d, it doesn't exist");
			return NULL;
		}
	}
	
	Entity *pEnt;
	string mediaFileExtension = ToLowerCaseString(GetFileExtension(m_files[slideNum]));
	string entName = "slide" + toString(slideNum);

	if (IsImageFile(mediaFileExtension))
	{
		pEnt = CreateOverlayEntity(m_pParentEnt, entName, m_slideDir + m_files[slideNum], (float)GetScreenSizeX() / 2, (float)GetScreenSizeY() / 2);
		SetAlignmentEntity(pEnt, ALIGNMENT_CENTER);
		auto vSize = GetSize2DEntity(pEnt);
		EntitySetScaleBySize(pEnt, GetScreenSize(), true, vSize.x < vSize.y);
		SetTouchPaddingEntity(pEnt, CL_Rectf(0, 0, 0, 0));
		EntityComponent* pDragComp = pEnt->AddComponent(new TouchDragComponent);
		pDragComp->GetVar("limitedToThisFingerID")->Set(uint32(0)); //only allow left mouse button
		EntityComponent* pDragMoveComp = pEnt->AddComponent(new TouchDragMoveComponent);
		EntityComponent* pMouseWheelZoom = pEnt->AddComponent(new ScrollToZoomComponent);
		
		//EntitySetScaleBySize(pSubEnt, GetScreenSize(), true, vSize.x < vSize.y);
	}
	else
	{
		//well, it must be a movie then
		pEnt = AddNewStream(entName, m_slideDir + m_files[slideNum], 333, m_pParentEnt, bDoTransitions)->GetParent();
		pEnt->SetName(entName);
		SetAlignmentEntity(pEnt, ALIGNMENT_CENTER);
		SetPos2DEntity(pEnt, CL_Vec2f((float)GetScreenSizeX() / 2, (float)GetScreenSizeY() / 2));

		//turn video looping on by default.  First, get a handle to the LibVlcStreamComponent
		LibVlcStreamComponent* pLibVlcStreamComp = (LibVlcStreamComponent*)pEnt->GetComponentByName("LibVlcStream");
		pLibVlcStreamComp->GetVar("looping")->Set(uint32(1));
	}


	Entity* pSubEnt = m_pParentEnt->AddEntity(new Entity("MarkupEntity" + toString(slideNum)));

	EntityComponent* pDragComp2 = pSubEnt->AddComponent(new TouchDragComponent);
	pDragComp2->GetVar("limitedToThisFingerID")->Set(uint32(1)); //only allow right mouse button

	SetAlignmentEntity(pSubEnt, GetAlignmentEntity(pEnt));
	SetPos2DEntity(pSubEnt, GetSize2DEntity(pEnt));
	SetScale2DEntity(pSubEnt, CL_Vec2f(1, 1));
	SetTouchPaddingEntity(pSubEnt, CL_Rectf(0, 0, 0, 0));
	EntityComponent* pDragMarkupComp2 = pSubEnt->AddComponent(new TouchDragMarkupComponent);
	pDragMarkupComp2->GetVar("entityToFollow")->Set(pEnt);

	//listen in for special key commands while window is being moved around
	ScrollToZoomComponent *pScrollZoomComp = (ScrollToZoomComponent*)pEnt->GetComponentByName("ScrollToZoom");
	pScrollZoomComp->m_sig_input_while_mousedown.connect( &OnInputToSlide);

	//oh, do we want it to show the coordinates on the TouchDragMoveComponent and ScrollToZoomComponent?  We'll check and change the "showCoords" var on each component now
	
	EntityComponent* pTouchDragMoveOnBaseEnt = pEnt->GetComponentByName("TouchDragMove");

	pScrollZoomComp->GetVar("showCoords")->Set(uint32(GetApp()->m_showCoords));
	pTouchDragMoveOnBaseEnt->GetVar("showCoords")->Set(uint32(GetApp()->m_showCoords));

	
#ifdef _DEBUG
	m_pParentEnt->PrintTreeAsText();
#endif

	//pEnt->AddComponent(new RenderScissorComponent); //probably need this later to fix letter boxing etc

	if (bDoTransitions)
	{
		m_activeSlide = slideNum;
		
		if (bBackwards)
		{
			SlideScreen(pEnt, true, m_slideSpeedMS);
		}
		else
		{
			SlideScreenBackwards(pEnt, true, m_slideSpeedMS);
		}

		if (GetApp()->m_autoSlideTimeBetweenMS != 0)
		{
			SetupProgressBar(pEnt, GetApp()->m_autoSlideTimeBetweenMS);
		}
		m_slideTimer = GetTick() + m_timeBetweenSlidesMS;
	}
	

	return pEnt;
}

void SlideManager::NextSlide()
{
	if (m_slideTimer > GetTick()) return; //not ready yet

	PlaySlideSFX();
	
	GetRidOfActiveSlide(false);

	if (m_activeSlide >= (int)m_files.size()-1) m_activeSlide = -1; //wrap around
	ShowSlide(m_activeSlide+1);
}

void SlideManager::ModScale(float mod, bool bApplyMovementToo)
{
	Entity* pSlide = m_pParentEnt->GetEntityByName("slide" + toString(m_activeSlide));

	if (pSlide)
	{
		float scaleMod = 1.1f;
		if (mod < 0)
		{
			scaleMod = 0.9f;
		}
		
		CL_Vec2f scale = GetScale2DEntity(pSlide) * scaleMod;
		SetScale2DEntity(pSlide, scale);

		//Let's also move the image depending on where the cursor is, so we zoom in/out towards a specific point

		CL_Vec2f vCurPosRatio = g_lastMousePos / GetScreenSize(); //the ratio of cursor's position
		CL_Vec2f vNewPos;
		CL_Vec2f vCurPos = GetPos2DEntity(pSlide); //the position is the center of the image

		if (bApplyMovementToo)
		{
			if (mod > 0)
			{
				// Change vCurPos to the new position based on the ratio of the cursor position to the screen size
				CL_Vec2f vDiff = (vCurPosRatio - 0.5f) * 2.0f; //The difference between the cursor ratio and the image center
				// Calculate the change in position due to the scaling
				CL_Vec2f vPosChange = vDiff * (1.0f - scaleMod);
				vPosChange *= 1500;
				//LogMsg("Change: %.2f, %.2f", vPosChange.x, vPosChange.y);
				// Update the position of the image to account for the scale change
				vNewPos = vCurPos + vPosChange;
			}
			else
			{
				//actually, let's just try to center the image as we're zooming out
				vNewPos = LerpVector(GetScreenSize() / 2, vCurPos, 0.87f);
				//but let's lerp it in
			}

			SetPos2DEntity(pSlide, vNewPos);
		}
	}
}

void SlideManager::ModPos(CL_Vec2f vMod)
{
	Entity* pSlide = m_pParentEnt->GetEntityByName("slide" + toString(m_activeSlide));

	if (pSlide)
	{
		auto vPos = GetPos2DEntity(pSlide);
		vPos += vMod;
		SetPos2DEntity(pSlide, vPos);
	}
}

void SlideManager::PlaySlideSFX()
{
	if (GetApp()->m_slide_sfx_vol == 0) return;

	AudioHandle handle = GetAudioManager()->Play("audio/click.wav");
	GetAudioManager()->SetVol(handle, GetApp()->m_slide_sfx_vol);
}

void SlideManager::PreviousSlide()
{
	if (m_slideTimer > GetTick()) return; //not ready yet
	PlaySlideSFX();
	GetRidOfActiveSlide(true);

	if (m_activeSlide < 1)
	{
		m_activeSlide = (int)m_files.size(); //last slide
	}

	ShowSlide(m_activeSlide-1);
}

