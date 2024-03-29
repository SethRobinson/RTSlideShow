#ifndef IntroMenu_h__
#define IntroMenu_h__

#include "App.h"
Entity * IntroMenuCreate(Entity *pParentEnt);
extern CL_Vec2f g_lastMousePos;

class SlideManager;
extern SlideManager g_slideManager;
#endif // IntroMenu_h__