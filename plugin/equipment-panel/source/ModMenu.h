#pragma once

// The settings of the mod in SKSE Menu Framework, for a player who keeps every mod's menu there. The pages are the
// ones of the MCM menus, shipped as SKSE/Plugins/InventoryReforged/pages.json with translation keys, and what is set
// goes into the same file both MCM menus keep, so the three menus always agree. A game without the framework, or
// with a framework that answers to its calls like Apocrypha Menu Framework, needs nothing more
namespace ModMenu
{
	// once every plugin is loaded, the framework is one of them
	void Register();

	// what a menu of the mod set holds from now on. The panel switched off or on hides or shows the hidden slots on
	// the body as well
	void Reload();
}
