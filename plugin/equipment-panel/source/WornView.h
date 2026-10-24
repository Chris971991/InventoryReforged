#pragma once

// A worn piece as it sits on the body: a copy of the player in the standing pose where everything but
// that piece is switched off. Boots stand, a cloak hangs, a helmet looks ahead
namespace WornView
{
	// nothing when the piece has no 3D on the player now: not worn, hidden, or still loading
	RE::NiPointer<RE::NiAVObject> Make(RE::TESForm& a_item);

	// turns a model to the camera and sizes it into a square of a_size (share of the screen height)
	// around a point, by the room its drawn shapes take. a_degrees turns it around its upright axis first
	void Fit(RE::NiAVObject& a_model, float a_centerX, float a_centerY, float a_size, float a_degrees);

	// for the inspect view: the turn and size a fitted model got, so it can be turned around its middle
	struct Fitted
	{
		RE::NiMatrix3 facing;
		RE::NiPoint3 middle;  // middle of the drawn shapes in the model's own space
		float scale = 1.0F;
	};
	Fitted FitAround(RE::NiAVObject& a_model, const RE::NiMatrix3& a_facing, float a_centerX, float a_centerY, float a_size);
}
