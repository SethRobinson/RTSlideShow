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

class RestManager
{
public:
	RestManager();
	virtual ~RestManager();

	void UpdateWaitingForConnection();

	bool Init();

	void UpdateClients();

	void Update();

	NetSocket m_listener; //initial listen, then we transfer the connecting party to the below socket(s)
	NetSocket m_hostSocket; //just supporting one right now

	int m_hostPort = 8095;

	list<RestClient*> m_clients;

	int m_idCounter = 0;

protected:

};
