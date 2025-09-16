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
		
		//Convert buffer to string
		string allCommands = s.data();
		
		//Split by newline to handle multiple commands
		vector<string> commands = StringTokenize(allCommands, "\n");
		
		//Process each command separately
		for (const string& command : commands)
		{
			if (!command.empty())
			{
				//Sometimes we'll get something like ID2_Volume|47| so let's separate them into words
				vector<string> words = StringTokenize(command, "|");
				
				string parm1 = "";
				if (words.size() > 1)
				{
					parm1 = words[1];
				}
				LaunchScript(words[0], parm1);
			}
		}
	}

	m_client.GetBuffer().clear();

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
