#pragma once

// The words of a mod and its menus in the language of the game, from Interface/Translations/<stem>_<LANGUAGE>.txt
// or the English one when that language has no file: InventoryReforged for the panel, KnowYourSkyrim for the browser.
// The movie takes them from the DLL as they are in the file: the game keeps its strings in one pool that ignores
// letter case, so a word put into its translator comes back as HEALTH when the game had that one first.
// The menus can only read the game's translator, and SKSE fills it from the file only for a plugin enabled in
// plugins.txt, so the DLL puts the lines there when SKSE hasn't. A mod may put a translator of its own in the game's
// place, then nothing goes into it: read as the game's own it took the game down at its start (a player's crash log
// of Know your Skyrim 1.0.1). A mod that needs no menu words from the game keeps off its translator altogether
namespace Translations
{
	// a_prefix starts every key of the file, "$IR_". a_intoGame false never touches the game's translator, Game()
	// then reads the translation files of the plugins itself
	void Load(std::string_view a_stem, std::string_view a_prefix, bool a_intoGame = true);
	// every key of the file as a member of a_words, its words as the file has them
	void Fill(RE::GFxMovie& a_movie, RE::GFxValue& a_words);
	// The words the game's translator has for a key like "$TNG_TCT", some mods name their items so. Empty when it
	// has none
	std::string Game(std::string_view a_key);
	// the words of the file for one of its keys like "$IR_Panel", the key without its prefix when the file has none
	std::string Word(std::string_view a_key);
}
