#include "PCH.h"

#include "Copy.h"

#include "Profile.h"
#include "Scene.h"

namespace Copy
{
	namespace
	{
		// a part animated by sequences of its own, the bound bow or the wings of a mod's sword
		bool Sequenced(const RE::NiAVObject& a_part)
		{
			for (auto* controller = a_part.controllers.get(); controller; controller = controller->next.get()) {
				if (netimmerse_cast<RE::NiControllerManager*>(controller)) {
					return true;
				}
			}
			return false;
		}

		bool animatedTold = false;

		// The player's bones carry the bodies of the ragdoll and a copy that shares them takes the game
		// down when it's released. So the copy is made while the bodies are off the bones.
		// The game never clones a model that already plays its sequences, its clone of one calls through an empty
		// pointer and crashes (a player's log, a sword with flapping wings, on the second open after it was put
		// on). Those parts are copied with their controllers off and stand still on the doll
		RE::NiPointer<RE::NiAVObject> WithoutBodies(RE::NiAVObject& a_body)
		{
			std::vector<std::pair<RE::NiAVObject*, RE::NiPointer<RE::NiCollisionObject>>> bodies;
			std::vector<std::pair<RE::NiAVObject*, RE::NiPointer<RE::NiTimeController>>> animated;
			Scene::Visit(a_body, [&](RE::NiAVObject& a_part) {
				if (a_part.collisionObject) {
					bodies.emplace_back(&a_part, a_part.collisionObject);
					a_part.collisionObject.reset();
				}
				if (Sequenced(a_part)) {
					animated.emplace_back(&a_part, a_part.controllers);
					a_part.controllers.reset();
				}
			});

			RE::NiPointer<RE::NiAVObject> made{ static_cast<RE::NiAVObject*>(a_body.Clone()) };

			for (const auto& [part, body] : bodies) {
				part->collisionObject = body;
			}
			for (const auto& [part, controllers] : animated) {
				part->controllers = controllers;
			}
			if (!animated.empty() && !std::exchange(animatedTold, true)) {
				logger::info("{} parts of the body play sequences of their own, the doll gets them still", animated.size());
			}
			return made;
		}

		struct Pieces
		{
			std::vector<RE::BSFlattenedBoneTree*> trees;
			std::vector<RE::NiSkinInstance*> skins;
		};

		Pieces PiecesOf(RE::NiAVObject& a_root)
		{
			Pieces pieces;
			Scene::Visit(a_root, [&](RE::NiAVObject& a_part) {
				if (auto* tree = netimmerse_cast<RE::BSFlattenedBoneTree*>(&a_part)) {
					pieces.trees.push_back(tree);
				}
				auto* shape = a_part.AsGeometry();
				if (auto* skin = shape ? shape->GetGeometryRuntimeData().skinInstance.get() : nullptr) {
					pieces.skins.push_back(skin);
				}
			});
			return pieces;
		}

		// the bone of a flat list a skin of the body draws from, as its place in that list
		std::optional<std::pair<std::size_t, std::size_t>> FlatBone(const std::vector<RE::BSFlattenedBoneTree*>& a_trees, const RE::NiTransform* a_at)
		{
			constexpr auto kStep = sizeof(RE::BSFlattenedBoneTree::BoneEntry);
			const auto at = reinterpret_cast<std::uintptr_t>(a_at);
			for (std::size_t t = 0; t < a_trees.size(); ++t) {
				const auto& lists = a_trees[t]->GetRuntimeData();
				if (!lists.boneEntries) {
					continue;
				}
				const auto first = reinterpret_cast<std::uintptr_t>(&lists.boneEntries[0].world);
				if (at >= first && at < first + lists.numBones * kStep && (at - first) % kStep == 0) {
					return std::pair{ t, (at - first) / kStep };
				}
			}
			return std::nullopt;
		}

		// Bones with no node of their own (fingers, wrists and more on the game's skeleton) live only in the flat
		// list of the skeleton, and a skin draws them straight from it. The clone leaves those pointers of a copy
		// empty, so gauntlets and bracers fell apart on the doll. They go to the same bones of the copy's own list
		void LinkFlatBones(RE::NiAVObject& a_body, RE::NiAVObject& a_copy)
		{
			const auto live = PiecesOf(a_body);
			const auto copied = PiecesOf(a_copy);
			if (live.trees.size() != copied.trees.size() || live.skins.size() != copied.skins.size()) {
				return;
			}
			for (std::size_t s = 0; s < live.skins.size(); ++s) {
				const auto* from = live.skins[s];
				auto* to = copied.skins[s];
				if (!from->skinData || !to->skinData || from->skinData->bones != to->skinData->bones || !from->boneWorldTransforms || !to->boneWorldTransforms) {
					continue;
				}
				for (std::uint32_t i = 0; i < to->skinData->bones; ++i) {
					if (to->bones[i] || to->boneWorldTransforms[i]) {
						continue;
					}
					const auto bone = FlatBone(live.trees, from->boneWorldTransforms[i]);
					if (!bone) {
						continue;
					}
					auto& lists = copied.trees[bone->first]->GetRuntimeData();
					if (lists.boneEntries && bone->second < lists.numBones) {
						to->boneWorldTransforms[i] = &lists.boneEntries[bone->second].world;
					}
				}
			}
		}

		std::atomic<std::uint32_t> bonesFreed{ 0 };
		std::atomic<bool> oddToldOnce{ false };

		void FreeBones(RE::BSFlattenedBoneTree& a_tree)
		{
			auto& lists = a_tree.GetRuntimeData();
			auto* entries = lists.boneEntries;
			if (entries) {
				auto* block = reinterpret_cast<std::uint64_t*>(entries) - 1;
				if (*block == lists.numBones) {
					for (std::uint32_t i = 0; i < lists.numBones; ++i) {
						std::destroy_at(std::addressof(entries[i].nodeName));
					}
					RE::free(block);
					Profile::Freed(Profile::Held::kBoneLists);
					const auto freed = ++bonesFreed;
					if (freed == 1 || freed % 100 == 0) {
						logger::info("bone lists of body copies freed: {}, {} bones in this one", freed, lists.numBones);
					}
				} else if (!oddToldOnce.exchange(true)) {
					// a list without the count in front isn't the clone's, it stays in memory as it did before
					logger::warn("a body copy has a bone list of {} bones without its count, it's left in memory", lists.numBones);
				}
			}
			lists.numBones = 0;
			lists.numPopulatedBones = 0;
			lists.boneEntries = nullptr;
		}
	}

	RE::NiPointer<RE::NiAVObject> Body(RE::NiAVObject& a_body)
	{
		IR_BRACKET("the body {} {} is copied", a_body.name.c_str() ? a_body.name.c_str() : "", static_cast<const void*>(&a_body));
		auto made = WithoutBodies(a_body);
		if (!made) {
			return nullptr;
		}
		LinkFlatBones(a_body, *made);
		// The live body can be faded near the camera, the copy shows it whole. A follower walking up to the camera is
		// being faded out: the game marks their body not visible and puts 0.98 into the shaders, a copy of it was
		// never drawn and the doll and the outfit's slots stood empty.
		// A node the body keeps switched off stays off on the copy: mods that show carried gear on the body switch
		// their own nodes off (Immersive Equipment Displays' shield on the back while the shield is in hand, gear
		// marked invisible), and the copy switching every node on showed both shields. Only the switches of the camera
		// go: in first person the player's body and the bones of its skeleton, first person mods keep them out of sight
		// The body hidden at its root counts as first person too: the inventory opened through the tween menu finds the
		// camera in the tween menu's state, not first person, while the body is still hidden for first person. Its
		// copies stayed hidden then, nothing of a worn piece was measured and the slots of worn gear came out small
		auto* player = RE::PlayerCharacter::GetSingleton();
		const bool firstPerson = player && &a_body == player->Get3D(false) && (RE::PlayerCamera::GetSingleton()->IsInFirstPerson() || a_body.GetAppCulled());
		std::int64_t trees = 0;
		Scene::Visit(*made, [&](RE::NiAVObject& a_part) {
			a_part.GetFlags().reset(RE::NiAVObject::Flag::kNotVisible);
			if (auto* fading = a_part.AsFadeNode()) {
				fading->GetRuntimeData().currentFade = 1.0F;
			}
			if (auto* shape = a_part.AsGeometry(); shape && shape->GetGeometryRuntimeData().shaderProperty) {
				shape->GetGeometryRuntimeData().shaderProperty->alpha = 1.0F;
			}
			if (firstPerson && a_part.AsNode() && (&a_part == made.get() || std::string_view(a_part.name.c_str()).starts_with("NPC "))) {
				a_part.SetAppCulled(false);
			}
			if (netimmerse_cast<RE::BSFlattenedBoneTree*>(&a_part)) {
				++trees;
			}
		});
		Profile::Made(Profile::Held::kBodyCopies);
		Profile::Made(Profile::Held::kBoneLists, trees);
		return made;
	}

	// The skeleton root keeps a flat list of its bones for the animation system. The game builds that list
	// as an aligned block and frees it so, but its clone makes the list of a copy as a counted array, the count
	// in front of it (function 70730 of 1.7.104). Freed the aligned way the copy broke the heap, so here the
	// list of a copy goes the way it was made: names released, the whole block freed. The map of bone names
	// is a real copy and the game frees it right
	void Release(RE::NiPointer<RE::NiAVObject>& a_copy)
	{
		if (!a_copy) {
			return;
		}
		IR_BRACKET("the copy {} {} is let go, held {} times", a_copy->name.c_str() ? a_copy->name.c_str() : "", static_cast<const void*>(a_copy.get()), a_copy->GetRefCount());
		bool body = false;
		Scene::Visit(*a_copy, [&](RE::NiAVObject& a_part) {
			if (auto* tree = netimmerse_cast<RE::BSFlattenedBoneTree*>(&a_part)) {
				FreeBones(*tree);
				body = true;
			}
		});
		if (body) {
			Profile::Freed(Profile::Held::kBodyCopies);
		}
		// the copy itself goes with the last pointer to it, one kept somewhere else keeps it all in memory
		if (a_copy->GetRefCount() > 1) {
			Profile::Made(Profile::Held::kHeldElsewhere);
		}
		a_copy.reset();
	}

	std::size_t Makeup(RE::NiAVObject& a_body)
	{
		std::size_t makeup = 0;
		// A new shape can land where a freed one lay, its name keeps two helmets apart then. Body mods change the look
		// in the same shapes a moment after a body is built (morphs, skin overlays, a race's body put on later), their
		// geometry, skin and material go in too
		Scene::Visit(a_body, [&](RE::NiAVObject& a_part) {
			if (auto* shape = a_part.AsGeometry()) {
				const auto& runtime = shape->GetGeometryRuntimeData();
				for (const auto part : { reinterpret_cast<std::size_t>(&a_part), reinterpret_cast<std::size_t>(a_part.name.data()), reinterpret_cast<std::size_t>(runtime.rendererData),
						 reinterpret_cast<std::size_t>(runtime.skinInstance.get()), reinterpret_cast<std::size_t>(runtime.shaderProperty.get()) }) {
					makeup = makeup * 31 + part;
				}
			}
		});
		return makeup;
	}

	bool IsSkin(RE::NiAVObject& a_geometry)
	{
		auto* shape = a_geometry.AsGeometry();
		if (!shape) {
			return false;
		}
		auto* lighting = netimmerse_cast<RE::BSLightingShaderProperty*>(shape->GetGeometryRuntimeData().shaderProperty.get());
		const auto* material = lighting ? lighting->material : nullptr;
		if (!material) {
			return false;
		}
		const auto feature = material->GetFeature();
		return feature == RE::BSShaderMaterial::Feature::kFaceGen || feature == RE::BSShaderMaterial::Feature::kFaceGenRGBTint;
	}
}
