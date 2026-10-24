#pragma once

// How much room the player takes standing straight, in units of the game world
// A character is measured once and the result is kept under the hash of that character
namespace PlayerSize
{
	struct Size
	{
		float height;
		float width;
	};

	Size Of(RE::Actor& a_player, RE::NiAVObject& a_body);
}
