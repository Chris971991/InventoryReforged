#include "PCH.h"

#include "Pose.h"

#include "Scene.h"
#include "Settings.h"
#include "StandingPose.h"

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#include <Windows.h>

namespace Pose
{
	namespace
	{
		constexpr std::string_view kBonePrefix = "NPC ";
		// riding or sitting moves the body down, these get the place of the pose as well as its turn
		constexpr std::array kPlaced{ "NPC Root [Root]"sv, "NPC COM [COM ]"sv };
	}

	bool Stand(RE::NiAVObject& a_copy, bool a_female)
	{
		std::size_t posed = 0;
		const std::span<const StandingPose::Bone> bones = a_female && !StandingPose::kFemale.empty() ? std::span<const StandingPose::Bone>(StandingPose::kFemale) : std::span<const StandingPose::Bone>(StandingPose::kMale);
		for (const auto& bone : bones) {
			auto* node = a_copy.GetObjectByName(RE::BSFixedString(bone.name));
			if (!node) {
				continue;
			}
			for (std::size_t row = 0; row < 3; ++row) {
				for (std::size_t column = 0; column < 3; ++column) {
					node->local.rotate.entry[row][column] = bone.turn[row * 3 + column];
				}
			}
			if (std::ranges::find(kPlaced, bone.name) != kPlaced.end()) {
				node->local.translate = { bone.place[0], bone.place[1], bone.place[2] };
			}
			++posed;
		}
		return posed > 0;
	}

	namespace
	{
		// the skeleton file of the actor's race and sex as the game keeps it, main thread only
		RE::NiPointer<RE::NiNode> AtRest(RE::Actor& a_actor)
		{
			auto* race = a_actor.GetRace();
			const auto* base = a_actor.GetActorBase();
			const char* path = race && base ? race->skeletonModels[base->GetSex()].GetModel() : nullptr;
			RE::NiPointer<RE::NiNode> atRest;
			const RE::BSModelDB::DBTraits::ArgsType how;
			IR_BRACKET("the skeleton {} is read", path ? path : "");
			if (!path || !*path || RE::BSModelDB::Demand(path, atRest, how) != RE::BSResource::ErrorCode::kNone) {
				return nullptr;
			}
			return atRest;
		}
	}

	bool KeepsWorldPose()
	{
		const auto* camera = RE::PlayerCamera::GetSingleton();
		return Settings::Get().pose == Settings::Pose::kAsInWorld && !(camera && camera->IsInFirstPerson());
	}

	void LookAhead(RE::NiAVObject& a_copy, RE::Actor& a_player)
	{
		static constexpr std::array turning{ "NPC Spine [Spn0]", "NPC Spine1 [Spn1]", "NPC Spine2 [Spn2]", "NPC Neck [Neck]", "NPC Head [Head]" };

		const auto atRest = AtRest(a_player);
		if (!atRest) {
			return;
		}
		for (const char* name : turning) {
			auto* bone = a_copy.GetObjectByName(name);
			const auto* resting = atRest->GetObjectByName(name);
			if (bone && resting) {
				bone->local.rotate = resting->local.rotate;
			}
		}
	}

	void Dump(RE::NiAVObject& a_body, bool a_female)
	{
		const auto folder = SKSE::log::log_directory();
		if (!folder) {
			return;
		}
		std::ofstream file(*folder / (a_female ? "InventoryReforged-pose-female.txt" : "InventoryReforged-pose-male.txt"));
		std::size_t bones = 0;
		Scene::Visit(a_body, [&](RE::NiAVObject& a_part) {
			const std::string_view name = a_part.name.c_str() ? a_part.name.c_str() : "";
			if (!a_part.AsNode() || !name.starts_with(kBonePrefix)) {
				return;
			}
			const auto& turn = a_part.local.rotate.entry;
			const auto& place = a_part.local.translate;
			file << name << '|';
			for (const auto& row : turn) {
				file << std::format("{:.6f} {:.6f} {:.6f} ", row[0], row[1], row[2]);
			}
			file << std::format("|{:.4f} {:.4f} {:.4f}\n", place.x, place.y, place.z);
			++bones;
		});
		logger::info("the pose of {} bones is written for a {} body", bones, a_female ? "female" : "male");
	}

	void Tell(RE::NiAVObject& a_body, RE::NiAVObject& a_copy, RE::Actor& a_shown)
	{
		static constexpr std::array bones{ "NPC Pelvis [Pelv]", "NPC Spine [Spn0]", "NPC Spine1 [Spn1]", "NPC Spine2 [Spn2]", "NPC Neck [Neck]",
			"NPC Head [Head]", "NPC L Clavicle [LClv]", "NPC L UpperArm [LUar]", "NPC L Forearm [LLar]", "NPC L Hand [LHnd]", "NPC L Thigh [LThg]",
			"NPC L Calf [LClf]", "NPC L Foot [Lft ]" };
		static std::string told;
		const auto atRest = AtRest(a_shown);
		// each bone as its offset from its parent and its own size, the parts of a reading that tell a squeezed spine
		const auto of = [](RE::NiAVObject* a_root, const char* a_name) {
			const auto* bone = a_root ? a_root->GetObjectByName(a_name) : nullptr;
			return bone ? std::format("{:.1f}x{:.2f}", bone->local.translate.Length(), bone->local.scale) : std::string("-");
		};
		std::string reading;
		for (const char* name : bones) {
			reading += std::format(" {} {}/{}/{}", std::string_view(name).substr(4), of(&a_body, name), of(&a_copy, name), of(atRest.get(), name));
		}
		const auto* camera = RE::PlayerCamera::GetSingleton();
		const bool firstPerson = camera && camera->IsInFirstPerson();
		const bool improved = GetModuleHandleW(L"ImprovedCameraSE.dll") != nullptr;
		reading += std::format("; {} person, Improved Camera {}, scale {:.2f}, the doll {}", firstPerson ? "first" : "third", improved ? "loaded" : "absent", a_shown.GetScale(),
			KeepsWorldPose() ? "as in the world" : "standing");
		if (reading == told) {
			return;
		}
		told = reading;
		logger::info("bones of {}, offset x size in the body/doll/race skeleton:{}", a_shown.GetName(), reading);
	}
}
