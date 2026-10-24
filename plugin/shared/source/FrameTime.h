#pragma once

#include <mutex>
#include <optional>

// The game counts the physics of a frame from the speed of time at the start of the frame (function 77850) and sets
// the solver up later in the same frame (77851) from the speed of time then. A speed changed in between leaves the
// solver set for a step the world does not take. A drop from 1 to 0.05 gave 87 solver steps with tau 6.9 for one
// step and threw every ragdoll that still moved (docs/thrown-bodies.txt). So a speed asked for during a frame is put
// in right before the timer of the next frame counts it, and the game time, the physics step and the solver agree
namespace FrameTime
{
	struct Speeds
	{
		float world = 1.0F;
		// the speed of the player while the world is slowed, the way the Slow Time shout sets it. None leaves it alone
		std::optional<float> player;
	};

	namespace Detail
	{
		// the call of the timer in the function of a frame, found in game version 1.7.104
		inline const REL::ID kFrame{ 36577 };
		inline constexpr std::size_t kTimerCall = 0x70;
		inline const REL::ID kTimerUpdate{ 68244 };
		inline constexpr std::uint8_t kCall = 0xE8;
		inline constexpr std::size_t kCallLength = 5;

		inline std::mutex lock;
		inline std::optional<Speeds> asked;
		inline bool atFrameStart = false;

		inline void Put(const Speeds& a_speeds)
		{
			if (a_speeds.player) {
				const bool slowed = a_speeds.world < 1.0F;
				RE::VATS::GetSingleton()->SetMagicTimeSlowdown(slowed ? a_speeds.world : 0.0F, slowed ? *a_speeds.player : 0.0F);
			}
			RE::BSTimer::GetSingleton()->SetGlobalTimeMultiplier(a_speeds.world, true);
		}

		struct TimerUpdate
		{
			static void thunk(RE::BSTimer* a_timer, std::int32_t a_ticks)
			{
				std::optional<Speeds> due;
				{
					const std::scoped_lock guard(lock);
					due.swap(asked);
				}
				if (due) {
					Put(*due);
				}
				original(a_timer, a_ticks);
			}

			static inline REL::Relocation<decltype(thunk)> original;
		};

		// the place is taken only where it calls the timer of the game, or a hook another plugin put there
		inline bool CallsTheTimer(std::uintptr_t a_call)
		{
			if (*reinterpret_cast<const std::uint8_t*>(a_call) != kCall) {
				return false;
			}
			const auto target = a_call + kCallLength + *reinterpret_cast<const std::int32_t*>(a_call + 1);
			const auto code = REL::Module::get().segment(REL::Segment::textx);
			const bool inGame = target >= code.address() && target < code.address() + code.size();
			return !inGame || target == kTimerUpdate.address();
		}
	}

	// takes 14 bytes of the trampoline, which the plugin allocates
	inline bool Install()
	{
		using namespace Detail;
		if (!REL::Module::IsAE()) {
			return false;
		}
		const REL::Relocation<std::uintptr_t> call{ kFrame, kTimerCall };
		if (!CallsTheTimer(call.address())) {
			return false;
		}
		TimerUpdate::original = SKSE::GetTrampoline().write_call<5>(call.address(), TimerUpdate::thunk);
		atFrameStart = true;
		return true;
	}

	// put in at the start of the next frame, or at once where the start of a frame could not be hooked
	inline void Ask(const Speeds& a_speeds)
	{
		using namespace Detail;
		if (!atFrameStart) {
			Put(a_speeds);
			return;
		}
		const std::scoped_lock guard(lock);
		asked = a_speeds;
	}

	// the speed of the world as the next frame will have it
	inline float World()
	{
		using namespace Detail;
		const std::scoped_lock guard(lock);
		return asked ? asked->world : RE::BSTimer::QGlobalTimeMultiplier();
	}
}
