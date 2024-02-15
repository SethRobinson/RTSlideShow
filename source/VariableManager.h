//  ***************************************************************
//  VariableManager - Creation date: 9/13/2023 11:04:16 AM
//  -------------------------------------------------------------
//  License: Uh, check for license.txt or license.md for that?
//
//  ***************************************************************
//  Programmer(s):  Seth A. Robinson (seth@rtsoft.com)
//  ***************************************************************

#pragma once

class VariableManager
{
public:
	VariableManager();
	virtual ~VariableManager();

	void SetVar(string name, string var);

	string ReplaceVars(string in);

protected:

	//create map for the variables, string key, string value
	std::map<string, string> m_varMap;

};
