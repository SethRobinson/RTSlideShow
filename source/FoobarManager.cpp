#include "PlatformPrecomp.h"
#include "FoobarManager.h"
#include "Entity/HTTPComponent.h"
#include "Entity/EntityUtils.h"
#include "util/cJSON.h"
#include "App.h"

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

void OnDownloadError(VariantList* pVList)
{
	NetHTTP::eError e = (NetHTTP::eError)pVList->m_variant[1].GetUINT32();

	string msg = "`4Unable to connect.  Try later. (" + toString(e) + ")";
	if (e == NetHTTP::ERROR_COMMUNICATION_TIMEOUT)
	{
		msg = "`4Connection timed out. Try Later.";
	}
	//ShowTextMessageSimple(msg, 0);
	LogMsg(msg.c_str());

	Entity* pEnt = pVList->m_variant[0].GetComponent()->GetParent();
	pEnt->SetTaggedForDeletion();
}


void OnDownloadHTTPFinish(VariantList* pVList)
{
	Entity* pMenu = pVList->m_variant[0].GetComponent()->GetParent();

	LogMsg(pVList->m_variant[1].GetString().c_str());
	Entity* pEnt = pVList->m_variant[0].GetComponent()->GetParent();
	pEnt->SetTaggedForDeletion();

}

void OnGotStatus(VariantList* pVList)
{
	Entity* pEntity = pVList->m_variant[0].GetComponent()->GetParent(); //us

	string jSONReply = pVList->m_variant[1].GetString().c_str();

	//Use cJSON.h to parse the JSON reply
	cJSON* pRoot = cJSON_Parse(jSONReply.c_str());
	if (!pRoot)
	{
		LogMsg("Error parsing JSON: %s", cJSON_GetErrorPtr());
		return;
	}

	//Get sub object "player"
	cJSON* pPlayer = cJSON_GetObjectItem(pRoot, "player");
	if (!pPlayer)
	{
		LogMsg("Error: No player found in JSON");
		return;
	}

	//Look for the key of "playState" and get the value
	cJSON* pPlayState = cJSON_GetObjectItem(pPlayer, "playbackState");
	if (!pPlayState)
	{
		LogMsg("Error: No playBackState found in JSON");
		return;
	}
	//Get the value of the playState
	string playState = pPlayState->valuestring;

	//LogMsg("State: %s ", playState.c_str());


	string action = pEntity->GetVar("action")->GetString();

	if (action == "unpause")
	{
		if (playState == "paused")
		{
			GetApp()->m_foobarManager.Play(0);
		}
	}

	Entity* pEnt = pVList->m_variant[0].GetComponent()->GetParent();
	pEnt->SetTaggedForDeletion();
}


FoobarManager::FoobarManager()
{

}

FoobarManager::~FoobarManager()
{
}

void FoobarManager::CheckIfAvailable()
{
#ifdef WIN32
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		LogMsg("FoobarManager: Could not create socket for availability check");
		m_bAvailable = false;
		return;
	}

	// Set a short timeout (2 seconds) so we don't block forever
	DWORD timeout = 2000;
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons((u_short)port);
	inet_pton(AF_INET, url.c_str(), &server.sin_addr);

	int result = connect(sock, (struct sockaddr*)&server, sizeof(server));
	closesocket(sock);

	if (result == 0)
	{
		LogMsg("FoobarManager: Foobar beefweb plugin detected on %s:%d", url.c_str(), port);
		m_bAvailable = true;
	}
	else
	{
		LogMsg("FoobarManager: Foobar not available on %s:%d - disabling Foobar control", url.c_str(), port);
		m_bAvailable = false;
	}
#else
	m_bAvailable = false;
#endif
}

void FoobarManager::Play(int delayMS)
{
	if (!m_bAvailable) return;

	Entity* pEntity = new Entity("FoobarCommand");

	Entity* pParent = GetEntityRoot()->GetEntityByName("GUI");
	pParent->AddEntity(pEntity);

	//add a HTTPComponent
	EntityComponent* pComp = pEntity->AddComponent(new HTTPComponent);

	VariantList v;
	((HTTPComponent*)pComp)->GetNetHTTP()->SetForcePost(true);

	v.Reset();

	v.m_variant[0].Set(url);
	v.m_variant[1].Set(port);
	v.m_variant[2].Set("api/player/play");
	v.m_variant[3].Set(uint32(NetHTTP::END_OF_DATA_SIGNAL_HTTP)); //need this for it to detect a disconnect instead of the weird RTsoft symbol
	GetMessageManager()->CallComponentFunction(pComp, delayMS, "Init", &v);
	pComp->GetFunction("OnError")->sig_function.connect(&OnDownloadError);
	pComp->GetFunction("OnFinish")->sig_function.connect(&OnDownloadHTTPFinish);
}

void FoobarManager::UpdateStatusAndDoSomething(int delayMS, string action)
{
	if (!m_bAvailable) return;

	Entity* pEntity = new Entity("FoobarStatusUpdate");
	
	Entity* pParent = GetEntityRoot()->GetEntityByName("GUI");
	pParent->AddEntity(pEntity);
	//add a HTTPComponent
	EntityComponent* pComp = pEntity->AddComponent(new HTTPComponent);
	VariantList v;
	
	v.Reset();

	v.m_variant[0].Set(url);
	v.m_variant[1].Set(port);
	v.m_variant[2].Set("api/player");
	v.m_variant[3].Set(uint32(NetHTTP::END_OF_DATA_SIGNAL_HTTP)); //need this for it to detect a disconnect instead of the weird RTsoft symbol
	GetMessageManager()->CallComponentFunction(pComp, delayMS, "Init", &v);
	pComp->GetFunction("OnError")->sig_function.connect(&OnDownloadError);
	pComp->GetFunction("OnFinish")->sig_function.connect(&OnGotStatus);

	//embed our action so we can grab it later.  The manager can't keep track of per-instance things, but this entity we created is unique to this call, so we put it there.
	pEntity->GetVar("action")->Set(action);
}

void FoobarManager::SetPause(bool bPause, int delayMS)
{
	if (!m_bAvailable) return;

	if (bPause == false)
	{
		//let's only do anything if we're actually paused to begin with
		UpdateStatusAndDoSomething(delayMS, "unpause"); //will get processed at this time
		return;
	}

	Entity* pEntity = new Entity("FoobarCommand");

	Entity* pParent = GetEntityRoot()->GetEntityByName("GUI");
	pParent->AddEntity(pEntity);
	
	//add a HTTPComponent
	EntityComponent* pComp = pEntity->AddComponent(new HTTPComponent);
	
	VariantList v;
	((HTTPComponent*)pComp)->GetNetHTTP()->SetForcePost(true);

	v.Reset();

	v.m_variant[0].Set(url);
	v.m_variant[1].Set(port);
	v.m_variant[2].Set("api/player/pause");
	v.m_variant[3].Set(uint32(NetHTTP::END_OF_DATA_SIGNAL_HTTP)); //need this for it to detect a disconnect instead of the weird RTsoft symbol
	GetMessageManager()->CallComponentFunction(pComp, delayMS, "Init", &v);
	pComp->GetFunction("OnError")->sig_function.connect(&OnDownloadError);
	pComp->GetFunction("OnFinish")->sig_function.connect(&OnDownloadHTTPFinish);

}
