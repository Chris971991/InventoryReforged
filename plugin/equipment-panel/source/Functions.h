#pragma once

#include "Profile.h"

// A function a movie calls as skse.plugins.<plugin>.<name>, timed by the profile under its name. Inventory Reforged and
// Know your Skyrim both make theirs so
namespace Functions
{
	using Params = RE::GFxFunctionHandler::Params;

	inline float Number(const RE::GFxValue& a_value)
	{
		return a_value.IsNumber() ? static_cast<float>(a_value.GetNumber()) : 0.0F;
	}

	inline bool Flag(const RE::GFxValue& a_value)
	{
		return a_value.IsBool() ? a_value.GetBool() : Number(a_value) != 0.0F;
	}

	inline std::string Text(const RE::GFxValue& a_value)
	{
		return a_value.IsString() ? std::string(a_value.GetString()) : std::string();
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
			Body::Run(a_params);
		}

	private:
		std::size_t work;
	};

	template <class Body>
	void Add(RE::GFxMovieView& a_view, RE::GFxValue& a_root, const char* a_name)
	{
		// kept for the whole game and never freed, at exit Scaleform's memory may be gone before this DLL lets go
		static auto* const handler = new Function<Body>(Profile::Work(a_name));
		RE::GFxValue function;
		a_view.CreateFunction(&function, handler);
		a_root.SetMember(a_name, function);
	}
}
