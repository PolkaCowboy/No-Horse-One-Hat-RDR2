#pragma once

#include <windows.h>
#include <string>

struct ModConfig
{

	bool HideThatHorse = true;

	//Reference examples for various data types
	/*
	std::string message = "Hello from the template";
	float modifier = 0.5f;
	int maxCount = 10;
	bool enabled = true;
	DWORD toggleKey = VK_F19;
	*/
	bool enableTextLogging = false;

};

ModConfig LoadConfig();
