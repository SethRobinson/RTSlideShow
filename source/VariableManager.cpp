#include "PlatformPrecomp.h"
#include "VariableManager.h"


VariableManager::VariableManager()
{
	
}

VariableManager::~VariableManager()
{
}


void VariableManager::SetVar(string name, string var)
{
	m_varMap[name] = var;
}

//a function that gets a string, and replaces all %var_name% markers with their actual string values and returns it
string VariableManager::ReplaceVars(string in)
{
	string out = in;
	
	//loop through all vars
	for (auto it = m_varMap.begin(); it != m_varMap.end(); ++it)
	{
		//replace all instances of %var_name% with the value
		string varName = "%" + it->first + "%";
		string varValue = it->second;
		StringReplace(varName, varValue, out);
	}

	return out;
}
