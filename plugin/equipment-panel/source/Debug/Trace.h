#pragma once

// The debug edition (CMake option IR_DEBUG_EDITION) logs what the plugin and the panel do, so a player's log tells
// where a bug or a freeze comes from. In the release build every macro here is empty and its arguments are never
// looked at
#ifndef IR_DEBUG_EDITION
#	define IR_DEBUG_EDITION 0
#endif

#if IR_DEBUG_EDITION
#	include "Debug/Log.h"
#	include "Debug/Script.h"
#	include "Profile.h"

// a line of the debug log
#	define IR_LOG(...) logger::debug(__VA_ARGS__)
// a line before a call into the game and one after it with the time it took, the last line without its pair is
// where a crash or a freeze happened
#	define IR_BRACKET(...) const ::Debug::Bracket irBracket{ __VA_ARGS__ }
// a frequent thing counted, the counts go out with each report
#	define IR_COUNT(a_what) ::Debug::Count(a_what)
// the plugin's own work timed under a name, a stall names it
#	define IR_TIMER(a_name)                              \
		static const auto irWork = ::Profile::Work(a_name); \
		const ::Profile::Timer irTimer { irWork }
// a hook of the plugin that calls the game's own function, a stall names it and its time isn't the plugin's
#	define IR_HOOK(a_name)                              \
		static const auto irWork = ::Profile::Work(a_name); \
		const ::Profile::Timer irTimer { irWork, true }
// a few words on what a timed work does right now, a stall or stuck work prints them
#	define IR_NOTE(a_timer, ...) ::Debug::Note(a_timer, __VA_ARGS__)
// a call into the panel script timed and logged under its path
#	define IR_SCRIPT(a_path)                                            \
		static const auto irScriptWork = ::Profile::Work(a_path);         \
		const ::Profile::Timer irScriptTimer { irScriptWork };            \
		const ::Debug::Bracket irScriptBracket { "script call {}", a_path }
// the same for a call that comes too often for a line each time
#	define IR_SCRIPT_QUIET(a_path)                              \
		static const auto irScriptWork = ::Profile::Work(a_path); \
		const ::Profile::Timer irScriptTimer { irScriptWork }
// a menu of the panel drew a frame
#	define IR_FRAME_DRAWN(a_menu) ::Debug::Script::FrameDrawn((a_menu)->uiMovie.get())
#else
#	define IR_LOG(...) ((void)0)
#	define IR_BRACKET(...) ((void)0)
#	define IR_COUNT(a_what) ((void)0)
#	define IR_TIMER(a_name) ((void)0)
#	define IR_HOOK(a_name) ((void)0)
#	define IR_NOTE(a_timer, ...) ((void)0)
#	define IR_SCRIPT(a_path) ((void)0)
#	define IR_SCRIPT_QUIET(a_path) ((void)0)
#	define IR_FRAME_DRAWN(a_menu) ((void)0)
#endif
