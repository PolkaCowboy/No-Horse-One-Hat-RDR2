#include "script.h"
#include "keyboard.h"
#include "log.h"
#include "config.h"
#include "NHA.h";

#include <format>

namespace
{
	bool g_enabled = false;
	ModConfig g_config;

	void OnTick()
	{
		
		int now = MISC::GET_GAME_TIMER();

		if (g_config.HideThatHorse) {
			NHA::HideThatHorse(now);
			NHA::HideThatBlip();
		}
	}
}

void ScriptMain()
{

	g_config = LoadConfig();

	if (g_config.enableTextLogging) {
		Log::Init();
		Log::Write(std::format("ScriptMain started"));
	}

	while (true)
	{
		OnTick();
		WAIT(0);
	}
}
