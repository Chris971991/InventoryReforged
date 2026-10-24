#include "PCH.h"

#include "PlayerSize.h"

namespace PlayerSize
{
	namespace
	{
		// The length of a bone is the same in every pose, so the body is measured along its bones
		// and the menu can be opened while the player runs, sneaks or sits
		constexpr std::array kLeg{ "NPC L Foot [Lft ]"sv, "NPC L Calf [LClf]"sv, "NPC L Thigh [LThg]"sv };
		constexpr std::array kBack{ "NPC Pelvis [Pelv]"sv, "NPC Spine [Spn0]"sv, "NPC Spine1 [Spn1]"sv, "NPC Spine2 [Spn2]"sv, "NPC Neck [Neck]"sv, "NPC Head [Head]"sv };
		constexpr std::array kShoulders{ "NPC L UpperArm [LUar]"sv, "NPC R UpperArm [RUar]"sv };
		constexpr std::array kScaled{ "NPC Head [Head]"sv, "NPC L Foot [Lft ]"sv, "NPC L UpperArm [LUar]"sv };

		// How bones turn into the body around them, taken from the skeleton the game ships with
		// Its leg and back make 121.6 units of a body 131.8 tall, its shoulder joints are 27.7 apart
		// and its hands hang 46 apart. Both sizes leave some room for hair, a helmet and a shield
		constexpr float kHeightPerBones = 1.14F;
		constexpr float kWidthPerShoulders = 2.4F;
		constexpr float kWidthPerHeight = 0.5F;
		// a body measured before the game placed its bones has every joint in one spot and comes out flat
		constexpr float kLeast = 1.0F;

		RE::NiAVObject* Joint(RE::NiAVObject& a_body, std::string_view a_name)
		{
			return a_body.GetObjectByName(RE::BSFixedString(a_name));
		}

		std::optional<float> Length(RE::NiAVObject& a_body, std::span<const std::string_view> a_joints)
		{
			float length = 0.0F;
			const RE::NiAVObject* previous = nullptr;
			for (const auto name : a_joints) {
				const auto* joint = Joint(a_body, name);
				if (!joint) {
					return std::nullopt;
				}
				if (previous) {
					length += joint->world.translate.GetDistance(previous->world.translate);
				}
				previous = joint;
			}
			return length;
		}

		Size Told(RE::Actor& a_player)
		{
			const float height = a_player.GetHeight();
			return { height, height * kWidthPerHeight };
		}

		bool Usable(const Size& a_size)
		{
			return a_size.height >= kLeast && a_size.width >= kLeast;
		}

		Size Measure(RE::Actor& a_player, RE::NiAVObject& a_body)
		{
			const auto leg = Length(a_body, kLeg);
			const auto back = Length(a_body, kBack);
			const auto shoulders = Length(a_body, kShoulders);

			if (leg && back && shoulders) {
				return { (*leg + *back) * kHeightPerBones, *shoulders * kWidthPerShoulders };
			}
			// a skeleton without these bones belongs to a beast form, the height the game tells has to do
			return Told(a_player);
		}

		// Everything the size comes from: the race and the sex choose the skeleton,
		// and a scale set on any bone above a joint shows in the scale of that joint
		std::size_t Character(RE::Actor& a_player, RE::NiAVObject& a_body)
		{
			const auto* base = a_player.GetActorBase();
			std::size_t hash = a_player.GetRace()->GetFormID();
			const auto add = [&](float a_value) {
				hash = hash * 31 + static_cast<std::size_t>(std::lround(a_value * 1000.0F));
			};

			add(static_cast<float>(base->GetSex()));
			add(base->weight);
			add(a_body.world.scale);
			for (const auto name : kScaled) {
				if (const auto* joint = Joint(a_body, name)) {
					add(joint->world.scale);
				}
			}
			return hash;
		}
	}

	Size Of(RE::Actor& a_player, RE::NiAVObject& a_body)
	{
		static std::unordered_map<std::size_t, Size> known;

		const auto character = Character(a_player, a_body);
		if (const auto found = known.find(character); found != known.end()) {
			return found->second;
		}
		const auto measured = Measure(a_player, a_body);
		// a flat one isn't kept, the next doll measures the body again
		if (!Usable(measured)) {
			const auto told = Told(a_player);
			SKSE::log::info("character {:X}: measured {:.1f} by {:.1f} before its bones were placed, the game's height {:.1f} stands in", character, measured.height,
				measured.width, told.height);
			return Usable(told) ? told : Size{ kLeast, kLeast };
		}
		known.emplace(character, measured);
		SKSE::log::info("character {:X}: height {:.1f}, width {:.1f}", character, measured.height, measured.width);
		return measured;
	}
}
