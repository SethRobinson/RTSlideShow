#include "PlatformPrecomp.h"
#include "RestManager.h"
#include "Script.h"
#include <chrono>

RestManager::RestManager()
{
}

RestManager::~RestManager()
{
	Stop();
}

//Parse one command line ("ID2_Volume|47|") and launch it.  MUST run on the main thread,
//because LaunchScript touches entities, the message manager and (indirectly) GL.
static void LaunchFromLine(const std::string& line)
{
	if (line.empty()) return;

	//Sometimes we'll get something like ID2_Volume|47| so let's separate them into words
	vector<string> words = StringTokenize(line, "|");
	if (words.empty()) return;

	string parm1 = "";
	if (words.size() > 1)
	{
		parm1 = words[1];
	}
	LaunchScript(words[0], parm1);
}

void RestManager::UpdateWaitingForConnection()
{
	uint32 AcceptSocket = SOCKET_ERROR; //don't change to SOCKET, will break linux compile

	AcceptSocket = (uint32)accept(m_listener.GetSocket(), NULL, NULL);

	if (AcceptSocket != SOCKET_ERROR)
	{
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

	//Run all socket I/O on a dedicated thread so the responsiveness of the controllers no
	//longer depends on the render frame rate.  Even if a frame stalls for a while (loading
	//media, a heavy transition, a GL context recreate), the link stays alive, heartbeats get
	//acked, and incoming commands keep getting buffered.  The main thread just drains and
	//launches them in Update().
	m_bRunning = true;
	m_netThread = std::thread(&RestManager::NetThreadMain, this);

	return true; //success
}

void RestManager::NetThreadMain()
{
	while (m_bRunning)
	{
		m_listener.Update();
		UpdateWaitingForConnection();

		std::vector<std::string> commands;

		//Service every client: read/write its socket, ack heartbeats, collect commands, and
		//drop any that disconnected or went dead.
		for (list<RestClient*>::iterator itor = m_clients.begin(); itor != m_clients.end();)
		{
			RestClient* pClient = *itor;
			if (!pClient->UpdateNet(commands) || pClient->IsRequestingDisconnection())
			{
				SAFE_DELETE(pClient);
				itor = m_clients.erase(itor);
				LogMsg("CLIENT DISCONNECTED: %d connected.", m_clients.size());
				continue;
			}
			itor++;
		}

		if (!commands.empty())
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			//hand the finished command lines off to the main thread
			m_commandQueue.insert(m_commandQueue.end(), commands.begin(), commands.end());
		}

		//Poll quickly (sub-frame) for low latency without pegging a core.
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

void RestManager::Update()
{
	//Main thread: pull whatever the network thread has collected and launch it here.
	std::vector<std::string> toRun;
	{
		std::lock_guard<std::mutex> lock(m_queueMutex);
		if (!m_commandQueue.empty())
		{
			toRun.swap(m_commandQueue);
		}
	}

	for (const std::string& line : toRun)
	{
		LaunchFromLine(line);
	}
}

void RestManager::Stop()
{
	if (m_bRunning)
	{
		m_bRunning = false;
		if (m_netThread.joinable())
		{
			m_netThread.join();
		}
	}

	//Clean up any remaining clients (now that the thread is stopped, this is safe).
	for (RestClient* pClient : m_clients)
	{
		SAFE_DELETE(pClient);
	}
	m_clients.clear();
}
