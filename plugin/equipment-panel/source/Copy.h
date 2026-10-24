#pragma once

// Copies of the player's 3D for the menu scene: the doll and the worn pieces in the slots
namespace Copy
{
	// a copy of the whole body with no physics, ready to stand in the menu scene
	RE::NiPointer<RE::NiAVObject> Body(RE::NiAVObject& a_body);

	// lets a copy go, a_copy is empty after
	void Release(RE::NiPointer<RE::NiAVObject>& a_copy);

	// what the model is made of right now, changes when the game puts an item on or takes one off
	std::size_t Makeup(RE::NiAVObject& a_body);

	// skin of the body, which armor meshes carry under the pieces too
	bool IsSkin(RE::NiAVObject& a_geometry);
}

inline bool Female(RE::Actor& a_actor)
{
	const auto* base = a_actor.GetActorBase();
	return base && base->GetSex() == RE::SEX::kFemale;
}
