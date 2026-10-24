#pragma once

// What happens in the game around the panel: every menu that opens and closes, what the player and the follower shown
// put on and take off, what goes in and out of their inventories, the messages of SKSE with the names of the saves
namespace Debug::Journal
{
	// once the game data is loaded
	void Listen();

	void Message(const SKSE::MessagingInterface::Message& a_message);

	// a call of the panel's native functions told as one line before and one after, for the calls that change the game
	class NativeCall
	{
	public:
		NativeCall(const char* a_name, const RE::GFxFunctionHandler::Params& a_params) noexcept;
		~NativeCall();
		NativeCall(const NativeCall&) = delete;
		NativeCall& operator=(const NativeCall&) = delete;

	private:
		const char* name;
		const RE::GFxFunctionHandler::Params& params;
		std::chrono::steady_clock::time_point start{ std::chrono::steady_clock::now() };
	};
}
