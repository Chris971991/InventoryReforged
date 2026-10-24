#include "PCH.h"

#include "Inspect.h"

#include "Copy.h"
#include "ItemView.h"
#include "Scene.h"
#include "WornView.h"

namespace Inspect
{
	namespace
	{
		constexpr float kNearest = 4.0F;
		constexpr float kFarthest = 0.4F;
		constexpr float kSteepest = 80.0F;

		RE::NiPointer<RE::NiAVObject> model;
		bool asWorn = false;
		WornView::Fitted fitted;
		RE::NiPoint3 middle;
		float yaw = 0.0F;
		float pitch = 0.0F;
		float closer = 1.0F;

		float Radians(float a_degrees)
		{
			return a_degrees * std::numbers::pi_v<float> / 180.0F;
		}

		// Anything else in the 3D of the menu would stand over the inspected item: the game's preview of the selected
		// item, the models of the panel or of the item browser's tile, whatever another mod put there. Only what was
		// hidden here comes back, a model the game or a mod keeps out of sight stays so, and so does the preview of
		// an item that is not selected any more
		struct SteppedOut
		{
			RE::NiPointer<RE::NiAVObject> model;
			bool preview;
		};
		std::vector<SteppedOut> othersHidden;

		void HideOthers(bool a_hidden)
		{
			if (!a_hidden) {
				for (const auto& hidden : othersHidden) {
					if (!hidden.preview || Scene::SelectedPreview(*hidden.model)) {
						hidden.model->SetAppCulled(false);
					}
				}
				othersHidden.clear();
				return;
			}
			auto* scene = RE::UI3DSceneManager::GetSingleton();
			const RE::BSSpinLockGuard locker(scene->lock);
			for (const auto& node : scene->menuObjects) {
				if (!node) {
					continue;
				}
				for (const auto& child : node->GetChildren()) {
					if (child && child.get() != model.get() && !child->GetAppCulled()) {
						child->SetAppCulled(true);
						othersHidden.push_back({ child, Scene::HeldPreview(*child) });
					}
				}
			}
		}

		// the turn and size go on around the middle of the item, which stays in the middle of the screen
		void Place()
		{
			const auto camera = Scene::Camera();
			const auto turn = Scene::About(camera.right, Radians(pitch)) * Scene::About(camera.up, Radians(yaw)) * fitted.facing;
			const float scale = fitted.scale * closer;
			model->local.rotate = turn;
			model->local.scale = scale;
			model->local.translate = middle - (turn * fitted.middle) * scale;
			RE::NiUpdateData update;
			model->Update(update);
		}
	}

	bool Begin(RE::FormID a_item, bool a_worn, float a_centerX, float a_centerY, float a_size)
	{
		End();
		auto* form = RE::TESForm::LookupByID(a_item);
		if (!form) {
			return false;
		}
		RE::NiMatrix3 facing;
		if (a_worn) {
			model = WornView::Make(*form);
			asWorn = model != nullptr;
			if (model) {
				const auto camera = Scene::Camera();
				facing = Scene::Columns(camera.up.Cross(camera.look), camera.look * -1.0F, camera.up);
			}
		}
		if (!model) {
			model = ItemView::Ground(*form, facing);
		}
		if (!model) {
			return false;
		}
		yaw = pitch = 0.0F;
		closer = 1.0F;
		fitted = WornView::FitAround(*model, facing, a_centerX, a_centerY, a_size);
		middle = Scene::Point(a_centerX, a_centerY);
		Scene::Add(*model, Scene::Around(a_centerX, a_centerY, a_size, a_size), Scene::Layer::kInspect);
		Scene::Hide(true);
		HideOthers(true);
		Place();
		return true;
	}

	void Turn(float a_yaw, float a_pitch)
	{
		if (!model) {
			return;
		}
		yaw = std::fmod(yaw + a_yaw, 360.0F);
		pitch = std::clamp(pitch + a_pitch, -kSteepest, kSteepest);
		Place();
	}

	void Zoom(float a_factor)
	{
		if (!model) {
			return;
		}
		closer = std::clamp(closer * a_factor, kFarthest, kNearest);
		Place();
	}

	void End()
	{
		if (!model) {
			return;
		}
		Scene::Remove(*model);
		if (asWorn) {
			Copy::Release(model);
		}
		ItemView::LetGo(model);
		asWorn = false;
		Scene::Hide(false);
		HideOthers(false);
	}

	bool Active()
	{
		return model != nullptr;
	}
}
