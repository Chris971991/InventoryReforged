Equipment.NEW_DOT = 3;
Equipment.JUNK_SIGN = 12;
Equipment.JUNK_TEXT = 0x8A8579;
Equipment.TOO_HEAVY = 0xD8664F;
Equipment.HEAVY_STACK = 0xE2B456;
// past this the game's carry limit is a mod's way of saying none
Equipment.NO_CARRY_LIMIT = 100000;
Equipment.ITEM_ICON_COLUMN = 0;

// New things: the plugin tells a row its item came after the inventory was last closed. The dot sits on the corner
// of the row's icon and goes once the pointer passed the row, the plugin forgets them all when the inventory closes.
// Rows are built anew after every move, so what the pointer passed is kept by form for the rest of the menu
Equipment.newSeen = {};

Equipment.isNew = function(entry)
{
   return Equipment.settings.newMarks && entry.equipmentNew == true && Equipment.newSeen["f" + entry.formId] != true;
};

Equipment.showNew = function(row, field, entry, state)
{
   var dot = row.equipmentNew;
   var place;
   if(!Equipment.isNew(entry))
   {
      dot._visible = false;
      return undefined;
   }
   if(dot == undefined)
   {
      dot = row.createEmptyMovieClip("equipmentNew", row.getNextHighestDepth());
   }
   if(dot.color != Equipment.GOLD)
   {
      dot.clear();
      Equipment.drawDot(dot, 0, 0, Equipment.NEW_DOT, Equipment.GOLD);
      dot.color = Equipment.GOLD;
   }
   place = Equipment.columnOfType(state, Equipment.ITEM_ICON_COLUMN);
   dot._x = place != undefined ? place.x + place.width - Equipment.NEW_DOT : field._x - 2 * Equipment.NEW_DOT;
   dot._y = place != undefined ? place.y + Equipment.NEW_DOT : field._y + field._height / 2;
   dot._visible = true;
};

Equipment.drawDot = function(clip, x, y, radius, color)
{
   clip.lineStyle(1, 0, 60, true);
   clip.beginFill(color, 100);
   clip.moveTo(x + radius, y);
   clip.curveTo(x + radius, y + radius, x, y + radius);
   clip.curveTo(x - radius, y + radius, x - radius, y);
   clip.curveTo(x - radius, y - radius, x, y - radius);
   clip.curveTo(x + radius, y - radius, x + radius, y);
   clip.endFill();
};

Equipment.columnOfType = function(state, type)
{
   var columns = state.list.layout.columnLayoutData;
   var i = 0;
   while(i < columns.length)
   {
      if(columns[i].type == type)
      {
         return columns[i];
      }
      i++;
   }
   return undefined;
};

// the pointer passed a new row, its dot and the one of its subcategory go. SkyUI also selects the first row of a
// category by itself, that one wasn't looked at, so only the row under the pointer counts
Equipment.passNew = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   var entry = list.selectedEntry;
   if(entry == undefined || !Equipment.isNew(entry) || Equipment.rowUnderMouse() != entry)
   {
      return undefined;
   }
   Equipment.newSeen["f" + entry.formId] = true;
   list.requestUpdate();
   Equipment.markChips();
};

// A subcategory holding something new gets the same dot on its sign
Equipment.markChips = function()
{
   var lists = Equipment.menu.inventoryLists;
   var entries = lists.itemList.entryList;
   var flag = lists.categoryList.selectedEntry.flag;
   var name;
   var chip;
   var i;
   var shown;
   for(name in Equipment.chips)
   {
      chip = Equipment.chips[name];
      if(!(chip instanceof MovieClip) || chip.key == undefined)
      {
         continue;
      }
      shown = false;
      i = 0;
      while(!shown && i < entries.length)
      {
         shown = Equipment.isNew(entries[i]) && lists._typeFilter.isMatch(entries[i], flag) && Equipment.chipHolds(chip.key, entries[i]);
         i++;
      }
      if(chip.newDot == undefined)
      {
         chip.newDot = chip.createEmptyMovieClip("newDot", 5);
         Equipment.drawDot(chip.newDot, Equipment.CHIP_ICON - 1, (Equipment.STRIP_HIGH - Equipment.CHIP_ICON) / 2 + 1, Equipment.NEW_DOT, Equipment.GOLD);
      }
      chip.newDot._visible = shown;
   }
};

// Junk: the name goes dim and a small sign stands after the game's icons, where the arrow starts otherwise
Equipment.junkShown = function(entry)
{
   return Equipment.settings.junk && entry.equipmentJunk == true;
};

Equipment.showJunk = function(row, field, entry, state)
{
   var sign = row.equipmentJunk;
   if(!Equipment.junkShown(entry))
   {
      sign._visible = false;
      return undefined;
   }
   field.textColor = Equipment.inkOf("junk");
   if(sign == undefined)
   {
      sign = row.createEmptyMovieClip("equipmentJunk", row.getNextHighestDepth());
      Equipment.drawIcon(sign, "junk", 0, -Equipment.JUNK_SIGN / 2, Equipment.JUNK_SIGN, Equipment.inkOf("junk"), 100);
   }
   sign._x = Equipment.questEnd(row, field, entry, state);
   sign._y = field._y + field._height / 2;
   sign._visible = true;
};

// What a quest wants: the quest's arrowhead right after the game's icons, before the junk sign. Gold while a quest
// still needs the item, a dimmer gold when a quest that hasn't begun will, grey once its quest is over. The kinds are
// those of questLine in 38-stats.as
Equipment.QUEST_SIGN = 12;
Equipment.QUEST_LATER = 55;
Equipment.QUEST_DONE = 80;

Equipment.questShown = function(entry)
{
   return Equipment.settings.questMarks && Equipment.number(entry.equipmentQuest) > 0;
};

// the colour and the strength of the sign of a kind
Equipment.questLook = function(kind)
{
   if(kind == 6)
   {
      return {color:Equipment.accentInk(), alpha:Equipment.QUEST_LATER};
   }
   if(Equipment.questNeeds({equipmentQuest:kind}))
   {
      return {color:Equipment.accentInk(), alpha:100};
   }
   return {color:Equipment.inkOf("quiet"), alpha:Equipment.QUEST_DONE};
};

Equipment.showQuest = function(row, field, entry, state)
{
   var sign = row.equipmentQuestSign;
   var look;
   if(!Equipment.questShown(entry))
   {
      sign._visible = false;
      return undefined;
   }
   look = Equipment.questLook(Equipment.number(entry.equipmentQuest));
   if(sign == undefined)
   {
      sign = row.createEmptyMovieClip("equipmentQuestSign", row.getNextHighestDepth());
   }
   // rows are reused as the list scrolls, a sign is drawn again only when its kind of look changes
   if(sign.color != look.color || sign.alpha != look.alpha)
   {
      sign.clear();
      Equipment.drawIcon(sign, "quest", 0, -Equipment.QUEST_SIGN / 2, Equipment.QUEST_SIGN, look.color, look.alpha);
      sign.color = look.color;
      sign.alpha = look.alpha;
   }
   sign._x = Equipment.iconsEnd(row, field, entry, state);
   sign._y = field._y + field._height / 2;
   sign._visible = true;
};

// For the test stand: the kind of sign of a row, "needed", "later", "done", or nothing
Equipment.questKind = function(entry)
{
   var quest = Equipment.number(entry.equipmentQuest);
   if(quest <= 0)
   {
      return "";
   }
   return quest == 6 ? "later" : (Equipment.questNeeds(entry) ? "needed" : "done");
};

// For the test stand: the first row the list shows with this kind of sign, scrolled into sight, written into the log
Equipment.scrollToQuest = function(kind)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var rows = list.listEnumeration;
   var i = 0;
   while(i < rows.size() && Equipment.questKind(rows.at(i)) != String(kind).toLowerCase())
   {
      i++;
   }
   Equipment.questFound = i < rows.size() ? rows.at(i) : undefined;
   skse.Log("PANELTEST quest sign " + kind + ": " + (Equipment.questFound != undefined ? Equipment.questFound.text + " at " + i + " of " + rows.size() + ", kind " + Equipment.questFound.equipmentQuest + ", junk " + Equipment.questFound.equipmentJunk : "no such row"));
   if(Equipment.questFound != undefined)
   {
      list.scrollPosition = Math.max(0, Math.min(i - 3, list._maxScrollPosition));
      list.requestUpdate();
   }
};

// For the test stand: the junk mark of the row scrollToQuest found goes on or off, the way the menu of a row does it
Equipment.junkQuestRow = function()
{
   if(Equipment.questFound != undefined)
   {
      Equipment.toggleJunk(Equipment.questFound);
   }
};

// Weight: in a chest or a shop a row of the other side that would slow the player once taken shows its weight in
// red, one whose whole stack would in amber. The column reads the colour off the row, SkyUI's colorAttribute.
// A carry limit a mod pushed past any sense says nothing
Equipment.WEIGHT_COLOR = "equipmentWeightColor";

Equipment.tintWeights = function(entries, flags)
{
   var carry = Equipment.plugin() != undefined && Equipment.kind != "inventory" ? Equipment.plugin().Carry() : undefined;
   var room = carry != undefined ? carry.most - carry.carried : undefined;
   var useful = Equipment.settings.weightWarnings && room != undefined && carry.most < Equipment.NO_CARRY_LIMIT;
   var entry;
   var one;
   var i = 0;
   while(i < entries.length)
   {
      entry = entries[i];
      i++;
      one = Equipment.number(entry.infoWeight);
      entry[Equipment.WEIGHT_COLOR] = undefined;
      if(!useful || one <= 0 || Equipment.own(entry, flags))
      {
         continue;
      }
      if(one > room)
      {
         entry[Equipment.WEIGHT_COLOR] = Equipment.inkOf("tooHeavy");
      }
      else if(one * Math.max(Equipment.number(entry.count), 1) > room)
      {
         entry[Equipment.WEIGHT_COLOR] = Equipment.inkOf("heavyStack");
      }
   }
};

// every state of a column that shows a weight takes its colour off the row
// Kome's Inventory Tweaks brings a column with no count of states, the loop over them hung the menu (2026-10-23)
Equipment.tintColumn = function(column)
{
   var states = Equipment.count(column.states);
   var i = 1;
   var state;
   var shown;
   while(i <= states)
   {
      state = column["state" + i];
      shown = state.entry != undefined ? String(state.entry.text) : "";
      if(state.colorAttribute == undefined && (shown == "@infoWeight" || shown == "@infoStackWeight" || shown == "@infoTotalWeight"))
      {
         state.colorAttribute = Equipment.WEIGHT_COLOR;
      }
      i++;
   }
};
