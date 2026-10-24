#include "PCH.h"

#include "Acquired.h"
#include "Api.h"
#include "Companions.h"
#include "Eyes.h"
#include "Hidden.h"
#include "Holders.h"
#include "Inject.h"
#include "Inspect.h"
#include "Junk.h"
#include "Keep.h"
#include "ItemView.h"
#include "Magic.h"
#include "ModMenu.h"
#include "Motion.h"
#include "Pad.h"
#include "PlayerView.h"
#include "Profile.h"
#include "Stacks.h"
#include "Quests.h"
#include "Recharge.h"
#include "Rows.h"
#include "Scene.h"
#include "Settings.h"
#include "Stats.h"
#include "StillCamera.h"
#include "StolenRows.h"
#include "Subject.h"
#include "Translations.h"
#include "ZoomKey.h"

#if IR_DEBUG_EDITION
#	include "Debug/Journal.h"
#	include "Debug/Script.h"
#endif

// The panel script reaches these as skse.plugins.EquipmentPanel.<name>
// Places are shares of the screen from the left and from the top, sizes are shares of its height
namespace
{
	constexpr std::uint32_t kCosave = 'EQPN';
	constexpr std::size_t kRoomForACall = 14;
	constexpr auto kZoomKeyScript = "_global.Equipment.zoomKey";
	constexpr auto kGameZoomScript = "_global.Equipment.gameZoom";

	// a NaN or an infinity of the script would end up in the places and sizes of models
	float Number(const RE::GFxValue& a_value)
	{
		const float told = a_value.IsNumber() ? static_cast<float>(a_value.GetNumber()) : 0.0F;
		return std::isfinite(told) ? told : 0.0F;
	}

	bool Flag(const RE::GFxValue& a_value)
	{
		return a_value.IsBool() ? a_value.GetBool() : Number(a_value) != 0.0F;
	}

	// a form id needs all 32 bits, a float keeps 24 of them
	RE::FormID Form(const RE::GFxValue& a_value)
	{
		const double told = a_value.IsNumber() ? a_value.GetNumber() : 0.0;
		return told >= 0.0 && told <= static_cast<double>(UINT32_MAX) ? static_cast<RE::FormID>(told) : 0;
	}

	using Params = RE::GFxFunctionHandler::Params;

	double Milliseconds(std::chrono::steady_clock::time_point a_start)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a_start).count();
	}

	template <class Body>
	class Function : public RE::GFxFunctionHandler
	{
	public:
		explicit Function(std::size_t a_work) :
			work(a_work)
		{}

		void Call(Params& a_params) override
		{
			const Profile::Timer timer{ work };
#if IR_DEBUG_EDITION
			if (told) {
				const Debug::Journal::NativeCall call{ name, a_params };
				Body::Run(a_params);
				return;
			}
#endif
			Body::Run(a_params);
		}

#if IR_DEBUG_EDITION
		const char* name = "";
		// a call that changes the game gets a line before and after, the rest are counted by the profile
		bool told = false;
#endif

	private:
		std::size_t work;
	};

#if IR_DEBUG_EDITION
	constexpr std::array kToldCalls{ "HideModels"sv, "HidePlayer"sv, "Inspect"sv, "InspectEnd"sv, "SetHidden"sv, "SetHiddenPiece"sv, "Subject"sv, "SwitchTo"sv,
		"Wear"sv, "OutfitCard"sv, "OutfitModel"sv, "TakeOffPiece"sv, "TakeOffHand"sv, "Gift"sv, "SetSetting"sv, "SetJunk"sv, "Favorite"sv };
#endif

	// ShowPlayer(centerX, centerY, width, height), a doll made anew brings back worn pieces that were late
	struct ShowPlayer
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount == 4) {
				const auto& args = a_params.args;
				const auto start = std::chrono::steady_clock::now();
				if (PlayerView::Show({ Number(args[0]), Number(args[1]), Number(args[2]), Number(args[3]) })) {
					logger::info("the doll was made anew in {:.1f} ms", Milliseconds(start));
				}
				// with the doll off too, a piece the game hadn't put on the body yet gets its worn look now
				ItemView::Refresh();
			}
		}
	};

	// TurnPlayer(degrees)
	struct TurnPlayer
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount == 1) {
				PlayerView::Turn(Number(a_params.args[0]));
			}
		}
	};

	// PlacePreview(centerX, centerY, size, room high, room wide), the model fills the room both ways
	struct PlacePreview
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount >= 3) {
				const float size = Number(a_params.args[2]);
				const float high = a_params.argCount >= 4 ? Number(a_params.args[3]) : size;
				ItemView::PlacePreview(Number(a_params.args[0]), Number(a_params.args[1]), size, high, a_params.argCount >= 5 ? Number(a_params.args[4]) : high);
			}
		}
	};

	// ShowSlots(items) with items as a flat list of slot, form id, centerX, centerY, worn, size for every slot,
	// gives back the slots that got a model
	struct ShowSlots
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount != 1 || !a_params.args[0].IsArray()) {
				return;
			}
			constexpr std::uint32_t fields = 6;
			const auto& items = a_params.args[0];
			std::vector<ItemView::Slot> slots;
			for (std::uint32_t at = 0; at + fields <= items.GetArraySize(); at += fields) {
				std::array<RE::GFxValue, fields> field;
				for (std::uint32_t part = 0; part < fields; ++part) {
					items.GetElement(at + part, &field[part]);
				}
				slots.push_back({ static_cast<std::int32_t>(Number(field[0])), Form(field[1]), Number(field[2]), Number(field[3]), Flag(field[4]), Number(field[5]) });
			}
#if IR_DEBUG_EDITION
			// for the test stand: the first slots of a menu wait, the watch has to tell the menu stood still
			static RE::GFxMovie* stalled = nullptr;
			if (const auto stall = Settings::Get().stallSeconds; stall > 0 && std::exchange(stalled, a_params.movie) != a_params.movie) {
				std::this_thread::sleep_for(std::chrono::seconds(stall));
			}
#endif
			std::vector<std::int32_t> modelled;
			if (Settings::Get().slotModels) {
				const auto start = std::chrono::steady_clock::now();
				modelled = ItemView::ShowSlots(slots);
				logger::info("{} of {} slot models shown in {:.1f} ms", modelled.size(), slots.size(), Milliseconds(start));
			} else {
				ItemView::Clear();
			}
			if (a_params.retVal) {
				a_params.movie->CreateArray(a_params.retVal);
				for (const auto index : modelled) {
					a_params.retVal->PushBack(RE::GFxValue(static_cast<double>(index)));
				}
			}
		}
	};

	// HideModels(hidden) takes the player and the slot models out of the picture or brings them back
	struct HideModels
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount == 1) {
				Scene::Hide(Flag(a_params.args[0]));
			}
		}
	};

	// HidePlayer() takes the doll away while the item browser of Know your Skyrim stands in its room, the next
	// ShowPlayer makes it anew
	struct HidePlayer
	{
		static void Run(Params&)
		{
			PlayerView::Hide();
		}
	};

	// DumpBody(note) writes every shape of the body the panel shows into the log, for the test stand: what is drawn,
	// which body parts of a skin are on and what texture a shape has
	struct DumpBody
	{
		static void Run(Params& a_params)
		{
			const auto shown = Subject::Get();
			auto* body = shown ? shown->Get3D(false) : nullptr;
			if (!body) {
				return;
			}
			const std::string note = a_params.argCount > 0 && a_params.args[0].IsString() ? a_params.args[0].GetString() : "";
			Scene::Visit(*body, [&](RE::NiAVObject& a_part) {
				auto* shape = a_part.AsGeometry();
				if (!shape) {
					return;
				}
				bool culled = false;
				for (auto* at = &a_part; at; at = at->parent) {
					culled = culled || at->GetAppCulled();
				}
				std::string parts;
				if (auto* skin = netimmerse_cast<RE::BSDismemberSkinInstance*>(shape->GetGeometryRuntimeData().skinInstance.get())) {
					const auto& data = skin->GetRuntimeData();
					for (std::int32_t i = 0; i < data.numPartitions; ++i) {
						parts += fmt::format("{}{} ", data.partitions[i].slot, data.partitions[i].editorVisible ? "" : "off");
					}
				}
				std::string texture;
				int feature = -1;
				auto* lighting = netimmerse_cast<RE::BSLightingShaderProperty*>(shape->GetGeometryRuntimeData().shaderProperty.get());
				if (auto* material = lighting ? static_cast<RE::BSLightingShaderMaterialBase*>(lighting->material) : nullptr) {
					feature = static_cast<int>(material->GetFeature());
					if (material->textureSet) {
						texture = material->textureSet->GetTexturePath(RE::BSTextureSet::Texture::kDiffuse);
					}
				}
				const auto& runtime = shape->GetGeometryRuntimeData();
				logger::info("body {}: {} under {}, culled {}, flags {:X}, shader alpha {:.2f}, blending {}, parts {}, feature {}, texture {}, bound r {:.1f}", note, a_part.name.c_str(),
					a_part.parent ? a_part.parent->name.c_str() : "", culled, a_part.GetFlags().underlying(), runtime.shaderProperty ? runtime.shaderProperty->alpha : -1.0F,
					runtime.alphaProperty ? "yes" : "no", parts, feature, texture, a_part.worldBound.radius);
			});
		}
	};

	// DumpBones(note, word) writes every node of the player's body whose name holds the word and the bones the skin
	// of slot 52 is tied to, for the test stand: which node it is, its parent, its place and turn in the parent
	struct DumpBones
	{
		static void Run(Params& a_params)
		{
			auto* body = RE::PlayerCharacter::GetSingleton()->Get3D(false);
			if (!body || a_params.argCount < 2 || !a_params.args[0].IsString() || !a_params.args[1].IsString()) {
				return;
			}
			const std::string note = a_params.args[0].GetString();
			std::string word = a_params.args[1].GetString();
			std::ranges::transform(word, word.begin(), [](unsigned char a_letter) { return static_cast<char>(std::tolower(a_letter)); });
			const auto told = [](const RE::NiAVObject& a_node) {
				const auto& turn = a_node.local.rotate.entry;
				return fmt::format("{} at {:X} under {}, local ({:.1f} {:.1f} {:.1f}) turn ({:.2f} {:.2f} {:.2f} / {:.2f} {:.2f} {:.2f}) scale {:.3f}, world z {:.1f}", a_node.name.c_str(),
					reinterpret_cast<std::uintptr_t>(&a_node), a_node.parent ? a_node.parent->name.c_str() : "", a_node.local.translate.x, a_node.local.translate.y,
					a_node.local.translate.z, turn[0][0], turn[1][0], turn[2][0], turn[0][1], turn[1][1], turn[2][1], a_node.local.scale, a_node.world.translate.z);
			};
			logger::info("bones {}: root {:X}", note, reinterpret_cast<std::uintptr_t>(body));
			Scene::Visit(*body, [&](RE::NiAVObject& a_part) {
				std::string name = a_part.name.c_str();
				std::ranges::transform(name, name.begin(), [](unsigned char a_letter) { return static_cast<char>(std::tolower(a_letter)); });
				if (a_part.AsNode() && name.find(word) != std::string::npos) {
					logger::info("bones {}: node {}", note, told(a_part));
				}
				auto* shape = a_part.AsGeometry();
				auto* skin = shape ? netimmerse_cast<RE::BSDismemberSkinInstance*>(shape->GetGeometryRuntimeData().skinInstance.get()) : nullptr;
				if (!skin || !skin->skinData) {
					return;
				}
				const auto& parts = skin->GetRuntimeData();
				const bool genitals = std::ranges::any_of(std::span(parts.partitions, parts.numPartitions), [](const auto& a_partition) { return a_partition.slot == 52; });
				if (!genitals) {
					return;
				}
				for (std::uint32_t at = 0; at < skin->skinData->GetBoneCount(); ++at) {
					const auto* bone = skin->bones ? skin->bones[at] : nullptr;
					const auto* place = skin->boneWorldTransforms ? skin->boneWorldTransforms[at] : nullptr;
					logger::info("bones {}: skin of {} bone {}: {}, draws from its own node {}", note, a_part.name.c_str(), at, bone ? told(*bone) : "none",
						bone && place == &bone->world);
				}
			});
		}
	};

	// DumpDoll(note, word) writes the bones of the doll whose names hold the word, for the test stand: the node's place
	// in its parent and in the world, and the same bone in the flat list of the skeleton the skins may draw from
	struct DumpDoll
	{
		static void Run(Params& a_params)
		{
			auto* doll = PlayerView::Doll();
			if (!doll || a_params.argCount < 2 || !a_params.args[0].IsString() || !a_params.args[1].IsString()) {
				return;
			}
			const std::string note = a_params.args[0].GetString();
			const std::string word = a_params.args[1].GetString();
			if (const auto* lens = RE::UI3DSceneManager::GetSingleton()->camera.get()) {
				const auto& frustum = lens->GetRuntimeData2().viewFrustum;
				logger::info("doll {}: menu camera at ({:.1f} {:.1f} {:.1f}), frustum left {:.3f} right {:.3f} top {:.3f} bottom {:.3f} near {:.1f} far {:.1f}, doll at ({:.1f} {:.1f} {:.1f}) scale {:.3f}",
					note, lens->world.translate.x, lens->world.translate.y, lens->world.translate.z, frustum.fLeft, frustum.fRight, frustum.fTop, frustum.fBottom, frustum.fNear,
					frustum.fFar, doll->world.translate.x, doll->world.translate.y, doll->world.translate.z, doll->world.scale);
			}
			Scene::Visit(*doll, [&](RE::NiAVObject& a_part) {
				const std::string_view name = a_part.name.c_str() ? a_part.name.c_str() : "";
				if (a_part.AsNode() && name.find(word) != std::string_view::npos) {
					logger::info("doll {}: node {} local ({:.1f} {:.1f} {:.1f}) world ({:.1f} {:.1f} {:.1f})", note, name, a_part.local.translate.x, a_part.local.translate.y,
						a_part.local.translate.z, a_part.world.translate.x, a_part.world.translate.y, a_part.world.translate.z);
				}
				auto* shape = a_part.AsGeometry();
				if (auto* skin = shape ? shape->GetGeometryRuntimeData().skinInstance.get() : nullptr; skin && skin->skinData && skin->bones && skin->boneWorldTransforms) {
					for (std::uint32_t at = 0; at < skin->skinData->bones; ++at) {
						auto* bone = skin->bones[at];
						const std::string_view boneName = bone && bone->name.c_str() ? bone->name.c_str() : "";
						if (boneName.find(word) == std::string_view::npos) {
							continue;
						}
						const auto* place = skin->boneWorldTransforms[at];
						const RE::NiAVObject* own = doll->GetObjectByName(bone->name);
						logger::info("doll {}: skin of {} bone {} {} draws from {} at ({:.1f} {:.1f} {:.1f})", note, name, at, boneName,
							place == &bone->world ? (own == bone ? "its node in the doll" : "a node outside the doll") : "elsewhere", place ? place->translate.x : 0.0F,
							place ? place->translate.y : 0.0F, place ? place->translate.z : 0.0F);
					}
				}
				auto* tree = netimmerse_cast<RE::BSFlattenedBoneTree*>(&a_part);
				if (!tree) {
					return;
				}
				const auto& lists = tree->GetRuntimeData();
				for (std::uint32_t at = 0; lists.boneEntries && at < lists.numBones; ++at) {
					const auto& entry = lists.boneEntries[at];
					const std::string_view bone = entry.nodeName.c_str() ? entry.nodeName.c_str() : "";
					if (bone.find(word) == std::string_view::npos) {
						continue;
					}
					logger::info("doll {}: flat {} local ({:.1f} {:.1f} {:.1f}) world ({:.1f} {:.1f} {:.1f}) node {}", note, bone, entry.local.translate.x, entry.local.translate.y,
						entry.local.translate.z, entry.world.translate.x, entry.world.translate.y, entry.world.translate.z,
						entry.node ? (entry.node->world.translate == entry.world.translate ? "same world" : "other world") : "none");
				}
			});
			// what each shape of the doll is drawn with: switched off or not, the alpha of its shader and the fade node the
			// shader takes its fade from, inside the doll or outside it
			std::set<const RE::NiAVObject*> own;
			Scene::Visit(*doll, [&](RE::NiAVObject& a_part) { own.insert(&a_part); });
			Scene::Visit(*doll, [&](RE::NiAVObject& a_part) {
				if (a_part.AsNode() && (&a_part == doll || a_part.AsFadeNode())) {
					logger::info("doll {}: bound of {} at ({:.1f} {:.1f} {:.1f}) radius {:.1f}, flags {:X}/{:X}", note, a_part.name.c_str() ? a_part.name.c_str() : "", a_part.worldBound.center.x,
						a_part.worldBound.center.y, a_part.worldBound.center.z, a_part.worldBound.radius, a_part.GetFlags().underlying(), a_part.GetFlags02());
				}
				if (auto* fading = a_part.AsFadeNode()) {
					const auto& fade = fading->GetRuntimeData();
					logger::info("doll {}: fade node {} {:.2f} {:.2f} {:.2f} {:X} {:X} {:X} {:.2f} {:X} {:X} {:.2f} {:X} {:X} {:X} {:X} {:X} {:X}", note, a_part.name.c_str() ? a_part.name.c_str() : "",
						fade.unk128, fade.unk12C, fade.currentFade, fade.unk134, fade.unk138, fade.unk13C, fade.unk140, fade.unk144, fade.unk148, fade.unk14C, fade.unk150, fade.unk152,
						fade.unk153, fade.unk154, fade.unk155, fade.unk156);
				}
			});
			const auto shown = Subject::Get();
			auto* body = shown ? shown->Get3D(false) : nullptr;
			Scene::Visit(*doll, [&](RE::NiAVObject& a_part) {
				auto* shape = a_part.AsGeometry();
				const auto* shader = shape ? shape->GetGeometryRuntimeData().shaderProperty.get() : nullptr;
				if (!shape) {
					return;
				}
				const auto* fade = shader ? shader->fadeNode : nullptr;
				auto* live = body && a_part.name.c_str() ? body->GetObjectByName(a_part.name) : nullptr;
				const auto* liveShape = live ? live->AsGeometry() : nullptr;
				const auto* liveShader = liveShape ? liveShape->GetGeometryRuntimeData().shaderProperty.get() : nullptr;
				const auto* blend = shape->GetGeometryRuntimeData().alphaProperty.get();
				const auto* lighting = shader ? netimmerse_cast<const RE::BSLightingShaderProperty*>(shader) : nullptr;
				const auto* material = lighting ? static_cast<const RE::BSLightingShaderMaterialBase*>(lighting->material) : nullptr;
				logger::info("doll {}: shape {} bound ({:.1f} {:.1f} {:.1f}) r {:.1f}", note, a_part.name.c_str() ? a_part.name.c_str() : "", a_part.worldBound.center.x,
					a_part.worldBound.center.y, a_part.worldBound.center.z, a_part.worldBound.radius);
				logger::info("doll {}: shape {} off {} flags {:X}/{:X} shader alpha {:.2f} ({}, the body's {:.2f}) shader flags {:X} pass {} (the body's {}) passes {} material alpha {:.2f} alpha property {} fade node {} fade {:.2f}",
					note, a_part.name.c_str() ? a_part.name.c_str() : "", a_part.GetAppCulled(), a_part.GetFlags().underlying(), a_part.GetFlags02(), shader ? shader->alpha : -1.0F,
					!liveShader ? "no body shape" : (liveShader == shader ? "shared with the body" : "its own"), liveShader ? liveShader->alpha : -1.0F,
					shader ? shader->flags.underlying() : 0, shader ? shader->lastRenderPassState : -1, liveShader ? liveShader->lastRenderPassState : -1,
					shader && shader->renderPassList.head ? "some" : "none", material ? material->materialAlpha : -1.0F,
					blend ? fmt::format("{:X}", blend->alphaFlags) : std::string("none"), fade ? (own.contains(fade) ? "in the doll" : "outside the doll") : "none",
					fade ? fade->GetRuntimeData().currentFade : -1.0F);
			});
		}
	};

	// ShowPreview(shown) tells whether the item window is up, the model of the selected item goes with it
	struct ShowPreview
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount == 1) {
				Scene::ShowPreview(Flag(a_params.args[0]));
			}
		}
	};

	// PadTurns(what) tells what the right stick of a gamepad turns now: 0 nothing, 1 the doll, 2 the inspected item
	struct PadTurns
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount == 1) {
				Pad::SetTurns(static_cast<Pad::Turns>(std::clamp(static_cast<int>(Number(a_params.args[0])), 0, 2)));
			}
		}
	};

	// Cover(left, top, right, bottom) in shares of the screen width and height, Cover() lifts it
	struct Cover
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount == 4) {
				const auto& args = a_params.args;
				Scene::Cover({ Number(args[0]), Number(args[1]), Number(args[2]), Number(args[3]) });
			} else {
				Scene::Uncover();
			}
		}
	};

	// Inspect(formId, worn, centerX, centerY, size), tells whether the item could be shown
	struct InspectBegin
	{
		static void Run(Params& a_params)
		{
			bool shown = false;
			if (a_params.argCount == 5) {
				const auto& args = a_params.args;
				shown = Inspect::Begin(Form(args[0]), Flag(args[1]), Number(args[2]), Number(args[3]), Number(args[4]));
			}
			if (a_params.retVal) {
				a_params.retVal->SetBoolean(shown);
			}
		}
	};

	// InspectTurn(yawDegrees, pitchDegrees)
	struct InspectTurn
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount == 2) {
				Inspect::Turn(Number(a_params.args[0]), Number(a_params.args[1]));
			}
		}
	};

	// InspectZoom(factor)
	struct InspectZoom
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount == 1) {
				Inspect::Zoom(Number(a_params.args[0]));
			}
		}
	};

	struct InspectEnd
	{
		static void Run(Params&)
		{
			Inspect::End();
		}
	};

	// The one the panel shows hides things on themselves: the player, or the follower of the trade. A follower a mod
	// keeps as they are hides nothing, their body is built again in the trade's next frame
	RE::NiPointer<RE::Actor> HidesOn()
	{
		auto shown = Subject::Get();
		return shown && (shown->IsPlayerRef() || !Subject::ReadOnly()) ? shown : nullptr;
	}

	void HiddenChanged(RE::Actor& a_actor)
	{
		if (!a_actor.IsPlayerRef()) {
			Subject::BodyAgain();
		}
	}

	// SetHidden(slot, hidden) gives back the mask of hidden slots of the one shown
	struct SetHidden
	{
		static void Run(Params& a_params)
		{
			const auto actor = Subject::Get();
			if (const auto on = HidesOn(); on && a_params.argCount == 2) {
				Hidden::SetOn(*on, static_cast<std::int32_t>(Number(a_params.args[0])), Flag(a_params.args[1]));
				HiddenChanged(*on);
			}
			if (a_params.retVal) {
				a_params.retVal->SetNumber(static_cast<double>(actor ? Hidden::MaskOf(*actor) : 0));
			}
		}
	};

	// HiddenSlots() is the mask of hidden slots of the one shown, bit 1 << slot
	struct HiddenSlots
	{
		static void Run(Params& a_params)
		{
			const auto actor = Subject::Get();
			if (a_params.retVal) {
				a_params.retVal->SetNumber(static_cast<double>(actor ? Hidden::MaskOf(*actor) : 0));
			}
		}
	};

	void TellPieces(Params& a_params)
	{
		const auto actor = Subject::Get();
		if (a_params.retVal && a_params.movie) {
			a_params.movie->CreateArray(a_params.retVal);
			for (const auto form : actor ? Hidden::PiecesOf(*actor) : std::vector<RE::FormID>{}) {
				a_params.retVal->PushBack(RE::GFxValue(static_cast<double>(form)));
			}
		}
	}

	// SetHiddenPiece(formId, hidden) gives back the form ids of the pieces the one shown hides one by one
	struct SetHiddenPiece
	{
		static void Run(Params& a_params)
		{
			if (const auto on = HidesOn(); on && a_params.argCount == 2) {
				Hidden::SetPieceOn(*on, Form(a_params.args[0]), Flag(a_params.args[1]));
				HiddenChanged(*on);
			}
			TellPieces(a_params);
		}
	};

	// HiddenPieces() is the form ids of the pieces hidden one by one
	struct HiddenPieces
	{
		static void Run(Params& a_params)
		{
			TellPieces(a_params);
		}
	};

	// HungPieces() is the form ids of worn pieces whose copies another mod hangs on the body, like MARA's jewelry
	struct HungPieces
	{
		static void Run(Params& a_params)
		{
			if (a_params.retVal && a_params.movie) {
				a_params.movie->CreateArray(a_params.retVal);
				// copies of jewelry are found on the player's body alone
				for (const auto form : Subject::Other() ? std::vector<RE::FormID>{} : Hidden::Hung()) {
					a_params.retVal->PushBack(RE::GFxValue(static_cast<double>(form)));
				}
			}
		}
	};

	// Eyes(numbers) hands the eyes of the slots in sight to their own movie, drawn over the 3D. False when that movie
	// is not there, the panel draws them itself then
	struct ShowEyes
	{
		static void Run(Params& a_params)
		{
			const bool drawn = a_params.argCount == 1 && Eyes::Show(a_params.args[0]);
			if (a_params.retVal) {
				a_params.retVal->SetBoolean(drawn);
			}
		}
	};

	// ContextOver(told) hands a copy of the open menu of a slot or a row to the movie of the eyes, drawn over the 3D.
	// False when that movie is not there, the panel keeps its menu off the models itself then
	struct ContextOver
	{
		static void Run(Params& a_params)
		{
			const bool drawn = a_params.argCount == 1 && Eyes::ShowMenu(a_params.args[0]);
			if (a_params.retVal) {
				a_params.retVal->SetBoolean(drawn);
			}
		}
	};

	// DumpHanging() writes the worn pieces and the copies hanging on the body into the log, for the test stand
	struct DumpHanging
	{
		static void Run(Params&)
		{
			Hidden::DumpHanging();
		}
	};

	// Stats(formId) is an object with the numbers of a weapon or a piece of armor
	struct ItemStats
	{
		static void Run(Params& a_params)
		{
			if (a_params.retVal && a_params.movie && a_params.argCount == 1) {
				Stats::Fill(*a_params.movie, *a_params.retVal, Form(a_params.args[0]));
			}
		}
	};

	// HeroStats() is an object with the numbers of the one the panel shows for the character window
	struct HeroStats
	{
		static void Run(Params& a_params)
		{
			// parts of the sheet are asked of loaded actors only, a follower may have walked off under a running menu
			if (const auto shown = Subject::Get(); shown && (shown->IsPlayerRef() || shown->Is3DLoaded()) && a_params.retVal && a_params.movie) {
				Stats::FillHero(*a_params.movie, *a_params.retVal, *shown);
			}
		}
	};

	// Subject() is who the panel of this menu shows, settled as the menu loaded: an object with player (true for the
	// player), name and formId
	struct ShownActor
	{
		static void Run(Params& a_params)
		{
			const auto shown = Subject::Get();
			if (!shown || !a_params.retVal || !a_params.movie) {
				return;
			}
			const bool player = shown->IsPlayerRef();
			a_params.movie->CreateObject(a_params.retVal);
			a_params.retVal->SetMember("player", RE::GFxValue(player));
			RE::GFxValue name;
			const char* named = shown->GetDisplayFullName();
			a_params.movie->CreateString(&name, named ? named : "");
			a_params.retVal->SetMember("name", name);
			a_params.retVal->SetMember("formId", RE::GFxValue(static_cast<double>(shown->GetFormID())));
		}
	};

	// Companions() is the companions near the player, nearest first: objects with formId and name
	struct NearCompanions
	{
		static void Run(Params& a_params)
		{
			if (!a_params.retVal || !a_params.movie) {
				return;
			}
			a_params.movie->CreateArray(a_params.retVal);
			for (const auto& companion : Companions::Near()) {
				RE::GFxValue row;
				RE::GFxValue name;
				a_params.movie->CreateObject(&row);
				a_params.movie->CreateString(&name, companion.name.c_str());
				row.SetMember("formId", RE::GFxValue(static_cast<double>(companion.formId)));
				row.SetMember("name", name);
				a_params.retVal->PushBack(row);
			}
		}
	};

	// SwitchTo(formId, fromInventory) shuts the menu the script is in and opens that companion's trade, the player's
	// inventory for 0. True when the switch is on its way
	struct SwitchCompanion
	{
		static void Run(Params& a_params)
		{
			const bool started = a_params.argCount == 2 && a_params.args[0].IsNumber() &&
			                     Companions::SwitchTo(static_cast<RE::FormID>(a_params.args[0].GetNumber()),
									 Flag(a_params.args[1]) ? RE::InventoryMenu::MENU_NAME.data() : RE::ContainerMenu::MENU_NAME.data());
			if (a_params.retVal) {
				a_params.retVal->SetBoolean(started);
			}
		}
	};

	// Outfit() is what the follower shown wears of their outfit, rows the trade never lists: objects with formId, text,
	// formType, partMask, infoArmor, weightClass, infoWeight, infoDamage, weaponType and equipState as the list's own
	// rows have them, a piece without its weight class counted for no piece of armor in the set. The list's
	// armor and damage are the player's numbers, with skill and perks, these are counted the same way or a copy of
	// the outfit's armor in the list would read better than the worn one
	struct OutfitPieces
	{
		static double Shown(float a_value)
		{
			return std::round(static_cast<double>(a_value) * 100.0) / 100.0;
		}

		static void Run(Params& a_params)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!a_params.retVal || !a_params.movie || !player) {
				return;
			}
			a_params.movie->CreateArray(a_params.retVal);
			for (const auto& piece : Subject::Outfit()) {
				RE::GFxValue row;
				RE::GFxValue text;
				RE::InventoryEntryData counted{ piece.object, 1 };
				a_params.movie->CreateObject(&row);
				a_params.movie->CreateString(&text, piece.object->GetName());
				row.SetMember("text", text);
				row.SetMember("formId", RE::GFxValue(static_cast<double>(piece.object->GetFormID())));
				row.SetMember("formType", RE::GFxValue(static_cast<double>(std::to_underlying(piece.object->GetFormType()))));
				row.SetMember("outfit", RE::GFxValue(true));
				double state = 1.0;
				if (auto* armor = piece.object->As<RE::TESObjectARMO>()) {
					row.SetMember("partMask", RE::GFxValue(static_cast<double>(armor->GetSlotMask().underlying())));
					row.SetMember("infoArmor", RE::GFxValue(Shown(player->GetArmorValue(&counted))));
					row.SetMember("weightClass", RE::GFxValue(static_cast<double>(std::to_underlying(armor->GetArmorType()))));
					row.SetMember("infoWeight", RE::GFxValue(Shown(armor->GetWeight())));
				} else if (const auto* weapon = piece.object->As<RE::TESObjectWEAP>()) {
					row.SetMember("weaponType", RE::GFxValue(static_cast<double>(std::to_underlying(weapon->GetWeaponType()))));
					row.SetMember("infoDamage", RE::GFxValue(Shown(player->GetDamage(&counted))));
					state = piece.right && piece.left ? 4.0 : (piece.right ? 3.0 : 2.0);
				} else if (piece.object->Is(RE::FormType::Light)) {
					state = 2.0;
				}
				row.SetMember("equipState", RE::GFxValue(state));
				if (const char* origin = Rows::Origin(*piece.object)) {
					RE::GFxValue file;
					a_params.movie->CreateString(&file, origin);
					row.SetMember("equipmentOrigin", file);
				}
				a_params.retVal->PushBack(row);
			}
		}
	};

	// a piece the shown follower wears of their outfit, by its form
	RE::TESBoundObject* OutfitPiece(const RE::GFxValue& a_form)
	{
		const auto formId = a_form.IsNumber() ? static_cast<RE::FormID>(a_form.GetNumber()) : 0;
		for (const auto& piece : Subject::Outfit()) {
			if (piece.object->GetFormID() == formId) {
				return piece.object;
			}
		}
		return nullptr;
	}

	// OutfitCard(formId) fills the menu's card with a piece of the shown follower's outfit, what the game does for a
	// selected row and the trade has none for. Gives the card's object for UpdateItemCardInfo, nothing when the
	// follower wears no such piece. The card doesn't keep the item it's given, so one on the stack does
	struct OutfitCard
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount != 1 || !a_params.retVal) {
				return;
			}
			const auto menu = RE::UI::GetSingleton()->GetMenu<RE::ContainerMenu>();
			auto* card = menu ? menu->GetRuntimeData().itemCard : nullptr;
			auto* object = OutfitPiece(a_params.args[0]);
			if (!card || !object) {
				return;
			}
			RE::InventoryEntryData item{ object, 1 };
			card->SetItem(&item, false);
			*a_params.retVal = card->obj;
		}
	};

	// OutfitModel(formId) puts the piece's model up once its window stands where it shows. The game sizes a model as
	// it puts it up, one put up before the window moved kept the size of the window before
	struct OutfitModel
	{
		static void Run(Params& a_params)
		{
			auto* preview = RE::Inventory3DManager::GetSingleton();
			auto* object = a_params.argCount == 1 ? OutfitPiece(a_params.args[0]) : nullptr;
			if (preview && object) {
				IR_BRACKET("the game's preview loads the model of {:08X} {}", object->GetFormID(), object->GetName());
				preview->LoadInventoryItem(object, nullptr);
			}
		}
	};

	// TakeOffPiece(formId) takes a worn piece of the follower's outfit off, true when it came off
	struct TakeOffPiece
	{
		static void Run(Params& a_params)
		{
			const bool done = a_params.argCount == 1 && a_params.args[0].IsNumber() &&
			                  Subject::TakeOffPiece(static_cast<RE::FormID>(a_params.args[0].GetNumber()));
			if (a_params.retVal) {
				a_params.retVal->SetBoolean(done);
			}
		}
	};

	// Hands() tells the panel what the one it shows holds: right hand, left hand, voice, each as
	// {formId, name, item, bothHands, spell}, formId 0 for an empty one. A spell also has the school and the
	// resistance of its costliest effect as actor value numbers, the ones SkyUI picks its sign by, and its cost
	struct Hands
	{
		static void Run(Params& a_params)
		{
			if (!a_params.retVal) {
				return;
			}
			a_params.movie->CreateArray(a_params.retVal);
			const auto shown = Subject::Get();
			for (const auto& held : shown ? Magic::Hands(*shown) : std::array<Magic::Held, Magic::kPlaces>{}) {
				RE::GFxValue one;
				RE::GFxValue name;
				a_params.movie->CreateObject(&one);
				a_params.movie->CreateString(&name, held.form ? held.form->GetName() : "");
				one.SetMember("formId", RE::GFxValue(static_cast<double>(held.form ? held.form->GetFormID() : 0)));
				one.SetMember("name", name);
				one.SetMember("item", RE::GFxValue(held.item));
				one.SetMember("bothHands", RE::GFxValue(held.bothHands));
				auto* spell = held.form ? held.form->As<RE::SpellItem>() : nullptr;
				one.SetMember("spell", RE::GFxValue(spell != nullptr));
				if (spell) {
					const auto* effect = spell->GetCostliestEffectItem();
					const auto* base = effect ? effect->baseEffect : nullptr;
					one.SetMember("school", RE::GFxValue(static_cast<double>(base ? std::to_underlying(base->GetMagickSkill()) : -1)));
					one.SetMember("resistance", RE::GFxValue(static_cast<double>(base ? std::to_underlying(base->data.resistVariable) : -1)));
					one.SetMember("cost", RE::GFxValue(static_cast<double>(spell->CalculateMagickaCost(shown.get()))));
				}
				a_params.retVal->PushBack(one);
			}
		}
	};

	// TakeOffHand(place) takes the item of a hand off from the magic menu, true when it went
	struct TakeOffHand
	{
		static void Run(Params& a_params)
		{
			const bool done = a_params.argCount == 1 && Magic::TakeOff(static_cast<Magic::Place>(static_cast<std::int32_t>(Number(a_params.args[0]))));
			if (a_params.retVal) {
				a_params.retVal->SetBoolean(done);
			}
		}
	};

	// Gift() notes the follower's worn gear before the player gives them a row to put on
	struct NoteGift
	{
		static void Run(Params&)
		{
			Subject::Gift();
		}
	};

	// ReadOnly() tells whether a mod asked the follower shown be looked at only, asked each time since a mod may ask
	// while the trade is up
	struct ShownReadOnly
	{
		static void Run(Params& a_params)
		{
			if (a_params.retVal) {
				a_params.retVal->SetBoolean(Subject::ReadOnly());
			}
		}
	};

	// Wear(itemIndex, formId, left, on) puts the item of a row of a follower's side on them or takes it off, true when
	// done. The form id says which item the script means, a row that moved meanwhile is refused
	struct WearOnShown
	{
		static void Run(Params& a_params)
		{
			const bool done = a_params.argCount == 4 && a_params.args[0].IsNumber() && a_params.args[1].IsNumber() &&
			                  Subject::Wear(static_cast<std::int32_t>(a_params.args[0].GetNumber()), static_cast<RE::FormID>(a_params.args[1].GetNumber()), Flag(a_params.args[2]),
								  Flag(a_params.args[3]));
			if (a_params.retVal) {
				a_params.retVal->SetBoolean(done);
			}
		}
	};

	bool Numbers(const Params& a_params, std::uint32_t a_count)
	{
		if (a_params.argCount != a_count) {
			return false;
		}
		for (std::uint32_t i = 0; i < a_count; ++i) {
			if (!a_params.args[i].IsNumber()) {
				return false;
			}
		}
		return true;
	}

	std::int32_t Index(const RE::GFxValue& a_value)
	{
		return static_cast<std::int32_t>(a_value.GetNumber());
	}

	// ChargeOf(itemIndex, formId) is how full the weapon of a row is, 0 to 100, -1 for one of someone else's or one
	// that takes no charge
	struct ChargeOfRow
	{
		static void Run(Params& a_params)
		{
			if (a_params.retVal) {
				a_params.retVal->SetNumber(Numbers(a_params, 2) ? Recharge::ChargeOf(a_params.movie, Index(a_params.args[0]), Form(a_params.args[1])) : -1.0);
			}
		}
	};

	// SoulGems(itemIndex, formId) is the item card's list of the soul gems for the weapon of that row, undefined for none
	struct SoulGemsForRow
	{
		static void Run(Params& a_params)
		{
			if (Numbers(a_params, 2) && a_params.retVal && a_params.movie) {
				Recharge::SoulGems(*a_params.movie, Index(a_params.args[0]), Form(a_params.args[1]), *a_params.retVal);
			}
		}
	};

	// Weapons(itemIndex, formId) is the item card's list of the drained weapons the gem of that row can charge,
	// undefined for none
	struct WeaponsForGem
	{
		static void Run(Params& a_params)
		{
			if (Numbers(a_params, 2) && a_params.retVal && a_params.movie) {
				Recharge::Weapons(*a_params.movie, Index(a_params.args[0]), Form(a_params.args[1]), *a_params.retVal);
			}
		}
	};

	// Recharge(weaponIndex, weaponFormId, gemIndex, gemFormId) charges the weapon of one row with the gem of another,
	// true when done
	struct ChargeRow
	{
		static void Run(Params& a_params)
		{
			const bool done = Numbers(a_params, 4) &&
			                  Recharge::Charge(a_params.movie, Index(a_params.args[0]), Form(a_params.args[1]), Index(a_params.args[2]), Form(a_params.args[3]));
			if (a_params.retVal) {
				a_params.retVal->SetBoolean(done);
			}
		}
	};

	// The character drawer stays the way the player left it from one opening of a menu to the next, while the game runs
	struct Drawer
	{
		bool open = false;
		double high = 0.0;
		double first = 0.0;
	};
	Drawer drawer;

	// HeroDrawer() is an object with how the character drawer was left: open, high, first line shown
	struct HeroDrawer
	{
		static void Run(Params& a_params)
		{
			if (a_params.retVal && a_params.movie) {
				a_params.movie->CreateObject(a_params.retVal);
				a_params.retVal->SetMember("open", RE::GFxValue(drawer.open));
				a_params.retVal->SetMember("high", RE::GFxValue(drawer.high));
				a_params.retVal->SetMember("first", RE::GFxValue(drawer.first));
			}
		}
	};

	// SetHeroDrawer(open, high, first)
	struct SetHeroDrawer
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount == 3) {
				drawer = { Flag(a_params.args[0]), Number(a_params.args[1]), Number(a_params.args[2]) };
			}
		}
	};

	// a button of the item menus pressed and let go, the way a key the game maps to it does
	void PressMenuButton(std::string_view a_event)
	{
		const auto* map = RE::ControlMap::GetSingleton();
		const auto key = map ? map->GetMappedKey(a_event, RE::INPUT_DEVICE::kKeyboard, RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu) : RE::ControlMap::kInvalid;
		const RE::BSFixedString name(a_event);
		for (const float value : { 1.0F, 0.0F }) {
			auto* button = RE::ButtonEvent::Create(RE::INPUT_DEVICE::kKeyboard, name, key, value, value > 0.0F ? 0.0F : 0.1F);
			RE::InputEvent* first = button;
			RE::BSInputDeviceManager::GetSingleton()->SendEvent(&first);
			RE::free(button);
		}
	}

	// Favorite() marks the selected row a favorite or takes the mark off. The menu script has no call for it,
	// the game takes it from its favorite button, so that button is pressed once the script is done
	struct Favorite
	{
		static void Run(Params&)
		{
			SKSE::GetTaskInterface()->AddTask([] {
				IR_BRACKET("the favorite button of the item menus is pressed");
				PressMenuButton("YButton");
			});
		}
	};

	// SetJunk(formId, junk) marks or unmarks an item as junk, false when there is no such item
	struct SetJunk
	{
		static void Run(Params& a_params)
		{
			const bool done = a_params.argCount == 2 && a_params.args[0].IsNumber() &&
			                  Junk::Set(Form(a_params.args[0]), Flag(a_params.args[1]));
			if (a_params.retVal) {
				a_params.retVal->SetBoolean(done);
			}
		}
	};

	// Carry() is what the player carries and the most before the weight slows him, as the game counts them now
	struct Carry
	{
		static void Run(Params& a_params)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!a_params.retVal || !a_params.movie || !player) {
				return;
			}
			a_params.movie->CreateObject(a_params.retVal);
			a_params.retVal->SetMember("carried", static_cast<double>(player->GetWeightInContainer()));
			a_params.retVal->SetMember("most", static_cast<double>(player->GetTotalCarryWeight()));
		}
	};

	// Settings() is an object with what the player set in the menu of the mod
	struct GetSettings
	{
		static void Run(Params& a_params)
		{
			if (a_params.retVal && a_params.movie) {
				Settings::Fill(*a_params.movie, *a_params.retVal);
			}
		}
	};

	// SetSetting(id, value) sets what the panel offers in the inventory itself, "iScale:Doll", into the same file the
	// menu of the mod keeps. It holds at once
	struct ChangeSetting
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount == 2 && a_params.args[0].IsString()) {
				Settings::Change(a_params.args[0].GetString(), Number(a_params.args[1]));
			}
		}
	};

	// ScriptStarts(part) and ScriptStops() wrap a build of the panel script ("build", "rows"), for the line about the
	// opening and for a stall, which then says the panel's script is the one running
	struct ScriptStarts
	{
		static void Run(Params& a_params)
		{
			const bool named = a_params.argCount == 1 && a_params.args[0].IsString();
			Profile::ScriptStarts(named ? a_params.args[0].GetString() : "");
		}
	};

	struct ScriptStops
	{
		static void Run(Params&) { Profile::ScriptStops(); }
	};

	// Clock() is a steady clock in milliseconds for the stand's measures, getTimer of the menu stands still within a frame
	struct ScriptClock
	{
		static void Run(Params& a_params)
		{
			static const auto start = std::chrono::steady_clock::now();
			if (a_params.retVal) {
				a_params.retVal->SetNumber(Milliseconds(start));
			}
		}
	};

	// Words() gives the panel the words of the translation file of the game's language, as the file has them
	struct GetWords
	{
		static void Run(Params& a_params)
		{
			if (a_params.retVal && a_params.movie) {
				Translations::Fill(*a_params.movie, *a_params.retVal);
			}
		}
	};

	// ScreenSize() is the size of the menu's picture in pixels and how its stage is fit in (fit, the game's scale mode).
	// A menu made on SkyUI 5 has a stage 2560 wide that the game stretches over the screen, the panel squeezes its own
	// drawing back by the difference
	struct GetScreenSize
	{
		static void Run(Params& a_params)
		{
			if (!a_params.retVal || !a_params.movie) {
				return;
			}
			const auto* view = static_cast<RE::GFxMovieView*>(a_params.movie);
			RE::GViewport port;
			view->GetViewport(&port);
			a_params.movie->CreateObject(a_params.retVal);
			a_params.retVal->SetMember("width", RE::GFxValue(port.width));
			a_params.retVal->SetMember("height", RE::GFxValue(port.height));
			a_params.retVal->SetMember("fit", RE::GFxValue(static_cast<std::int32_t>(view->GetViewScaleMode())));
		}
	};

	// Remember(key, words) keeps a few words for the panel until the game ends, Recall(key) gives them back, "" for
	// nothing kept. The panel's script is loaded anew with every menu: the subcategories chosen in a category were
	// gone the next time the inventory opened while the game kept the category itself. Only the menus' thread calls
	constexpr std::size_t kRememberedMost = 64;
	std::unordered_map<std::string, std::string> remembered;

	struct Remember
	{
		static void Run(Params& a_params)
		{
			if (a_params.argCount != 2 || !a_params.args[0].IsString() || !a_params.args[1].IsString()) {
				return;
			}
			std::string key = a_params.args[0].GetString();
			if (remembered.size() < kRememberedMost || remembered.contains(key)) {
				remembered[std::move(key)] = a_params.args[1].GetString();
			}
		}
	};

	struct Recall
	{
		static void Run(Params& a_params)
		{
			if (!a_params.retVal || !a_params.movie) {
				return;
			}
			const auto found = a_params.argCount == 1 && a_params.args[0].IsString() ? remembered.find(a_params.args[0].GetString()) : remembered.end();
			a_params.movie->CreateString(a_params.retVal, found != remembered.end() ? found->second.c_str() : "");
		}
	};

	template <class Body>
	void AddFunction(RE::GFxMovieView* a_view, RE::GFxValue* a_root, const char* a_name)
	{
		// kept for the whole game and never freed, at exit Scaleform's memory may be gone before this DLL lets go
		static auto* const handler = new Function<Body>(Profile::Work(a_name));
#if IR_DEBUG_EDITION
		handler->name = a_name;
		handler->told = std::ranges::find(kToldCalls, std::string_view(a_name)) != kToldCalls.end();
#endif
		RE::GFxValue function;
		a_view->CreateFunction(&function, handler);
		a_root->SetMember(a_name, function);
	}

	bool AddMenuFunctions(RE::GFxMovieView* a_view, RE::GFxValue* a_root)
	{
		AddFunction<ShowPlayer>(a_view, a_root, "ShowPlayer");
		AddFunction<TurnPlayer>(a_view, a_root, "TurnPlayer");
		AddFunction<PlacePreview>(a_view, a_root, "PlacePreview");
		AddFunction<ShowSlots>(a_view, a_root, "ShowSlots");
		AddFunction<HideModels>(a_view, a_root, "HideModels");
		AddFunction<HidePlayer>(a_view, a_root, "HidePlayer");
		AddFunction<Cover>(a_view, a_root, "Cover");
		AddFunction<ShowPreview>(a_view, a_root, "ShowPreview");
		AddFunction<PadTurns>(a_view, a_root, "PadTurns");
		AddFunction<DumpBody>(a_view, a_root, "DumpBody");
		AddFunction<DumpBones>(a_view, a_root, "DumpBones");
		AddFunction<DumpDoll>(a_view, a_root, "DumpDoll");
		AddFunction<InspectBegin>(a_view, a_root, "Inspect");
		AddFunction<InspectTurn>(a_view, a_root, "InspectTurn");
		AddFunction<InspectZoom>(a_view, a_root, "InspectZoom");
		AddFunction<InspectEnd>(a_view, a_root, "InspectEnd");
		AddFunction<SetHidden>(a_view, a_root, "SetHidden");
		AddFunction<HiddenSlots>(a_view, a_root, "HiddenSlots");
		AddFunction<SetHiddenPiece>(a_view, a_root, "SetHiddenPiece");
		AddFunction<HiddenPieces>(a_view, a_root, "HiddenPieces");
		AddFunction<HungPieces>(a_view, a_root, "HungPieces");
		AddFunction<DumpHanging>(a_view, a_root, "DumpHanging");
		AddFunction<ShowEyes>(a_view, a_root, "Eyes");
		AddFunction<ContextOver>(a_view, a_root, "ContextOver");
		AddFunction<ItemStats>(a_view, a_root, "Stats");
		AddFunction<HeroStats>(a_view, a_root, "HeroStats");
		AddFunction<ShownActor>(a_view, a_root, "Subject");
		AddFunction<NearCompanions>(a_view, a_root, "Companions");
		AddFunction<SwitchCompanion>(a_view, a_root, "SwitchTo");
		AddFunction<WearOnShown>(a_view, a_root, "Wear");
		AddFunction<ChargeOfRow>(a_view, a_root, "ChargeOf");
		AddFunction<SoulGemsForRow>(a_view, a_root, "SoulGems");
		AddFunction<WeaponsForGem>(a_view, a_root, "Weapons");
		AddFunction<ChargeRow>(a_view, a_root, "Recharge");
		AddFunction<OutfitPieces>(a_view, a_root, "Outfit");
		AddFunction<OutfitCard>(a_view, a_root, "OutfitCard");
		AddFunction<OutfitModel>(a_view, a_root, "OutfitModel");
		AddFunction<TakeOffPiece>(a_view, a_root, "TakeOffPiece");
		AddFunction<Hands>(a_view, a_root, "Hands");
		AddFunction<TakeOffHand>(a_view, a_root, "TakeOffHand");
		AddFunction<ShownReadOnly>(a_view, a_root, "ReadOnly");
		AddFunction<NoteGift>(a_view, a_root, "Gift");
		AddFunction<HeroDrawer>(a_view, a_root, "HeroDrawer");
		AddFunction<SetHeroDrawer>(a_view, a_root, "SetHeroDrawer");
		AddFunction<GetSettings>(a_view, a_root, "Settings");
		AddFunction<ChangeSetting>(a_view, a_root, "SetSetting");
		AddFunction<GetWords>(a_view, a_root, "Words");
		AddFunction<GetScreenSize>(a_view, a_root, "ScreenSize");
		AddFunction<Remember>(a_view, a_root, "Remember");
		AddFunction<Recall>(a_view, a_root, "Recall");
		AddFunction<ScriptStarts>(a_view, a_root, "ScriptStarts");
		AddFunction<ScriptStops>(a_view, a_root, "ScriptStops");
		AddFunction<ScriptClock>(a_view, a_root, "Clock");
		AddFunction<SetJunk>(a_view, a_root, "SetJunk");
		AddFunction<Carry>(a_view, a_root, "Carry");
		AddFunction<Favorite>(a_view, a_root, "Favorite");
#if IR_DEBUG_EDITION
		Debug::Script::AddFunctions(a_view, a_root);
#endif
		if (a_view) {
			Inject::IntoMenu(*a_view);
		}
		return true;
	}

	// The inventory draws its picture and then its 3D over it, so a window of the panel over the doll had to
	// hide the doll. The doll is drawn before the picture now and the windows lie over it like over the world.
	// A container menu has a panel only while it shows a follower
	template <class Menu>
	struct MenuDisplay
	{
		static void thunk(RE::IMenu* a_menu)
		{
			if constexpr (std::is_same_v<Menu, RE::ContainerMenu>) {
				Recharge::Frame();
				if (!Subject::Other()) {
					Profile::MenuFrame();
					IR_FRAME_DRAWN(a_menu);
					original(a_menu);
					Eyes::Display();
					return;
				}
				Subject::Frame();
			}
			if constexpr (std::is_same_v<Menu, RE::MagicMenu>) {
				if (!Settings::Get().magicMenu) {
					original(a_menu);
					return;
				}
			}
			static const auto work = Profile::Work("menu frame");
			Profile::MenuFrame();
			IR_FRAME_DRAWN(a_menu);
			bool under = false;
			{
				const Profile::Timer timer{ work };
				Hidden::MenuFrame();
				PlayerView::Frame();
				Scene::KeepPreview();
				ItemView::FitPreview();
				Pad::Frame();
				Motion::Frame();
				under = a_menu->uiMovie && Scene::DrawUnder();
			}
			{
				const Scene::TemporalBlend blend{ !under };
				original(a_menu);
				// the item menus draw their 3D over the picture themselves, the magic menu has none, the slot models
				// get that pass here
				if constexpr (std::is_same_v<Menu, RE::MagicMenu>) {
					RE::Inventory3DManager::GetSingleton()->Render();
				}
			}
			if (under) {
				Scene::DrawnUnder();
			}
			Eyes::Display();
		}

		static void Install()
		{
			constexpr std::size_t kPostDisplay = 6;
			REL::Relocation<std::uintptr_t> table{ Menu::VTABLE[0] };
			original = table.write_vfunc(kPostDisplay, thunk);
		}

		static inline REL::Relocation<decltype(thunk)> original;
	};

#if IR_DEBUG_EDITION
	// The steps of an item menu, being made, a message from the game and its advance. Each is a hook with the game's
	// own work inside, so a stall says which step it got stuck in. Slots and arguments are the ones of IMenu in
	// CommonLib, 2 is PostCreate(), 4 is ProcessMessage(UIMessage&) and 5 is AdvanceMovie(float, std::uint32_t)
	template <class Menu>
	struct MenuSteps
	{
		static void Create(RE::IMenu* a_menu)
		{
			IR_HOOK("item menu made");
			IR_NOTE(irTimer, "{}", Menu::MENU_NAME);
			create(a_menu);
		}

		static RE::UI_MESSAGE_RESULTS Message(RE::IMenu* a_menu, RE::UIMessage& a_message)
		{
			IR_HOOK("item menu message");
			IR_NOTE(irTimer, "{} message {}", Menu::MENU_NAME, std::to_underlying(a_message.type.get()));
			return message(a_menu, a_message);
		}

		static void Advance(RE::IMenu* a_menu, float a_interval, std::uint32_t a_time)
		{
			IR_HOOK("item menu advance");
			IR_NOTE(irTimer, "{} frame {}", Menu::MENU_NAME, Debug::Frame());
			advance(a_menu, a_interval, a_time);
		}

		// goes in after ZoomKey::Install, which reads the game's own message handler of the inventory from slot 4
		static void Install()
		{
			constexpr std::size_t kPostCreate = 2;
			constexpr std::size_t kProcessMessage = 4;
			constexpr std::size_t kAdvanceMovie = 5;
			REL::Relocation<std::uintptr_t> table{ Menu::VTABLE[0] };
			create = table.write_vfunc(kPostCreate, Create);
			message = table.write_vfunc(kProcessMessage, Message);
			advance = table.write_vfunc(kAdvanceMovie, Advance);
		}

		static inline REL::Relocation<decltype(Create)> create;
		static inline REL::Relocation<decltype(Message)> message;
		static inline REL::Relocation<decltype(Advance)> advance;
	};
#endif

	// the item menu the panel lives in that is open now: the inventory, a container or a shop
	RE::GPtr<RE::IMenu> OpenItemMenu()
	{
		auto* ui = RE::UI::GetSingleton();
		for (const auto name : { RE::InventoryMenu::MENU_NAME, RE::ContainerMenu::MENU_NAME, RE::BarterMenu::MENU_NAME }) {
			if (ui->IsMenuOpen(name)) {
				if (auto menu = ui->GetMenu(name); menu && menu->uiMovie) {
					return menu;
				}
			}
		}
		return nullptr;
	}

	// A right click on a slot or a row opens its menu. In item menus the game turns a right press into its
	// equip to the left hand and never hands it to a clip as a press, so the plugin watches the mouse and tells
	// the panel script, which knows what is under the pointer
	// A right click with Ctrl, Shift or Alt held belongs to SkyUI and other mods (SkyUI's equip mode in a chest, Follower
	// Equip Control's left hand of a follower), the panel tells the script so and the menu of a row stays shut
	class RightClickWatch : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
		{
			constexpr std::uint32_t kRightButton = 1;
			for (auto* event = a_events ? *a_events : nullptr; event; event = event->next) {
				const auto* button = event->AsButtonEvent();
				if (!button) {
					continue;
				}
				if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
					NoteModifier(button->GetIDCode(), button->IsPressed());
					continue;
				}
				if (button->GetDevice() != RE::INPUT_DEVICE::kMouse || button->GetIDCode() != kRightButton || !button->IsDown()) {
					continue;
				}
				if (const auto menu = OpenItemMenu()) {
					const RE::GFxValue keyHeld{ ModifierHeld() };
					IR_SCRIPT("_global.Equipment.rightPress");
					menu->uiMovie->Invoke("_global.Equipment.rightPress", nullptr, &keyHeld, 1);
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}

	private:
		static constexpr std::array<std::uint32_t, 6> kModifiers{ 29, 157, 42, 54, 56, 184 };
		// a held key is told every frame, one not told for this long was let go where the game didn't see it
		static constexpr auto kStillHeld = std::chrono::milliseconds(500);

		void NoteModifier(std::uint32_t a_key, bool a_down)
		{
			for (std::size_t i = 0; i < kModifiers.size(); ++i) {
				if (kModifiers[i] == a_key) {
					pressedAt[i] = a_down ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
				}
			}
		}

		bool ModifierHeld() const
		{
			const auto now = std::chrono::steady_clock::now();
			return std::ranges::any_of(pressedAt, [&](const auto& a_at) { return now - a_at < kStillHeld; });
		}

		std::array<std::chrono::steady_clock::time_point, kModifiers.size()> pressedAt{};
	};

	// the menus the panel is reached through, the tween menu comes before the inventory
	constexpr std::array kPanelWay{ RE::TweenMenu::MENU_NAME, RE::InventoryMenu::MENU_NAME, RE::ContainerMenu::MENU_NAME, RE::BarterMenu::MENU_NAME, RE::MagicMenu::MENU_NAME };

	// this menu is on the way to the panel and none of the others is up
	bool AloneOnPanelWay(const RE::BSFixedString& a_name)
	{
		auto* ui = RE::UI::GetSingleton();
		bool onWay = false;
		for (const auto name : kPanelWay) {
			if (a_name == name) {
				onWay = true;
			} else if (ui->IsMenuOpen(name)) {
				return false;
			}
		}
		return onWay;
	}

	// SkyUI switches the inventory and the magic menu into each other (LAlt), and the one coming may open before the one
	// going closes. The one going leaves the doll, the slot models and the 3D of the item menus to it then: the magic
	// menu's End3D took the lights, the previews and the slot models of the inventory just opened over it
	bool HandedOver(const RE::BSFixedString& a_closing)
	{
		auto* ui = RE::UI::GetSingleton();
		if (a_closing == RE::MagicMenu::MENU_NAME) {
			return ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) || ui->IsMenuOpen(RE::BarterMenu::MENU_NAME);
		}
		return Settings::Get().magicMenu && ui->IsMenuOpen(RE::MagicMenu::MENU_NAME);
	}

	// Added at data load, so mods loaded after this one hear a menu open later and the camera here is still the player's
	class MenuWatch : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event) {
				return RE::BSEventNotifyControl::kContinue;
			}
			if (AloneOnPanelWay(a_event->menuName)) {
				if (a_event->opening) {
					PlayerView::MenusOpening();
				} else {
					PlayerView::MenusClosed();
				}
			}
			const bool itemMenu = a_event->menuName == RE::InventoryMenu::MENU_NAME || a_event->menuName == RE::ContainerMenu::MENU_NAME ||
				a_event->menuName == RE::BarterMenu::MENU_NAME;
			// the magic menu draws no 3D of its own, the doll gets the item menus' scene and light
			const bool magicMenu = a_event->menuName == RE::MagicMenu::MENU_NAME && Settings::Get().magicMenu;
			if (a_event->opening) {
				if (itemMenu) {
					Profile::MenuOpened(a_event->menuName.c_str());
					Rows::MenuOpened();
				}
				if (magicMenu) {
					RE::Inventory3DManager::GetSingleton()->Begin3D(RE::INTERFACE_LIGHT_SCHEME::kInventory);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
			if (itemMenu) {
				Eyes::Clear();
				Quests::Refresh();
				// the menu of a row in a chest or a shop covers its box too, a chest closed with it open left the cover
				// over the slot models of the next inventory
				Scene::Uncover();
				Motion::MenuClosed();
				// the window of a chest or a shop moved the game's model of the item into it, the next menu finds it home
				ItemView::RestorePreview();
			}
			const bool follower = a_event->menuName == RE::ContainerMenu::MENU_NAME && Subject::Trading();
			// a trade ends with its container menu, an inventory a script opened over it leaves it as it was
			if (a_event->menuName == RE::ContainerMenu::MENU_NAME) {
				Subject::Finish();
			}
			if (a_event->menuName == RE::InventoryMenu::MENU_NAME || follower) {
				if (!follower) {
					Acquired::Seen();
				}
				Pad::Closed();
				Inspect::End();
				if (!HandedOver(a_event->menuName)) {
					PlayerView::Closed();
					ItemView::Clear();
					ItemView::RestorePreview();
				}
				Scene::Hide(false);
				Scene::ShowPreview(true);
				Scene::KeepPreview();
				// an inventory a script opened over a follower's trade took the one doll and the slot models along,
				// the trade under it makes its own again
				if (!follower && Subject::Trading()) {
					Subject::SceneAgain();
				}
			} else if (a_event->menuName == RE::JournalMenu::MENU_NAME) {
				// the MCM menu of the mod lives in the journal, what was set there holds from now on
				ModMenu::Reload();
			} else if (magicMenu) {
				Pad::Closed();
				Scene::Hide(false);
				if (!HandedOver(a_event->menuName)) {
					PlayerView::Closed();
					ItemView::Clear();
					RE::Inventory3DManager::GetSingleton()->End3D();
				}
			}
			if (itemMenu) {
				Profile::Report("menu closed");
				Companions::MenuClosed(a_event->menuName);
			}
#if IR_DEBUG_EDITION
			// the debug edition watches the magic menu for stalls too, its watch ends here
			if (magicMenu) {
				Profile::Report("menu closed");
			}
#endif
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	// the button of the mod's menu that shows every hidden slot again
	void ShowAllHidden(RE::StaticFunctionTag*)
	{
		Hidden::ShowAll();
	}

	// the menu of the mod reads and sets one setting by its id, "bModels:Slots"
	float GetSetting(RE::StaticFunctionTag*, RE::BSFixedString a_id)
	{
		return Settings::Value(a_id.c_str());
	}

	void SetSetting(RE::StaticFunctionTag*, RE::BSFixedString a_id, float a_value)
	{
		Settings::Set(a_id.c_str(), a_value);
	}

	// the button of the mod's menu for a follower a mod left read only and is gone
	void ClearReadOnly(RE::StaticFunctionTag*)
	{
		Subject::ForgetReadOnly();
	}

	bool BindMenuScript(RE::BSScript::IVirtualMachine* a_machine)
	{
		a_machine->RegisterFunction("ShowAllHidden", "InventoryReforged", ShowAllHidden);
		a_machine->RegisterFunction("ClearReadOnly", "InventoryReforged", ClearReadOnly);
		a_machine->RegisterFunction("GetSetting", "InventoryReforged", GetSetting);
		a_machine->RegisterFunction("SetSetting", "InventoryReforged", SetSetting);
		Api::Bind(*a_machine);
		return true;
	}

	void SaveGame(SKSE::SerializationInterface* a_save)
	{
		Profile::Report("game saved");
		IR_BRACKET("the plugin writes its records into the save");
		Acquired::Save(a_save);
		Junk::Save(a_save);
		Hidden::Save(a_save);
		Keep::Save(a_save);
		Api::Save(a_save);
	}

	void LoadGame(SKSE::SerializationInterface* a_save)
	{
		Acquired::Forget();
		Junk::Forget();
		Hidden::Forget();
		Keep::Forget();
		Quests::Forget();
		Api::Revert();
		// a trade up while a save loads ends with the old game, its last events go nowhere
		Subject::Abandon();
		std::uint32_t record = 0, version = 0, length = 0;
		while (a_save->GetNextRecordInfo(record, version, length)) {
			IR_LOG("the save holds the plugin's record {}{}{}{} version {}, {} bytes", static_cast<char>(record >> 24), static_cast<char>(record >> 16),
				static_cast<char>(record >> 8), static_cast<char>(record), version, length);
			if (record == 'ACQD') {
				Acquired::Read(a_save, version);
			} else if (record == 'HIDE') {
				Hidden::Read(a_save, version);
			} else if (record == Hidden::kFollowersRecord) {
				Hidden::ReadFollowers(a_save, version);
			} else if (record == Keep::kRecord) {
				Keep::Read(a_save, version);
			} else if (record == 'JUNK') {
				Junk::Read(a_save, version);
			} else if (record == Api::kRecord || record == Api::kReadOnlyRecord) {
				Api::Load(a_save, record);
			}
		}
	}

	void Revert(SKSE::SerializationInterface*)
	{
		IR_LOG("a new game or a save: what the plugin kept of the last one is forgotten");
		Acquired::Forget();
		Junk::Forget();
		Hidden::Forget();
		Keep::Forget();
		Quests::Forget();
		Api::Revert();
		// a trade up while a save loads ends with the old game, its last events go nowhere
		Subject::Abandon();
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
#if IR_DEBUG_EDITION
		Debug::Journal::Message(*a_message);
		if (a_message->type == SKSE::MessagingInterface::kDataLoaded) {
			Debug::Journal::Listen();
		}
#endif
		switch (a_message->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			{
				static MenuWatch watch;
				RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&watch);
				static RightClickWatch rightClicks;
				RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&rightClicks);
				Acquired::Listen();
				Rows::Listen();
				Hidden::Listen();
				Keep::Listen();
				Subject::Listen();
				StillCamera::Listen();
				// The panel and the pages read the file through the plugin, the menus in MCM get it from SKSE with the
				// plugin of the menu in the load order. A mod that swaps or rebuilds the game's translator crashed Know
				// your Skyrim writing into it, so this keeps off it too
				Translations::Load("InventoryReforged", "$IR_", false);
				Pad::Listen();
				// the pages speak the language of the game, so they come after its words
				ModMenu::Register();
				// who holds which item is in the records alone, read here so no menu waits on it
				Holders::Read();
				Profile::Watch();
				break;
			}
		case SKSE::MessagingInterface::kPostLoadGame:
			// the body may have been built before the save told which slots are hidden
			Hidden::Refresh();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
#if IR_DEBUG_EDITION
	SKSE::Init(a_skse, false);
	Debug::StartLog(*a_skse);
#else
	SKSE::Init(a_skse);
#endif
#if IR_DOVAKARN
	// Dovakarn: its build is made and checked for the one game its server runs, Skyrim 1.6.1170, and loads on no other
	if (const auto runtime = a_skse->RuntimeVersion(); runtime != SKSE::RUNTIME_SSE_1_6_1170) {
		logger::critical("Dovakarn's build of Inventory Reforged runs on Skyrim 1.6.1170 only, this game is {}: the plugin does not load",
			runtime.string("."));
		return false;
	}
#endif
	// plugins load on the thread the game later runs its frames on, the message of loaded data comes from another one
	Stacks::MainThreadIsThis();
	Settings::Load();

	// the zoom key and the calls of the item lists
	SKSE::AllocTrampoline(kRoomForACall * (2 + StolenRows::kCalls));
	ZoomKey::Install(kZoomKeyScript, kGameZoomScript, [] { return Settings::Get().panel; });
	// a follower's trade shows the panel too, the game's zoom there faded the menu and left the doll over the world
	ZoomKey::InstallTrade(kZoomKeyScript, kGameZoomScript, [] { return Settings::Get().panel && Subject::Other(); });
	StolenRows::Install();
	MenuDisplay<RE::InventoryMenu>::Install();
	MenuDisplay<RE::ContainerMenu>::Install();
	MenuDisplay<RE::MagicMenu>::Install();
#if IR_DEBUG_EDITION
	// after ZoomKey, it looks for the game's own handler in slot 4 of the inventory's and the container menu's tables
	MenuSteps<RE::InventoryMenu>::Install();
	MenuSteps<RE::ContainerMenu>::Install();
	MenuSteps<RE::BarterMenu>::Install();
	MenuSteps<RE::MagicMenu>::Install();
#endif
	Motion::Install();
	Eyes::Install();
	Hidden::Install();

	SKSE::GetScaleformInterface()->Register(AddMenuFunctions, "EquipmentPanel");
	SKSE::GetPapyrusInterface()->Register(BindMenuScript);
	Rows::Install();

	const auto* saving = SKSE::GetSerializationInterface();
	saving->SetUniqueID(kCosave);
	saving->SetSaveCallback(SaveGame);
	saving->SetLoadCallback(LoadGame);
	saving->SetRevertCallback(Revert);

	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
