#pragma once

// What the player put on a follower and took off them through the panel holds against everything else that dresses
// them: the game puts their default outfit back on as they change cells or a save loads, follower mods swap helmets
// by place or by fight, their AI picks "better" gear. Kept per follower in the save. Once the item menus are closed
// and someone else changed the follower's gear, the player's choice goes back on a moment later. A follower another
// mod keeps changing back gets it back a few times a minute at most, the log tells which piece and who
namespace Keep
{
	// the game did what the player chose on a follower: a piece on (in the left hand for a weapon) or off
	void Chosen(RE::Actor& a_actor, RE::TESBoundObject& a_object, bool a_on, bool a_left);
	// hears followers with choices change their gear, called once the data is loaded
	void Listen();
	// the world's frame on the main thread
	void Frame();

	constexpr std::uint32_t kRecord = 'KEEP';
	void Save(SKSE::SerializationInterface* a_save);
	void Read(SKSE::SerializationInterface* a_save, std::uint32_t a_version);
	void Forget();
}
