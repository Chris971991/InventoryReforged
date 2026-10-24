#pragma once

// Where a reference or a spot of a cell is, the way a player names it: inside, the cell and the place around it
// (Bleak Falls Sanctum, Bleak Falls Barrow); outside, the settlement or the dungeon of its square, or the marker of the
// world map nearest to it. A place carries the marker of the map that leads there. The markers are read from the
// references that last the whole game on the first place named. A spot at the very origin of a world, where mods
// keep things until a script needs them, names no place
namespace Places
{
	struct Where
	{
		std::string name;
		bool nearby = false;
		RE::FormID marker = 0;
		// what stands there, a chest, the page names it before the place
		std::string thing;
		// the place of the game the name comes from, none for a marker of the map
		const RE::BGSLocation* location = nullptr;
	};

	Where At(RE::FormID a_cell, RE::FormID a_world, const RE::NiPoint3& a_at);
	Where Of(const RE::TESObjectREFR& a_reference);
	Where OfReference(RE::FormID a_reference);

	// The place of the game a reference is in, as Of names it: the place of its cell inside, of its square outside
	const RE::BGSLocation* LocationOf(const RE::TESObjectREFR& a_reference);

	// the marker of the map that leads to a place or to the place around it
	RE::FormID MarkerOf(const RE::BGSLocation* a_location);

	// the developers' rooms by their names, "Amulets Test Room", "Marker Storage Unit"
	bool Testing(std::string_view a_name);

	// Every reference that lasts the whole game, the walk is long. The markers of the map are taken along on the
	// first walk, a_take gets each reference
	void WalkLasting(const std::function<void(RE::FormID, const RE::TESObjectREFR&)>& a_take);

	// The markers of the map read a share at a time, for a reader on the main thread that mustn't hold up a frame:
	// the first call lists the lasting references, the next ones read them until the time is up. True once all are in
	bool ReadMarkersFor(std::chrono::microseconds a_budget);
}
