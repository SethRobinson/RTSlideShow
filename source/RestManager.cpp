#include "PlatformPrecomp.h"
#include "RestManager.h"

RestManager::RestManager()
{
}

RestManager::~RestManager()
{
}

void RestManager::UpdateWaitingForConnection()
{
	uint32 AcceptSocket = SOCKET_ERROR; //don't change to SOCKET, will break linux compile

	AcceptSocket = (uint32)accept(m_listener.GetSocket(), NULL, NULL);

	if (AcceptSocket != SOCKET_ERROR)
	{
		
		//m_hostSocket.Write("\r\n\r\nCONNECTED.\r\n\r\n:");
		RestClient* pRestClient = new RestClient;
		//init it
		m_clients.push_back(pRestClient);
		pRestClient->Init(this, AcceptSocket, m_idCounter++); //let it know where we are, and what ID it is

		LogMsg("NEW CONNECTION: %d connected.", m_clients.size());
	}
}

bool RestManager::Init()
{
	LogMsg("RestManager created");

	if (!m_listener.InitHost(m_hostPort, 20))
	{
		return false;
	}
	return true; //success
}

void RestManager::UpdateClients()
{
	list<RestClient*>::iterator itor = m_clients.begin();

	RestClient* pClient;

	for (; itor != m_clients.end();)
	{
		pClient = *itor;
		if (!pClient->Update() || pClient->IsRequestingDisconnection())
		{
			//we're done here, delete it
			SAFE_DELETE(pClient);

			list<RestClient*>::iterator temp = itor++;
			m_clients.erase(temp);
			LogMsg("CLIENT DISCONNECTED: %d connected.", m_clients.size());
			continue;
		}

		itor++;
	}
}

void RestManager::Update()
{
	m_listener.Update();
	UpdateWaitingForConnection();
	UpdateClients();
	
}
