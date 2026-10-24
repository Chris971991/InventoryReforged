#include "PCH.h"

#include "Holders.h"

#include <unordered_set>

#include "Profile.h"

namespace Holders
{
	namespace
	{
		// The test chests of the developers hold hundreds of things each, "All Misc Items" of the game holds 373, a
		// merchant's chest of the game holds 26 at most. A chest past this holds everything for testing and gives nothing
		constexpr std::uint32_t kTestChest = 50;

		std::optional<Known> known;

		void ReadLists(Known& a_known, RE::TESDataHandler& a_data)
		{
			for (const auto* list : a_data.GetFormArray<RE::TESLevItem>()) {
				if (!Lasting(list)) {
					continue;
				}
				for (const auto& entry : list->entries) {
					if (Lasting(entry.form)) {
						a_known.parents[entry.form].push_back({ list, entry.level });
					}
				}
			}
			for (const auto* list : a_data.GetFormArray<RE::TESLevCharacter>()) {
				if (!Lasting(list)) {
					continue;
				}
				for (const auto& entry : list->entries) {
					if (Lasting(entry.form)) {
						a_known.encounters[entry.form].push_back({ list, entry.level });
					}
				}
			}
		}

		void ReadContainer(Known& a_known, const RE::TESContainer& a_container, const RE::TESForm& a_by, Way a_way)
		{
			for (std::uint32_t i = 0; i < a_container.numContainerObjects; ++i) {
				const auto* entry = a_container.containerObjects[i];
				if (entry && Lasting(entry->obj)) {
					a_known.holders[entry->obj].push_back({ &a_by, a_way });
				}
			}
		}

		// a merchant sells what the chest of the faction holds, the people of that faction are the merchants
		void ReadPeople(Known& a_known, RE::TESDataHandler& a_data)
		{
			for (const auto* faction : a_data.GetFormArray<RE::TESFaction>()) {
				const auto* chest = Lasting(faction) ? faction->vendorData.merchantContainer : nullptr;
				if (const auto* base = chest ? chest->GetBaseObject() : nullptr; Lasting(base)) {
					a_known.vendorChests[base].push_back(faction);
				}
			}
			for (const auto* chest : a_data.GetFormArray<RE::TESObjectCONT>()) {
				if (Lasting(chest) && chest->numContainerObjects <= kTestChest) {
					ReadContainer(a_known, *chest, *chest, Way::kContainer);
				}
			}
			// the player's own record starts with things too, the player gets nothing from the player
			constexpr RE::FormID kPlayerBase = 0x7;
			for (const auto* person : a_data.GetFormArray<RE::TESNPC>()) {
				if (!Lasting(person) || person->GetFormID() == kPlayerBase) {
					continue;
				}
				ReadContainer(a_known, *person, *person, Way::kCarried);
				for (const auto* outfit : { person->defaultOutfit, person->sleepOutfit }) {
					if (!Lasting(outfit)) {
						continue;
					}
					for (const auto* piece : outfit->outfitItems) {
						if (Lasting(piece)) {
							a_known.holders[piece].push_back({ person, Way::kCarried });
						}
					}
				}
				if (Lasting(person->deathItem)) {
					a_known.holders[person->deathItem].push_back({ person, Way::kDropped });
				}
				for (const auto& rank : person->factions) {
					if (Lasting(rank.faction) && rank.faction->vendorData.merchantContainer) {
						a_known.vendors[rank.faction].push_back(person);
					}
				}
			}
		}

		void ReadRecipes(Known& a_known, RE::TESDataHandler& a_data)
		{
			std::unordered_set<const RE::BGSKeyword*> benches;
			for (const auto* recipe : a_data.GetFormArray<RE::BGSConstructibleObject>()) {
				if (!Lasting(recipe) || !Lasting(recipe->createdItem)) {
					continue;
				}
				a_known.makes.emplace(recipe->createdItem, recipe);
				for (std::uint32_t i = 0; i < recipe->requiredItems.numContainerObjects; ++i) {
					const auto* entry = recipe->requiredItems.containerObjects[i];
					if (entry && Lasting(entry->obj)) {
						a_known.takes.emplace(entry->obj, recipe);
					}
				}
				benches.insert(recipe->benchKeyword);
			}
			// a bench is named after the first piece of furniture of the world that has its keyword
			for (const auto* furniture : a_data.GetFormArray<RE::TESFurniture>()) {
				if (!Named(furniture)) {
					continue;
				}
				for (const auto* keyword : furniture->GetKeywords()) {
					if (benches.contains(keyword) && !a_known.benches.contains(keyword)) {
						a_known.benches.emplace(keyword, furniture->GetName());
					}
				}
			}
		}
	}

	bool Lasting(const RE::TESForm* a_form)
	{
		return a_form && (a_form->GetFormID() >> 24) != 0xFF;
	}

	bool Named(const RE::TESForm* a_form)
	{
		const char* name = a_form ? a_form->GetName() : nullptr;
		return name && *name;
	}

	Known& Read()
	{
		if (known) {
			return *known;
		}
		static const auto work = Profile::Work("item holders");
		const Profile::Timer timer{ work };
		auto* data = RE::TESDataHandler::GetSingleton();
		known.emplace();
		ReadLists(*known, *data);
		ReadPeople(*known, *data);
		ReadRecipes(*known, *data);
		return *known;
	}

	bool OnlyPlaced(const RE::TESForm& a_item)
	{
		auto& read = Read();
		return !read.parents.contains(&a_item) && !read.holders.contains(&a_item) && !read.makes.contains(&a_item);
	}
}
