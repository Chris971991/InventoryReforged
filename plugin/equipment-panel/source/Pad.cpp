#include "PCH.h"

#include "Pad.h"

#include "Inspect.h"
#include "PlayerView.h"
#include "Settings.h"
#include "Subject.h"

namespace Pad
{
	namespace
	{
		constexpr float kHold = 0.5F;
		constexpr float kDeadZone = 0.2F;
		// degrees a second at full tilt. The doll's follows the turn speed of the mouse, 180 at its default
		constexpr float kDollDegrees = 360.0F;
		constexpr float kItemDegrees = 180.0F;
		// a frame longer than this is a hitch or the first one after a while, it doesn't turn more
		constexpr float kLongestFrame = 0.1F;
		constexpr auto kStickScript = "_global.Equipment.padStick";

		RE::NiPoint2 stick{};
		Turns turns = Turns::kNothing;
		bool holdTold = false;
		std::optional<std::chrono::steady_clock::time_point> lastFrame;

		// A book read from the inventory or a message box takes the menu input over it, the stick isn't the panel's then.
		// Any open menu that stops the game, reads menu input and stands above the inventory counts. The cursor, the HUD
		// and the overlays of mods stop nothing
		bool InventoryOnTop(RE::UI& a_ui, const RE::IMenu& a_inventory)
		{
			for (const auto& menu : a_ui.menuStack) {
				if (menu && menu.get() != &a_inventory && menu->PausesGame() && menu->UsesMenuContext() && menu->depthPriority > a_inventory.depthPriority) {
					return false;
				}
			}
			return true;
		}

		// the inventory, a follower's trade where the panel shows the follower, or the magic menu with the panel in it
		std::string_view PanelMenu(RE::UI& a_ui)
		{
			if (Subject::Other()) {
				return RE::ContainerMenu::MENU_NAME;
			}
			if (!a_ui.IsMenuOpen(RE::InventoryMenu::MENU_NAME) && Settings::Get().magicMenu && a_ui.IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
				return RE::MagicMenu::MENU_NAME;
			}
			return RE::InventoryMenu::MENU_NAME;
		}

		void Tell(bool a_held)
		{
			auto* ui = RE::UI::GetSingleton();
			const auto name = PanelMenu(*ui);
			if (!ui->IsMenuOpen(name)) {
				return;
			}
			if (const auto menu = ui->GetMenu(name); menu && menu->uiMovie && InventoryOnTop(*ui, *menu)) {
				RE::GFxValue held(a_held);
				IR_SCRIPT(kStickScript);
				menu->uiMovie->Invoke(kStickScript, nullptr, &held, 1);
			}
		}

		// the zoom control of a gamepad. The event may come named by another context of the menu, its button tells it then
		bool IsZoom(const RE::ButtonEvent& a_button)
		{
			return a_button.GetDevice() == RE::INPUT_DEVICE::kGamepad &&
				(a_button.QUserEvent() == RE::UserEvents::GetSingleton()->itemZoom || a_button.GetIDCode() == RE::BSWin32GamepadDevice::Keys::kRightThumb);
		}

		// A button event comes on the press, every frame it's held and on the let go. A let go before the hold time
		// is a click, the hold is told once when it's reached and its let go says nothing. A let go that comes late
		// with no held event before it is a hold all the same
		class InputWatch : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (!Settings::Get().panel) {
					return RE::BSEventNotifyControl::kContinue;
				}
				for (auto* event = a_events ? *a_events : nullptr; event; event = event->next) {
					if (const auto* thumb = event->AsThumbstickEvent(); thumb && thumb->IsRight()) {
						stick = { thumb->xValue, thumb->yValue };
						continue;
					}
					const auto* button = event->AsButtonEvent();
					if (!button || !IsZoom(*button)) {
						continue;
					}
					if (button->IsDown()) {
						holdTold = false;
					} else if (button->IsHeld() && !holdTold && button->HeldDuration() >= kHold) {
						holdTold = true;
						Tell(true);
					} else if (button->IsUp() && !holdTold) {
						Tell(button->HeldDuration() >= kHold);
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		float Tilt(float a_value)
		{
			return std::abs(a_value) > kDeadZone ? a_value : 0.0F;
		}
	}

	void Listen()
	{
		static InputWatch inputs;
		RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&inputs);
	}

	void SetTurns(Turns a_what)
	{
		turns = a_what;
	}

	// The stick is kept as its last event left it and turns by the time of a frame, so the pace holds whether the
	// pad sends a value every frame or only when it changes
	void Frame()
	{
		const auto now = std::chrono::steady_clock::now();
		const float seconds = lastFrame ? std::min(kLongestFrame, std::chrono::duration<float>(now - *lastFrame).count()) : 0.0F;
		lastFrame = now;
		const float x = Tilt(stick.x);
		const float y = Tilt(stick.y);
		if (seconds <= 0.0F || (x == 0.0F && y == 0.0F)) {
			return;
		}
		// pushed up the stick turns the item the way a drag of the mouse upward does
		if (turns == Turns::kItem && Inspect::Active()) {
			Inspect::Turn(x * kItemDegrees * seconds, -y * kItemDegrees * seconds);
		} else if (turns == Turns::kDoll) {
			PlayerView::Turn(x * kDollDegrees * Settings::Get().turnSpeed * seconds);
		}
	}

	void Closed()
	{
		stick = {};
		turns = Turns::kNothing;
		holdTold = false;
		lastFrame.reset();
	}
}
