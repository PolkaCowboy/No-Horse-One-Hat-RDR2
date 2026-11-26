#pragma once

namespace UI
{
	void DisplaySubtitle(const char* text);
	void DrawDebugText(const char* text, float pixelX, float pixelY, float scale);
	void DrawDebugText(const char* text);
	int ShowHelpText(const char* text, int durationMs);
}
