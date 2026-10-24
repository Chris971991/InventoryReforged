#pragma once

// Slots whose worn piece isn't drawn on the player: it stays worn, counts for armor and enchantments,
// only its 3D never goes onto the body. The game builds the worn 3D of an actor in one visitor,
// for the player that visitor skips the pieces of hidden slots. The choice lives in the save
namespace Hidden
{
	// slot numbers as the panel script orders them
	enum Slot : std::int32_t
	{
		kRight,
		kLeft,
		kAmmo,
		kAmulet,
		kRing,
		kHead,
		kBody,
		kHands,
		kFeet,
		kOther,
		kCount
	};

	void Install();
	// after the data is loaded, watches the player put things on
	void Listen();

	std::uint32_t Mask();
	// hides or shows a slot and has the player's 3D built again, false for a slot that can't be hidden
	bool Set(std::int32_t a_slot, bool a_hidden);
	// single pieces hidden from the strip of a slot that holds several, by form, whatever slot they show in
	std::vector<RE::FormID> Pieces();
	void SetPiece(RE::FormID a_form, bool a_hidden);
	// whether a slot can be hidden at all: weapons and shields stay, combat mods hit and block with their models,
	// and the game itself reports no shield while a shield isn't on the body
	bool CanHide(std::int32_t a_slot);
	// every slot and piece shown again, from the button of the mod's menu
	void ShowAll();
	// the player's worn pieces built again, after a load whose body may predate the hidden slots
	void Refresh();
	// Jewelry mods like MARA hang copies of worn pieces on the body themselves. A hidden piece's copies go out of
	// sight in the frames after a change, the item menus pause the world and call this in each of their frames
	void MenuFrame();
	// the pieces whose copies were seen hanging on the body, the panel shows an eye for them
	std::vector<RE::FormID> Hung();
	// the same for the doll, a copy of the body that comes with every node switched on
	void HideHanging(RE::NiAVObject& a_copy);
	// for the test stand: the worn pieces with the model files they're known by, and what hangs on the body
	void DumpHanging();
	// whether a piece drawn on the body covers the whole head, the game keeps the head off under it
	bool HeadUnderShownPiece(RE::PlayerCharacter& a_player);
	// the face of a copy of the player's body gets back what the hidden pieces cover, in first person too
	void UncoverCopiedFace(RE::NiAVObject& a_face, RE::PlayerCharacter& a_player);

	// The same for whoever the panel shows: the player's own, or a follower's kept in the save per actor. A follower's
	// body is built without their hidden pieces the way the player's is, their hair stays under a hidden helmet and a
	// newer piece their AI puts into a hidden slot pushes the older one off. Wigs pushed off, jewelry copies of other
	// mods and TNG's cover are the player's alone
	std::uint32_t MaskOf(RE::Actor& a_actor);
	bool SetOn(RE::Actor& a_actor, std::int32_t a_slot, bool a_hidden);
	std::vector<RE::FormID> PiecesOf(RE::Actor& a_actor);
	void SetPieceOn(RE::Actor& a_actor, RE::FormID a_form, bool a_hidden);
	// a follower's body was built again in their trade, the hair goes back under a hidden helmet
	void BodyBuilt(RE::Actor& a_actor);
	// whether a worn piece is left off the actor's body by what they hide
	bool Hides(RE::Actor& a_actor, RE::TESBoundObject* a_object);

	constexpr std::uint32_t kFollowersRecord = 'HIDF';

	void Save(SKSE::SerializationInterface* a_save);
	void Read(SKSE::SerializationInterface* a_save, std::uint32_t a_version);
	void ReadFollowers(SKSE::SerializationInterface* a_save, std::uint32_t a_version);
	void Forget();
}
