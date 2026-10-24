#include "PCH.h"

#include "Magic.h"

namespace Magic
{
	namespace
	{
		constexpr RE::FormID kRightHand = 0x13F42;
		constexpr RE::FormID kLeftHand = 0x13F43;

		Held Of(RE::TESForm* a_form)
		{
			Held held{ a_form };
			if (!a_form) {
				return held;
			}
			held.item = !a_form->Is(RE::FormType::Spell) && !a_form->Is(RE::FormType::Shout);
			if (const auto* weapon = a_form->As<RE::TESObjectWEAP>()) {
				held.bothHands = weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe() || weapon->IsBow() || weapon->IsCrossbow();
			}
			return held;
		}

		// the worn copy of an item of a hand, a shield and a torch are worn in the left hand with either mark
		std::pair<RE::TESBoundObject*, RE::ExtraDataList*> WornCopy(RE::Actor& a_actor, const std::function<bool(RE::TESBoundObject&)>& a_wanted, Place a_place)
		{
			auto* changes = a_actor.GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return {};
			}
			for (auto* entry : *changes->entryList) {
				if (!entry || !entry->object || !entry->extraLists || !a_wanted(*entry->object)) {
					continue;
				}
				const bool weapon = entry->object->Is(RE::FormType::Weapon);
				for (auto* list : *entry->extraLists) {
					if (!list) {
						continue;
					}
					const bool left = list->HasType(RE::ExtraDataType::kWornLeft);
					const bool right = list->HasType(RE::ExtraDataType::kWorn);
					if (weapon ? (a_place == kLeft ? left : right) : (left || right)) {
						return { entry->object, list };
					}
				}
			}
			return {};
		}

		bool Shield(RE::TESBoundObject& a_object)
		{
			const auto* armor = a_object.As<RE::TESObjectARMO>();
			return armor && armor->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kShield);
		}
	}

	std::array<Held, kPlaces> Hands(RE::Actor& a_actor)
	{
		std::array<Held, kPlaces> hands{};
		hands[kRight] = Of(a_actor.GetEquippedObject(false));
		auto* left = a_actor.GetEquippedObject(true);
		// the game keeps a shield out of the hands, it's a worn piece
		if (!left) {
			left = WornCopy(a_actor, Shield, kLeft).first;
		}
		hands[kLeft] = Of(left);
		hands[kVoice] = Of(a_actor.GetActorRuntimeData().selectedPower);
		return hands;
	}

	std::array<Held, kPlaces> Hands()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		return player ? Hands(*player) : std::array<Held, kPlaces>{};
	}

	bool TakeOff(Place a_place)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || (a_place != kRight && a_place != kLeft)) {
			IR_LOG("TakeOffHand refused: {}", !player ? "no player" : "only a hand can be emptied");
			return false;
		}
		const auto held = Hands()[a_place];
		if (!held.item || !held.form) {
			IR_LOG("TakeOffHand refused: the hand holds {}", held.form ? "a spell, the game can't take one off" : "nothing");
			return false;
		}
		const auto [object, copy] = WornCopy(*player, [&](RE::TESBoundObject& a_object) { return &a_object == held.form; }, held.bothHands ? kRight : a_place);
		if (!object) {
			IR_LOG("TakeOffHand refused: no worn copy of {:08X} in that hand", held.form->GetFormID());
			return false;
		}
		const auto* hand = object->Is(RE::FormType::Weapon) ? RE::TESForm::LookupByID<RE::BGSEquipSlot>(a_place == kLeft && !held.bothHands ? kLeftHand : kRightHand) : nullptr;
		IR_BRACKET("unequip {:08X} {} from the player's {} hand, list {}", object->GetFormID(), object->GetName(), a_place == kLeft ? "left" : "right",
			static_cast<const void*>(copy));
		RE::ActorEquipManager::GetSingleton()->UnequipObject(player, object, copy, 1, hand, true, false, true);
		logger::info("{} taken out of the {} hand from the magic menu", object->GetName(), a_place == kLeft ? "left" : "right");
		return true;
	}
}
