#include "ui.h"
#include "../inc/sdk.h"

#include <string>

namespace
{
	// Feed post layouts copied from Rockstar's own wrappers in the decompiled scripts, e.g.
	// abigail21_outro.c func_74 (help text) and abigail2_1.c func_1453 (objective). Both build a
	// 4-slot block and a 2-slot block, and every call site passes (duration, 0, 0, 1) and TRUE.
	// Every script variable is one 8-byte slot, hence alignas(8) on each field.
	struct FeedDuration
	{
		alignas(8) int ms = 0;
		alignas(8) int unknown1 = 0;
		alignas(8) int unknown2 = 0;
		alignas(8) int unknown3 = 1;
	};
	static_assert(sizeof(FeedDuration) == 4 * 8);

	struct FeedText
	{
		alignas(8) int unknown0 = 0; // never set by Rockstar's wrappers
		alignas(8) const char* text = nullptr; // a VAR_STRING, or a text label
	};
	static_assert(sizeof(FeedText) == 2 * 8);

	// One persistent set of buffers per feed type. Building these on the stack gets the post
	// accepted (non-zero id) but never shown; the feed may read them after the call returns, so
	// they live here instead. A second post before the first shows will overwrite its text.
	struct FeedPost
	{
		FeedDuration duration;
		FeedText data;
		std::string text;
	};
	FeedPost g_helpPost;

	void Prepare(FeedPost& post, const char* text, int durationMs)
	{
		post.text = text;
		post.duration.ms = durationMs;
		post.data.text = MISC::VAR_STRING(10, (char*)"LITERAL_STRING", (char*)post.text.c_str());
	}
}

int UI::ShowHelpText(const char* text, int durationMs)
{
	Prepare(g_helpPost, text, durationMs);
	return UIFEED::_UI_FEED_POST_HELP_TEXT(reinterpret_cast<Any*>(&g_helpPost.duration), reinterpret_cast<Any*>(&g_helpPost.data), TRUE);
}

void UI::DisplaySubtitle(const char* text)
{
	UILOG::_UILOG_SET_CACHED_OBJECTIVE(text);
	UILOG::_UILOG_PRINT_CACHED_OBJECTIVE();
	UILOG::_UILOG_CLEAR_HAS_DISPLAYED_CACHED_OBJECTIVE();
	UILOG::_UILOG_CLEAR_CACHED_OBJECTIVE();
}

void UI::DrawDebugText(const char* text, float pixelX, float pixelY, float scale)
{
	UIDEBUG::_BG_SET_TEXT_SCALE(scale, scale);
	UIDEBUG::_BG_SET_TEXT_COLOR(245, 245, 245, 255);

	const char* varString = MISC::VAR_STRING(10, (char*)"LITERAL_STRING", (char*)text);
	UIDEBUG::_BG_DISPLAY_TEXT(varString, pixelX / 1920.0f, pixelY / 1080.0f);
}

void UI::DrawDebugText(const char* text) {
	UI::DrawDebugText(text, 100.0f, 100.0f, 0.30f);
}
