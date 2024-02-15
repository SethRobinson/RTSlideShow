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

class RestManager;

class RestClient
{
public:
	RestClient();
	virtual ~RestClient();

	bool Init(RestManager* pRestManager, uint32 socket, int id);
	void ProcessCommand(string command);
	void UpdateInput();
	bool Update(); //when finished and it should be deleted, it returns false;
	int GetID();
	bool IsRequestingDisconnection() { return m_bRequestDisconnection; }

protected:

	int m_id = 0;
	NetSocket m_client;
	bool m_bRequestDisconnection = false;

	RestManager* m_pRestManager = NULL;
};
