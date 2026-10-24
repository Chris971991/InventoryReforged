#pragma once

// A full screen look at one item: big in the middle, turned by the mouse, brought closer by the wheel
// A worn piece is shown as it sits on the body, anything else as its own model
namespace Inspect
{
	// a_size is the share of the screen height the item takes before any zoom
	bool Begin(RE::FormID a_item, bool a_worn, float a_centerX, float a_centerY, float a_size);

	// degrees around the upright axis of the screen and around its left to right axis
	void Turn(float a_yaw, float a_pitch);

	// multiplies how close the item is, kept within sane bounds
	void Zoom(float a_factor);

	void End();
	bool Active();
}
