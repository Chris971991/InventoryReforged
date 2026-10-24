#pragma once

// A gamepad in the inventory, a follower's trade and the magic menu. The click of the stick the game zooms with goes to
// the panel: a short one moves the input between the list and the panel, a hold inspects what's in focus. The right
// stick turns the doll while the panel has the input, and the inspected item while one is looked at
namespace Pad
{
	void Listen();

	// what the stick turns now, the panel tells it. Only while a gamepad plays, so a drifting stick of a pad lying
	// by the keyboard turns nothing
	enum class Turns : std::int32_t
	{
		kNothing,
		kDoll,
		kItem
	};
	void SetTurns(Turns a_what);

	// once a frame of the inventory, turns what the stick holds
	void Frame();

	void Closed();
}
