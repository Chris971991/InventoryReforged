#pragma once

// Item models in the menu: the game's own preview of the selected item, moved to where the panel
// wants it, and the models of worn items in the slots
namespace ItemView
{
	struct Slot
	{
		std::int32_t index;
		RE::FormID item;
		float centerX;
		float centerY;
		// show it as it sits on the body when it can be, its own model otherwise
		bool worn;
		// the share of the screen height the model may take
		float size;
	};

	// places are shares of the screen, a_size is the share of the screen height the game sizes a model for, the room is
	// the free part of the window in shares of the screen height
	void PlacePreview(float a_centerX, float a_centerY, float a_size, float a_roomHigh, float a_roomWide);
	// each frame of the menu: the model fills its room both ways and stands in its middle
	void FitPreview();
	void RestorePreview();

	// gives back the slots that got a model, the rest keep their flat sign
	std::vector<std::int32_t> ShowSlots(const std::vector<Slot>& a_slots);
	// a worn piece the game hadn't put on the body yet when its slot was shown gets another try
	void Refresh();
	void Clear();
	// a model the menu is done with keeps living a few seconds, the game may still be loading its textures
	void LetGo(RE::NiPointer<RE::NiAVObject>& a_model);

	// the model the item has in the world, turned the way the game turns it for a menu
	RE::NiPointer<RE::NiAVObject> Ground(RE::TESForm& a_item, RE::NiMatrix3& a_facing);
}
