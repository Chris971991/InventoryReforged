#pragma once

// Numbers of an item the menu has no field for: how fast a weapon swings, how far it reaches, how hard it
// staggers and crits, and the game settings that turn armor rating into the share of damage it stops
namespace Stats
{
	void Fill(RE::GFxMovie& a_movie, RE::GFxValue& a_object, RE::FormID a_item);
	// the numbers of the player or of the follower the panel shows for the character window: pools and their
	// regeneration, resistances, attack and speed
	void FillHero(RE::GFxMovie& a_movie, RE::GFxValue& a_object, RE::Actor& a_actor);
	// the words of an effect with its strength and time put where the text asks for them
	std::string Words(std::string a_text, float a_magnitude, float a_duration);
}
