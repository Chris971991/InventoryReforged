#include "PCH.h"

#include "WornView.h"

#include "Copy.h"
#include "Pose.h"
#include "Scene.h"
#include "Settings.h"
#include "Subject.h"

namespace WornView
{
	namespace
	{
		// share of the square the piece fills, its outline is measured loosely from bone spheres
		constexpr float kFill = 1.05F;

		using Path = std::vector<std::uint16_t>;

		// child indices from the body down to a part, so the same part can be found in a copy
		std::optional<Path> PathTo(RE::NiAVObject& a_body, RE::NiAVObject& a_part)
		{
			Path path;
			for (auto* at = &a_part; at != &a_body; at = at->parent) {
				auto* parent = at->parent;
				if (!parent) {
					return std::nullopt;
				}
				const auto& children = parent->GetChildren();
				std::uint16_t index = 0;
				while (index < children.size() && children[index].get() != at) {
					++index;
				}
				if (index == children.size()) {
					return std::nullopt;
				}
				path.insert(path.begin(), index);
			}
			return path;
		}

		RE::NiAVObject* Follow(RE::NiAVObject& a_copy, const Path& a_path)
		{
			auto* at = &a_copy;
			for (const auto index : a_path) {
				auto* node = at->AsNode();
				if (!node || index >= node->GetChildren().size()) {
					return nullptr;
				}
				at = node->GetChildren()[index].get();
			}
			return at;
		}

		bool Under(RE::NiAVObject& a_part, const std::vector<RE::NiAVObject*>& a_pieces)
		{
			for (auto* at = &a_part; at; at = at->parent) {
				if (std::ranges::find(a_pieces, at) != a_pieces.end()) {
					return true;
				}
			}
			return false;
		}

		// Gloves on hands that hang by the hips stand far apart and each looks tiny in a slot. The left arm of
		// the copy is folded to nothing from the collarbone, so the right glove alone fills the slot. From the
		// elbow wasn't enough, gauntlets have vertices on the upper arm too and the slot was framed for both, and
		// Ashe's long gloves reach the collarbone
		constexpr auto kLeftArm = "NPC L Clavicle [LClv]";
		constexpr auto kLeftUpperArm = "NPC L UpperArm [LUar]";
		constexpr float kFolded = 0.0001F;
		constexpr std::uint32_t kHands = 0x8 | 0x10;
		constexpr std::uint32_t kBody = 0x4;

		bool OnlyOnHands(RE::TESForm& a_item)
		{
			const auto* armor = a_item.As<RE::TESObjectARMO>();
			const auto parts = armor ? armor->GetSlotMask().underlying() : 0;
			return (parts & kHands) != 0 && (parts & kBody) == 0;
		}

		bool ForRace(const RE::TESObjectARMA& a_addon, const RE::TESRace* a_race)
		{
			return a_addon.race == a_race || std::ranges::find(a_addon.additionalRaces, a_race) != a_addon.additionalRaces.end();
		}

		// the skin of a loaded piece moves with the bones of its own file, here it's tied to the copy's bones of
		// the same names. Bones with no node of their own on the game's skeleton live in the flat list of the copy
		void Bind(RE::NiAVObject& a_piece, RE::NiAVObject& a_copy)
		{
			RE::BSFlattenedBoneTree* tree = nullptr;
			Scene::Visit(a_copy, [&](RE::NiAVObject& a_part) {
				if (!tree) {
					tree = netimmerse_cast<RE::BSFlattenedBoneTree*>(&a_part);
				}
			});
			Scene::Visit(a_piece, [&](RE::NiAVObject& a_part) {
				auto* shape = a_part.AsGeometry();
				auto* skin = shape ? shape->GetGeometryRuntimeData().skinInstance.get() : nullptr;
				if (!skin || !skin->skinData || !skin->bones || !skin->boneWorldTransforms) {
					return;
				}
				for (std::uint32_t i = 0; i < skin->skinData->bones; ++i) {
					const auto* own = skin->bones[i];
					if (!own) {
						continue;
					}
					if (auto* bone = a_copy.GetObjectByName(own->name)) {
						skin->bones[i] = bone;
						skin->boneWorldTransforms[i] = &bone->world;
						continue;
					}
					if (!tree) {
						continue;
					}
					auto& lists = tree->GetRuntimeData();
					const auto found = lists.boneMap.find(own->name);
					if (found != lists.boneMap.end() && lists.boneEntries && found->second >= 0 && static_cast<std::uint32_t>(found->second) < lists.numBones) {
						skin->bones[i] = nullptr;
						skin->boneWorldTransforms[i] = &lists.boneEntries[found->second].world;
					}
				}
				skin->rootParent = &a_copy;
			});
		}

		// Parts of a piece numbered from 130 up are off in its file and the game switches them on when it puts the
		// piece on a body. Vanilla helmets, hoods and hats are made of nothing else and stayed invisible in a hidden
		// slot. Gore caps from 150 up stay off
		constexpr std::uint16_t kGoreParts = 150;

		void ShowParts(RE::NiAVObject& a_piece)
		{
			Scene::Visit(a_piece, [](RE::NiAVObject& a_part) {
				auto* shape = a_part.AsGeometry();
				auto* skin = shape ? netimmerse_cast<RE::BSDismemberSkinInstance*>(shape->GetGeometryRuntimeData().skinInstance.get()) : nullptr;
				if (!skin) {
					return;
				}
				const auto& parts = skin->GetRuntimeData();
				for (std::int32_t i = 0; i < parts.numPartitions; ++i) {
					if (!parts.partitions[i].editorVisible && parts.partitions[i].slot < kGoreParts) {
						skin->UpdateDismemberPartion(parts.partitions[i].slot, true);
					}
				}
			});
		}

		// An addon can dress its model in other textures, a robe of another colour is the same model. The game
		// swaps them in when it puts the piece on, here the material of the shape is made anew with the new
		// texture set the way the papyrus extender swaps texture sets
		void SwapTextures(RE::NiAVObject& a_piece, const RE::TESModelTextureSwap& a_model)
		{
			for (std::uint32_t i = 0; i < a_model.numAlternateTextures; ++i) {
				const auto& swap = a_model.alternateTextures[i];
				if (!swap.textureSet) {
					continue;
				}
				Scene::Visit(a_piece, [&](RE::NiAVObject& a_part) {
					auto* shape = a_part.AsGeometry();
					if (!shape || a_part.name != swap.name3D) {
						return;
					}
					auto* lighting = netimmerse_cast<RE::BSLightingShaderProperty*>(shape->GetGeometryRuntimeData().shaderProperty.get());
					auto* material = lighting ? static_cast<RE::BSLightingShaderMaterialBase*>(lighting->material) : nullptr;
					auto* fresh = material ? static_cast<RE::BSLightingShaderMaterialBase*>(material->Create()) : nullptr;
					if (!fresh) {
						return;
					}
					fresh->CopyMembers(material);
					fresh->ClearTextures();
					fresh->OnLoadTextureSet(0, swap.textureSet);
					// the property keeps its own copy of the material
					lighting->SetMaterial(fresh, true);
					lighting->SetupGeometry(shape);
					lighting->FinishSetupGeometry(shape);
					fresh->~BSLightingShaderMaterialBase();
					RE::free(fresh);
				});
			}
		}

		// A hidden piece never goes onto the body, so a copy of the body has none of it. Its worn model for the
		// player's race is loaded and tied to the copy's bones the way the game ties a piece to a skeleton, a
		// rigid one hangs from the bone its file names
		std::vector<RE::NiAVObject*> Dress(RE::NiAVObject& a_copy, RE::TESObjectARMO& a_armor, RE::Actor& a_player)
		{
			std::vector<RE::NiAVObject*> pieces;
			auto* root = a_copy.AsNode();
			const auto* race = a_player.GetRace();
			const auto sex = Female(a_player) ? RE::SEX::kFemale : RE::SEX::kMale;
			if (!root || !race) {
				return pieces;
			}
			for (auto* addon : a_armor.armorAddons) {
				if (!addon || !ForRace(*addon, race)) {
					continue;
				}
				const auto* model = &addon->bipedModels[sex];
				const char* path = model->GetModel();
				if (!path || !*path) {
					model = &addon->bipedModels[RE::SEX::kMale];
					path = model->GetModel();
				}
				RE::NiPointer<RE::NiNode> loaded;
				const RE::BSModelDB::DBTraits::ArgsType how;
				IR_BRACKET("the worn model {} of {:08X} is read, copied and put on a body copy", path ? path : "", a_armor.GetFormID());
				if (!path || !*path || RE::BSModelDB::Demand(path, loaded, how) != RE::BSResource::ErrorCode::kNone || !loaded) {
					continue;
				}
				RE::NiPointer<RE::NiAVObject> made{ static_cast<RE::NiAVObject*>(loaded->Clone()) };
				if (!made) {
					continue;
				}
				Scene::DropCollision(*made);
				ShowParts(*made);
				SwapTextures(*made, *model);
				Bind(*made, a_copy);
				auto* holder = root;
				if (const auto* hang = made->GetExtraData<RE::NiStringExtraData>("Prn"); hang && hang->value) {
					if (auto* bone = a_copy.GetObjectByName(hang->value); bone && bone->AsNode()) {
						holder = bone->AsNode();
					}
				}
				holder->AttachChild(made.get(), true);
				pieces.push_back(made.get());
			}
			return pieces;
		}

		RE::NiMatrix3 FacingCamera(float a_degrees)
		{
			const auto camera = Scene::Camera();
			const auto facing = Scene::Columns(camera.up.Cross(camera.look), camera.look * -1.0F, camera.up);
			return Scene::About(camera.up, a_degrees * std::numbers::pi_v<float> / 180.0F) * facing;
		}
	}

	RE::NiPointer<RE::NiAVObject> Make(RE::TESForm& a_item)
	{
		const auto wearer = Subject::Get();
		auto* player = wearer.get();
		auto* body = player ? player->Get3D(false) : nullptr;
		if (!body) {
			return nullptr;
		}
		const auto& biped = player->GetBiped(false);
		if (!body || !biped) {
			return nullptr;
		}

		std::vector<Path> paths;
		for (const auto& object : biped->objects) {
			if (object.item != &a_item || !object.partClone) {
				continue;
			}
			if (auto path = PathTo(*body, *object.partClone)) {
				if (std::ranges::find(paths, *path) == paths.end()) {
					paths.push_back(std::move(*path));
				}
			}
		}
		auto* armor = a_item.As<RE::TESObjectARMO>();
		if (paths.empty() && !armor) {
			return nullptr;
		}

		auto copy = Copy::Body(*body);
		if (!copy) {
			return nullptr;
		}
		std::vector<RE::NiAVObject*> pieces;
		for (const auto& path : paths) {
			if (auto* piece = Follow(*copy, path)) {
				pieces.push_back(piece);
			}
		}
		// a piece the body doesn't wear, a hidden one, is put on the copy
		if (pieces.empty() && armor) {
			pieces = Dress(*copy, *armor, *player);
		}
		if (pieces.empty()) {
			Copy::Release(copy);
			return nullptr;
		}
		if (Pose::KeepsWorldPose() || !Pose::Stand(*copy, Female(*player))) {
			Pose::LookAhead(*copy, *player);
		}
		if (OnlyOnHands(a_item)) {
			auto* arm = copy->GetObjectByName(kLeftArm);
			if (auto* upper = arm ? arm : copy->GetObjectByName(kLeftUpperArm)) {
				upper->local.scale = kFolded;
			}
		}

		std::size_t shown = 0;
		Scene::Visit(*copy, [&](RE::NiAVObject& a_part) {
			if (!a_part.AsGeometry()) {
				return;
			}
			const bool keep = Under(a_part, pieces) && !Copy::IsSkin(a_part);
			if (!keep) {
				a_part.SetAppCulled(true);
			} else if (!a_part.GetAppCulled()) {
				++shown;
			}
		});
		if (shown == 0) {
			Copy::Release(copy);
			return nullptr;
		}
		return copy;
	}

	Fitted FitAround(RE::NiAVObject& a_model, const RE::NiMatrix3& a_facing, float a_centerX, float a_centerY, float a_size)
	{
		RE::NiUpdateData update;
		const float start = a_model.local.scale;
		a_model.local.translate = {};
		a_model.local.rotate = a_facing;
		a_model.Update(update);

		auto extent = Scene::Measure(a_model);
		if (!extent.found) {
			extent.middle = a_model.worldBound.center;
			extent.wide = extent.high = 2.0F * std::max(a_model.worldBound.radius, 1.0F);
		}
		const float grow = kFill * Scene::Length(a_size) / std::max({ extent.wide, extent.high, 0.01F });

		Fitted fitted;
		fitted.facing = a_facing;
		fitted.scale = start * grow;
		fitted.middle = a_facing.Transpose() * (extent.middle / start);

		a_model.local.scale = fitted.scale;
		a_model.local.translate = Scene::Point(a_centerX, a_centerY) - extent.middle * grow;
		a_model.Update(update);
		return fitted;
	}

	void Fit(RE::NiAVObject& a_model, float a_centerX, float a_centerY, float a_size, float a_degrees)
	{
		FitAround(a_model, FacingCamera(a_degrees), a_centerX, a_centerY, a_size);
	}
}
