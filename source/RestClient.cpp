#include "PlatformPrecomp.h"

#include "RestClient.h"
#include "RestManager.h"
#include "Script.h"
#include "App.h"

RestClient::RestClient()
{
	
}

RestClient::~RestClient()
{
	m_client.Kill();
}


bool RestClient::Init(RestManager* pRestManager, uint32  socket, int id)
{
	m_client.SetSocket((int)socket);
	m_id = id;
	m_pRestManager = pRestManager;

	return true; //success
}


void RestClient::ProcessCommand(string command)
{
	if (command == "")
	{
		LogMsg("COOL BEANZ");
		return;
	}
}



void RestClient::UpdateInput()
{
	vector<char>& s = m_client.GetBuffer();

	if (!s.empty())
	{
		//add a null at the end of the vector string
		s.push_back(0);
		//LogMsg("Got %d of data. %s", s.size(), s.data());
		m_bRequestDisconnection = true;

		//I now need to convert s into a normal string.
		string command = s.data();
		
		LaunchScript(command);
	}

}

bool RestClient::Update()
{
	m_client.Update();

	if (m_client.WasDisconnected())
	{
			return false;
	}

	UpdateInput();
	return true;
}

int RestClient::GetID()
{
	assert(m_id != 0);
	return m_id;
}
