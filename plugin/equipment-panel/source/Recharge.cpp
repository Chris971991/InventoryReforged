#include "PCH.h"

#include "Recharge.h"

namespace Recharge
{
	namespace
	{
		// pieces of the game's own recharge in the inventory, numbers of game 1.6 and later
		constexpr REL::ID kSoulValue{ 15997 };    // the charge the soul of a row's gem gives, 0 or less for none
		constexpr REL::ID kRowCount{ 16004 };
		constexpr REL::ID kRowName{ 16018 };
		constexpr REL::ID kEnchantment{ 14563 };  // of an item with one list of its extra data
		constexpr REL::ID kMostCharge{ 14564 };
		constexpr REL::ID kChargeValue{ 34609 };  // the actor value that keeps the charge of a worn item, by its hand
		constexpr REL::ID kSetCharge{ 16021 };
		// CommonLib names 37803 for game 1.6 and later, that's another function there
		constexpr REL::ID kChargeToValue{ 38752 };
		constexpr REL::ID kSkillPoints{ 26492 };  // what the player learns of enchanting by the charge put in
		constexpr REL::ID kSetSoul{ 11620 };
		// The keyword of gems that stay, a default object, and whether it's set. Game 1.7 moved it one place on among
		// the default objects and CommonLib still counts it where 1.6 has it, so it's read the way the game reads it
		constexpr REL::ID kStaysKeyword{ 400223 };
		constexpr REL::ID kStaysKeywordSet{ 400259 };
		constexpr REL::ID kNotify{ 52933 };
		constexpr REL::ID kNoGemsText{ 377068 };
		constexpr REL::ID kGemsHeading{ 377071 };
		constexpr REL::ID kMenuSound{ 52939 };
		constexpr REL::ID kUpdateRows{ 51031 };   // the inventory's rows made anew, after the game's own recharge too
		constexpr REL::ID kFillRows{ 51144 };     // both sides of a trade filled anew
		constexpr std::size_t kUseSkill = 0xF7;
		// the kind of card that shows a list, ICT_LIST of SkyUI
		constexpr double kCardList = 14.0;
		constexpr auto kReadyScript = "_global.Equipment.refillReady";

		// a charge in a trade left its rows behind the inventories until the next fill
		std::atomic<bool> fillOwed{ false };

		// The menu a call came from and whose weapons it charges
		struct Menu
		{
			RE::ItemList* rows = nullptr;
			RE::NiPointer<RE::Actor> target;
			bool inventory = false;
		};

		std::optional<Menu> MenuOf(const RE::GFxMovie* a_movie)
		{
			auto* ui = RE::UI::GetSingleton();
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!REL::Module::IsAE() || !a_movie || !ui || !player) {
				return std::nullopt;
			}
			if (const auto menu = ui->GetMenu<RE::InventoryMenu>(); menu && menu->uiMovie.get() == a_movie) {
				return Menu{ menu->GetRuntimeData().itemList, RE::NiPointer<RE::Actor>(player), true };
			}
			const auto menu = ui->GetMenu<RE::ContainerMenu>();
			if (!menu || menu->uiMovie.get() != a_movie || RE::ContainerMenu::GetContainerMode() != RE::ContainerMenu::ContainerMode::kNPCMode || fillOwed.load()) {
				return std::nullopt;
			}
			const auto target = RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle());
			auto* actor = target ? target->As<RE::Actor>() : nullptr;
			if (!actor || actor->IsPlayerRef()) {
				return std::nullopt;
			}
			return Menu{ menu->GetRuntimeData().itemList, RE::NiPointer<RE::Actor>(actor), false };
		}

		RE::InventoryEntryData* LiveEntry(RE::TESObjectREFR& a_owner, const RE::TESBoundObject& a_object)
		{
			auto* changes = a_owner.GetInventoryChanges();
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

		struct Row
		{
			RE::NiPointer<RE::TESObjectREFR> owner;
			RE::TESBoundObject* object = nullptr;
			// the menu's copy of the row, made when it last filled its rows
			RE::InventoryEntryData* copy = nullptr;
			RE::InventoryEntryData* live = nullptr;
			// the copies of the row by their list as the inventory holds it now, none for plain copies
			RE::ExtraDataList* list = nullptr;
		};

		// A row of the player's or of the one traded with. The lists a copy names may be gone since the menu made it,
		// they're compared with the inventory's before anything reads them
		std::optional<Row> RowAt(const Menu& a_menu, std::int32_t a_index, RE::FormID a_form)
		{
			if (!a_menu.rows || a_index < 0 || static_cast<std::uint32_t>(a_index) >= a_menu.rows->items.size()) {
				return std::nullopt;
			}
			auto* item = a_menu.rows->items[static_cast<std::uint32_t>(a_index)];
			auto* copy = item ? item->data.objDesc : nullptr;
			auto* object = copy ? copy->object : nullptr;
			if (!object || object->GetFormID() != a_form) {
				return std::nullopt;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto owner = RE::TESObjectREFR::LookupByHandle(item->data.owner);
			if (!owner && a_menu.inventory) {
				owner.reset(player);
			}
			if (!owner || (owner.get() != player && owner.get() != a_menu.target.get())) {
				return std::nullopt;
			}
			Row row{ owner, object, copy, LiveEntry(*owner, *object) };
			const auto* named = copy->extraLists && !copy->extraLists->empty() ? copy->extraLists->front() : nullptr;
			if (named && row.live && row.live->extraLists) {
				for (auto* list : *row.live->extraLists) {
					if (list == named) {
						row.list = list;
					}
				}
			}
			if (named && !row.list) {
				IR_LOG("Recharge: row {} of {:08X} names a list the inventory no longer holds", a_index, a_form);
				return std::nullopt;
			}
			return row;
		}

		struct Weapon
		{
			Row row;
			RE::EnchantmentItem* enchantment = nullptr;
			float most = 0.0F;
		};

		// a weapon or staff of the one whose inventory is open, one the game charges
		std::optional<Weapon> WeaponOf(const Menu& a_menu, std::optional<Row> a_row)
		{
			if (!a_row || a_row->owner.get() != a_menu.target.get() || !a_row->object->Is(RE::FormType::Weapon)) {
				return std::nullopt;
			}
			static const REL::Relocation<RE::EnchantmentItem*(RE::TESBoundObject*, RE::ExtraDataList*)> enchantment{ kEnchantment };
			static const REL::Relocation<std::uint16_t(RE::TESBoundObject*, RE::ExtraDataList*)> most{ kMostCharge };
			Weapon weapon{ *a_row, enchantment(a_row->object, a_row->list), static_cast<float>(most(a_row->object, a_row->list)) };
			if (!weapon.enchantment || weapon.most <= 0.0F) {
				return std::nullopt;
			}
			return weapon;
		}

		// While worn the charge lives in an actor value of the hand that holds it and the list keeps what it had when
		// it went on, the game reads the player's own items so too
		float ChargeNow(const Menu& a_menu, const Weapon& a_weapon)
		{
			const auto* list = a_weapon.row.list;
			if (!list) {
				return a_weapon.most;
			}
			const bool left = list->HasType(RE::ExtraDataType::kWornLeft);
			if (left || list->HasType(RE::ExtraDataType::kWorn)) {
				static const REL::Relocation<std::int32_t(RE::EnchantmentItem*, std::uint32_t)> valueOf{ kChargeValue };
				if (const auto value = valueOf(a_weapon.enchantment, left ? 0 : 1); value != -1) {
					return a_menu.target->AsActorValueOwner()->GetActorValue(static_cast<RE::ActorValue>(value));
				}
			}
			const auto* charge = list->GetByType<RE::ExtraCharge>();
			return charge ? charge->charge : a_weapon.most;
		}

		// a soul gem with a soul, the player's or the teammate's
		bool IsGem(const std::optional<Row>& a_row)
		{
			static const REL::Relocation<std::int32_t(RE::InventoryEntryData*)> soul{ kSoulValue };
			static const REL::Relocation<std::int32_t(RE::InventoryEntryData*)> count{ kRowCount };
			return a_row && a_row->object->Is(RE::FormType::SoulGem) && soul(a_row->copy) > 0 && count(a_row->copy) > 0;
		}

		// the charge a gem puts in with the player's perks of Modify Soul Gem Recharge
		float SoulOf(const Row& a_gem, const Weapon& a_weapon)
		{
			static const REL::Relocation<std::int32_t(RE::InventoryEntryData*)> soul{ kSoulValue };
			auto value = static_cast<float>(soul(a_gem.copy));
			RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModSoulGemRecharge, RE::PlayerCharacter::GetSingleton(), a_weapon.row.object, &value);
			return value;
		}

		const char* GameText(REL::ID a_id)
		{
			const auto* text = *REL::Relocation<const char**>{ a_id }.get();
			return text ? text : "";
		}

		// A gem that stays (Azura's Star, the Black Star) gives its soul and stays empty, any other goes. One whose
		// soul is its record's has no list to empty, it's swapped for the empty gem it names
		void UseUp(const Row& a_gem)
		{
			auto* gem = a_gem.object->As<RE::TESSoulGem>();
			auto* keyword = *REL::Relocation<bool*>{ kStaysKeywordSet }.get() ? *REL::Relocation<RE::TESForm**>{ kStaysKeyword }.get() : nullptr;
			const auto* stays = keyword ? keyword->As<RE::BGSKeyword>() : nullptr;
			if (!gem || !stays || !gem->HasKeyword(stays)) {
				a_gem.owner->RemoveItem(a_gem.object, 1, RE::ITEM_REMOVE_REASON::kRemove, a_gem.list, nullptr);
				return;
			}
			if (a_gem.list) {
				static const REL::Relocation<void(RE::ExtraDataList*, RE::SOUL_LEVEL)> setSoul{ kSetSoul };
				setSoul(a_gem.list, RE::SOUL_LEVEL::kNone);
				return;
			}
			a_gem.owner->RemoveItem(a_gem.object, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
			if (gem->linkedSoulGem) {
				a_gem.owner->AddObjectToContainer(gem->linkedSoulGem, nullptr, 1, nullptr);
			}
		}

		// The menu's rows as the card's list shows them, a line each, with the row it stands for
		RE::GFxValue Line(RE::GFxMovie& a_movie, const Row& a_row, std::uint32_t a_index)
		{
			static const REL::Relocation<const char*(RE::InventoryEntryData*)> name{ kRowName };
			static const REL::Relocation<std::int32_t(RE::InventoryEntryData*)> count{ kRowCount };
			RE::GFxValue line;
			RE::GFxValue text;
			a_movie.CreateObject(&line);
			a_movie.CreateString(&text, name(a_row.copy));
			line.SetMember("text", text);
			line.SetMember("count", RE::GFxValue(static_cast<double>(count(a_row.copy))));
			line.SetMember("itemIndex", RE::GFxValue(static_cast<double>(a_index)));
			line.SetMember("formId", RE::GFxValue(static_cast<double>(a_row.object->GetFormID())));
			return line;
		}

		void Card(RE::GFxMovie& a_movie, RE::GFxValue& a_info, RE::GFxValue& a_list, double a_charge)
		{
			RE::GFxValue heading;
			a_movie.CreateString(&heading, GameText(kGemsHeading));
			a_movie.CreateObject(&a_info);
			a_info.SetMember("type", RE::GFxValue(kCardList));
			a_info.SetMember("name", heading);
			a_info.SetMember("upperCaseName", RE::GFxValue(false));
			a_info.SetMember("listItems", a_list);
			a_info.SetMember("currentCharge", RE::GFxValue(a_charge));
		}

		bool MenuReady(RE::IMenu& a_menu)
		{
			RE::GFxValue ready;
			IR_SCRIPT_QUIET(kReadyScript);
			return a_menu.uiMovie && a_menu.uiMovie->Invoke(kReadyScript, &ready, nullptr, 0) && ready.IsBool() && ready.GetBool();
		}
	}

	float ChargeOf(const RE::GFxMovie* a_movie, std::int32_t a_index, RE::FormID a_form)
	{
		const auto menu = MenuOf(a_movie);
		const auto weapon = menu ? WeaponOf(*menu, RowAt(*menu, a_index, a_form)) : std::nullopt;
		return weapon ? ChargeNow(*menu, *weapon) * 100.0F / weapon->most : -1.0F;
	}

	void SoulGems(RE::GFxMovie& a_movie, std::int32_t a_index, RE::FormID a_form, RE::GFxValue& a_info)
	{
		const auto menu = MenuOf(&a_movie);
		const auto weapon = menu ? WeaponOf(*menu, RowAt(*menu, a_index, a_form)) : std::nullopt;
		if (!weapon) {
			IR_LOG("SoulGems refused: row {} isn't {:08X} of a weapon that takes a charge here", a_index, a_form);
			return;
		}
		// The game lists the gems among the rows of its menu (51875), in a trade those are of both sides. The player's
		// come first
		auto* player = RE::PlayerCharacter::GetSingleton();
		RE::GFxValue list;
		a_movie.CreateArray(&list);
		std::uint32_t lines = 0;
		for (const bool players : { true, false }) {
			for (std::uint32_t i = 0; i < menu->rows->items.size(); ++i) {
				auto* item = menu->rows->items[i];
				auto* copy = item ? item->data.objDesc : nullptr;
				if (!copy || !copy->object || !copy->object->Is(RE::FormType::SoulGem)) {
					continue;
				}
				const auto gem = RowAt(*menu, static_cast<std::int32_t>(i), copy->object->GetFormID());
				if (!IsGem(gem) || (gem->owner.get() == player) != players) {
					continue;
				}
				auto line = Line(a_movie, *gem, i);
				RE::GFxValue owner;
				a_movie.CreateString(&owner, players ? "" : gem->owner->GetDisplayFullName());
				line.SetMember("owner", owner);
				line.SetMember("chargeAdded", RE::GFxValue(static_cast<double>(SoulOf(*gem, *weapon) * 100.0F / weapon->most)));
				list.PushBack(line);
				++lines;
			}
		}
		if (lines == 0) {
			static const REL::Relocation<void(const char*, const char*, bool)> notify{ kNotify };
			notify(GameText(kNoGemsText), nullptr, true);
			return;
		}
		Card(a_movie, a_info, list, ChargeNow(*menu, *weapon) * 100.0F / weapon->most);
	}

	void Weapons(RE::GFxMovie& a_movie, std::int32_t a_index, RE::FormID a_form, RE::GFxValue& a_info)
	{
		const auto menu = MenuOf(&a_movie);
		const auto gem = menu ? RowAt(*menu, a_index, a_form) : std::nullopt;
		if (!IsGem(gem)) {
			return;
		}
		RE::GFxValue list;
		a_movie.CreateArray(&list);
		double first = -1.0;
		for (std::uint32_t i = 0; i < menu->rows->items.size(); ++i) {
			auto* item = menu->rows->items[i];
			auto* copy = item ? item->data.objDesc : nullptr;
			auto* weaponForm = copy && copy->object ? copy->object->As<RE::TESObjectWEAP>() : nullptr;
			// one without an enchantment of its record and without lists has none of its own either
			if (!weaponForm || (!weaponForm->formEnchanting && (!copy->extraLists || copy->extraLists->empty()))) {
				continue;
			}
			const auto weapon = WeaponOf(*menu, RowAt(*menu, static_cast<std::int32_t>(i), weaponForm->GetFormID()));
			const float charge = weapon ? ChargeNow(*menu, *weapon) * 100.0F / weapon->most : 100.0F;
			if (!weapon || charge >= 100.0F) {
				continue;
			}
			auto line = Line(a_movie, weapon->row, i);
			line.SetMember("charge", RE::GFxValue(static_cast<double>(charge)));
			line.SetMember("chargeAdded", RE::GFxValue(static_cast<double>(SoulOf(*gem, *weapon) * 100.0F / weapon->most)));
			list.PushBack(line);
			first = first < 0.0 ? charge : first;
		}
		if (first >= 0.0) {
			Card(a_movie, a_info, list, first);
		}
	}

	bool Charge(const RE::GFxMovie* a_movie, std::int32_t a_weapon, RE::FormID a_weaponForm, std::int32_t a_gem, RE::FormID a_gemForm)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		const auto menu = MenuOf(a_movie);
		const auto weapon = menu ? WeaponOf(*menu, RowAt(*menu, a_weapon, a_weaponForm)) : std::nullopt;
		const auto gem = menu ? RowAt(*menu, a_gem, a_gemForm) : std::nullopt;
		auto* changes = weapon ? weapon->row.owner->GetInventoryChanges() : nullptr;
		if (!weapon || !weapon->row.list || !changes || !IsGem(gem)) {
			IR_LOG("Charge refused: row {} of {:08X} is no drained weapon here or row {} of {:08X} no gem with a soul", a_weapon, a_weaponForm, a_gem, a_gemForm);
			return false;
		}
		const float soul = SoulOf(*gem, *weapon);
		if (soul <= 0.0F) {
			return false;
		}
		static const REL::Relocation<float(float)> skillPoints{ kSkillPoints };
		using UseSkill = void (*)(RE::Actor*, RE::ActorValue, float, RE::TESForm*, std::uint32_t);
		(*reinterpret_cast<UseSkill**>(player))[kUseSkill](player, RE::ActorValue::kEnchanting, skillPoints(soul), nullptr, 0);

		const float before = ChargeNow(*menu, *weapon);
		const float after = std::clamp(before + soul, 0.0F, weapon->most);
		{
			IR_BRACKET("the charge of {:08X} of {:08X} set to {}", a_weaponForm, menu->target->GetFormID(), after);
			static const REL::Relocation<void(RE::InventoryEntryData*, float, RE::InventoryChanges*, RE::ExtraDataList*)> setCharge{ kSetCharge };
			setCharge(weapon->row.live, after, changes, weapon->row.list);
		}
		const bool left = weapon->row.list->HasType(RE::ExtraDataType::kWornLeft);
		if (left || weapon->row.list->HasType(RE::ExtraDataType::kWorn)) {
			static const REL::Relocation<void(RE::Actor*, RE::TESForm*, RE::ExtraDataList*, bool)> chargeToValue{ kChargeToValue };
			chargeToValue(menu->target.get(), weapon->row.object, weapon->row.list, left);
		}
		const std::string weaponName = weapon->row.object->GetName();
		const std::string gemName = gem->object->GetName();
		const std::string gemOwner = gem->owner->GetDisplayFullName();
		UseUp(*gem);
		static const REL::Relocation<void(const char*)> sound{ kMenuSound };
		sound("UIEnchantRecharge");
		// The inventory's rows are made anew right away, as the game does after its own recharge. A trade waits for the
		// list of the card to go, its fill acts on the selected row
		if (menu->inventory) {
			static const REL::Relocation<void(RE::ItemList*, RE::TESObjectREFR*)> update{ kUpdateRows };
			update(menu->rows, player);
		} else {
			fillOwed = true;
		}
		logger::info("{} of {} charged from {:.0f} to {:.0f} of {:.0f} with {} of {}", weaponName, menu->target->GetName(), before, after, weapon->most, gemName, gemOwner);
		return true;
	}

	// The fill is the container menu's own, both sides from the inventories, as a task of the interface once the
	// scripts of this frame are done. The game's moves act on the selected row by its place, so not while a row is held
	void Frame()
	{
		if (!fillOwed.load()) {
			return;
		}
		SKSE::GetTaskInterface()->AddUITask([] {
			const auto menu = RE::UI::GetSingleton()->GetMenu<RE::ContainerMenu>();
			if (!menu) {
				fillOwed = false;
				return;
			}
			if (!fillOwed.load() || !MenuReady(*menu)) {
				return;
			}
			fillOwed = false;
			static const REL::Relocation<void(RE::ContainerMenu*)> fill{ kFillRows };
			IR_BRACKET("the trade fills its rows anew after a charge");
			fill(menu.get());
		});
	}
}
