//  ***************************************************************
//  FooBarManagercpp - Creation date: 8/3/2023 2:46:44 PM
//  -------------------------------------------------------------
//  License: Uh, check for license.txt or license.md for that?
//
//  ***************************************************************
//  Programmer(s):  Seth A. Robinson (seth@rtsoft.com)
//  ***************************************************************

#pragma once

//This controls Foobar on the same local computer (could easily be modified to allow external network access too...)
//it must have https://github.com/hyperblast/beefweb installed in Foobar2000, which is a RESTful api to control it.


class FoobarManager
{
public:
	FoobarManager();
	virtual ~FoobarManager();

	void Play(int delayMS);

	void UpdateStatusAndDoSomething(int delayMS, string auction);

	void SetPause(bool bPause, int delayMS);

protected:

	string url = "127.0.0.1";
	uint32 port = 8880;


};
