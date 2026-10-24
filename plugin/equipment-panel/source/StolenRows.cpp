#include "PCH.h"

#include "StolenRows.h"

#include "Settings.h"

namespace StolenRows
{
	namespace
	{
		// The menus get their entries from the game one at a time, each a new copy the menu frees later: the entry at
		// an index, or all entries of one item for a row that changed. Papyrus and the rest of the game call the same
		// two and keep their lists whole, only the calls in the item menus are patched
		constexpr REL::ID kEntryAt{ 16106 };
		constexpr REL::ID kEntriesOf{ 16107 };

		struct Builder
		{
			std::uint64_t id;
			std::size_t calls;
		};
		// the lists of the shop, the container, the gift and the inventory menus, whole and for one changed row, and how
		// many calls of the two each makes. Numbers of game 1.6 and later
		constexpr std::array kBuilders{ Builder{ 50960, 1 }, Builder{ 50962, 2 }, Builder{ 51156, 2 }, Builder{ 51157, 1 }, Builder{ 51583, 2 },
			Builder{ 51584, 1 }, Builder{ 51866, 2 }, Builder{ 51867, 2 }, Builder{ 51868, 1 }, Builder{ 51873, 2 }, Builder{ 51874, 1 } };
		// the farthest call is 0x22E bytes into its list in game 1.7.104
		constexpr std::size_t kLookAhead = 0x400;
		constexpr std::size_t kCallSize = 5;
		constexpr std::uint8_t kCall = 0xE8;

		// What a merchant, a follower, a corpse or a pickpocketed one carries is all someone else's, those rows stay as
		// the game makes them. The player's own things and a chest's get split
		bool Splits(const RE::InventoryChanges* a_changes)
		{
			if (!Settings::Get().stolenApart || !a_changes || !a_changes->owner) {
				return false;
			}
			return a_changes->owner == RE::PlayerCharacter::GetSingleton() || !a_changes->owner->Is(RE::FormType::ActorCharacter);
		}

		// a row of stolen copies made here carries this in the spare bytes of its entry, so a move knows it without
		// reading the row's lists
		constexpr std::uint32_t kStolenRow = 0x4E4C5453;

		// Moves the copies that belong to someone else, by the game's own test of an owner against the player and his
		// factions, into an entry of their own and gives it back. Null when the copies are all one or all the other
		RE::InventoryEntryData* TakeStolen(RE::InventoryEntryData& a_entry)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !a_entry.object || !a_entry.extraLists) {
				return nullptr;
			}
			std::vector<RE::ExtraDataList*> theirs;
			std::vector<RE::ExtraDataList*> rest;
			std::int32_t count = 0;
			for (auto* list : *a_entry.extraLists) {
				auto* owner = list ? list->GetOwner() : nullptr;
				if (owner && !a_entry.IsOwnedBy(player, owner, true)) {
					theirs.push_back(list);
					count += list->GetCount();
				} else {
					rest.push_back(list);
				}
			}
			if (theirs.empty() || count >= a_entry.countDelta) {
				return nullptr;
			}
			// push_front turns the order over, so both go in from the back
			auto* stolen = new RE::InventoryEntryData(a_entry.object, count);
			stolen->pad14 = kStolenRow;
			for (auto list = theirs.rbegin(); list != theirs.rend(); ++list) {
				stolen->AddExtraList(*list);
			}
			if (rest.empty()) {
				delete a_entry.extraLists;
				a_entry.extraLists = nullptr;
			} else {
				a_entry.extraLists->clear();
				for (auto list = rest.rbegin(); list != rest.rend(); ++list) {
					a_entry.extraLists->push_front(*list);
				}
			}
			a_entry.countDelta -= count;
			return stolen;
		}

		// A whole list asks the entries by index from 0 until there's none. On the first ask the entries of the game
		// are made all at once with the stolen ones put after their own, the next asks get them in that order
		struct Listing
		{
			RE::InventoryChanges* changes = nullptr;
			std::vector<RE::InventoryEntryData*> entries;
		};
		Listing listing;
		std::mutex listingLock;

		void Forget()
		{
#if IR_DEBUG_EDITION
			if (const auto left = std::ranges::count_if(listing.entries, [](const RE::InventoryEntryData* a_entry) { return a_entry != nullptr; }); left > 0) {
				IR_LOG("{} rows of the item list before were never asked for, they are freed", left);
			}
#endif
			for (auto* entry : listing.entries) {
				delete entry;
			}
			listing.entries.clear();
			listing.changes = nullptr;
		}

		struct EntryAt
		{
			static RE::InventoryEntryData* thunk(RE::InventoryChanges* a_changes, std::int32_t a_index)
			{
				if (!Splits(a_changes) || a_index < 0) {
					return original(a_changes, a_index);
				}
				IR_HOOK("item list rows with stolen copies apart");
				const std::scoped_lock lock{ listingLock };
				if (a_index == 0) {
					Forget();
					listing.changes = a_changes;
					[[maybe_unused]] std::size_t split = 0;
					for (std::int32_t index = 0;; ++index) {
						auto* entry = original(a_changes, index);
						if (!entry) {
							break;
						}
						listing.entries.push_back(entry);
						if (auto* stolen = TakeStolen(*entry)) {
							listing.entries.push_back(stolen);
							++split;
						}
					}
					IR_LOG("the item list of {:08X} came as {} rows, {} of them stolen copies put apart", a_changes->owner ? a_changes->owner->GetFormID() : 0,
						listing.entries.size(), split);
				} else if (listing.changes != a_changes) {
					return original(a_changes, a_index);
				}
				const auto at = static_cast<std::size_t>(a_index);
				if (at >= listing.entries.size()) {
					Forget();
					return nullptr;
				}
				return std::exchange(listing.entries[at], nullptr);
			}

			static inline REL::Relocation<decltype(thunk)> original{ kEntryAt };
		};

		// The game's own gathering of an item's rows, when a row changes in an open menu, is told when it takes long
		constexpr auto kSlowGather = std::chrono::milliseconds(100);

		void TellSlowGather(std::chrono::steady_clock::time_point a_start, const RE::TESBoundObject* a_object, const RE::BSTArray<RE::InventoryEntryData*>* a_entries)
		{
			const auto took = std::chrono::steady_clock::now() - a_start;
			if (took < kSlowGather || !a_object || !a_entries) {
				return;
			}
			std::size_t lists = 0;
			for (const auto* entry : *a_entries) {
				if (entry && entry->extraLists) {
					lists += static_cast<std::size_t>(std::distance(entry->extraLists->begin(), entry->extraLists->end()));
				}
			}
			logger::info("the game took {:.0f} ms to gather the rows of {} {:08X} for a menu: {} rows, {} lists of extra data", std::chrono::duration<double, std::milli>(took).count(),
				a_object->GetName(), a_object->GetFormID(), a_entries->size(), lists);
		}

		struct EntriesOf
		{
			static void thunk(RE::InventoryChanges* a_changes, RE::TESBoundObject* a_object, RE::BSTArray<RE::InventoryEntryData*>* a_entries)
			{
				IR_HOOK("the rows of one item gathered for a menu");
				IR_NOTE(irTimer, "item {:08X}", a_object ? a_object->GetFormID() : 0);
				const auto start = std::chrono::steady_clock::now();
				original(a_changes, a_object, a_entries);
				TellSlowGather(start, a_object, a_entries);
				if (!a_entries || !Splits(a_changes)) {
					return;
				}
				std::vector<RE::InventoryEntryData*> entries;
				bool split = false;
				for (auto* entry : *a_entries) {
					entries.push_back(entry);
					if (auto* stolen = entry ? TakeStolen(*entry) : nullptr) {
						entries.push_back(stolen);
						split = true;
					}
				}
				if (split) {
					IR_LOG("the rows of {:08X} come as {} with the stolen copies apart", a_object ? a_object->GetFormID() : 0, entries.size());
					a_entries->clear();
					for (auto* entry : entries) {
						a_entries->push_back(entry);
					}
				}
			}

			static inline REL::Relocation<decltype(thunk)> original{ kEntriesOf };
		};

		// Moving some copies of a row, the game gives the move one extra list, the row's first, and only when that
		// list holds all of them and isn't worn (51825). Else it moves that many copies of the item as they come, the
		// free ones first, and a row of stolen copies each with a list of its own gave the player's own copies away.
		// So when the game picked no list and the one who gives has stolen copies of the item, the copies go list by
		// list. Taking from and storing into a container, selling and dropping pick this way
		constexpr REL::ID kListFor{ 51825 };
		constexpr std::size_t kRemoveItem = 0x56;
		constexpr std::size_t kDropObject = 0xCB;

		struct Mover
		{
			std::uint64_t id;
			std::size_t picks;
			std::size_t move;
		};
		constexpr std::array kMovers{ Mover{ 51141, 2, kRemoveItem }, Mover{ 50951, 1, kRemoveItem }, Mover{ 51857, 1, kDropObject } };

		constexpr std::size_t CallsOfAll()
		{
			std::size_t calls = 0;
			for (const auto& builder : kBuilders) {
				calls += builder.calls;
			}
			for (const auto& mover : kMovers) {
				calls += mover.picks * 2;
			}
			return calls;
		}
		static_assert(CallsOfAll() == kCalls);
		// the second pick of the container is 0x515 bytes in, game 1.7.104
		constexpr std::size_t kMoverLookAhead = 0x800;
		// the move follows its pick within this many bytes, a call through the table of the one that gives the copies
		constexpr std::size_t kMoveAfter = 0x80;
		constexpr std::uint8_t kCallThrough = 0xFF;
		constexpr std::size_t kCallThroughSize = 6;

		struct Picked
		{
			RE::TESBoundObject* item;
			std::int32_t count;
			bool stolen;
		};
		// the pick and its move run one right after the other on one thread
		thread_local std::optional<Picked> picked;

		// The row is the menu's own copy and its lists may be gone by the pick: dropping a worn piece takes it off and
		// frees the list it wore, and reading the row's lists then handed freed memory to the game, which crashed
		// later on the next pass through the inventory (stand, 2026-10-22). Only what the row itself says is kept
		struct ListFor
		{
			static RE::ExtraDataList* thunk(RE::InventoryEntryData* a_row, std::int32_t a_count, bool a_flag)
			{
				IR_HOOK("the copies of a move picked");
				auto* list = original(a_row, a_count, a_flag);
				picked.reset();
				if (!list && a_row && a_row->object && a_count > 0 && Settings::Get().stolenApart) {
					picked = Picked{ a_row->object, a_count, a_row->pad14 == kStolenRow };
				}
				return list;
			}

			static inline REL::Relocation<decltype(thunk)> original{ kListFor };
		};

		template <class Function>
		Function Virtual(void* a_object, std::size_t a_index)
		{
			return reinterpret_cast<Function>((*reinterpret_cast<std::uintptr_t**>(a_object))[a_index]);
		}

		struct Parts
		{
			std::int32_t free = 0;
			std::vector<std::pair<RE::ExtraDataList*, std::int32_t>> lists;
		};

		// The copies a row moves, read off the inventory of the one who gives them as it is at the move: a stolen row
		// its stolen lists, the other row its free copies and then its own lists. A worn copy is never handed over,
		// the game takes it off the body its own way. Nothing when no copy of the item there is stolen, the game moves
		// it as it always does
		std::optional<Parts> LiveParts(RE::TESObjectREFR& a_giver, const Picked& a_pick)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* changes = a_giver.GetInventoryChanges();
			if (!player || !changes || !changes->entryList) {
				return std::nullopt;
			}
			const auto found = std::ranges::find_if(*changes->entryList, [&](const RE::InventoryEntryData* a_entry) { return a_entry && a_entry->object == a_pick.item; });
			if (found == changes->entryList->end() || !(*found)->extraLists) {
				return std::nullopt;
			}
			auto& entry = **found;
			Parts parts;
			std::int32_t listed = 0;
			bool anyStolen = false;
			for (auto* list : *entry.extraLists) {
				if (!list) {
					continue;
				}
				const auto count = list->GetCount();
				auto* owner = list->GetOwner();
				const bool stolen = owner && !entry.IsOwnedBy(player, owner, true);
				const bool worn = list->HasType(RE::ExtraDataType::kWorn) || list->HasType(RE::ExtraDataType::kWornLeft);
				listed += count;
				anyStolen = anyStolen || stolen;
				if (stolen == a_pick.stolen && !worn) {
					parts.lists.emplace_back(list, count);
				}
			}
			if (!anyStolen) {
				return std::nullopt;
			}
			if (!a_pick.stolen) {
				const auto counts = a_giver.GetInventoryCounts([&](RE::TESBoundObject& a_object) { return &a_object == a_pick.item; });
				const auto total = counts.contains(a_pick.item) ? counts.at(a_pick.item) : 0;
				parts.free = std::max(total - listed, 0);
			}
			logger::info("{} copies of {} move list by list, {} of them free, {} lists of the {} row", a_pick.count, a_pick.item->GetName(), parts.free, parts.lists.size(),
				a_pick.stolen ? "stolen" : "own");
			return parts;
		}

		// the free copies the way the game moves them, then each list, what is left as it comes
		template <class Move>
		void InParts(std::int32_t a_count, const Parts& a_parts, Move a_move)
		{
			std::int32_t left = a_count;
			if (const auto free = std::min(a_parts.free, left); free > 0) {
				a_move(nullptr, free);
				left -= free;
			}
			for (const auto& [list, count] : a_parts.lists) {
				if (left <= 0) {
					break;
				}
				const auto part = std::min(left, count);
				a_move(list, part);
				left -= part;
			}
			if (left > 0) {
				a_move(nullptr, left);
			}
		}

		std::optional<Parts> PartsFor(RE::TESObjectREFR* a_giver, const RE::TESBoundObject* a_item, std::int32_t a_count, const RE::ExtraDataList* a_list)
		{
			const auto pick = std::exchange(picked, std::nullopt);
			if (!pick || !a_giver || a_list || pick->item != a_item || pick->count != a_count) {
				return std::nullopt;
			}
			return LiveParts(*a_giver, *pick);
		}

		struct Remove
		{
			using Function = RE::ObjectRefHandle* (*)(RE::TESObjectREFR*, RE::ObjectRefHandle*, RE::TESBoundObject*, std::int32_t, RE::ITEM_REMOVE_REASON, RE::ExtraDataList*,
				RE::TESObjectREFR*, const RE::NiPoint3*, const RE::NiPoint3*);

			static RE::ObjectRefHandle* thunk(RE::TESObjectREFR* a_this, RE::ObjectRefHandle* a_out, RE::TESBoundObject* a_item, std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason,
				RE::ExtraDataList* a_list, RE::TESObjectREFR* a_moveTo, const RE::NiPoint3* a_at, const RE::NiPoint3* a_turn)
			{
				IR_HOOK("copies moved out of an inventory");
				const auto remove = Virtual<Function>(a_this, kRemoveItem);
				const auto parts = PartsFor(a_this, a_item, a_count, a_list);
				if (!parts) {
					return remove(a_this, a_out, a_item, a_count, a_reason, a_list, a_moveTo, a_at, a_turn);
				}
				IR_BRACKET("{} copies of {:08X} go from {:08X} to {:08X} list by list", a_count, a_item ? a_item->GetFormID() : 0, a_this ? a_this->GetFormID() : 0,
					a_moveTo ? a_moveTo->GetFormID() : 0);
				InParts(a_count, *parts, [&](RE::ExtraDataList* a_part, std::int32_t a_partCount) {
					remove(a_this, a_out, a_item, a_partCount, a_reason, a_part, a_moveTo, a_at, a_turn);
				});
				return a_out;
			}
		};

		struct Drop
		{
			using Function = RE::ObjectRefHandle* (*)(RE::Actor*, RE::ObjectRefHandle*, const RE::TESBoundObject*, RE::ExtraDataList*, std::int32_t, const RE::NiPoint3*, const RE::NiPoint3*);

			static RE::ObjectRefHandle* thunk(RE::Actor* a_this, RE::ObjectRefHandle* a_out, const RE::TESBoundObject* a_item, RE::ExtraDataList* a_list, std::int32_t a_count,
				const RE::NiPoint3* a_at, const RE::NiPoint3* a_turn)
			{
				IR_HOOK("copies dropped");
				const auto drop = Virtual<Function>(a_this, kDropObject);
				const auto parts = PartsFor(a_this, a_item, a_count, a_list);
				if (!parts) {
					return drop(a_this, a_out, a_item, a_list, a_count, a_at, a_turn);
				}
				IR_BRACKET("{} copies of {:08X} are dropped by {:08X} list by list", a_count, a_item ? a_item->GetFormID() : 0, a_this ? a_this->GetFormID() : 0);
				InParts(a_count, *parts, [&](RE::ExtraDataList* a_part, std::int32_t a_partCount) {
					drop(a_this, a_out, a_item, a_part, a_partCount, a_at, a_turn);
				});
				return a_out;
			}
		};

		// The calls of a function to any of these targets, found by what they call so the places needn't be known for
		// every game version. Its own come first from its start, the next function may begin a single filler byte
		// after its end, so the search stops at the count it knows. A function gets patched only with all its calls
		// found, a plugin that took one of them first leaves that function as it is
		std::vector<std::uintptr_t> CallsIn(std::size_t a_count, std::uintptr_t a_start, std::uintptr_t a_end, std::size_t a_lookAhead, std::initializer_list<std::uintptr_t> a_targets)
		{
			std::vector<std::uintptr_t> calls;
			for (auto place = a_start; calls.size() < a_count && place < a_start + a_lookAhead && place + kCallSize <= a_end; ++place) {
				const auto* bytes = reinterpret_cast<const std::uint8_t*>(place);
				if (bytes[0] != kCall) {
					continue;
				}
				const auto target = place + kCallSize + *reinterpret_cast<const std::int32_t*>(place + 1);
				if (std::ranges::find(a_targets, target) != a_targets.end()) {
					calls.push_back(place);
				}
			}
			return calls;
		}

		// a call through the table of an object to the function at this place in it, call [register + distance]
		std::optional<std::uintptr_t> CallThroughAfter(std::uintptr_t a_from, std::uintptr_t a_end, std::size_t a_index)
		{
			for (auto place = a_from; place < a_from + kMoveAfter && place + kCallThroughSize <= a_end; ++place) {
				const auto* bytes = reinterpret_cast<const std::uint8_t*>(place);
				const bool throughRegister = (bytes[1] & 0xF8) == 0x90 && (bytes[1] & 7) != 4;
				if (bytes[0] == kCallThrough && throughRegister && *reinterpret_cast<const std::int32_t*>(place + 2) == static_cast<std::int32_t>(a_index * sizeof(void*))) {
					return place;
				}
			}
			return std::nullopt;
		}
	}

	void Install()
	{
		const auto game = REL::Module::get().version().string();
		if (!REL::Module::IsAE()) {
			logger::info("stolen copies stay in the rows of the rest, the calls are known for game 1.6 and later (game {})", game);
			return;
		}
		const auto code = REL::Module::get().segment(REL::Segment::textx);
		const auto end = code.address() + code.size();
		const auto entryAt = EntryAt::original.address();
		const auto entriesOf = EntriesOf::original.address();
		auto& trampoline = SKSE::GetTrampoline();
		std::size_t patched = 0;
		for (const auto& builder : kBuilders) {
			const auto start = REL::Relocation<std::uintptr_t>{ REL::ID(builder.id) }.address();
			const auto calls = CallsIn(builder.calls, start, end, kLookAhead, { entryAt, entriesOf });
			if (calls.size() != builder.calls) {
				logger::warn("the item list {} keeps stolen copies in the rows of the rest, {} of its {} calls found (game {})", builder.id, calls.size(), builder.calls, game);
				continue;
			}
			for (const auto place : calls) {
				const auto target = place + kCallSize + *reinterpret_cast<const std::int32_t*>(place + 1);
				if (target == entriesOf) {
					trampoline.write_call<5>(place, EntriesOf::thunk);
				} else {
					trampoline.write_call<5>(place, EntryAt::thunk);
				}
			}
			patched += calls.size();
		}
		const auto listFor = ListFor::original.address();
		for (const auto& mover : kMovers) {
			const auto start = REL::Relocation<std::uintptr_t>{ REL::ID(mover.id) }.address();
			const auto picks = CallsIn(mover.picks, start, end, kMoverLookAhead, { listFor });
			std::vector<std::uintptr_t> moves;
			for (const auto pick : picks) {
				if (const auto move = CallThroughAfter(pick + kCallSize, end, mover.move)) {
					moves.push_back(*move);
				}
			}
			if (picks.size() != mover.picks || moves.size() != mover.picks) {
				logger::warn("the moves of {} keep the game's own pick of copies, {} picks and {} moves of {} found (game {})", mover.id, picks.size(), moves.size(), mover.picks, game);
				continue;
			}
			for (std::size_t i = 0; i < picks.size(); ++i) {
				trampoline.write_call<5>(picks[i], ListFor::thunk);
				if (mover.move == kDropObject) {
					trampoline.write_call<6>(moves[i], Drop::thunk);
				} else {
					trampoline.write_call<6>(moves[i], Remove::thunk);
				}
			}
			patched += picks.size() + moves.size();
		}
		logger::info("stolen copies get rows of their own, {} of {} calls of the item menus patched", patched, kCalls);
	}
}
