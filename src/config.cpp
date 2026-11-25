#include "config.h"
#include "paths.h"
#include "../inc/SimpleIni.h"

#include <filesystem>
#include <format>

namespace fs = std::filesystem;

ModConfig LoadConfig()
{
	ModConfig cfg; // defaults from config.h

	const std::string path = (fs::path(ModDataDir()) / "config.ini").string();

	CSimpleIniA ini;
	ini.SetUnicode();

	if (!fs::exists(path))
	{
		// First run: write the defaults out so there is something to edit.
		ini.SetBoolValue("Horse", "HIDE_THAT_HORSE", cfg.HideThatHorse);

		ini.SaveFile(path.c_str(), false); // false = no UTF-8 BOM
		return cfg;
	}

	// Exists but unreadable: use the defaults rather than clobbering the file.
	if (ini.LoadFile(path.c_str()) < 0)
		return cfg;

	cfg.enableTextLogging = ini.GetBoolValue("Horse", "HIDE_THAT_HORSE", cfg.HideThatHorse);
	

	return cfg;
}
