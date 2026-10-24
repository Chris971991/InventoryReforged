#include "PCH.h"

#include "PlayerView.h"

#include <unordered_set>

#include "Copy.h"
#include "Hidden.h"
#include "PlayerSize.h"
#include "Pose.h"
#include "Scene.h"
#include "Settings.h"
#include "Subject.h"

namespace PlayerView
{
	namespace
	{
		constexpr auto kFaceNode = "BSFaceGenNiNodeSkinned"sv;
		// the world camera looks at the player's front from within 60 degrees of straight on
		constexpr float kFacing = -0.5F;
		// the tween menu goes a moment before the menu picked in it comes, menus this close are one visit
		constexpr auto kOneVisit = std::chrono::seconds(1);
		// The game stands still under the menus and StillCamera holds back the turns of Souls RE, so a camera that
		// turned more than 5 degrees or went 15 units since the menus came up was moved by a mod
		constexpr float kStillTurn = 0.996F;
		constexpr float kStillWay = 15.0F;
		RE::NiPointer<RE::NiAVObject> copy;
		std::size_t copied = 0;
		// a follower's body after the player's, both may be made of the same pieces
		const RE::NiAVObject* copiedBody = nullptr;
		std::size_t made = 0;
		Place placed{};
		RE::NiMatrix3 facing;
		float turned = 0.0F;
		bool gaveWay = false;

		struct CameraPose
		{
			RE::NiPoint3 at;
			RE::NiPoint3 look;
		};
		// where the camera stood when the menus came up, if it already looked the player in the face then. Menu
		// events come on a thread of their own
		std::mutex beforeLock;
		std::optional<CameraPose> ownFrontView;
		std::atomic<std::chrono::steady_clock::rep> menusClosed{ 0 };
		// the body was dressed anew while the doll is up, told to the panel by the next frame of the menu
		std::atomic<bool> bodyRebuilt{ false };
		// Body mods shape a body built anew a moment later in the same shapes, nothing the doll can tell from the body
		// changes: Lydia's steel armor stood flat on the doll and had her body's shape on a copy made a few seconds
		// after. For a while after the body is dressed anew the doll is copied again whether it looks changed or not
		constexpr auto kFreshFor = std::chrono::seconds(3);
		constexpr auto kFreshEvery = std::chrono::milliseconds(300);
		std::atomic<std::chrono::steady_clock::rep> freshUntil{ 0 };
		std::chrono::steady_clock::time_point freshCopied{};

		bool Fresh()
		{
			const auto now = std::chrono::steady_clock::now();
			if (now.time_since_epoch().count() >= freshUntil.load() || now - freshCopied < kFreshEvery) {
				return false;
			}
			freshCopied = now;
			return true;
		}

		// The size and the place the player gave the doll in the inventory: a share of the size that fits the room,
		// and shares of the room's width to the right and of its height upward
		struct Look
		{
			float scale = 1.0F;
			float right = 0.0F;
			float up = 0.0F;

			bool operator==(const Look&) const = default;
		};
		Look looked{};

		Look Wanted()
		{
			const auto& values = Settings::Get();
			return { static_cast<float>(values.dollScale) / 100.0F, static_cast<float>(values.dollRight) / 100.0F, static_cast<float>(values.dollUp) / 100.0F };
		}

		std::optional<CameraPose> CameraNow()
		{
			const auto* camera = RE::Main::WorldRootCamera();
			if (!camera) {
				return std::nullopt;
			}
			const auto& turn = camera->world.rotate;
			return CameraPose{ camera->world.translate, { turn.entry[0][0], turn.entry[1][0], turn.entry[2][0] } };
		}

		bool CameraFacesPlayer(const RE::PlayerCharacter& a_player)
		{
			const auto camera = CameraNow();
			if (!camera || RE::PlayerCamera::GetSingleton()->IsInFirstPerson()) {
				return false;
			}
			const float heading = a_player.data.angle.z;
			const RE::NiPoint3 ahead{ std::sin(heading), std::cos(heading), 0.0F };
			return camera->look.Dot(ahead) < kFacing;
		}

		// Show Player In Menus, Show Player In Inventory and Menu Studio show the real player in item menus, and
		// all of them turn the world camera round to the player's front once a menu opens. The doll would stand
		// over that player. A camera the player turned that way before the menus (watching an idle, say) keeps
		// the doll while it stays where it was
		bool RealPlayerShown(const RE::PlayerCharacter& a_player)
		{
			if (!CameraFacesPlayer(a_player)) {
				return false;
			}
			const std::scoped_lock locker(beforeLock);
			const auto now = CameraNow();
			return !ownFrontView || !now || now->look.Dot(ownFrontView->look) < kStillTurn || now->at.GetDistance(ownFrontView->at) > kStillWay;
		}

		void Face()
		{
			const auto camera = Scene::Camera();
			copy->local.rotate = Scene::About(camera.up, turned * std::numbers::pi_v<float> / 180.0F) * facing;
			RE::NiUpdateData update;
			copy->Update(update);
		}

		// Dovakarn: the local turn and place of every node of a doll whose name it holds once, by name. A name held twice
		// (two pieces that bring a node of one name) can't be told apart in the next copy, it's left out
		using HeldPose = std::unordered_map<std::string, std::optional<RE::NiTransform>>;

		HeldPose Hold(RE::NiAVObject& a_doll)
		{
			HeldPose held;
			Scene::Visit(a_doll, [&](RE::NiAVObject& a_part) {
				const char* name = a_part.name.c_str();
				if (&a_part == &a_doll || !a_part.AsNode() || !name || !*name) {
					return;
				}
				const auto [at, added] = held.try_emplace(name, a_part.local);
				if (!added) {
					at->second.reset();
				}
			});
			return held;
		}

		// Dovakarn: the skeleton's "NPC" node and every node above it up to the doll are where a body mod moves the whole
		// body (RaceMenu's high heels lift "NPC" by the heel height of the shoes worn). A change of gear changes that move
		// for real, so those nodes are always taken from the new copy, never held from the one before
		constexpr auto kWholeBodyNode = "NPC"sv;

		std::unordered_set<const RE::NiAVObject*> WholeBodyMoves(RE::NiAVObject& a_doll)
		{
			std::unordered_set<const RE::NiAVObject*> moves;
			Scene::Visit(a_doll, [&](RE::NiAVObject& a_part) {
				const char* name = a_part.name.c_str();
				if (!name || kWholeBodyNode != name) {
					return;
				}
				for (const RE::NiAVObject* up = &a_part; up && up != &a_doll; up = up->parent) {
					moves.insert(up);
				}
			});
			return moves;
		}

		// the held turns and places go onto the new copy's nodes of those names, its scale stays the copy's own
		void Restore(RE::NiAVObject& a_doll, const HeldPose& a_held)
		{
			if (a_held.empty()) {
				return;
			}
			std::unordered_map<std::string, std::size_t> seen;
			std::vector<RE::NiAVObject*> nodes;
			Scene::Visit(a_doll, [&](RE::NiAVObject& a_part) {
				const char* name = a_part.name.c_str();
				if (&a_part != &a_doll && a_part.AsNode() && name && *name && ++seen[name] == 1) {
					nodes.push_back(&a_part);
				}
			});
			const auto wholeBody = WholeBodyMoves(a_doll);
			std::size_t kept = 0;
			for (auto* node : nodes) {
				const std::string name = node->name.c_str();
				const auto found = a_held.find(name);
				if (found == a_held.end() || !found->second || seen[name] != 1 || wholeBody.contains(node)) {
					continue;
				}
				node->local.rotate = found->second->rotate;
				node->local.translate = found->second->translate;
				++kept;
			}
			static std::size_t told = 0;
			if (told < 3) {
				++told;
				logger::info("the doll copied again keeps the pose of the copy before on {} nodes", kept);
			}
		}

		// the copy faces the menu camera, as large as its room lets a body of this size be, then as the player sized
		// and moved it. Gives back the part of the screen the body takes
		Scene::Box Stand(const PlayerSize::Size& a_size)
		{
			const auto camera = Scene::Camera();
			const float grow = std::min(Scene::Length(placed.height) / a_size.height, Scene::Length(placed.width) / a_size.width) * looked.scale;
			// the width of the room is a share of the screen's height, the middle is a share of its width
			const float centerX = placed.centerX + looked.right * placed.width / Scene::Aspect();
			const float centerY = placed.centerY - looked.up * placed.height;
			copy->local.scale *= grow;
			copy->local.translate = Scene::Point(centerX, centerY) - camera.up * (a_size.height * grow * 0.5F);
			Scene::Turn(*copy, camera.up.Cross(camera.look), camera.look * -1.0F, camera.up);
			facing = copy->local.rotate;
			const float wide = placed.width * a_size.width * grow / Scene::Length(placed.width);
			const float high = placed.height * a_size.height * grow / Scene::Length(placed.height);
			return Scene::Around(centerX, centerY, wide, high);
		}
	}

	bool Show(const Place& a_place)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		const auto shown = Subject::Get();
		auto* body = shown ? shown->Get3D(false) : nullptr;
		const bool other = shown && shown.get() != player;
		if (!body || !Settings::Get().doll) {
			static RE::Actor* toldOf = nullptr;
			if (other && !body && std::exchange(toldOf, shown.get()) != shown.get()) {
				logger::info("{} has no body loaded, no doll", shown->GetName());
			}
			Hide();
			return false;
		}
		// the slots stay around the real player then
		if (!other && RealPlayerShown(*player)) {
			if (!gaveWay) {
				logger::info("the doll gives way, the camera looks at the player's front, another mod shows the player in this menu");
			}
			gaveWay = true;
			Hide();
			return false;
		}
		gaveWay = false;

		const std::size_t makeup = Copy::Makeup(*body);
		const bool moved = a_place.centerX != placed.centerX || a_place.centerY != placed.centerY || a_place.width != placed.width || a_place.height != placed.height ||
		                   Wanted() != looked;
		if (copy && makeup == copied && body == copiedBody && !moved && !Fresh()) {
			return false;
		}
		// Dovakarn: the doll keeps the pose it was made in when it is copied again from the same body. Only the bones
		// of the standing pose are set on a copy, every other node (skirt and cloak bones, fingers, the bones physics and
		// idles move) is taken as the body has it that moment. For 3 s after a change of gear the doll is copied every
		// 300 ms (Fresh), so those parts jumped to wherever the body had them, 2 to 3 times a second, and a running world
		// under the menu (Skyrim Souls RE) made the doll look like it updated at 2 FPS. Each node named once keeps its
		// turn and place from the copy before; what the new copy brings new (a piece just put on) is taken once. The "NPC"
		// node and the nodes above it are always the new copy's (WholeBodyMoves: RaceMenu's high heels after new shoes)
		const HeldPose held = copy && body == copiedBody ? Hold(*copy) : HeldPose{};
		Hide();
		placed = a_place;
		looked = Wanted();

		copy = Copy::Body(*body);
		if (!copy) {
			return false;
		}
		++made;
		// the copy switched every node back on, the head under a full helmet goes off again as on the body
		auto* face = copy->GetObjectByName(kFaceNode);
		if (other) {
			const auto* ownFace = body->GetObjectByName(kFaceNode);
			if (face && ownFace && ownFace->GetAppCulled()) {
				face->SetAppCulled(true);
			}
		} else {
			if (face) {
				Hidden::UncoverCopiedFace(*face, *player);
			}
			if (face && Hidden::HeadUnderShownPiece(*player)) {
				face->SetAppCulled(true);
			}
			Hidden::HideHanging(*copy);
		}
		copied = makeup;
		copiedBody = body;
		if (Settings::Get().dumpPose && !other) {
			Pose::Dump(*body, Female(*player));
		}
		if (Pose::KeepsWorldPose() || !Pose::Stand(*copy, Female(*shown))) {
			Pose::LookAhead(*copy, *shown);
		}
		Restore(*copy, held);
		Pose::Tell(*body, *copy, *shown);
		// the windows and menus of the panel lie over the doll, so it's drawn under the menu
		Scene::Add(*copy, Stand(PlayerSize::Of(*shown, *body)), Scene::Layer::kUnder);
		Face();
		return true;
	}

	void Hide()
	{
		if (copy) {
			Scene::Remove(*copy);
			Copy::Release(copy);
		}
	}

	void Closed()
	{
		Hide();
		turned = 0.0F;
		placed = {};
		copiedBody = nullptr;
		gaveWay = false;
	}

	void MenusOpening()
	{
		const auto closed = std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(menusClosed.load()));
		if (std::chrono::steady_clock::now() - closed < kOneVisit) {
			return;
		}
		const std::scoped_lock locker(beforeLock);
		ownFrontView = CameraFacesPlayer(*RE::PlayerCharacter::GetSingleton()) ? CameraNow() : std::nullopt;
		if (ownFrontView) {
			logger::info("the camera looked the player in the face before the menus, the player turned it there, the doll stays");
		}
	}

	void MenusClosed()
	{
		menusClosed = std::chrono::steady_clock::now().time_since_epoch().count();
	}

	void Turn(float a_degrees)
	{
		if (!std::isfinite(a_degrees)) {
			return;
		}
		turned = std::fmod(turned + a_degrees, 360.0F);
		if (copy) {
			Face();
		}
	}

	RE::NiAVObject* Doll()
	{
		return copy.get();
	}

	std::size_t Made()
	{
		return made;
	}

	// The pass of the worn pieces runs inside the game's rebuild of the body, maybe under its locks. It only marks,
	// the next frame of the menu queues the word to the panel
	void BodyRebuilt()
	{
		if (copy) {
			bodyRebuilt = true;
			freshUntil = (std::chrono::steady_clock::now() + kFreshFor).time_since_epoch().count();
		}
	}

	void Frame()
	{
		// A body mod can dress the body anew or change its look in place long after the change the doll followed for
		// a few seconds, a follower's after the trade built them again. Every few frames the body is looked at, a
		// look other than the doll's has the doll follow it
		static constexpr std::uint32_t kLookEvery = 15;
		static std::uint32_t frames = 0;
		if (copy && ++frames % kLookEvery == 0) {
			const auto shown = Subject::Get();
			auto* body = shown ? shown->Get3D(false) : nullptr;
			if (body && (body != copiedBody || Copy::Makeup(*body) != copied)) {
				bodyRebuilt = true;
			}
		}
		if (!bodyRebuilt.exchange(false)) {
			return;
		}
		SKSE::GetTaskInterface()->AddUITask([] {
			auto* ui = RE::UI::GetSingleton();
			for (const auto name : { RE::InventoryMenu::MENU_NAME, RE::ContainerMenu::MENU_NAME, RE::BarterMenu::MENU_NAME }) {
				if (const auto menu = ui->IsMenuOpen(name) ? ui->GetMenu(name) : nullptr; menu && menu->uiMovie) {
					IR_SCRIPT("_global.Equipment.bodyRebuilt");
					menu->uiMovie->Invoke("_global.Equipment.bodyRebuilt", nullptr, nullptr, 0);
				}
			}
		});
	}
}
