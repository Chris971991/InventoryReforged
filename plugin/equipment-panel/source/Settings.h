#pragma once

// What the player can tune. Read from Data/SKSE/Plugins/InventoryReforged.ini that ships with the mod and then
// from Data/MCM/Settings/InventoryReforged.ini where the menu keeps what was set, again each time the journal closes.
// That second file is the one MCM Helper kept, so what was set there before stays
namespace Settings
{
	enum class Pose : std::int32_t
	{
		kStanding,
		kAsInWorld
	};

	struct Values
	{
		bool panel = true;
		// the panel shows the one the player trades with in a follower's trade and in Actor.OpenInventory of any mod
		bool followers = true;
		// tabs over the doll switch the inventory and the trades of the companions near, off unless the player wants them
		bool companionTabs = false;
		// the doll in SkyUI's magic menu between the two hands and the voice
		bool magicMenu = true;
		// with Skyrim Souls RE and the like the camera does not turn while the inventory is open
		bool stillCamera = true;
		bool doll = true;
		Pose pose = Pose::kStanding;
		float turnSpeed = 0.5F;
		// the doll plays what mods of equipping animations play on the player as a piece goes on or off
		bool equipMotion = true;
		bool slotModels = true;
		bool wornLook = true;
		bool hideIcons = true;
		bool popup = true;
		bool doubleClickTakesOff = true;
		bool dragAndDrop = true;
		bool pieceStrips = true;
		bool arrows = true;
		bool equalSign = true;
		bool badges = true;
		bool subcategories = true;
		bool lastAdded = true;
		bool stackWeight = true;
		bool rowMenu = true;
		// a scan code of the keyboard, 0 for none
		std::int32_t stackKey = 45;
		// a dot on what came since the inventory was last closed
		bool newMarks = true;
		// junk marks, a scan code of the key that marks the row under the pointer, 36 is J
		bool junk = true;
		std::int32_t junkKey = 36;
		// in a chest or a shop the weight of a row that would slow the player is red
		bool weightWarnings = true;
		// a sign after the name of a row a quest wants
		bool questMarks = true;
		// the quests are read only for the sign of a row or the line of the window
		bool ReadsQuests() const { return questMarks || questLine; }
		// stolen copies of an item in a row of their own, read each time a list is made and each time a row moves
		bool stolenApart = true;
		// The doll in percent of the size that fits its room, and how far it stands from the middle of the room in
		// percent of the room's width to the right and of its height upward. Set in the inventory itself
		std::int32_t dollScale = 100;
		std::int32_t dollRight = 0;
		std::int32_t dollUp = 0;
		// slots the player took off the panel, bit 1 << slot in the order the panel numbers them
		std::uint32_t slotsOff = 0;
		// the item list of the inventory at its full width, 1 narrow, 2 narrower
		std::int32_t listWidth = 0;
		// the item window in percent of the size the panel gives it for the list and the screen
		std::int32_t windowSize = 100;
		// how the item window comes up over a row: 0 at once, 1 once the row stayed selected a moment, 2 never
		std::int32_t windowShow = 0;
		bool stats = true;
		bool heroStats = true;
		bool slotNumbers = false;
		// quiet lines in the window: how many you have, silver, the plugin it comes from
		bool quietTags = true;
		// the quiet line From <plugin> on its own, some find the file names noise
		bool modOrigin = true;
		// the line of the window that says what a quest wants with the item, apart from the other quiet lines
		bool questLine = true;
		// a skin's own item card stays where and how the skin draws it, the window holds the model and the numbers
		bool leaveCard = false;
		bool inspectOnZoomKey = true;
		// 0xRRGGBB of what is picked or lit and of names at rest, some UI looks want other than the gold
		std::uint32_t accentColor = 0xD6B068;
		std::uint32_t textColor = 0xE6E0C8;
		// for making the standing pose table, the pose of the live player goes to a file on every menu open
		bool dumpPose = false;
		// for the test stand, the pages of SKSE Menu Framework open in a window of their own as the game starts
		bool pagesWindow = false;
#if IR_DEBUG_EDITION
		// for the test stand, the first slots of each item menu wait this many seconds, the watch has to tell it
		std::int32_t stallSeconds = 0;
#endif
	};

	void Load();
	const Values& Get();

	// A setting of the menu by its id, "bModels:Slots" is bModels in [Slots], as both files say it together.
	// Set writes it into the menu's file, it holds once the journal closes
	float Value(std::string_view a_id);
	void Set(std::string_view a_id, float a_value);
	// The same from the panel in the inventory, there it holds at once. Only the game's own thread calls it
	void Change(std::string_view a_id, float a_value);

	// the values as an object of the menu script, names as the script reads them
	void Fill(RE::GFxMovie& a_movie, RE::GFxValue& a_object);
}
