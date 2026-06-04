//  ***************************************************************
//  RestClient - Creation date: 8/1/2023 1:54:46 PM
//  -------------------------------------------------------------
//  License: Uh, check for license.txt or license.md for that?
//
//  ***************************************************************
//  Programmer(s):  Seth A. Robinson (seth@rtsoft.com)
//  ***************************************************************

#pragma once

#include "util/ResourceUtils.h"
#include "Network/NetSocket.h"
#include <vector>
#include <string>

class RestManager;

class RestClient
{
public:
	RestClient();
	virtual ~RestClient();

	bool Init(RestManager* pRestManager, uint32 socket, int id);

	//Runs on the network thread: services the socket, acks heartbeats, and appends any
	//complete non-control command lines to outCommands (to be launched later on the main
	//thread).  Returns false when this client should be deleted (clean disconnect or a
	//half-open/dead link).
	bool UpdateNet(std::vector<std::string>& outCommands);

	int GetID();
	bool IsRequestingDisconnection() { return m_bRequestDisconnection; }

	//If we haven't heard a single byte from the client in this long, assume the link went
	//half-open (WiFi blip, device powered off out of range) and disconnect it.  The ESP
	//firmware pings every ~3s, so 15s of total silence reliably means the peer is gone.
	static const int C_REST_CLIENT_IDLE_TIMEOUT_MS = 15000;

protected:

	//Pull complete '\n'-terminated lines out of m_pending, ack heartbeats inline (we're on
	//the thread that owns the socket), and queue the rest as commands.
	void ExtractLines(std::vector<std::string>& outCommands);

	int m_id = 0;
	NetSocket m_client;
	bool m_bRequestDisconnection = false;

	//TCP is a byte stream, not a message stream.  A single recv() can contain a partial
	//line, or several lines plus a partial one.  We accumulate raw bytes here and only
	//process complete '\n'-terminated lines, keeping any unfinished tail for next time.
	std::string m_pending;

	RestManager* m_pRestManager = NULL;
};
