//  ***************************************************************
//  Script - Creation date: 8/1/2023 7:18:50 PM
//  -------------------------------------------------------------
//  License: Uh, check for license.txt or license.md for that?
//
//  ***************************************************************
//  Programmer(s):  Seth A. Robinson (seth@rtsoft.com)
//  ***************************************************************

#pragma once
#include "util/TextScanner.h"


class Script
{
public:
	Script();
	virtual ~Script();

	bool Load(string fname);
	void Run();

	string LocateFile(string pathAndFile);

protected:
	TextScanner m_scanner;

};
void LaunchScriptVariant(VariantList* pVList);
void LaunchScript(string command);
