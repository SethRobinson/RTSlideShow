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


void RestClient::ExtractLines(std::vector<std::string>& outCommands)
{
	vector<char>& s = m_client.GetBuffer();

	if (!s.empty())
	{
		//Append the raw bytes we just read onto whatever partial line we were holding.
		m_pending.append(s.data(), s.size());
		s.clear();
	}

	//Process only complete lines (up to the last newline); keep any unfinished tail.
	size_t lineEnd;
	while ((lineEnd = m_pending.find('\n')) != string::npos)
	{
		string line = m_pending.substr(0, lineEnd);
		m_pending.erase(0, lineEnd + 1);

		//Tolerate CRLF line endings by trimming a trailing '\r'.
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}

		if (line.empty()) continue;

		//Heartbeat / keep-alive messages exist only to keep the link warm and let us detect
		//a dead peer.  Don't queue them for LaunchScript (there's no script for them) - just
		//ack here (we're on the thread that owns the socket) so traffic flows both ways.
		if (line.find("_Ping") != string::npos || line.find("_KeepAlive") != string::npos)
		{
			m_client.Write("K\n");
			continue;
		}

		outCommands.push_back(line);
	}

	//Safety valve: if a misbehaving client never sends a newline, don't let the buffer grow
	//without bound.
	if (m_pending.size() > 8192)
	{
		LogMsg("RestClient %d: pending buffer overflow (%d bytes), clearing", m_id, (int)m_pending.size());
		m_pending.clear();
	}
}

bool RestClient::UpdateNet(std::vector<std::string>& outCommands)
{
	m_client.Update();

	if (m_client.WasDisconnected())
	{
			return false;
	}

	ExtractLines(outCommands);

	//Half-open detection: a peer that dropped off WiFi won't send a FIN, so WasDisconnected()
	//never trips.  Since clients ping every few seconds, prolonged total silence means the
	//link is dead - drop it so we don't accumulate zombie sockets.
	if (m_client.GetIdleReadTimeMS() > C_REST_CLIENT_IDLE_TIMEOUT_MS)
	{
		LogMsg("RestClient %d: no data for %dms, assuming dead link and disconnecting", m_id, m_client.GetIdleReadTimeMS());
		return false;
	}

	return true;
}

int RestClient::GetID()
{
	assert(m_id != 0);
	return m_id;
}
