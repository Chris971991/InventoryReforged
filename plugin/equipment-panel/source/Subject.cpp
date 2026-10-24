#include "PCH.h"

#include "Subject.h"

#include "Api.h"
#include "Hidden.h"
#include "Keep.h"
#include "Settings.h"

namespace Subject
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		constexpr RE::FormID kPersonKeyword = 0x13794;  // ActorTypeNPC
		constexpr RE::FormID kRightHand = 0x13F42;
		constexpr RE::FormID kLeftHand = 0x13F43;
		// rows filled anew at most this often, a fight under a running menu equips many times a second
		constexpr auto kFillEvery = std::chrono::milliseconds(100);
		constexpr auto kReadyScript = "_global.Equipment.refillReady";

		// the follower of the container menu up now, 0 for none, and their form id, which an event of any thread can
		// be told by without a lock of the game
		std::atomic<RE::RefHandle> shown{ 0 };
		std::atomic<RE::FormID> shownForm{ 0 };

		// What the menu still owes the follower's look: their body built anew, the rows filled anew. Set by the game's
		// events of equipping, which may come from any thread and with a lock of the game held, so they only set these
		// and the next frame of the menu asks for the work
		std::atomic<bool> bodyOwed{ false };
		std::atomic<bool> rowsOwed{ false };
		// an inventory over the trade closed, the trade makes its doll and slot models again in its next frame
		std::atomic<bool> sceneOwed{ false };
		std::atomic<Clock::rep> lastFill{ 0 };
		std::atomic<bool> toldWait{ false };
		// how many choices the mods were told of in this trade
		std::atomic<std::int32_t> told{ 0 };

		// The follower held for the whole trade: one a script deletes while it's up is still there for the last events
		std::mutex heldLock;
		RE::NiPointer<RE::Actor> held;

		// actors a mod wants shown but not dressed, kept in the save
		std::mutex readOnlyLock;
		std::set<RE::FormID> readOnly;
		constexpr std::size_t kMostReadOnly = 256;

		// A piece the player asked the panel to put on or take off waits for the game's word that it happened, then
		// the mods that listen hear of it. A piece the game refused never comes
		struct Choice
		{
			RE::FormID object;
			bool on;
			Clock::time_point at;
			bool left = false;
		};
		std::mutex choicesLock;
		std::vector<Choice> asked;
		std::vector<Choice> done;
		// the follower's worn gear as a gift from the player starts, their AI may put the gift on during the move
		std::set<RE::FormID> beforeGift;

		bool InventoryUp()
		{
			auto* ui = RE::UI::GetSingleton();
			return ui && ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME);
		}

		bool Gear(const RE::TESForm* a_form)
		{
			return a_form && (a_form->Is(RE::FormType::Armor) || a_form->Is(RE::FormType::Weapon) || a_form->Is(RE::FormType::Ammo) || a_form->Is(RE::FormType::Light));
		}

		bool BothHands(const RE::TESForm& a_form)
		{
			const auto* weapon = a_form.As<RE::TESObjectWEAP>();
			return weapon && !weapon->IsOneHandedSword() && !weapon->IsOneHandedDagger() && !weapon->IsOneHandedAxe() && !weapon->IsOneHandedMace() &&
			       !weapon->IsStaff() && !weapon->IsHandToHandMelee();
		}

		const RE::BGSEquipSlot* HandOf(const RE::TESBoundObject& a_object, bool a_left)
		{
			if (!a_object.Is(RE::FormType::Weapon)) {
				return nullptr;
			}
			return RE::TESForm::LookupByID<RE::BGSEquipSlot>(a_left ? kLeftHand : kRightHand);
		}

		bool WornIn(const RE::ExtraDataList& a_list, bool a_left)
		{
			return a_list.HasType(a_left ? RE::ExtraDataType::kWornLeft : RE::ExtraDataType::kWorn);
		}

		bool Worn(const RE::ExtraDataList& a_list)
		{
			return a_list.HasType(RE::ExtraDataType::kWorn) || a_list.HasType(RE::ExtraDataType::kWornLeft);
		}

		RE::InventoryEntryData* LiveEntry(RE::Actor& a_actor, const RE::TESBoundObject& a_object)
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

		std::set<RE::FormID> WornGear(RE::Actor& a_actor)
		{
			std::set<RE::FormID> worn;
			auto* changes = a_actor.GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return worn;
			}
			for (auto* entry : *changes->entryList) {
				if (!entry || !entry->object || !entry->extraLists || !entry->object->GetPlayable() || !Gear(entry->object)) {
					continue;
				}
				for (const auto* list : *entry->extraLists) {
					if (list && Worn(*list)) {
						worn.insert(entry->object->GetFormID());
					}
				}
			}
			return worn;
		}

		struct Held
		{
			RE::TESBoundObject* object;
			RE::ExtraDataList* list;
			bool left;
		};

		// What the new piece takes the place of: armor on any of its body slots, what's in its hand, and the other
		// hand too for a weapon of both hands, another copy of the same item too. A piece kept on by the panel or by a
		// script is never pushed off by the game, it comes off by hand first. a_going is the copy that goes on
		std::vector<Held> InTheWay(RE::Actor& a_actor, RE::TESBoundObject& a_object, const RE::ExtraDataList* a_going, bool a_left)
		{
			std::vector<Held> found;
			auto* changes = a_actor.GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return found;
			}
			const auto* armor = a_object.As<RE::TESObjectARMO>();
			const bool weapon = a_object.Is(RE::FormType::Weapon);
			const bool both = BothHands(a_object);
			const bool shield = armor && armor->IsShield();
			const bool light = a_object.Is(RE::FormType::Light);
			const bool leftHand = shield || light || (weapon && a_left);
			for (auto* entry : *changes->entryList) {
				// What can't be played with is a mod's own piece, a body of Devious Devices or the like: it has no row
				// to put it back by and its mod keeps it on, the game's own equip decides about it
				if (!entry || !entry->object || !entry->extraLists || !entry->object->GetPlayable()) {
					continue;
				}
				const auto* other = entry->object->As<RE::TESObjectARMO>();
				const bool otherWeapon = entry->object->Is(RE::FormType::Weapon);
				const bool otherBoth = BothHands(*entry->object);
				for (auto* list : *entry->extraLists) {
					if (!list || list == a_going) {
						continue;
					}
					const bool right = list->HasType(RE::ExtraDataType::kWorn);
					const bool left = list->HasType(RE::ExtraDataType::kWornLeft);
					bool clashes = false;
					if (armor && !shield) {
						clashes = other && right && (other->GetSlotMask().underlying() & armor->GetSlotMask().underlying()) != 0;
					} else if (a_object.Is(RE::FormType::Ammo)) {
						clashes = right && entry->object->Is(RE::FormType::Ammo);
					} else if (weapon || shield || light) {
						// a weapon and a torch in the left hand carry its mark, a shield there the plain worn mark
						const bool holdsLeft = left || (right && ((other && other->IsShield()) || entry->object->Is(RE::FormType::Light)));
						const bool holdsRight = right && otherWeapon;
						clashes = (leftHand && holdsLeft) || (!leftHand && holdsRight) || (both && (holdsLeft || holdsRight)) || (otherBoth && holdsRight);
					}
					if (clashes) {
						found.push_back({ entry->object, list, left });
					}
				}
			}
			return found;
		}

		class EquipWatch : public RE::BSTEventSink<RE::TESEquipEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
			{
				const auto form = shownForm.load();
				if (!a_event || form == 0 || !a_event->actor || a_event->actor->GetFormID() != form || !Gear(RE::TESForm::LookupByID(a_event->baseObject))) {
					return RE::BSEventNotifyControl::kContinue;
				}
				bodyOwed = true;
				rowsOwed = true;
				const std::scoped_lock lock{ choicesLock };
				const auto chosen = std::ranges::find_if(asked, [&](const Choice& a_choice) { return a_choice.object == a_event->baseObject && a_choice.on == a_event->equipped; });
				if (chosen != asked.end()) {
					done.push_back(*chosen);
					asked.erase(chosen);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// The rows are filled anew only while the menu script says nothing holds a row: no slider asking how many, no
		// drag, no menu of a row. The game's moves act on the selected row by its place, a fill under them would
		// hand them another row
		bool MenuReady(RE::IMenu& a_menu)
		{
			RE::GFxValue ready;
			IR_SCRIPT_QUIET(kReadyScript);
			return a_menu.uiMovie && a_menu.uiMovie->Invoke(kReadyScript, &ready, nullptr, 0) && ready.IsBool() && ready.GetBool();
		}
	}

	// An item menu loads. The inventory or a shop leave a trade under them as it is: a script's inventory over a
	// trade the game may never open, and the trade went on with the player's doll beside the follower's slots when
	// this ended it. A new container menu ends a trade still up first, with its events told
	RE::NiPointer<RE::Actor> Ask(bool a_container)
	{
		if (!a_container) {
			return Get();
		}
		Finish();
		if (!Settings::Get().followers || RE::ContainerMenu::GetContainerMode() != RE::ContainerMenu::ContainerMode::kNPCMode) {
			return Get();
		}
		const auto handle = RE::ContainerMenu::GetTargetRefHandle();
		const auto target = RE::TESObjectREFR::LookupByHandle(handle);
		auto* actor = target ? target->As<RE::Actor>() : nullptr;
		if (actor && !actor->IsPlayerRef() && Fits(*actor)) {
			{
				const std::scoped_lock lock{ heldLock };
				held.reset(actor);
			}
			shownForm = actor->GetFormID();
			shown = handle;
			logger::info("the panel shows {} in their trade", actor->GetName());
		}
		return Get();
	}

	void Closed()
	{
		shown = 0;
		shownForm = 0;
		bodyOwed = false;
		rowsOwed = false;
		toldWait = false;
		Abandon();
	}

	// A save loads while a trade is up: its last choices go nowhere, the new game's listeners never heard of them.
	// The trade itself is closed by its menu as the game closes every menu, that takes the doll and models away
	void Abandon()
	{
		told = 0;
		sceneOwed = false;
		{
			const std::scoped_lock lock{ choicesLock };
			asked.clear();
			done.clear();
			beforeGift.clear();
		}
		const std::scoped_lock lock{ heldLock };
		held.reset();
	}

	void Gift()
	{
		const auto actor = Other() ? Get() : nullptr;
		auto worn = actor ? WornGear(*actor) : std::set<RE::FormID>{};
		const std::scoped_lock lock{ choicesLock };
		beforeGift = std::move(worn);
	}

	// The trade closes: what the game did of the last choices is told first, then that the trade is over, so a mod
	// that keeps a set of gear saves it whole once. Told of the actor held since the trade opened
	void Finish()
	{
		RE::NiPointer<RE::Actor> actor;
		{
			const std::scoped_lock lock{ heldLock };
			actor = held;
		}
		if (actor && shown.load() != 0) {
			Tell();
			Api::Done(*actor, told.load() > 0);
		}
		Closed();
	}

	void SetReadOnly(RE::FormID a_actor, bool a_readOnly)
	{
		const std::scoped_lock lock{ readOnlyLock };
		if (!a_readOnly) {
			readOnly.erase(a_actor);
		} else if (readOnly.size() < kMostReadOnly) {
			readOnly.insert(a_actor);
		} else {
			static bool told = false;
			if (!std::exchange(told, true)) {
				logger::warn("SetReadOnly: {} actors are looked at only already, {:08X} and any after it are not taken", kMostReadOnly, a_actor);
			}
		}
	}

	bool ReadOnly()
	{
		return ReadOnlyActor(shownForm.load());
	}

	bool ReadOnlyActor(RE::FormID a_actor)
	{
		const std::scoped_lock lock{ readOnlyLock };
		return a_actor != 0 && readOnly.contains(a_actor);
	}

	bool IsShown(const RE::Actor& a_actor)
	{
		return shownForm.load() == a_actor.GetFormID();
	}

#if IR_DEBUG_EDITION
	RE::FormID ShownForm()
	{
		return shownForm.load();
	}
#endif

	// What goes into the save. An actor made in the game and deleted since stays out, a new one made later may get its
	// form id. One of a plugin stays whether found or not, its cell may only be unloaded now
	std::vector<RE::FormID> ReadOnlyActors()
	{
		const std::scoped_lock lock{ readOnlyLock };
		std::erase_if(readOnly, [](RE::FormID a_form) {
			if ((a_form >> 24) != 0xFF) {
				return false;
			}
			const auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_form);
			return !actor || actor->IsDeleted();
		});
		return { readOnly.begin(), readOnly.end() };
	}

	void ForgetReadOnly()
	{
		const std::scoped_lock lock{ readOnlyLock };
		readOnly.clear();
	}

	// A follower gone from the game while their trade is up (a script deleted them) leaves no one, never the player.
	// An inventory up over the trade is the player's
	RE::NiPointer<RE::Actor> Get()
	{
		if (const auto handle = shown.load(); handle != 0 && !InventoryUp()) {
			const auto target = RE::TESObjectREFR::LookupByHandle(handle);
			auto* actor = target ? target->As<RE::Actor>() : nullptr;
			return RE::NiPointer<RE::Actor>(actor && !actor->IsDeleted() ? actor : nullptr);
		}
		return RE::NiPointer<RE::Actor>(RE::PlayerCharacter::GetSingleton());
	}

	bool Other()
	{
		return shown.load() != 0 && !InventoryUp();
	}

	bool Trading()
	{
		return shown.load() != 0;
	}

	// A person, alive, there in the world with a body. One far away or disabled has no process the game equips
	// through, and parts of the character sheet are asked of loaded actors only. The game's fill of a trade's rows is
	// known by its number in game 1.6 and later only, the old game keeps its plain trade
	bool Fits(RE::Actor& a_actor)
	{
		static const auto* person = RE::TESForm::LookupByID<RE::BGSKeyword>(kPersonKeyword);
		const auto* race = a_actor.GetRace();
		// with its AI off it's a mannequin or a statue a mod posed, its own script dresses it and a body built anew
		// would lose the pose
		return REL::Module::IsAE() && person && race && race->HasKeyword(person) && !a_actor.IsDead() && !a_actor.IsDisabled() && a_actor.Is3DLoaded() &&
		       a_actor.IsAIEnabled();
	}

	bool Shows(RE::Actor& a_actor)
	{
		const auto& settings = Settings::Get();
		return settings.panel && settings.followers && !a_actor.IsPlayerRef() && Fits(a_actor);
	}

	void Listen()
	{
		static EquipWatch equips;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESEquipEvent>(&equips);
	}

	// the mods that listen hear what the game did of the player's choices
	void Tell()
	{
		std::vector<Choice> heard;
		{
			const std::scoped_lock lock{ choicesLock };
			heard.swap(done);
		}
		RE::NiPointer<RE::Actor> actor;
		{
			const std::scoped_lock lock{ heldLock };
			actor = held;
		}
		for (const auto& choice : heard) {
			if (auto* object = RE::TESForm::LookupByID(choice.object); actor && object) {
				Api::Worn(*actor, *object, choice.on);
				if (auto* bound = object->As<RE::TESBoundObject>()) {
					Keep::Chosen(*actor, *bound, choice.on, choice.left);
				}
				++told;
			}
		}
	}

	// Once a frame of the trade. The mods that listen hear what the game did of the player's choices. The body is
	// built anew and the rows filled anew as a task of the interface once the scripts of this frame are done. The
	// fill is the container menu's own, the one it runs when its view changes, both sides from the inventories. The
	// body only under a paused menu: the game puts a follower's new look on them in their own update, which waits
	// then, and under a running menu a body built anew mid swing would only cost
	void SceneAgain()
	{
		sceneOwed = true;
	}

	void BodyAgain()
	{
		bodyOwed = true;
	}

	bool Rebuildable(RE::Actor& a_actor)
	{
		const auto* state = a_actor.AsActorState();
		return a_actor.Is3DLoaded() && !a_actor.IsOnMount() && !a_actor.IsInRagdollState() && !a_actor.IsInKillMove() &&
		       (!state || (state->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal && state->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal && !state->IsBleedingOut()));
	}

	void Frame()
	{
		const auto handle = shown.load();
		if (handle == 0) {
			return;
		}
		if (sceneOwed.exchange(false)) {
			SKSE::GetTaskInterface()->AddUITask([] {
				if (const auto menu = RE::UI::GetSingleton()->GetMenu(RE::ContainerMenu::MENU_NAME); menu && menu->uiMovie) {
					IR_SCRIPT("_global.Equipment.sceneAgain");
					menu->uiMovie->Invoke("_global.Equipment.sceneAgain", nullptr, nullptr, 0);
				}
			});
		}
		const auto now = Clock::now();
		Tell();
		const bool paused = RE::UI::GetSingleton()->GameIsPaused();
		const bool body = bodyOwed.exchange(false) && paused;
		const bool soon = now - Clock::time_point(Clock::duration(lastFill.load())) < kFillEvery;
		const bool rows = !soon && rowsOwed.exchange(false);
		if (!body && !rows) {
			return;
		}
		if (rows) {
			lastFill = now.time_since_epoch().count();
		}
		SKSE::GetTaskInterface()->AddUITask([handle, body, rows] {
			if (shown.load() != handle) {
				return;
			}
			const auto target = RE::TESObjectREFR::LookupByHandle(handle);
			auto* actor = target ? target->As<RE::Actor>() : nullptr;
			if (actor && body && Rebuildable(*actor)) {
				IR_BRACKET("the body of {:08X} is built again for the trade", actor->GetFormID());
				if (auto* process = actor->GetActorRuntimeData().currentProcess) {
					process->Set3DUpdateFlag(RE::RESET_3D_FLAGS::kModel);
				}
				actor->Update3DModel();
				Hidden::BodyBuilt(*actor);
			}
			const auto menu = RE::UI::GetSingleton()->GetMenu<RE::ContainerMenu>();
			if (menu && rows) {
				if (!MenuReady(*menu)) {
					rowsOwed = true;
					if (!toldWait.exchange(true)) {
						logger::info("the rows of the trade wait for a fill, the menu holds a row");
					}
					return;
				}
				static const REL::Relocation<void(RE::ContainerMenu*)> fill{ REL::ID(51144) };
				IR_BRACKET("the trade fills its rows anew");
				fill(menu.get());
			}
		});
	}

	bool Wear(std::int32_t a_index, RE::FormID a_form, bool a_left, bool a_on)
	{
		const auto actor = Get();
		if (!actor || actor.get() == RE::PlayerCharacter::GetSingleton() || !actor->Is3DLoaded() || ReadOnly()) {
			IR_LOG("Wear refused: {}", !actor ? "no one is shown" : (actor.get() == RE::PlayerCharacter::GetSingleton() ? "the player is shown" :
				(!actor->Is3DLoaded() ? "the one shown has no body loaded" : "a mod keeps the one shown read only")));
			return false;
		}
		const auto menu = RE::UI::GetSingleton()->GetMenu<RE::ContainerMenu>();
		auto* list = menu ? menu->GetRuntimeData().itemList : nullptr;
		if (!list || a_index < 0 || static_cast<std::uint32_t>(a_index) >= list->items.size()) {
			IR_LOG("Wear refused: {}", !list ? "no list of a trade" : fmt::format("no row {} among {}", a_index, list->items.size()));
			return false;
		}
		// The menu's row is a copy made when it last filled its rows, a script or the follower's AI may have changed
		// their gear since. It only names the item and which of its copies, the lists themselves are taken from the
		// follower's inventory as it is now. A row that moved to another place since the script asked is refused
		auto* item = list->items[static_cast<std::uint32_t>(a_index)];
		auto* row = item ? item->data.objDesc : nullptr;
		auto* object = row ? row->object : nullptr;
		if (!object || object->GetFormID() != a_form || RE::TESObjectREFR::LookupByHandle(item->data.owner).get() != actor.get()) {
			IR_LOG("Wear refused: row {} holds {:08X}, the panel asked for {:08X}, or the row isn't of the one shown", a_index, object ? object->GetFormID() : 0, a_form);
			return false;
		}
		// The hand counts for a weapon alone. A shield is worn with the plain mark, a torch with the mark of the left
		// hand, either of their worn copies is the one
		const bool weapon = object->Is(RE::FormType::Weapon);
		const bool left = a_left && weapon;
		// The copies of this row by their lists. A row of plain copies names none, it owns every copy the other rows of
		// the same item don't: a superior helmet is a row of its own beside the plain ones
		std::vector<const RE::ExtraDataList*> named;
		std::vector<const RE::ExtraDataList*> others;
		for (const auto* other : list->items) {
			auto* copy = other ? other->data.objDesc : nullptr;
			if (!copy || copy->object != object || !copy->extraLists) {
				continue;
			}
			for (const auto* copyList : *copy->extraLists) {
				(copy == row ? named : others).push_back(copyList);
			}
		}
		const auto ofRow = [&](const RE::ExtraDataList* a_list) {
			return named.empty() ? std::ranges::find(others, a_list) == others.end() : std::ranges::find(named, a_list) != named.end();
		};
		auto* live = LiveEntry(*actor, *object);
		RE::ExtraDataList* going = nullptr;
		RE::ExtraDataList* coming = nullptr;
		bool already = false;
		bool otherHand = false;
		if (live && live->extraLists) {
			for (auto* copy : *live->extraLists) {
				// a copy of the outfit is no row's, a plain armor put on goes in its place
				if (!copy || copy->HasType(RE::ExtraDataType::kOutfitItem) || !ofRow(copy)) {
					continue;
				}
				if (weapon ? WornIn(*copy, left) : Worn(*copy)) {
					coming = coming ? coming : copy;
					already = true;
				} else if (Worn(*copy)) {
					otherHand = true;
				} else if (!going) {
					going = copy;
				}
			}
		}
		// with no copy of the row's own left unworn the game picks a plain one, a second sword for the other hand
		if (!a_on && !coming) {
			IR_LOG("Wear refused: {:08X} {} has no worn copy of the row to take off", object->GetFormID(), object->GetName());
			return false;
		}
		// The whole stack of arrows goes on, the way the game puts them on the player. The row's count is the stack, the
		// inventory keeps only how many more or fewer there are than the actor's record gives
		const auto count = object->Is(RE::FormType::Ammo) ? std::max<std::uint32_t>(item->data.GetCount(), 1) : 1;
		const auto inTheWay = a_on ? InTheWay(*actor, *object, already ? coming : going, left) : std::vector<Held>{};
		// the only copy of a weapon, worn in the other hand, moves: it comes off there first
		const bool moves = a_on && !already && !going && otherHand;
		const auto now = Clock::now();
		const auto wornNow = a_on && already ? WornGear(*actor) : std::set<RE::FormID>{};
		{
			const std::scoped_lock lock{ choicesLock };
			// A piece the follower put on by themselves when it was given is the player's choice all the same, the game
			// sends no word of an equip that changes nothing. What it pushed off then came off by the gift, it's told
			// first, as the worn gear before the gift and now tell it
			if (a_on && already) {
				for (const auto form : beforeGift) {
					if (!wornNow.contains(form)) {
						done.push_back({ form, false, now });
					}
				}
				done.push_back({ object->GetFormID(), true, now, left });
			} else {
				if (moves) {
					asked.push_back({ object->GetFormID(), false, now, !left });
				}
				asked.push_back({ object->GetFormID(), a_on, now, left });
			}
			for (const auto& held : inTheWay) {
				asked.push_back({ held.object->GetFormID(), false, now, held.left });
			}
			beforeGift.clear();
		}
		// What goes on through the panel stays on, the way EquipItem with abPreventRemoval keeps it: a follower picks
		// their gear anew whenever their inventory changes, and Lydia swapped an iron helmet put on her for a better
		// one as soon as she was given a sword. A piece comes off whatever kept it on, the player took it off, and
		// what stands in the way of a new one comes off first, a kept piece would stay where the new one goes
		auto* equip = RE::ActorEquipManager::GetSingleton();
		if (a_on) {
			for (const auto& held : inTheWay) {
				const auto stack = static_cast<std::uint32_t>(held.object->Is(RE::FormType::Ammo) ? std::max<std::int32_t>(held.list->GetCount(), 1) : 1);
				IR_BRACKET("unequip {:08X} {} from {:08X} in the way, list {}, {} of it", held.object->GetFormID(), held.object->GetName(), actor->GetFormID(),
					static_cast<const void*>(held.list), stack);
				equip->UnequipObject(actor.get(), held.object, held.list, stack, HandOf(*held.object, held.left), false, true, false, true);
			}
			IR_BRACKET("equip {:08X} {} on {:08X}, list {}, {} of it, left hand {}", object->GetFormID(), object->GetName(), actor->GetFormID(),
				static_cast<const void*>(already ? coming : going), count, left);
			equip->EquipObject(actor.get(), object, already ? coming : going, count, HandOf(*object, left), false, true, true, true);
		} else {
			IR_BRACKET("unequip {:08X} {} from {:08X}, list {}, {} of it, left hand {}", object->GetFormID(), object->GetName(), actor->GetFormID(),
				static_cast<const void*>(coming), count, left);
			equip->UnequipObject(actor.get(), object, coming, count, HandOf(*object, left), false, true, true, true);
		}
		// The game sends its event of equipping within the call. What it didn't answer by now it refused, and a later
		// equip of the same item by the AI or a script mustn't pass for the player's choice
		std::size_t refused = 0;
		{
			const std::scoped_lock lock{ choicesLock };
			refused = std::erase_if(asked, [&](const Choice& a_choice) { return a_choice.at == now; });
		}
		logger::info("{} {} {} {}{}", a_on ? "put" : "took", object->GetName(), a_on ? "on" : "off", actor->GetName(),
			refused > 0 ? std::format(", the game refused {} of {}", refused, inTheWay.size() + (moves ? 2 : 1)) : "");
		bodyOwed = true;
		rowsOwed = true;
		return true;
	}

	std::vector<Piece> Outfit()
	{
		std::vector<Piece> pieces;
		const auto actor = Get();
		auto* changes = actor && Other() ? actor->GetInventoryChanges() : nullptr;
		if (!changes || !changes->entryList) {
			return pieces;
		}
		// The game puts its worn mark back on an outfit's piece when the inventory changes, also where a piece the
		// panel keeps on holds the place: Lydia's outfit shield was marked worn beside the sword in her left hand. What
		// the body carries is the truth, the doll is a copy of it. A piece hidden on them is worn and off the body
		std::set<const RE::TESForm*> onBody;
		if (const auto& biped = actor->GetBiped(false)) {
			for (const auto& object : biped->objects) {
				if (object.item) {
					onBody.insert(object.item);
				}
			}
		}
		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object || !entry->extraLists || (!onBody.empty() && !onBody.contains(entry->object) && !Hidden::Hides(*actor, entry->object))) {
				continue;
			}
			for (auto* list : *entry->extraLists) {
				if (list && Worn(*list) && list->HasType(RE::ExtraDataType::kOutfitItem)) {
					pieces.push_back({ entry->object, list->HasType(RE::ExtraDataType::kWorn), list->HasType(RE::ExtraDataType::kWornLeft) });
				}
			}
		}
		return pieces;
	}

	bool TakeOffPiece(RE::FormID a_form)
	{
		const auto actor = Get();
		if (!actor || !Other() || !actor->Is3DLoaded() || ReadOnly()) {
			IR_LOG("TakeOffPiece refused: {}", !actor || !Other() ? "no follower is shown" : (!actor->Is3DLoaded() ? "the one shown has no body loaded" : "a mod keeps the one shown read only"));
			return false;
		}
		auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(a_form);
		auto* live = object ? LiveEntry(*actor, *object) : nullptr;
		RE::ExtraDataList* worn = nullptr;
		if (live && live->extraLists) {
			for (auto* copy : *live->extraLists) {
				if (copy && Worn(*copy) && copy->HasType(RE::ExtraDataType::kOutfitItem)) {
					worn = copy;
					break;
				}
			}
		}
		if (!worn) {
			IR_LOG("TakeOffPiece refused: {:08X} is no worn piece of the outfit of {:08X}", a_form, actor->GetFormID());
			return false;
		}
		const bool left = worn->HasType(RE::ExtraDataType::kWornLeft) && object->Is(RE::FormType::Weapon);
		const auto now = Clock::now();
		{
			const std::scoped_lock lock{ choicesLock };
			asked.push_back({ a_form, false, now, left });
		}
		{
			IR_BRACKET("unequip {:08X} {} of the outfit from {:08X}, list {}", a_form, object->GetName(), actor->GetFormID(), static_cast<const void*>(worn));
			RE::ActorEquipManager::GetSingleton()->UnequipObject(actor.get(), object, worn, 1, HandOf(*object, left), false, true, true, true);
		}
		bool refused = false;
		{
			const std::scoped_lock lock{ choicesLock };
			refused = std::erase_if(asked, [&](const Choice& a_choice) { return a_choice.at == now; }) > 0;
		}
		logger::info("took {} of the outfit off {}{}", object->GetName(), actor->GetName(), refused ? ", the game refused" : "");
		bodyOwed = true;
		rowsOwed = true;
		return !refused;
	}
}
