//  ***************************************************************
//  RestManager - Creation date: 7/31/2023 7:33:09 PM
//  -------------------------------------------------------------
//  License: Uh, check for license.txt or license.md for that?
//
//  ***************************************************************
//  Programmer(s):  Seth A. Robinson (seth@rtsoft.com)
//  ***************************************************************

#pragma once
#include "Network/NetSocket.h"
#include "RestClient.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

class RestManager
{
public:
	RestManager();
	virtual ~RestManager();

	bool Init();

	//Called from the main (render/script) thread.  Drains commands collected by the network
	//thread and launches them here, where it's safe to touch entities/GL/scripts.
	void Update();

	void Stop(); //stop the network thread and clean up clients

	NetSocket m_listener; //initial listen, then we transfer the connecting party to the below socket(s)
	NetSocket m_hostSocket; //just supporting one right now

	int m_hostPort = 8095;

	//Only the network thread touches m_clients/m_idCounter, so they need no locking.
	list<RestClient*> m_clients;

	int m_idCounter = 0;

protected:

	void UpdateWaitingForConnection(); //network thread: accept new connections
	void NetThreadMain();              //the network I/O loop (runs off the render thread)

	std::thread m_netThread;
	std::atomic<bool> m_bRunning{ false };

	//Complete command lines produced by the network thread, consumed on the main thread.
	std::mutex m_queueMutex;
	std::vector<std::string> m_commandQueue;
};
