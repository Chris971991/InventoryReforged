#include "PCH.h"

#include "Keep.h"

#include "Subject.h"

namespace Keep
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		constexpr std::uint32_t kVersion = 1;
		constexpr std::size_t kMostActors = 64;
		constexpr std::size_t kMostPieces = 32;
		// a change by someone else settles this long before the choice goes back, a script that dresses in a few steps
		// is done by then
		constexpr auto kSettle = 1s;
		// a change back this soon after the choice went back is a fight, after this many in a row the panel leaves it
		constexpr auto kFightWindow = 5s;
		constexpr int kFightsToLeave = 3;
		constexpr RE::FormID kLeftHand = 0x13F43;
		constexpr RE::FormID kRightHand = 0x13F42;

		struct On
		{
			RE::FormID form;
			bool left;
		};

		struct Kept
		{
			std::vector<On> on;
			std::vector<RE::FormID> off;
			std::optional<Clock::time_point> owedSince;
			Clock::time_point restoredAt{};
			int fights = 0;
			bool leftAlone = false;
		};

		std::mutex keptLock;
		std::unordered_map<RE::FormID, Kept> kept;
		std::atomic<bool> any{ false };
		// the actor whose gear the panel puts back right now, its own equip events are no change by someone else
		std::atomic<RE::FormID> restoring{ 0 };

		bool Gear(const RE::TESForm* a_form)
		{
			return a_form && (a_form->Is(RE::FormType::Armor) || a_form->Is(RE::FormType::Weapon) || a_form->Is(RE::FormType::Ammo) || a_form->Is(RE::FormType::Light));
		}

		std::uint32_t Parts(const RE::TESForm* a_form)
		{
			const auto* armor = a_form ? a_form->As<RE::TESObjectARMO>() : nullptr;
			return armor && !armor->IsShield() ? armor->GetSlotMask().underlying() : 0;
		}

		bool BothHands(const RE::TESForm* a_form)
		{
			const auto* weapon = a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr;
			return weapon && !weapon->IsOneHandedSword() && !weapon->IsOneHandedDagger() && !weapon->IsOneHandedAxe() && !weapon->IsOneHandedMace() &&
			       !weapon->IsStaff() && !weapon->IsHandToHandMelee();
		}

		// what a hand holds: a weapon in its hand, a shield or a torch in the left one
		bool InLeft(const RE::TESForm* a_form, bool a_left)
		{
			const auto* armor = a_form ? a_form->As<RE::TESObjectARMO>() : nullptr;
			return (armor && armor->IsShield()) || (a_form && a_form->Is(RE::FormType::Light)) || (a_form && a_form->Is(RE::FormType::Weapon) && a_left);
		}

		bool Held(const RE::TESForm* a_form)
		{
			const auto* armor = a_form ? a_form->As<RE::TESObjectARMO>() : nullptr;
			return (armor && armor->IsShield()) || (a_form && (a_form->Is(RE::FormType::Weapon) || a_form->Is(RE::FormType::Light)));
		}

		// whether two pieces on would take the same place: body slots for armor, a hand for what is held, ammo for ammo
		bool SamePlace(const On& a_one, const On& a_other)
		{
			const auto* one = RE::TESForm::LookupByID(a_one.form);
			const auto* other = RE::TESForm::LookupByID(a_other.form);
			if (!one || !other) {
				return false;
			}
			if (one->Is(RE::FormType::Ammo) || other->Is(RE::FormType::Ammo)) {
				return one->Is(RE::FormType::Ammo) && other->Is(RE::FormType::Ammo);
			}
			if (Held(one) && Held(other)) {
				return BothHands(one) || BothHands(other) || InLeft(one, a_one.left) == InLeft(other, a_other.left);
			}
			return (Parts(one) & Parts(other)) != 0;
		}

		RE::InventoryEntryData* Entry(RE::Actor& a_actor, const RE::TESBoundObject& a_object)
		{
			auto* changes = a_actor.GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return nullptr;
			}
			for (auto* entry : *changes->entryList) {
				if (entry && entry->object == &a_object) {
					return entry;
				}
			}
			return nullptr;
		}

		std::int32_t Count(RE::Actor& a_actor, RE::TESBoundObject& a_object)
		{
			const auto counts = a_actor.GetInventory([&](RE::TESBoundObject& a_item) { return &a_item == &a_object; });
			const auto found = counts.find(&a_object);
			return found != counts.end() ? found->second.first : 0;
		}

		bool WornIn(const RE::ExtraDataList& a_list, bool a_left)
		{
			return a_list.HasType(a_left ? RE::ExtraDataType::kWornLeft : RE::ExtraDataType::kWorn);
		}

		bool Worn(const RE::ExtraDataList& a_list)
		{
			return a_list.HasType(RE::ExtraDataType::kWorn) || a_list.HasType(RE::ExtraDataType::kWornLeft);
		}

		const RE::BGSEquipSlot* HandOf(const RE::TESBoundObject& a_object, bool a_left)
		{
			return a_object.Is(RE::FormType::Weapon) ? RE::TESForm::LookupByID<RE::BGSEquipSlot>(a_left ? kLeftHand : kRightHand) : nullptr;
		}

		// the copy that is worn where the choice wants it, and one that isn't worn at all
		std::pair<RE::ExtraDataList*, RE::ExtraDataList*> Copies(RE::InventoryEntryData* a_entry, bool a_weapon, bool a_left)
		{
			RE::ExtraDataList* worn = nullptr;
			RE::ExtraDataList* free = nullptr;
			if (a_entry && a_entry->extraLists) {
				for (auto* list : *a_entry->extraLists) {
					if (!list) {
						continue;
					}
					if (a_weapon ? WornIn(*list, a_left) : Worn(*list)) {
						worn = worn ? worn : list;
					} else if (!Worn(*list)) {
						free = free ? free : list;
					}
				}
			}
			return { worn, free };
		}

		// Puts the follower's gear back the way the player chose it. True when anything changed
		bool Restore(RE::Actor& a_actor, Kept& a_kept)
		{
			auto* equip = RE::ActorEquipManager::GetSingleton();
			bool changed = false;
			restoring = a_actor.GetFormID();
			for (const auto form : a_kept.off) {
				auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(form);
				auto* entry = object ? Entry(a_actor, *object) : nullptr;
				if (!entry || !entry->extraLists) {
					continue;
				}
				for (auto* list : *entry->extraLists) {
					if (list && Worn(*list)) {
						const bool left = list->HasType(RE::ExtraDataType::kWornLeft);
						const auto count = static_cast<std::uint32_t>(object->Is(RE::FormType::Ammo) ? std::max<std::int32_t>(list->GetCount(), 1) : 1);
						IR_BRACKET("unequip {:08X} {} from {:08X} the way the player chose, list {}", form, object->GetName(), a_actor.GetFormID(), static_cast<const void*>(list));
						equip->UnequipObject(&a_actor, object, list, count, HandOf(*object, left), false, true, false, true);
						logger::info("{} went back off {}, the player took it off", object->GetName(), a_actor.GetName());
						changed = true;
						break;
					}
				}
			}
			std::erase_if(a_kept.on, [&](const On& a_on) {
				auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(a_on.form);
				if (!object || Count(a_actor, *object) <= 0) {
					logger::info("{:08X} is no longer with {}, the panel doesn't keep it on", a_on.form, a_actor.GetName());
					return true;
				}
				const bool weapon = object->Is(RE::FormType::Weapon);
				auto* entry = Entry(a_actor, *object);
				const auto [worn, free] = Copies(entry, weapon, a_on.left);
				if (worn) {
					return false;
				}
				const auto count = static_cast<std::uint32_t>(object->Is(RE::FormType::Ammo) ? std::max<std::int32_t>(Count(a_actor, *object), 1) : 1);
				IR_BRACKET("equip {:08X} {} on {:08X} the way the player chose, list {}", a_on.form, object->GetName(), a_actor.GetFormID(), static_cast<const void*>(free));
				equip->EquipObject(&a_actor, object, free, count, HandOf(*object, a_on.left), false, true, false, true);
				logger::info("{} went back on {}, the player put it on", object->GetName(), a_actor.GetName());
				changed = true;
				return false;
			});
			restoring = 0;
			return changed;
		}

		bool ItemMenuOpen()
		{
			auto* ui = RE::UI::GetSingleton();
			return ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) ||
				ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) || ui->IsMenuOpen(RE::GiftMenu::MENU_NAME);
		}

		void Owe(RE::FormID a_actor)
		{
			const auto now = Clock::now();
			const std::scoped_lock lock{ keptLock };
			const auto found = kept.find(a_actor);
			if (found == kept.end() || found->second.leftAlone) {
				return;
			}
			auto& one = found->second;
			if (one.restoredAt == Clock::time_point{} || now - one.restoredAt >= kFightWindow) {
				one.fights = 0;
			} else if (++one.fights >= kFightsToLeave) {
				one.leftAlone = true;
				one.owedSince.reset();
				const auto* actor = RE::TESForm::LookupByID(a_actor);
				logger::warn("another mod changes the gear of {} back each time, the panel leaves it until the player chooses again", actor ? actor->GetName() : "");
				return;
			}
			one.owedSince = now;
		}

		struct Equips : RE::BSTEventSink<RE::TESEquipEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
			{
				if (!any.load() || !a_event || !a_event->actor || a_event->actor->IsPlayerRef()) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto actor = a_event->actor->GetFormID();
				if (actor == restoring.load() || !Gear(RE::TESForm::LookupByID(a_event->baseObject))) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Owe(actor);
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// the body built anew in a new cell or after a load, the game dresses it in its outfit then
		struct Loads : RE::BSTEventSink<RE::TESObjectLoadedEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
			{
				if (any.load() && a_event && a_event->loaded) {
					Owe(a_event->formID);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Chosen(RE::Actor& a_actor, RE::TESBoundObject& a_object, bool a_on, bool a_left)
	{
		if (a_actor.IsPlayerRef() || !Gear(&a_object)) {
			return;
		}
		const auto form = a_object.GetFormID();
		const std::scoped_lock lock{ keptLock };
		auto found = kept.find(a_actor.GetFormID());
		if (found == kept.end()) {
			if (kept.size() >= kMostActors) {
				logger::warn("the panel keeps the gear of {} followers already, not of {}", kMostActors, a_actor.GetName());
				return;
			}
			found = kept.emplace(a_actor.GetFormID(), Kept{}).first;
		}
		auto& one = found->second;
		const On chosen{ form, a_left };
		if (a_on) {
			std::erase(one.off, form);
			std::erase_if(one.on, [&](const On& a_other) { return a_other.form == form || SamePlace(a_other, chosen); });
			if (one.on.size() < kMostPieces) {
				one.on.push_back(chosen);
			}
		} else {
			std::erase_if(one.on, [&](const On& a_other) { return a_other.form == form; });
			if (std::ranges::find(one.off, form) == one.off.end() && one.off.size() < kMostPieces) {
				one.off.push_back(form);
			}
		}
		// a new choice ends a fight the panel left, and what changed by the trade itself is no change to undo
		one.leftAlone = false;
		one.fights = 0;
		one.owedSince.reset();
		any = true;
	}

	void Listen()
	{
		static Equips equips;
		static Loads loads;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESEquipEvent>(&equips);
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESObjectLoadedEvent>(&loads);
	}

	void Frame()
	{
		if (!any.load() || ItemMenuOpen()) {
			return;
		}
		const auto now = Clock::now();
		std::vector<RE::FormID> due;
		{
			const std::scoped_lock lock{ keptLock };
			for (const auto& [actor, one] : kept) {
				if (one.owedSince && now - *one.owedSince >= kSettle) {
					due.push_back(actor);
				}
			}
		}
		for (const auto id : due) {
			auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
			if (!actor || !actor->Is3DLoaded() || actor->IsDead() || actor->IsDisabled() || Subject::ReadOnlyActor(id)) {
				continue;
			}
			const std::scoped_lock lock{ keptLock };
			auto found = kept.find(id);
			if (found == kept.end()) {
				continue;
			}
			found->second.owedSince.reset();
			if (Restore(*actor, found->second)) {
				found->second.restoredAt = now;
			}
		}
	}

	void Save(SKSE::SerializationInterface* a_save)
	{
		const std::scoped_lock lock{ keptLock };
		if (kept.empty()) {
			return;
		}
		if (!a_save->OpenRecord(kRecord, kVersion)) {
			logger::error("the gear the player chose for followers could not be saved");
			return;
		}
		a_save->WriteRecordData(static_cast<std::uint32_t>(kept.size()));
		for (const auto& [actor, one] : kept) {
			a_save->WriteRecordData(actor);
			a_save->WriteRecordData(static_cast<std::uint32_t>(one.on.size()));
			for (const auto& on : one.on) {
				a_save->WriteRecordData(on.form);
				a_save->WriteRecordData(static_cast<std::uint8_t>(on.left ? 1 : 0));
			}
			a_save->WriteRecordData(static_cast<std::uint32_t>(one.off.size()));
			for (const auto form : one.off) {
				a_save->WriteRecordData(form);
			}
		}
	}

	void Read(SKSE::SerializationInterface* a_save, std::uint32_t a_version)
	{
		std::uint32_t actors = 0;
		if (a_version != kVersion || !a_save->ReadRecordData(actors)) {
			return;
		}
		std::unordered_map<RE::FormID, Kept> read;
		for (std::uint32_t at = 0; at < actors && at < kMostActors; ++at) {
			RE::FormID saved = 0;
			std::uint32_t count = 0;
			Kept one;
			if (!a_save->ReadRecordData(saved) || !a_save->ReadRecordData(count)) {
				break;
			}
			// every piece is read so the next one starts in its place, a mod taken out lets its pieces go
			for (std::uint32_t piece = 0; piece < count; ++piece) {
				RE::FormID form = 0;
				std::uint8_t left = 0;
				RE::FormID now = 0;
				// a record cut short ends the reading, the count of a broken one could be anything
				if (!a_save->ReadRecordData(form) || !a_save->ReadRecordData(left)) {
					break;
				}
				if (a_save->ResolveFormID(form, now) && one.on.size() < kMostPieces) {
					one.on.push_back({ now, left != 0 });
				}
			}
			if (!a_save->ReadRecordData(count)) {
				break;
			}
			for (std::uint32_t piece = 0; piece < count; ++piece) {
				RE::FormID form = 0;
				RE::FormID now = 0;
				if (!a_save->ReadRecordData(form)) {
					break;
				}
				if (a_save->ResolveFormID(form, now) && one.off.size() < kMostPieces) {
					one.off.push_back(now);
				}
			}
			RE::FormID actor = 0;
			if (a_save->ResolveFormID(saved, actor) && (!one.on.empty() || !one.off.empty())) {
				// the game dresses everyone anew as the save loads, the choices go back on once it settles
				one.owedSince = Clock::now();
				read.emplace(actor, std::move(one));
			}
		}
		const std::scoped_lock lock{ keptLock };
		kept = std::move(read);
		any = !kept.empty();
		logger::info("the gear the player chose is kept for {} followers, read from the save", kept.size());
	}

	void Forget()
	{
		const std::scoped_lock lock{ keptLock };
		kept.clear();
		any = false;
	}
}
