// the button stands in the bottom bar right after the one of the item browser, as high as it
Equipment.LOOK_BUTTON = 22;
Equipment.LOOK_BAR_GAP = 8;
Equipment.LOOK_WIDE = 284;
Equipment.LOOK_PAD = 10;
Equipment.LOOK_LINE = 26;
Equipment.LOOK_SLOT_LINE = 22;
Equipment.LOOK_GAP = 6;
Equipment.LOOK_TEXT = 13;
Equipment.LOOK_ARROW = 18;
Equipment.LOOK_VALUE = 92;
Equipment.LOOK_CHECK = 12;
Equipment.LOOK_ICON = 14;
Equipment.LOOK_SHADE = 92;
// SkyUI's own measure of a list and the place of its column dialog at that measure, applyDynamicWidth moves every
// part of the list by the difference from it
Equipment.SKYUI_LIST_WIDTH = 530;
Equipment.SKYUI_DIALOG_X = 554;
// the shares of its full width the item list keeps: full, narrow, narrower
Equipment.LIST_WIDTHS = [1, 0.88, 0.76];
// what a press on an arrow does to a number, the ids are those of the menu of the mod. A press on the left arrow
// of the list makes it narrower
Equipment.LOOK_STEPS = {
   dollScale:{id:"iScale:Doll", step:5, least:60, most:140},
   dollRight:{id:"iShiftRight:Doll", step:5, least:-40, most:40},
   dollUp:{id:"iShiftUp:Doll", step:5, least:-40, most:40},
   listWidth:{id:"iListWidth:Panel", step:-1, least:0, most:2},
   windowSize:{id:"iSize:Window", step:10, least:70, most:160},
   windowShow:{id:"iShow:Window", step:1, least:0, most:2}
};

// The look of the panel is set in the inventory, where it shows at once. A small button in the bottom bar, beside
// the eye of Know your Skyrim when it's there, opens a sheet beside the left column of slots, over the list, so the
// doll and both columns stay in sight: the doll on or off, its size and its place, the slots on the panel, the
// width of the item list, when the item window pops up and its size. Each change goes into the settings of the mod, the menu
// of the mod shows the same. Nothing
// here runs while it's shut
Equipment.buildLook = function()
{
   var menu = Equipment.menu;
   var bar = menu.bottomBar;
   var size = Equipment.LOOK_BUTTON;
   var button = bar.createEmptyMovieClip("equipmentLookButton", bar.getNextHighestDepth());
   var sheet = Equipment.over.createEmptyMovieClip("equipmentLook", Equipment.over.getNextHighestDepth());
   button.createTextField("touch", 0, 0, 0, size, size);
   button.touch.selectable = false;
   button.createEmptyMovieClip("look", 1);
   button.onRollOver = function()
   {
      this.lit = true;
      Equipment.drawLookButton();
   };
   button.onRollOut = button.onReleaseOutside = function()
   {
      this.lit = false;
      Equipment.drawLookButton();
   };
   button.onPress = function()
   {
      Equipment.toggleLook();
   };
   button.onPressAux = function()
   {
   };
   Equipment.lookButton = button;
   sheet._visible = false;
   Equipment.look = sheet;
   // a press anywhere else closes the sheet after what it pressed had its turn
   Equipment.lookCatcher = {};
   Equipment.lookCatcher.onMouseDown = function()
   {
      var x = _root._xmouse;
      var y = _root._ymouse;
      if(Equipment.look._visible && !Equipment.look.hitTest(x, y, true) && !Equipment.lookButton.hitTest(x, y, true))
      {
         Equipment.closeLook();
      }
   };
   Mouse.addListener(Equipment.lookCatcher);
   menu.lookHandleInput = menu.handleInput;
   menu.handleInput = function(details, pathToFocus)
   {
      if(Equipment.look._visible && Shared.GlobalFunc.IsKeyPressed(details) && details.navEquivalent == gfx.ui.NavigationCode.ESCAPE)
      {
         Equipment.closeLook();
         return true;
      }
      return this.lookHandleInput(details, pathToFocus);
   };
   Equipment.placeLookButton();
};

// Right after the eye button of Know your Skyrim, which follows the hints of SkyUI, or after the hints themselves. A menu
// with no hints has it at the foot of the panel
Equipment.placeLookButton = function()
{
   var button = Equipment.lookButton;
   var browse = Equipment.browseButton;
   var bar = Equipment.menu.bottomBar;
   var spot = Equipment.afterHints(Equipment.LOOK_BUTTON);
   if(button == undefined)
   {
      return undefined;
   }
   // what the sheet sets is kept and drawn by the plugin
   button._visible = Equipment.plugin() != undefined;
   if(browse != undefined)
   {
      button._x = Math.round(browse._x + browse.wide + Equipment.LOOK_BAR_GAP);
      button._y = browse._y;
   }
   else if(spot != undefined)
   {
      button._x = spot.x;
      button._y = spot.y;
   }
   else
   {
      button._x = Equipment.frame != undefined ? Math.round(Equipment.frame.right / Equipment.squash - bar._x - Equipment.LOOK_BUTTON) : 0;
      button._y = -Equipment.LOOK_BUTTON - 4;
   }
   Equipment.drawLookButton();
};

Equipment.drawLookButton = function()
{
   var button = Equipment.lookButton;
   var size = Equipment.LOOK_BUTTON;
   var lit = button.lit || Equipment.look._visible;
   // the same box as the eye beside it
   button.look.clear();
   button.look.lineStyle(1, Equipment.accentInk(), lit ? 100 : 75, true);
   Equipment.buttonFill(button.look, lit ? 62 : 42);
   Equipment.roundBox(button.look, 0, 0, size, size, 5);
   button.look.endFill();
   button.look.lineStyle();
   Equipment.drawIcon(button.look, "sliders", 3, 3, size - 6, lit ? Equipment.accentInk() : Equipment.inkOf("sign"), 100);
};

Equipment.toggleLook = function()
{
   if(Equipment.look._visible)
   {
      Equipment.closeLook();
   }
   else if(Equipment.listsAnswer())
   {
      Equipment.openLook();
   }
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

// the windows and menus of the panel go and so does the selected row with its window, the sheet lies over a quiet
// list. The rows under it took the pointer, the wheel and a click, a click read a book or drank a potion
Equipment.openLook = function()
{
   Equipment.closePopup();
   Equipment.closeContext();
   Equipment.closePieces();
   Equipment.menu.inventoryLists.itemList.selectedIndex = -1;
   Equipment.makeLook();
   Equipment.look._visible = true;
   Equipment.holdRows(true);
   Equipment.coverLook();
   Equipment.drawLookButton();
};

Equipment.closeLook = function()
{
   if(!Equipment.look._visible)
   {
      return undefined;
   }
   Equipment.look._visible = false;
   Equipment.holdRows(false);
   Equipment.coverLook();
   Equipment.drawLookButton();
};

// The model of a row the list selects under the sheet is drawn over the menu, the part of it the sheet lies over
// steps out while the sheet is open
Equipment.coverLook = function()
{
   var sheet = Equipment.look;
   var bounds;
   var from;
   var to;
   if(Equipment.plugin() == undefined)
   {
      return undefined;
   }
   if(!sheet._visible)
   {
      Equipment.plugin().Cover();
      return undefined;
   }
   bounds = sheet.getBounds(Equipment.over);
   from = Equipment.share(bounds.xMin, bounds.yMin);
   to = Equipment.share(bounds.xMax, bounds.yMax);
   Equipment.plugin().Cover(from.x, from.y, to.x, to.y);
};

// The sheet is made anew each time it opens. The doll first, then the slots in two columns as the panel has them,
// then the list
Equipment.makeLook = function()
{
   var sheet = Equipment.look;
   var pad = Equipment.LOOK_PAD;
   var wide = Equipment.LOOK_WIDE;
   var inner = wide - 2 * pad;
   var half = inner / 2;
   var y = pad;
   var slot;
   var i = 0;
   for(var name in sheet)
   {
      if(sheet[name] instanceof MovieClip)
      {
         sheet[name].removeMovieClip();
      }
   }
   sheet.clear();
   // a drawn shape takes no mouse in this game, the touch keeps the rows under the sheet from the pointer
   sheet.createTextField("touch", 0, 0, 0, wide, 10);
   sheet.touch.selectable = false;
   sheet.depth = 1;
   sheet.lines = [];
   sheet.wide = wide;
   y += Equipment.lookHead(sheet, Equipment.say("Character"), pad, y, inner);
   y += Equipment.lookCheck(sheet, "doll", Equipment.say("Show the character"), undefined, pad, y, inner, Equipment.LOOK_LINE);
   y += Equipment.lookStepper(sheet, "dollScale", Equipment.say("Size"), "size", pad, y, inner);
   y += Equipment.lookStepper(sheet, "dollRight", Equipment.say("Sideways"), "across", pad, y, inner);
   y += Equipment.lookStepper(sheet, "dollUp", Equipment.say("Up and down"), "upward", pad, y, inner);
   y += Equipment.lookReset(sheet, pad, y, inner);
   y += Equipment.LOOK_GAP;
   y += Equipment.lookHead(sheet, Equipment.say("Slots on the panel"), pad, y, inner);
   while(i < Equipment.SLOTS.length)
   {
      slot = Equipment.SLOTS[i];
      Equipment.lookCheck(sheet, "slot" + i, slot.title, slot.icon, pad + slot.column * half, y + slot.row * Equipment.LOOK_SLOT_LINE, half - 4, Equipment.LOOK_SLOT_LINE);
      i++;
   }
   y += Equipment.ROWS * Equipment.LOOK_SLOT_LINE;
   if(Equipment.listNarrows())
   {
      y += Equipment.LOOK_GAP;
      y += Equipment.lookHead(sheet, Equipment.say("Item list"), pad, y, inner);
      y += Equipment.lookStepper(sheet, "listWidth", Equipment.say("Width"), "across", pad, y, inner);
   }
   y += Equipment.LOOK_GAP;
   y += Equipment.lookHead(sheet, Equipment.say("Item window"), pad, y, inner);
   y += Equipment.lookStepper(sheet, "windowShow", Equipment.say("Pops up"), "across", pad, y, inner);
   y += Equipment.lookStepper(sheet, "windowSize", Equipment.say("Size"), "size", pad, y, inner);
   y += pad;
   sheet.high = y;
   sheet.touch._height = y;
   Equipment.panelBox(sheet, wide, y, Equipment.LOOK_SHADE, 40);
   Equipment.placeLook();
   Equipment.showLookValues();
};

// beside the left column of slots and level with the top of the panel, over the list
Equipment.placeLook = function()
{
   var sheet = Equipment.look;
   var bottom = Equipment.barTop() - Equipment.MARGIN;
   sheet._x = Math.round(Equipment.frame.left - Equipment.POPUP_GAP - sheet.wide);
   sheet._y = Math.round(Math.max(Equipment.screenRect().y, Math.min(Equipment.frame.top, bottom - sheet.high)));
   if(sheet._visible)
   {
      Equipment.coverLook();
   }
};

// the name of a group in small gold capitals over a thin gold line, as in the character drawer
Equipment.lookHead = function(sheet, words, x, y, wide)
{
   var head = sheet.createEmptyMovieClip("head" + sheet.depth, sheet.depth);
   sheet.depth++;
   head._x = x;
   head._y = y;
   head.next = 0;
   return Equipment.heroHead(head, words, 0, 0, wide) + 2;
};

// a part of the sheet that lights under the pointer and takes a press, it's drawn by its own function
Equipment.lookControl = function(parent, name, depth, x, y, wide, high)
{
   var control = parent.createEmptyMovieClip(name, depth);
   control._x = x;
   control._y = y;
   control.wide = wide;
   control.high = high;
   control.createTextField("touch", 0, 0, 0, wide, high);
   control.touch.selectable = false;
   control.createEmptyMovieClip("look", 1);
   control.onRollOver = function()
   {
      this.lit = true;
      this.draw(this);
   };
   control.onRollOut = control.onReleaseOutside = function()
   {
      this.lit = false;
      this.draw(this);
   };
   return control;
};

// a line with a box that's ticked while the thing is on, a slot has its sign before its name
Equipment.lookCheck = function(sheet, key, words, icon, x, y, wide, high)
{
   var line = Equipment.lookControl(sheet, "line" + sheet.depth, sheet.depth, x, y, wide, high);
   sheet.depth++;
   line.key = key;
   line.icon = icon;
   Equipment.writeLine(line, "words", 2, words, Equipment.LOOK_TEXT, Equipment.WHITE);
   line.words._x = Equipment.LOOK_CHECK + 8 + (icon != undefined ? Equipment.LOOK_ICON + 6 : 0);
   line.words._y = Math.round((high - line.words._height) / 2);
   line.draw = Equipment.drawLookCheck;
   line.onPress = function()
   {
      Equipment.flipLook(this.key);
   };
   sheet.lines.push(line);
   return high;
};

// A name, then a value between two arrows. The line itself takes no press, so its arrows get theirs
Equipment.lookStepper = function(sheet, key, words, kind, x, y, wide)
{
   var high = Equipment.LOOK_LINE;
   var arrow = Equipment.LOOK_ARROW;
   var valueWide = Equipment.LOOK_VALUE;
   var line = sheet.createEmptyMovieClip("line" + sheet.depth, sheet.depth);
   sheet.depth++;
   line._x = x;
   line._y = y;
   line.key = key;
   line.kind = kind;
   line.wide = wide;
   line.high = high;
   Equipment.writeLine(line, "words", 1, words, Equipment.LOOK_TEXT, Equipment.PARCHMENT);
   line.words._y = Math.round((high - line.words._height) / 2);
   line.createTextField("value", 2, wide - arrow - valueWide, Math.round((high - 20) / 2), valueWide, 20);
   Equipment.styleText(line.value, Equipment.LOOK_TEXT, "center", Equipment.WHITE);
   line.less = Equipment.lookArrow(line, "less", 3, wide - 2 * arrow - valueWide, -1);
   line.more = Equipment.lookArrow(line, "more", 4, wide - arrow, 1);
   line.draw = Equipment.drawLookStepper;
   sheet.lines.push(line);
   return high;
};

Equipment.lookArrow = function(line, name, depth, x, way)
{
   var size = Equipment.LOOK_ARROW;
   var arrow = Equipment.lookControl(line, name, depth, x, Math.round((line.high - size) / 2), size, size);
   arrow.way = way;
   arrow.draw = Equipment.drawLookArrow;
   arrow.onPress = function()
   {
      Equipment.stepLook(this._parent.key, this.way);
   };
   return arrow;
};

// a small button across the middle that puts the doll back to its size and place
Equipment.lookReset = function(sheet, x, y, wide)
{
   var high = Equipment.LOOK_LINE - 4;
   var button = Equipment.lookControl(sheet, "reset", sheet.depth, x, y + 2, wide, high);
   sheet.depth++;
   Equipment.writeLine(button, "words", 2, Equipment.say("Reset size and place"), Equipment.LOOK_TEXT, Equipment.WHITE);
   button.words._x = Math.round((wide - button.words._width) / 2);
   button.words._y = Math.round((high - button.words._height) / 2);
   button.draw = Equipment.drawLookReset;
   button.onPress = function()
   {
      Equipment.resetDoll();
   };
   sheet.lines.push(button);
   return Equipment.LOOK_LINE + 2;
};

Equipment.showLookValues = function()
{
   var lines = Equipment.look.lines;
   var i = 0;
   while(i < lines.length)
   {
      lines[i].draw(lines[i]);
      i++;
   }
};

Equipment.lookOn = function(key)
{
   if(key == "doll")
   {
      return Equipment.settings.doll == true;
   }
   return Equipment.slotShown(Equipment.SLOTS[Number(key.substr(4))]);
};

Equipment.drawLookCheck = function(line)
{
   var on = Equipment.lookOn(line.key);
   var look = line.look;
   var box = Equipment.LOOK_CHECK;
   var top = Math.round((line.high - box) / 2);
   look.clear();
   look.beginFill(Equipment.GOLD, line.lit ? 16 : 0);
   Equipment.roundBox(look, 0, 0, line.wide, line.high, 3);
   look.endFill();
   look.lineStyle(1, on || line.lit ? Equipment.GOLD : Equipment.PARCHMENT, on || line.lit ? 100 : 55, true);
   look.beginFill(0, 45);
   Equipment.roundBox(look, 2, top, box, box, 2);
   look.endFill();
   if(on)
   {
      look.lineStyle(2, Equipment.GOLD, 100, true, "normal", "round", "round");
      look.moveTo(5, top + box / 2);
      look.lineTo(2 + box * 0.42, top + box - 3);
      look.lineTo(box - 1, top + 3);
   }
   look.lineStyle();
   if(line.icon != undefined)
   {
      Equipment.drawIcon(look, line.icon, box + 8, Math.round((line.high - Equipment.LOOK_ICON) / 2), Equipment.LOOK_ICON, Equipment.PARCHMENT, on ? 90 : 40);
   }
   line.words.textColor = line.lit ? Equipment.GOLD : (on ? Equipment.WHITE : Equipment.inkOf("quiet", true));
};

// The value in words between the arrows. The numbers of the doll go quiet while the doll is off
Equipment.drawLookStepper = function(line)
{
   var value = Equipment.number(Equipment.settings[line.key]);
   var quiet = line.key != "listWidth" && Equipment.settings.doll != true;
   line.value.text = Equipment.lookWords(line.key, value);
   line.value.textColor = quiet ? Equipment.inkOf("quiet", true) : Equipment.WHITE;
   line.words.textColor = quiet ? Equipment.inkOf("quiet", true) : Equipment.PARCHMENT;
   line.less.can = Equipment.lookStepped(line.key, -1) != value;
   line.more.can = Equipment.lookStepped(line.key, 1) != value;
   Equipment.drawLookArrow(line.less);
   Equipment.drawLookArrow(line.more);
};

// A small box with a sign: minus and plus for the size, arrows the way the doll or the list's edge goes. An arrow at
// the end of its range goes faint
Equipment.drawLookArrow = function(arrow)
{
   var size = Equipment.LOOK_ARROW;
   var look = arrow.look;
   var kind = arrow._parent.kind;
   var lit = arrow.lit && arrow.can;
   var mid = size / 2;
   var reach = 4;
   var turn = arrow.way > 0 ? 1 : -1;
   look.clear();
   look.lineStyle(1, lit ? Equipment.GOLD : Equipment.WHITE, lit ? 90 : 28, true);
   look.beginFill(lit ? Equipment.GOLD : 0, lit ? 22 : 40);
   Equipment.roundBox(look, 0, 0, size, size, 3);
   look.endFill();
   look.lineStyle(1.6, lit ? Equipment.GOLD : Equipment.PARCHMENT, arrow.can ? 100 : 30, true, "normal", "round", "round");
   if(kind == "size")
   {
      look.moveTo(mid - reach, mid);
      look.lineTo(mid + reach, mid);
      if(arrow.way > 0)
      {
         look.moveTo(mid, mid - reach);
         look.lineTo(mid, mid + reach);
      }
   }
   else if(kind == "upward")
   {
      look.moveTo(mid - reach, mid + 2 * turn);
      look.lineTo(mid, mid - 2 * turn);
      look.lineTo(mid + reach, mid + 2 * turn);
   }
   else
   {
      look.moveTo(mid - 2 * turn, mid - reach);
      look.lineTo(mid + 2 * turn, mid);
      look.lineTo(mid - 2 * turn, mid + reach);
   }
   look.lineStyle();
};

Equipment.drawLookReset = function(button)
{
   var look = button.look;
   look.clear();
   look.lineStyle(1, button.lit ? Equipment.GOLD : Equipment.WHITE, button.lit ? 90 : 35, true);
   look.beginFill(button.lit ? Equipment.GOLD : 1710618, button.lit ? 25 : 80);
   Equipment.roundBox(look, 0, 0, button.wide, button.high, 4);
   look.endFill();
   look.lineStyle();
   button.words.textColor = button.lit ? Equipment.GOLD : Equipment.WHITE;
};

// the words of a value: 100%, 10% right, Middle, Narrow
Equipment.lookWords = function(key, value)
{
   switch(key)
   {
      case "dollScale":
      case "windowSize":
         return Equipment.say("#%", value);
      case "dollRight":
         return value == 0 ? Equipment.say("Middle") : (value > 0 ? Equipment.say("#% right", value) : Equipment.say("#% left", -value));
      case "dollUp":
         return value == 0 ? Equipment.say("Middle") : (value > 0 ? Equipment.say("#% up", value) : Equipment.say("#% down", -value));
      case "windowShow":
         return [Equipment.say("On hover"), Equipment.say("After a pause"), Equipment.say("Never")][Math.max(0, Math.min(2, value))];
      case "listWidth":
         return [Equipment.say("Full"), Equipment.say("Narrow"), Equipment.say("Narrower")][Math.max(0, Math.min(2, value))];
   }
   return String(value);
};

// the value a press on an arrow gives, within its range
Equipment.lookStepped = function(key, way)
{
   var steps = Equipment.LOOK_STEPS[key];
   var value = Equipment.number(Equipment.settings[key]) + way * steps.step;
   return Math.max(steps.least, Math.min(steps.most, value));
};

// The doll is made anew at its new size or place, the plugin keeps its turn. A narrower list gives its room to the
// panel, which is laid out again
Equipment.stepLook = function(key, way)
{
   var value = Equipment.lookStepped(key, way);
   if(value == Equipment.number(Equipment.settings[key]))
   {
      return undefined;
   }
   Equipment.changeSetting(key, Equipment.LOOK_STEPS[key].id, value);
   if(key == "listWidth")
   {
      if(Equipment.narrowList())
      {
         Equipment.relayout();
      }
   }
   else if(key == "windowSize")
   {
      Equipment.sizeWindow();
      Equipment.showWindow();
   }
   else if(key == "windowShow")
   {
      Equipment.wantWindow(Equipment.menu.inventoryLists.itemList.selectedIndex >= 0);
      Equipment.showWindow();
   }
   else
   {
      Equipment.showPlayer();
   }
   Equipment.showLookValues();
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

Equipment.flipLook = function(key)
{
   if(key == "doll")
   {
      Equipment.changeSetting("doll", "bShow:Doll", Equipment.settings.doll != true);
      Equipment.showPlayer();
   }
   else
   {
      Equipment.flipSlot(Equipment.SLOTS[Number(key.substr(4))]);
   }
   Equipment.showLookValues();
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

Equipment.resetDoll = function()
{
   Equipment.changeSetting("dollScale", Equipment.LOOK_STEPS.dollScale.id, 100);
   Equipment.changeSetting("dollRight", Equipment.LOOK_STEPS.dollRight.id, 0);
   Equipment.changeSetting("dollUp", Equipment.LOOK_STEPS.dollUp.id, 0);
   Equipment.showPlayer();
   Equipment.showLookValues();
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

// A slot taken off the panel or put back. Its window, its strip and its filter go with it, the others keep their
// places and the models are made for the slots in sight
Equipment.flipSlot = function(slot)
{
   var index = Equipment.slotIndex(slot);
   var shown = !Equipment.slotShown(slot);
   var off = Equipment.number(Equipment.settings.slotsOff);
   Equipment.settings.slotsOff = shown ? off & ~(1 << index) : off | (1 << index);
   Equipment.tellSetting("bShow" + slot.name.charAt(0).toUpperCase() + slot.name.substr(1) + ":Slots", shown);
   Equipment.closePopup();
   Equipment.closePieces();
   if(!shown && Equipment.activeSlot == slot)
   {
      Equipment.filterSlot(slot);
   }
   Equipment.layout();
   Equipment.showModels(true);
};

// SkyUI 6 widens its list by the entry width of its config, applyDynamicWidth moves every part of the list by the
// difference from its own measure. Menus made on SkyUI 5 have no such call, their list keeps its width and the sheet
// leaves the width out
Equipment.listNarrows = function()
{
   var lists = Equipment.menu.inventoryLists;
   var layout = lists.itemList.layout;
   return Equipment.kind == "inventory" && layout != undefined && layout.entryWidth > 0 && lists.applyDynamicWidth != undefined;
};

// The list is narrowed the same way, from the width it has now to the share of its full width the player chose, and
// the columns follow the new entry width. The name column gives the room, its long names are cut sooner. True when
// the width changed
Equipment.narrowList = function()
{
   var lists = Equipment.menu.inventoryLists;
   var layout = lists.itemList.layout;
   var share = Equipment.LIST_WIDTHS[Equipment.number(Equipment.settings.listWidth)];
   var wanted;
   if(!Equipment.listNarrows())
   {
      return false;
   }
   if(Equipment.listFullWidth == undefined)
   {
      Equipment.listFullWidth = layout.entryWidth;
      Equipment.listFullWide = Equipment.rowsEdge().wide;
   }
   wanted = Math.round(Equipment.listFullWidth * (share != undefined ? share : 1));
   if(wanted == layout.entryWidth)
   {
      return false;
   }
   lists.applyDynamicWidth(Equipment.SKYUI_LIST_WIDTH + wanted - layout.entryWidth);
   if(lists._columnSelectDialogX != undefined)
   {
      lists._columnSelectDialogX = Equipment.SKYUI_DIALOG_X + wanted - Equipment.SKYUI_LIST_WIDTH;
   }
   layout.entryWidth = wanted;
   layout.updateLayout();
   lists.categoryList.UpdateList();
   // the subcategories are fitted to the width of the list once, they're fitted to the new one
   Equipment.stripScale = undefined;
   return true;
};

// Everything that stands by the list's edge follows it: the slots and the doll's room, the item window, the drawer,
// the subcategories and the models of the slots. The doll follows the room with the models
Equipment.relayout = function()
{
   Equipment.closePopup();
   Equipment.closeContext();
   Equipment.closePieces();
   Equipment.layout();
   Equipment.placeWindowLeft();
   Equipment.placeHero();
   Equipment.placeCompanions();
   Equipment.placeLookButton();
   Equipment.showChips();
   Equipment.wearing = undefined;
   Equipment.showModels();
   Equipment.browserRoomChanged();
   Equipment.placeLook();
};

// For the test stand: the middle of a part of the sheet as aimedX, aimedY. "button", "reset", "doll", "slot,<name>",
// "less,<key>", "more,<key>"
Equipment.aimLook = function(part)
{
   // Papyrus hands words over in the case the game met them first, "Button" for "button"
   var parts = String(part).split(",");
   var sheet = Equipment.look;
   parts[0] = parts[0].toLowerCase();
   var clip;
   var lines = sheet.lines;
   var i = 0;
   var bounds;
   var share;
   Equipment.aimedX = Equipment.aimedY = -1;
   if(parts[0] == "button")
   {
      clip = Equipment.lookButton;
   }
   else if(parts[0] == "reset")
   {
      clip = sheet.reset;
   }
   while(clip == undefined && lines != undefined && i < lines.length)
   {
      if(parts[0] == "doll" && lines[i].key == "doll" || parts[0] == "slot" && lines[i].key == "slot" + Equipment.slotIndex(Equipment.slotNamed(parts[1])))
      {
         clip = lines[i];
      }
      else if((parts[0] == "less" || parts[0] == "more") && lines[i].key == parts[1])
      {
         clip = lines[i][parts[0]];
      }
      i++;
   }
   if(clip == undefined)
   {
      return undefined;
   }
   bounds = clip.getBounds(_root);
   share = Equipment.screenShare((bounds.xMin + bounds.xMax) / 2, (bounds.yMin + bounds.yMax) / 2);
   Equipment.aimedX = share.x;
   Equipment.aimedY = share.y;
};

// For the test stand: what the sheet set and where the list, the slots and the doll's room stand now
Equipment.lookReport = function(note)
{
   var settings = Equipment.settings;
   var stage = Equipment.stage;
   var off = [];
   var i = 0;
   while(i < Equipment.SLOTS.length)
   {
      if(!Equipment.slotShown(Equipment.SLOTS[i]))
      {
         off.push(Equipment.SLOTS[i].name);
      }
      i++;
   }
   skse.Log("PANELTEST look " + note + ": sheet " + Equipment.look._visible + ", doll " + settings.doll + " " + settings.dollScale + "% right " + settings.dollRight + " up " + settings.dollUp + ", slots off " + (off.length > 0 ? off.join("/") : "-") + ", list width " + settings.listWidth + " entry " + Equipment.menu.inventoryLists.itemList.layout.entryWidth + ", " + Equipment.edgeReport() + ", room " + Math.round(stage._x) + ".." + Math.round(stage._x + stage.touch._width) + ", cells " + Equipment.cells().length);
};
