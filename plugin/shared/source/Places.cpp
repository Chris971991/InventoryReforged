#include "PCH.h"

#include "Places.h"

#include "Holders.h"

namespace Places
{
	namespace
	{
		using Holders::Lasting;
		using Holders::Named;

		// a marker of the world map with the name the map gives it
		struct Marker
		{
			RE::FormID reference;
			RE::NiPoint3 position;
			std::string name;
		};

		// the markers of the world map by the world they stand in
		std::unordered_map<RE::FormID, std::vector<Marker>> markers;
		bool markersRead = false;

		void AddMarker(const RE::TESObjectREFR& a_reference)
		{
			const auto* marker = a_reference.extraList.GetByType<RE::ExtraMapMarker>();
			const char* name = marker && marker->mapData ? marker->mapData->locationName.GetFullName() : nullptr;
			// most markers stand in the cell of the world that lasts, which names no world itself, the game's own
			// question finds the world of those too
			const auto* world = name && *name ? a_reference.GetWorldspace() : nullptr;
			if (world) {
				markers[world->GetFormID()].push_back({ a_reference.GetFormID(), a_reference.GetPosition(), name });
			}
		}

		// Only lasting references stay in memory for the whole game, a reference of a cell that isn't loaded is in
		// its plugin file alone. Under the lock of the game's forms only their ids are taken, each reference is looked
		// up after it: what a reference is asked takes locks of its own, and a thread of the game holding one of those
		// while it waits to add a form would wait on this walk forever
		std::vector<RE::FormID> LastingIds()
		{
			std::vector<RE::FormID> lasting;
			const auto& [forms, lock] = RE::TESForm::GetAllForms();
			const RE::BSReadLockGuard locker{ lock };
			if (!forms) {
				return lasting;
			}
			for (const auto& [id, form] : *forms) {
				const auto type = form ? form->GetFormType() : RE::FormType::None;
				if (!Lasting(form) || (type != RE::FormType::Reference && type != RE::FormType::ActorCharacter) ||
					(form->GetFormFlags() & RE::TESObjectREFR::RecordFlags::kPersistent) == 0 || form->IsDeleted()) {
					continue;
				}
				lasting.push_back(id);
			}
			return lasting;
		}

		template <class F>
		void ForEachLasting(F&& a_take)
		{
			for (const auto id : LastingIds()) {
				if (const auto* reference = RE::TESForm::LookupByID<RE::TESObjectREFR>(id)) {
					a_take(id, *reference);
				}
			}
		}

		void ReadMarkers()
		{
			if (!markersRead) {
				WalkLasting([](RE::FormID, const RE::TESObjectREFR&) {});
			}
		}

		// Mods and the game itself keep things at the very origin of a world until a script moves them where they're
		// needed: Skyrim.esm has a potion, an ingredient and a misc item there in Whiterun's world, a mod's market
		// keeps its potions within 22 units of Solitude's. No one finds them there, and the marker nearest to the
		// origin of Skyrim's own world is Bleakwind Basin. Things that really stand near a world's origin stand on its
		// ground, hundreds of units above or below it
		constexpr float kHoldingSpot = 64.0F;

		bool Holding(const RE::NiPoint3& a_at)
		{
			return a_at.Length() < kHoldingSpot;
		}

		// A whole hold or the world, too wide to tell where something is
		bool Broad(const RE::BGSLocation& a_location)
		{
			if (!a_location.parentLoc) {
				return true;
			}
			return std::ranges::any_of(a_location.GetKeywords(), [](const RE::BGSKeyword* a_keyword) {
				return a_keyword && std::string_view(a_keyword->formEditorID.c_str()) == "LocTypeHold";
			});
		}

		// the marker of the map closest to a spot of a world, a city that is a world of its own uses the markers of
		// the world it lies in
		const Marker* Nearest(const RE::TESWorldSpace* a_world, const RE::NiPoint3& a_at)
		{
			ReadMarkers();
			for (int depth = 0; a_world && depth < 4; ++depth, a_world = a_world->parentWorld) {
				const auto found = markers.find(a_world->GetFormID());
				if (found == markers.end() || found->second.empty()) {
					continue;
				}
				return &*std::ranges::min_element(found->second, {}, [&](const Marker& a_marker) { return a_marker.position.GetSquaredDistance(a_at); });
			}
			return nullptr;
		}

		// A place inside is its cell and the place around it. A cell with no name in no place is where the game keeps
		// people until a quest needs them, it tells nothing, and neither do the developers' test rooms
		Where Inside(const RE::TESObjectCELL& a_cell)
		{
			std::string name = Named(&a_cell) ? a_cell.GetName() : "";
			if (Testing(name)) {
				return {};
			}
			const auto* location = a_cell.GetLocation();
			std::string around;
			// places of a mod can name each other as parents in a ring
			int depth = 0;
			for (const auto* at = location; at && !Broad(*at) && depth < 8; at = at->parentLoc, ++depth) {
				if (Named(at) && name != at->GetName()) {
					around = at->GetName();
					break;
				}
			}
			if (name.empty()) {
				std::swap(name, around);
			}
			if (name.empty()) {
				return {};
			}
			return { around.empty() ? name : std::format("{}, {}", name, around), false, MarkerOf(location), {}, location };
		}

		// the cell of the square a spot of a world lies in, a reference that lasts sits in the cell of the world that
		// lasts
		const RE::TESObjectCELL* SquareOf(const RE::TESWorldSpace& a_world, const RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_at)
		{
			if (a_cell && a_cell != a_world.persistentCell) {
				return a_cell;
			}
			const RE::CellID square{ static_cast<std::int16_t>(std::floor(a_at.y / 4096.0F)), static_cast<std::int16_t>(std::floor(a_at.x / 4096.0F)) };
			const auto found = a_world.cellMap.find(square);
			return found != a_world.cellMap.end() ? found->second : nullptr;
		}

		Where Outside(const RE::TESWorldSpace& a_world, const RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_at)
		{
			a_cell = SquareOf(a_world, a_cell, a_at);
			const auto* location = a_cell ? a_cell->GetLocation() : nullptr;
			const auto* marker = Nearest(&a_world, a_at);
			if (location && Named(location) && !Broad(*location)) {
				return { location->GetName(), false, marker ? marker->reference : 0, {}, location };
			}
			if (marker) {
				return { marker->name, true, marker->reference };
			}
			return { Named(&a_world) ? a_world.GetName() : "", false, 0 };
		}
	}

	void WalkLasting(const std::function<void(RE::FormID, const RE::TESObjectREFR&)>& a_take)
	{
		const bool withMarkers = !markersRead;
		markersRead = true;
		ForEachLasting([&](RE::FormID a_id, const RE::TESObjectREFR& a_reference) {
			if (withMarkers) {
				AddMarker(a_reference);
			}
			a_take(a_id, a_reference);
		});
	}

	bool ReadMarkersFor(std::chrono::microseconds a_budget)
	{
		static std::vector<RE::FormID> unread;
		static bool listed = false;
		if (markersRead) {
			return true;
		}
		const auto start = std::chrono::steady_clock::now();
		if (!std::exchange(listed, true)) {
			unread = LastingIds();
			return false;
		}
		while (!unread.empty() && std::chrono::steady_clock::now() - start < a_budget) {
			if (const auto* reference = RE::TESForm::LookupByID<RE::TESObjectREFR>(unread.back())) {
				AddMarker(*reference);
			}
			unread.pop_back();
		}
		markersRead = unread.empty();
		return markersRead;
	}

	Where At(RE::FormID a_cell, RE::FormID a_world, const RE::NiPoint3& a_at)
	{
		const auto* cell = RE::TESForm::LookupByID<RE::TESObjectCELL>(a_cell);
		if (cell && cell->IsInteriorCell()) {
			return Inside(*cell);
		}
		const auto* world = a_world != 0 ? RE::TESForm::LookupByID<RE::TESWorldSpace>(a_world) : (cell ? cell->GetRuntimeData().worldSpace : nullptr);
		return world && !Holding(a_at) ? Outside(*world, cell, a_at) : Where{};
	}

	Where Of(const RE::TESObjectREFR& a_reference)
	{
		const auto* cell = a_reference.GetParentCell();
		if (!cell) {
			return {};
		}
		const auto* world = cell->IsInteriorCell() ? nullptr : a_reference.GetWorldspace();
		return At(cell->GetFormID(), world ? world->GetFormID() : 0, a_reference.GetPosition());
	}

	Where OfReference(RE::FormID a_reference)
	{
		const auto* reference = RE::TESForm::LookupByID<RE::TESObjectREFR>(a_reference);
		return reference ? Of(*reference) : Where{};
	}

	const RE::BGSLocation* LocationOf(const RE::TESObjectREFR& a_reference)
	{
		const auto* cell = a_reference.GetParentCell();
		if (!cell || cell->IsInteriorCell()) {
			return cell ? cell->GetLocation() : nullptr;
		}
		if (Holding(a_reference.GetPosition())) {
			return nullptr;
		}
		const auto* world = a_reference.GetWorldspace();
		const auto* square = world ? SquareOf(*world, cell, a_reference.GetPosition()) : cell;
		return square ? square->GetLocation() : nullptr;
	}

	// The map marker that leads to a place inside: the location's marker outside, or its parent's
	RE::FormID MarkerOf(const RE::BGSLocation* a_location)
	{
		for (int depth = 0; a_location && depth < 8; ++depth, a_location = a_location->parentLoc) {
			const auto outside = a_location->worldLocMarker.get();
			const auto* world = outside ? outside->GetWorldspace() : nullptr;
			if (const auto* marker = world ? Nearest(world, outside->GetPosition()) : nullptr) {
				return marker->reference;
			}
		}
		return 0;
	}

	// The developers' rooms, "Amulets Test Room", "Editor Smoke Test Cell", "Marker Storage Unit", hold one of
	// everything and no player ever goes there
	bool Testing(std::string_view a_name)
	{
		std::string lowered(a_name);
		std::ranges::transform(lowered, lowered.begin(), [](unsigned char a_letter) { return static_cast<char>(std::tolower(a_letter)); });
		if (lowered.find("storage unit") != std::string::npos) {
			return true;
		}
		for (std::size_t at = lowered.find("test"); at != std::string::npos; at = lowered.find("test", at + 1)) {
			const bool starts = at == 0 || !std::isalpha(static_cast<unsigned char>(lowered[at - 1]));
			const bool ends = at + 4 == lowered.size() || !std::isalpha(static_cast<unsigned char>(lowered[at + 4]));
			if (starts && ends) {
				return true;
			}
		}
		return false;
	}
}
