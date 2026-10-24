#pragma once

// Who holds, deals out and makes what, as the records of the game tell it: loot lists and the lists that hold them,
// chests, the things people carry, wear and drop, merchants and the chests of their factions, recipes. Read on the
// first ask and kept while the game runs, records don't change after the data is loaded. The quest lines of Inventory
// Reforged's item window ask whether anything gives an item at all, the item browser of Know your Skyrim tells it all
namespace Holders
{
	enum class Way
	{
		kContainer,
		kCarried,
		kDropped
	};

	struct Holder
	{
		const RE::TESForm* by;
		Way way;
	};

	struct Parent
	{
		const RE::TESForm* list;
		std::uint16_t level;
	};

	struct Known
	{
		std::unordered_map<const RE::TESForm*, std::vector<Parent>> parents;
		std::unordered_map<const RE::TESForm*, std::vector<Holder>> holders;
		std::unordered_map<const RE::TESForm*, std::vector<const RE::TESFaction*>> vendorChests;
		std::unordered_map<const RE::TESFaction*, std::vector<const RE::TESNPC*>> vendors;
		std::unordered_multimap<const RE::TESForm*, const RE::BGSConstructibleObject*> makes;
		std::unordered_multimap<const RE::TESForm*, const RE::BGSConstructibleObject*> takes;
		std::unordered_map<const RE::BGSKeyword*, std::string> benches;
		// leveled lists of people and creatures that hold each of them
		std::unordered_map<const RE::TESForm*, std::vector<Parent>> encounters;
	};

	Known& Read();

	// no loot list, chest, merchant, person or recipe gives the item, it lies somewhere or a quest hands it out
	bool OnlyPlaced(const RE::TESForm& a_item);

	// A form the game made while it runs can be gone after the next load, the lists keep only forms of the plugins
	bool Lasting(const RE::TESForm* a_form);
	bool Named(const RE::TESForm* a_form);
}
