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
	void SetParms(string parm1);
	string LocateFile(string pathAndFile);
	string ReplaceLocalVariables(string in);
protected:
	TextScanner m_scanner;
	string m_parm1;
};
void LaunchScriptVariant(VariantList* pVList);
void LaunchScript(string command, string parm1 = "");
