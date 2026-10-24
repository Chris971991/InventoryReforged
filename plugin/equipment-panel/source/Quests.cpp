#include "PCH.h"

#include "Quests.h"

#include <cctype>

#include "Holders.h"
#include "Places.h"
#include "Profile.h"
#include "SafeRead.h"
#include "Settings.h"

// windows.h renames GetObject to GetObjectW, a script variable has a GetObject of its own
#undef GetObject

namespace Quests
{
	namespace
	{
		enum Kind : int
		{
			kNone = 0,
			kKept = 1,
			kFollowed = 2,
			kNamed = 3,
			kDone = 4,
			kPointed = 5,
			kLater = 6,
			kOver = 7
		};

		// which kind wins when an item has several, what holds the item itself first
		constexpr std::array kRank{ kKept, kFollowed, kNamed, kPointed, kLater, kDone, kOver };

		int RankOf(int a_kind)
		{
			const auto at = std::ranges::find(kRank, a_kind);
			return at == kRank.end() ? static_cast<int>(kRank.size()) : static_cast<int>(at - kRank.begin());
		}

		// an objective in small letters to look in, and as the journal writes it, the same length
		struct Asked
		{
			const RE::TESQuest* quest;
			std::string text;
			std::string written;
		};

		// a quest that points at an item, and the reference its alias is forced on when there is one
		struct Pointer
		{
			const RE::TESQuest* quest;
			RE::FormID placed;
		};

		using Pointers = std::unordered_map<RE::FormID, std::vector<Pointer>>;

		std::mutex guard;
		std::vector<Asked> asked;
		// what the quests point at, put in whole once the reading after a load is done
		Pointers pointers;
		bool pointersRead = false;
		// the markers of the map are read once a game, a place is named only after that
		bool placesReady = false;
		bool toldUnread = false;

		std::string Lower(std::string_view a_text)
		{
			std::string lower(a_text);
			for (auto& letter : lower) {
				letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
			}
			return lower;
		}

		bool Live(const RE::TESQuest* a_quest)
		{
			return a_quest && const_cast<RE::TESQuest*>(a_quest)->IsRunning() && !a_quest->IsCompleted();
		}

		// a quest that isn't running either never began or is behind the player, a stage it reached tells which
		bool Begun(const RE::TESQuest& a_quest)
		{
			return a_quest.IsCompleted() || a_quest.currentStage > 0 || a_quest.alreadyRun;
		}

		bool Letter(char a_letter)
		{
			return std::isalnum(static_cast<unsigned char>(a_letter)) != 0;
		}

		// the name as whole words, or with an s or es after it as a plural says it
		// The journal writes the names of things with capitals. A capital word right before the name, not the first
		// word of the objective, or right after it makes the name part of a longer one: Flawless Sapphire, King Olaf
		bool PartOfLonger(const std::string& a_written, std::size_t a_at, std::size_t a_end)
		{
			const auto capital = [&](std::size_t a_letter) { return a_letter < a_written.size() && std::isupper(static_cast<unsigned char>(a_written[a_letter])) != 0; };
			if (a_at >= 2 && a_written[a_at - 1] == ' ') {
				auto start = a_at - 1;
				while (start > 0 && Letter(a_written[start - 1])) {
					start--;
				}
				if (start > 0 && start < a_at - 1 && capital(start)) {
					return true;
				}
			}
			return a_end + 1 < a_written.size() && a_written[a_end] == ' ' && capital(a_end + 1);
		}

		bool Names(const Asked& a_asked, const std::string& a_name)
		{
			const auto& text = a_asked.text;
			if (a_name.size() < 3) {
				return false;
			}
			for (auto at = text.find(a_name); at != std::string::npos; at = text.find(a_name, at + 1)) {
				if (at > 0 && Letter(text[at - 1])) {
					continue;
				}
				auto end = at + a_name.size();
				if (end < text.size() && text[end] == 'e' && end + 1 < text.size() && text[end + 1] == 's') {
					end += 2;
				} else if (end < text.size() && text[end] == 's') {
					end += 1;
				}
				if ((end >= text.size() || !Letter(text[end])) && !PartOfLonger(a_asked.written, at, end)) {
					return true;
				}
			}
			return false;
		}

		// an item the player can carry, by itself or by a reference of it placed in the world
		const RE::TESForm* ItemOf(const RE::TESForm* a_form)
		{
			if (const auto* reference = a_form ? a_form->As<RE::TESObjectREFR>() : nullptr) {
				a_form = reference->GetBaseObject();
			}
			return a_form && a_form->IsInventoryObject() && a_form->GetFormType() != RE::FormType::Light ? a_form : nullptr;
		}

		// The reading after a load goes a few quests a frame on the main thread, so no frame and no menu waits on it and
		// a lock of the game it takes is held for one quest's scripts at a time. Only the main thread touches it
		using Clock = std::chrono::steady_clock;
		constexpr auto kFrameBudget = std::chrono::microseconds(1000);

		struct Reading
		{
			std::size_t next = 0;
			Pointers found;
			// how many ties each way of pointing gave, and the forced aliases that held no reference, for the log
			std::array<std::size_t, 3> by{};
			std::size_t emptyForced = 0;
			std::size_t frames = 0;
			Clock::duration spent{};
		};
		std::optional<Reading> reading;
		std::atomic<bool> readingOwed{ true };
		// Quests that start with a game fill their aliases some seconds after a load, the reading waits for them. It's
		// read again after every item menu, what began meanwhile comes in, the last reading serves until then
		constexpr auto kSettle = std::chrono::seconds(5);
		std::atomic<Clock::rep> readNotBefore{ 0 };
		bool placesAsked = false;
		bool placesOwed = false;
		std::size_t toldItems = 0;

		void Point(Reading& a_reading, const RE::TESForm* a_form, const RE::TESQuest& a_quest, std::size_t a_way)
		{
			const auto* item = ItemOf(a_form);
			if (!item) {
				return;
			}
			a_reading.by[a_way]++;
			const auto* reference = a_form->As<RE::TESObjectREFR>();
			auto& found = a_reading.found[item->GetFormID()];
			if (std::ranges::none_of(found, [&](const Pointer& a_one) { return a_one.quest == &a_quest; })) {
				found.push_back({ &a_quest, reference ? reference->GetFormID() : 0 });
			}
		}

		// The variables of the scripts are read as they lie, their type and then their value, eight bytes each. The
		// scripts run on threads of their own while a menu builds its rows, and a copy of the smart pointer GetObject
		// gives would count a reference on their object from here. A script of another mod may also hold anything
		// there: ZaZ Animation Pack's Config quest had an "object" of 0xA00000006 in an array, and reading through it
		// crashed the game on the inventory (a player's crash log, 1.2.1). So nothing is read through a value that
		// can't be a pointer, and every read of their memory that faults gives nothing back. Under the lock of the
		// scripts only the handles get copied, the forms of them are looked up after it, so that lock never waits on
		// the lock of the game's forms
		constexpr std::size_t kVariables = 0x30;
		constexpr std::size_t kVariableSize = 0x10;
		constexpr std::size_t kObjectHandle = 0x20;
		constexpr std::size_t kArraySize = 0x10;
		constexpr std::size_t kArrayElements = 0x20;
		// Papyrus arrays hold 128 at most, the script extender's helpers make longer ones
		constexpr std::uint64_t kMostElements = 4096;
		constexpr std::uint32_t kMostVariables = 1024;
		// values read for one quest at most, so the lock is held a short while whatever a script holds
		constexpr std::size_t kMostValues = 16384;

		// a bare __try read would use up a guard page quietly, SafeRead leaves those alone
		bool Peek(std::uintptr_t a_at, std::uint64_t& a_value, SafeRead::Regions& a_regions)
		{
			return SafeRead::Copy(&a_value, a_at, sizeof(a_value), a_regions);
		}

		bool Pointer(std::uint64_t a_value)
		{
			constexpr std::uint64_t kLowest = 0x10000;
			constexpr std::uint64_t kHighest = 0x00007FFF'FFFFFFFF;
			return a_value >= kLowest && a_value < kHighest && a_value % alignof(void*) == 0;
		}

		// the handle of an object a variable holds, or of each object an array of them holds
		void CopyHandles(std::uintptr_t a_variable, std::vector<std::uint64_t>& a_handles, bool a_inArray, std::size_t& a_read, SafeRead::Regions& a_regions)
		{
			std::uint64_t raw = 0;
			std::uint64_t value = 0;
			if (a_read >= kMostValues || !Peek(a_variable, raw, a_regions) || !Peek(a_variable + sizeof(void*), value, a_regions) || !Pointer(value)) {
				return;
			}
			a_read++;
			const RE::BSScript::TypeInfo type{ static_cast<RE::BSScript::TypeInfo::RawType>(raw) };
			if (type.IsObject()) {
				// a handle of the game's forms carries the form id in its low half, read without the lock the game's
				// GetHandle takes on the object
				std::uint64_t handle = 0;
				if (Peek(value + kObjectHandle, handle, a_regions)) {
					a_handles.push_back(handle);
				}
			} else if (type.IsObjectArray() && !a_inArray) {
				std::uint64_t size = 0;
				if (!Peek(value + kArraySize, size, a_regions)) {
					return;
				}
				const auto count = std::min<std::uint64_t>(size & 0xFFFFFFFF, kMostElements);
				for (std::uint64_t i = 0; i < count && a_read < kMostValues; ++i) {
					CopyHandles(value + kArrayElements + i * kVariableSize, a_handles, true, a_read, a_regions);
				}
			}
		}

		// called under the lock of the scripts
		void CopyScripts(RE::BSScript::Internal::VirtualMachine& a_machine, RE::VMHandle a_handle, std::vector<std::uint64_t>& a_handles, std::size_t& a_read,
			SafeRead::Regions& a_regions)
		{
			const auto found = a_machine.attachedScripts.find(a_handle);
			if (found == a_machine.attachedScripts.end()) {
				return;
			}
			for (std::uint32_t s = 0; s < found->second.size(); ++s) {
				const auto* script = found->second[s].get();
				// an object the scripts haven't finished making holds no values yet
				if (!script || !script->constructed || !script->initialized || !script->valid) {
					continue;
				}
				const auto* type = script->GetTypeInfo();
				if (!type) {
					continue;
				}
				const auto variables = reinterpret_cast<std::uintptr_t>(script) + kVariables;
				const auto count = std::min(type->GetTotalNumVariables(), kMostVariables);
				for (std::uint32_t v = 0; v < count && a_read < kMostValues; ++v) {
					CopyHandles(variables + v * kVariableSize, a_handles, false, a_read, a_regions);
				}
			}
		}

		// what one quest points at: the forced and made references of its aliases, the objects its scripts and the
		// scripts of its aliases hold
		void ReadQuest(Reading& a_reading, RE::BSScript::Internal::VirtualMachine* a_machine, RE::BSScript::IObjectHandlePolicy* a_policy, const RE::TESQuest& a_quest)
		{
			std::vector<RE::VMHandle> attached;
			if (a_policy) {
				attached.push_back(a_policy->GetHandleForObject(RE::FormType::Quest, &a_quest));
			}
			for (auto* alias : a_quest.aliases) {
				if (!alias || alias->GetVMTypeID() != RE::BGSRefAlias::VMTYPEID) {
					continue;
				}
				const auto* reference = static_cast<RE::BGSRefAlias*>(alias);
				using Fill = RE::BGSBaseAlias::FILL_TYPE;
				if (alias->fillType.get() == Fill::kForced) {
					const auto forced = reference->fillData.forced.forcedRef.get();
					a_reading.emptyForced += forced ? 0 : 1;
					Point(a_reading, forced.get(), a_quest, 0);
				} else if (alias->fillType.get() == Fill::kCreated) {
					Point(a_reading, reference->fillData.created.object, a_quest, 1);
				}
				if (a_policy) {
					attached.push_back(a_policy->GetHandleForObject(RE::BGSRefAlias::VMTYPEID, alias));
				}
			}
			if (!a_machine) {
				return;
			}
			std::vector<std::uint64_t> held;
			std::size_t read = 0;
			// what Windows says of the memory holds for this one look under the lock
			SafeRead::Regions regions;
			{
				const RE::BSSpinLockGuard locker{ a_machine->attachedScriptsLock };
				for (const auto handle : attached) {
					CopyScripts(*a_machine, handle, held, read, regions);
				}
			}
			for (const auto handle : held) {
				Point(a_reading, RE::TESForm::LookupByID(static_cast<RE::FormID>(handle & 0xFFFFFFFF)), a_quest, 2);
			}
		}

		// The game fills the rows of a menu while it builds the menu, before the menu says it opened, so the rows ask
		// for the quests themselves when they are old. Called under the guard
		bool stale = true;

		void ReadObjectives()
		{
			asked.clear();
			for (auto* quest : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESQuest>()) {
				if (!Live(quest)) {
					continue;
				}
				for (const auto* objective : quest->objectives) {
					if (objective && objective->state.get() == RE::QUEST_OBJECTIVE_STATE::kDisplayed && !objective->displayText.empty()) {
						asked.push_back({ quest, Lower(objective->displayText.c_str()), objective->displayText.c_str() });
					}
				}
			}
			stale = false;
		}

		struct Found
		{
			const RE::TESQuest* quest = nullptr;
			int kind = kNone;
			RE::FormID placed = 0;

			void Take(const RE::TESQuest* a_quest, int a_kind, RE::FormID a_placed = 0)
			{
				if (kind == kNone || RankOf(a_kind) < RankOf(kind)) {
					quest = a_quest;
					kind = a_kind;
					placed = a_placed;
				}
			}
		};

		void ByAliases(Found& a_found, RE::InventoryEntryData& a_item)
		{
			if (!a_item.extraLists) {
				return;
			}
			for (auto* list : *a_item.extraLists) {
				const auto* aliases = list ? list->GetByType<RE::ExtraAliasInstanceArray>() : nullptr;
				if (!aliases) {
					continue;
				}
				RE::BSReadLockGuard lock{ aliases->lock };
				for (const auto* one : aliases->aliases) {
					if (!one || !one->quest || !one->alias) {
						continue;
					}
					if (one->alias->flags.any(RE::BGSBaseAlias::FLAGS::kQuestObject)) {
						a_found.Take(one->quest, Live(one->quest) ? kKept : kDone);
					} else if (Live(one->quest)) {
						a_found.Take(one->quest, kFollowed);
					}
				}
			}
		}

		void ByWords(Found& a_found, const RE::TESBoundObject& a_object)
		{
			const char* name = a_object.GetName();
			if (!name || !*name) {
				return;
			}
			const auto lower = Lower(name);
			for (const auto& one : asked) {
				if (Names(one, lower)) {
					a_found.Take(one.quest, kNamed);
					return;
				}
			}
		}

		// Quests point at gold and iron swords too, to hand them out as rewards. Only a thing nobody sells, carries,
		// makes or deals out in loot is the quest's own, a dragon claw or a letter
		void ByPointers(Found& a_found, const RE::TESBoundObject& a_object)
		{
			const auto found = pointers.find(a_object.GetFormID());
			if (found == pointers.end()) {
				return;
			}
			if (!Holders::OnlyPlaced(a_object)) {
				return;
			}
			for (const auto& one : found->second) {
				// many quests run from the start of a game at stage 0 and wait for the player, they haven't begun for him
				const bool waits = Live(one.quest) && one.quest->currentStage == 0;
				a_found.Take(one.quest, waits ? kLater : (Live(one.quest) ? kPointed : (Begun(*one.quest) ? kOver : kLater)), one.placed);
			}
		}
	}

	void Refresh()
	{
		{
			const std::scoped_lock lock{ guard };
			stale = true;
		}
		readingOwed = true;
	}

	void Forget()
	{
		{
			const std::scoped_lock lock{ guard };
			pointers.clear();
			pointersRead = false;
			toldUnread = false;
			stale = true;
		}
		readNotBefore = (Clock::now() + kSettle).time_since_epoch().count();
		readingOwed = true;
		toldItems = 0;
	}

	void Frame()
	{
		// with the sign and the line both switched off nothing of the quests and their scripts is read
		if (!Settings::Get().ReadsQuests()) {
			return;
		}
		// the markers of the map name the place of a quest with no name of its own, read once a game after the first
		// reading, a share each frame
		if (placesOwed) {
			static const auto markers = Profile::Work("map markers");
			const Profile::Timer markersTimer{ markers };
			if (Places::ReadMarkersFor(kFrameBudget)) {
				placesOwed = false;
				const std::scoped_lock lock{ guard };
				placesReady = true;
			}
			return;
		}
		if (Clock::now().time_since_epoch().count() < readNotBefore.load()) {
			return;
		}
		if (readingOwed.exchange(false)) {
			reading.emplace();
		}
		if (!reading) {
			return;
		}
		static const auto work = Profile::Work("quest pointers");
		const Profile::Timer timer{ work };
		const auto start = Clock::now();
		const auto& quests = RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESQuest>();
		auto* machine = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		auto* policy = machine ? machine->GetObjectHandlePolicy1() : nullptr;
		while (reading->next < quests.size() && Clock::now() - start < kFrameBudget) {
			if (const auto* quest = quests[reading->next]) {
				IR_NOTE(timer, "quest {:08X} {}", quest->GetFormID(), quest->GetFormEditorID() ? quest->GetFormEditorID() : "");
				ReadQuest(*reading, machine, policy, *quest);
			}
			reading->next++;
		}
		reading->spent += Clock::now() - start;
		reading->frames++;
		if (reading->next < quests.size()) {
			return;
		}
		const auto by = reading->by;
		const auto emptyForced = reading->emptyForced;
		const auto frames = reading->frames;
		const double milliseconds = std::chrono::duration<double, std::milli>(reading->spent).count();
		std::size_t items = 0;
		{
			const std::scoped_lock lock{ guard };
			pointers = std::move(reading->found);
			pointersRead = true;
			items = pointers.size();
		}
		reading.reset();
		// told once after a load and again when it changed, it's read after every item menu
		if (std::exchange(toldItems, items) != items) {
			logger::info("quests point at {} items: {} by aliases on placed things, {} by aliases that make one, {} by properties of scripts; {} forced aliases hold no reference; read in {} frames, {:.1f} ms",
				items, by[0], by[1], by[2], emptyForced, frames, milliseconds);
		}
		placesOwed = !std::exchange(placesAsked, true);
	}

	void Tell(RE::GFxMovieView& a_view, RE::GFxValue& a_row, RE::InventoryEntryData& a_item)
	{
		Found found;
		ByAliases(found, a_item);
		if (a_item.object && (found.kind == kNone || found.kind == kDone)) {
			const std::scoped_lock lock{ guard };
			if (stale) {
				ReadObjectives();
			}
			ByWords(found, *a_item.object);
			if (pointersRead) {
				ByPointers(found, *a_item.object);
			} else if (!std::exchange(toldUnread, true)) {
				logger::info("what the quests point at is still being read, rows get marks from aliases and the journal only");
			}
		}
		if (found.kind == kNone) {
			return;
		}
		// a quest of a dungeon often has no name of its own, the place of what its alias holds names it then
		std::string name = found.quest && found.quest->GetName() ? found.quest->GetName() : "";
		bool place = false;
		if (name.empty() && found.placed != 0) {
			// the places read the map markers once and keep them, rows of two menus may come on two threads
			const std::scoped_lock lock{ guard };
			const auto* reference = placesReady ? RE::TESForm::LookupByID<RE::TESObjectREFR>(found.placed) : nullptr;
			if (reference) {
				name = Places::Of(*reference).name;
				place = !name.empty();
			}
		}
		a_row.SetMember("equipmentQuest", static_cast<double>(found.kind));
		RE::GFxValue shown;
		a_view.CreateString(&shown, name.c_str());
		a_row.SetMember("equipmentQuestName", shown);
		a_row.SetMember("equipmentQuestPlace", place);
	}
}
