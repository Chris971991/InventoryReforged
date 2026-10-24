#pragma once

// The inventory turns its item zoom key, C on the keyboard and the click of the right stick on a gamepad, into a toggle
// of the game's own zoom. With this a script of the menu is asked first and may take the key, the game's zoom stays
// out then. Inventory Reforged and Know your Skyrim both use it, one patch of the game's call at a time: the first to
// install takes it, the call isn't found for another after it
namespace ZoomKey
{
	// a_ask is the script asked whether it takes the key, true when it did. a_told, when given, is told whether the
	// game's own zoom went in. a_on says whether the script is asked at all now. Needs room for a call in the trampoline
	void Install(const char* a_ask, const char* a_told, bool (*a_on)());

	// The container menu zooms by the same key. Its script is asked the same way, a_on says when its panel is up
	void InstallTrade(const char* a_ask, const char* a_told, bool (*a_on)());
}
