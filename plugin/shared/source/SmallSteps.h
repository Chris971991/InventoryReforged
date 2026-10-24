#pragma once

#include <Windows.h>

#include <algorithm>
#include <format>

// Havok pushes bodies apart and pulls joints together by tau times the error over the length of a step. The game
// keeps tau at 0.6 however short the step is, so in slowed time, with steps twenty times shorter, the same few
// millimetres of a fresh ragdoll in the floor or in itself come out twenty times faster, so a body killed or knocked
// down just before the world slowed jumps a man's height (docs/thrown-bodies.txt). Here tau is scaled by the share
// of a full step the step is, and the error goes away at the pace it has in normal time, only slowed with the rest
namespace SmallSteps
{
	namespace Detail
	{
		// in game version 1.7.104 the world update has just set up the solver and calls this on the Havok world and the
		// time of the frame before its steps, in one thread or many
		inline const REL::ID kWorldUpdate{ 77851 };
		inline constexpr std::size_t kBeforeSteps = 0x353;
		inline const REL::ID kWorldCall{ 61414 };
		inline const REL::ID kStep{ 389091 };
		inline const REL::ID kFullStep{ 389103 };
		inline constexpr std::uint8_t kCall = 0xE8;
		inline constexpr std::size_t kCallLength = 5;

		// the solver of hkpWorld as the world update writes it
		inline constexpr std::ptrdiff_t kTau = 0x2C4;
		inline constexpr std::ptrdiff_t kDamping = 0x2C8;
		inline constexpr std::ptrdiff_t kFrictionTau = 0x2CC;
		inline constexpr std::ptrdiff_t kDampingByTau = 0x310;
		inline constexpr std::ptrdiff_t kTauByDamping = 0x314;
		inline constexpr float kShortestShare = 0.001F;

		// the game sets friction tau once, it is kept from a full step to scale it from
		inline float frictionTauOfFullStep = 0.0F;

		inline float& At(RE::hkpWorld* a_world, std::ptrdiff_t a_offset)
		{
			return *reinterpret_cast<float*>(reinterpret_cast<std::uint8_t*>(a_world) + a_offset);
		}

		inline void Scale(RE::hkpWorld* a_world)
		{
			const float step = *REL::Relocation<float*>{ kStep };
			const float fullStep = *REL::Relocation<float*>{ kFullStep };
			if (!a_world || fullStep <= 0.0F || step <= 0.0F) {
				return;
			}
			const float share = std::clamp(step / fullStep, kShortestShare, 1.0F);
			float& frictionTau = At(a_world, kFrictionTau);
			if (share >= 1.0F) {
				frictionTauOfFullStep = frictionTau;
				return;
			}
			float& tau = At(a_world, kTau);
			const float damping = At(a_world, kDamping);
			tau *= share;
			At(a_world, kDampingByTau) = damping / tau;
			At(a_world, kTauByDamping) = tau / damping;
			if (frictionTauOfFullStep > 0.0F) {
				frictionTau = frictionTauOfFullStep * share;
			}
		}

		// the call sets the time the simulation steps up to (function 61796 adds the frame time to its current time),
		// so the frame time has to reach it untouched
		struct BeforeSteps
		{
			static void thunk(RE::hkpWorld* a_world, float a_frameTime)
			{
				Scale(a_world);
				original(a_world, a_frameTime);
			}

			static inline REL::Relocation<decltype(thunk)> original;
		};

		inline bool CallsTheWorld(std::uintptr_t a_call)
		{
			if (*reinterpret_cast<const std::uint8_t*>(a_call) != kCall) {
				return false;
			}
			const auto target = a_call + kCallLength + *reinterpret_cast<const std::int32_t*>(a_call + 1);
			const auto code = REL::Module::get().segment(REL::Segment::textx);
			const bool inGame = target >= code.address() && target < code.address() + code.size();
			return !inGame || target == kWorldCall.address();
		}

		// Two of our plugins in one game would scale tau twice, the first to load takes the place
		inline bool FirstInThisGame()
		{
			const auto name = std::format(L"Local\\SkyrimSmallSteps{}", GetCurrentProcessId());
			const HANDLE mark = CreateMutexW(nullptr, FALSE, name.c_str());
			return mark && GetLastError() != ERROR_ALREADY_EXISTS;
		}
	}

	// takes 14 bytes of the trampoline, which the plugin allocates. False where another plugin of ours has it
	// or the place is not known for this version
	inline bool Install()
	{
		using namespace Detail;
		if (!REL::Module::IsAE()) {
			return false;
		}
		const REL::Relocation<std::uintptr_t> call{ kWorldUpdate, kBeforeSteps };
		if (!CallsTheWorld(call.address()) || !FirstInThisGame()) {
			return false;
		}
		BeforeSteps::original = SKSE::GetTrampoline().write_call<5>(call.address(), BeforeSteps::thunk);
		return true;
	}
}
