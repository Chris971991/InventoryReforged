#pragma once

// What a person's record takes from the templates it names, and which other records may lend it a face. Inventory
// Reforged and Know your Skyrim both build item models from ItemView, only the browser of Know your Skyrim shows
// people and tells which records belong together
namespace People
{
	// The record a person takes its looks from: one that takes its traits from a template has the template's race,
	// sex and skin, a list of templates gives its first person. The race such a record names itself is a stand-in,
	// often a fox
	const RE::TESNPC& TraitsOf(const RE::TESNPC& a_person);
	const RE::TESRace* RaceOf(const RE::TESNPC& a_person);

	// the other records shown as the same person, tried for a face when the person's own record has none
	using Lookalikes = std::span<RE::TESNPC* const> (*)(const RE::TESNPC& a_person);
	void SetLookalikes(Lookalikes a_find);
	std::span<RE::TESNPC* const> LookalikesOf(const RE::TESNPC& a_person);
}
