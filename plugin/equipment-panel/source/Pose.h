#pragma once

// The doll stands the same way whatever the player does at the moment the menu opens: riding, sitting,
// sneaking or mid swing. Bone turns of a calm standing idle, taken once from the game, go onto the copy
namespace Pose
{
	// puts the copy into the shipped standing pose of its sex, false if the copy has none of its bones (beast form)
	bool Stand(RE::NiAVObject& a_copy, bool a_female);

	// Whether the copy keeps the pose of the moment, the player's choice "As in the world". Not in first person: the
	// player never sees their body there, and mods of first person animations play a stance made for the camera on
	// it, a crouch with the knees bent. A doll of that stood squashed in the belly with the spine straightened
	bool KeepsWorldPose();

	// Head and spine of a copy taken as it is get the turn of the race skeleton file, so a player who looked
	// aside or ran with the camera turned doesn't stand twisted. Arms and legs keep their pose
	void LookAhead(RE::NiAVObject& a_copy, RE::Actor& a_player);

	// writes the bones of the live player into SKSE/InventoryReforged-pose-<sex>.txt, the source of StandingPose.h
	void Dump(RE::NiAVObject& a_body, bool a_female);

	// A player saw the doll squashed in the belly, the body fine in the world. The log gets the length and size of the
	// spine, arm and leg bones in the live body, in the doll and in the race skeleton at rest, with the camera, once
	// for each new reading
	void Tell(RE::NiAVObject& a_body, RE::NiAVObject& a_copy, RE::Actor& a_shown);
}
