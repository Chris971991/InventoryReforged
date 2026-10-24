#include "PCH.h"

#include "Rows.h"

#include "Acquired.h"
#include "Inject.h"
#include "Junk.h"
#include "Profile.h"
#include "Quests.h"
#include "Settings.h"
#include "Subject.h"

namespace Rows
{
	namespace
	{
		constexpr RE::FormID kPlayer = 0x14;
		// an item of the game itself needs no word on where it comes from
		constexpr std::array kGameFiles{ "Skyrim.esm"sv, "Update.esm"sv, "Dawnguard.esm"sv, "HearthFires.esm"sv, "Dragonborn.esm"sv };

		std::mutex guard;
		// how many of each item the player carries, counted again after the inventory changed
		std::unordered_map<RE::FormID, std::int32_t> carried;
		bool counted = false;

#if IR_DEBUG_EDITION
		// the rows told since the last frame of a menu, one line for them all
		std::atomic<std::uint32_t> rowsTold{ 0 };
		std::atomic<std::uint64_t> rowsNanoseconds{ 0 };
		std::atomic<std::uint64_t> questNanoseconds{ 0 };
		std::atomic<std::uint32_t> recounts{ 0 };
		std::atomic<const char*> toldIn{ "" };

		std::uint64_t Since(std::chrono::steady_clock::time_point a_start)
		{
			return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - a_start).count());
		}
#endif

		class Moves : public RE::BSTEventSink<RE::TESContainerChangedEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
			{
				if (a_event && (a_event->newContainer == kPlayer || a_event->oldContainer == kPlayer)) {
					const std::scoped_lock lock{ guard };
					counted = false;
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// The inventory is counted with the lock let go, the sink of moves takes the same lock from whatever thread moves
		// a thing, so the lock never waits on the game
		std::int32_t Carried(RE::FormID a_form)
		{
			bool fresh = false;
			{
				const std::scoped_lock lock{ guard };
				fresh = counted;
			}
			if (!fresh) {
#if IR_DEBUG_EDITION
				++recounts;
#endif
				std::unordered_map<RE::FormID, std::int32_t> counts;
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					for (const auto& [object, count] : player->GetInventoryCounts()) {
						if (object && count > 0) {
							counts[object->GetFormID()] = count;
						}
					}
				}
				const std::scoped_lock lock{ guard };
				carried = std::move(counts);
				counted = true;
			}
			const std::scoped_lock lock{ guard };
			const auto found = carried.find(a_form);
			return found == carried.end() ? 0 : found->second;
		}

		// How many copies of the row belong to someone else, by the game's own test of an owner against the player and
		// the factions he is in. Copies with no list of their own are the player's, so most rows cost nothing here
		std::int32_t Stolen(RE::InventoryEntryData& a_item)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			std::int32_t stolen = 0;
			if (!a_item.extraLists || !player) {
				return 0;
			}
			for (auto* list : *a_item.extraLists) {
				auto* owner = list ? list->GetOwner() : nullptr;
				if (owner && !a_item.IsOwnedBy(player, owner, true)) {
					stolen += list->GetCount();
				}
			}
			return stolen;
		}

		// The game marks worn rows of the player alone. In a follower's trade the follower's worn rows come with no mark,
		// they get the marks of the game: 1 worn, 2 left hand, 3 right hand, 4 both hands
		void MarkWorn(RE::GFxValue& a_row, const RE::InventoryEntryData& a_item)
		{
			RE::GFxValue state;
			if (!a_item.extraLists || !a_row.GetMember("equipState", &state) || (state.IsNumber() && state.GetNumber() != 0.0)) {
				return;
			}
			bool right = false;
			bool left = false;
			for (const auto* list : *a_item.extraLists) {
				right = right || (list && list->HasType(RE::ExtraDataType::kWorn));
				left = left || (list && list->HasType(RE::ExtraDataType::kWornLeft));
			}
			if (!right && !left) {
				return;
			}
			double mark = 1.0;
			if (const auto* weapon = a_item.object->As<RE::TESObjectWEAP>()) {
				const bool twoHanded = !weapon->IsOneHandedSword() && !weapon->IsOneHandedDagger() && !weapon->IsOneHandedAxe() && !weapon->IsOneHandedMace() &&
				                       !weapon->IsStaff() && !weapon->IsHandToHandMelee();
				mark = right && (left || twoHanded) ? 4.0 : (right ? 3.0 : 2.0);
			} else if (a_item.object->Is(RE::FormType::Light)) {
				mark = 2.0;
			}
			a_row.SetMember("equipState", RE::GFxValue{ mark });
		}

		void Tell(RE::GFxMovieView* a_view, RE::GFxValue* a_row, RE::InventoryEntryData* a_item)
		{
			// SKSE hands over the rows of the favorites and crafting menus too, the panel isn't in them
			if (!a_view || !a_row || !a_item || !a_item->object || !Inject::HoldsPanel(*a_view)) {
				return;
			}
			static const auto work = Profile::Work("item rows told what the panel shows on them");
			const Profile::Timer timer{ work };
			IR_NOTE(timer, "item {:08X}", a_item->object->GetFormID());
#if IR_DEBUG_EDITION
			const auto start = std::chrono::steady_clock::now();
			toldIn = Debug::Script::KindOf(a_view);
#endif
			// which menus the rows come from, once a menu, for reports of trouble
			static RE::GFxMovieView* toldOf = nullptr;
			if (std::exchange(toldOf, a_view) != a_view) {
				const auto* movie = a_view->GetMovieDef();
				logger::info("rows of {} get what the panel shows on them", movie && movie->GetFileURL() ? movie->GetFileURL() : "a menu");
			}
			if (Subject::Other()) {
				MarkWorn(*a_row, *a_item);
			}
			const auto form = a_item->object->GetFormID();
			const auto number = Acquired::NumberOf(form);
			a_row->SetMember("equipmentAdded", RE::GFxValue{ static_cast<double>(number) });
			a_row->SetMember("equipmentNew", RE::GFxValue{ number > 0 && Acquired::IsNew(number) });
			a_row->SetMember("equipmentJunk", RE::GFxValue{ Junk::Has(form) });
			a_row->SetMember("equipmentCarried", RE::GFxValue{ static_cast<double>(Carried(form)) });
			a_row->SetMember("equipmentStolen", RE::GFxValue{ static_cast<double>(Stolen(*a_item)) });
			if (const char* origin = Origin(*a_item->object)) {
				RE::GFxValue name;
				a_view->CreateString(&name, origin);
				a_row->SetMember("equipmentOrigin", name);
			}
			// with the sign and the line both switched off nothing of the quests and their scripts is read
			if (Settings::Get().ReadsQuests()) {
#if IR_DEBUG_EDITION
				const auto quests = std::chrono::steady_clock::now();
				Quests::Tell(*a_view, *a_row, *a_item);
				questNanoseconds += Since(quests);
#else
				Quests::Tell(*a_view, *a_row, *a_item);
#endif
			}
#if IR_DEBUG_EDITION
			rowsNanoseconds += Since(start);
			++rowsTold;
#endif
		}
	}

	const char* Origin(const RE::TESForm& a_form)
	{
		const auto* file = a_form.GetFile(0);
		if (!file) {
			return nullptr;
		}
		const auto name = file->GetFilename();
		for (const auto game : kGameFiles) {
			if (_strnicmp(name.data(), game.data(), game.size()) == 0 && name.size() == game.size()) {
				return nullptr;
			}
		}
		return file->fileName;
	}

	void Install()
	{
		SKSE::GetScaleformInterface()->Register(Tell);
	}

	void Listen()
	{
		static Moves moves;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESContainerChangedEvent>(&moves);
	}

	void MenuOpened()
	{
		{
			const std::scoped_lock lock{ guard };
			counted = false;
		}
		Quests::Refresh();
	}

#if IR_DEBUG_EDITION
	void TellFill()
	{
		const auto told = rowsTold.exchange(0);
		if (told == 0) {
			return;
		}
		logger::info("rows of the {} got what the panel shows on them: {} rows in {:.1f} ms, {:.1f} ms of it on quest marks, the player's counts read {} times",
			toldIn.load(), told, static_cast<double>(rowsNanoseconds.exchange(0)) / 1e6, static_cast<double>(questNanoseconds.exchange(0)) / 1e6, recounts.exchange(0));
	}
#endif
}
