Equipment.BETTER = 7195502;
Equipment.WORSE = 14702938;
Equipment.EQUAL = 11579568;
Equipment.ARROW_WIDTH = 9;
Equipment.ARROW_HEIGHT = 7;
Equipment.ICON_GAP = 5;
Equipment.ICON_STEP = 1.25;
Equipment.EQUIP_COLUMN = 1;
Equipment.BADGE_INSET = 3;
Equipment.MARK_ROOM = 48;
Equipment.SHORTEST_NAME = 8;
Equipment.NAME_CUT = 3;

// wraps the name column of list rows so every row can carry an arrow and the badge of its slot
Equipment.decorateRows = function()
{
   var row = _global.InventoryListEntry.prototype;
   if(row == undefined || row.equipmentFormatName != undefined)
   {
      return undefined;
   }
   row.equipmentFormatName = row.formatName;
   row.formatName = function(field, entry, state)
   {
      this.equipmentFormatName(field, entry, state);
      this.equipmentNameField = field;
      Equipment.makeRoom(this, field, entry, state);
      Equipment.showQuest(this, field, entry, state);
      Equipment.showJunk(this, field, entry, state);
      Equipment.showDifference(this, field, entry, state);
      Equipment.showBadge(this, entry, state);
      Equipment.showNew(this, field, entry, state);
   };
};

// a row shows its mark only when the settings allow that kind of mark
Equipment.markShown = function(entry)
{
   if(entry.equipmentDifference == undefined || !Equipment.settings.arrows)
   {
      return false;
   }
   return Math.round(entry.equipmentDifference) != 0 || Equipment.settings.equalSign;
};

// where on a row the column of a field ends, nothing when the list has no such column
Equipment.columnEnd = function(field)
{
   var columns = Equipment.menu.inventoryLists.itemList.layout.columnLayoutData;
   var i = 0;
   while(i < columns.length)
   {
      if(columns[i].stageName == field._name)
      {
         return columns[i].x + columns[i].width;
      }
      i++;
   }
   return undefined;
};

// SkyUI cuts a name by its letters and lets a long one run into the next columns, the arrow after it then lay
// under their text. The name of a row that has an arrow is cut shorter until the arrow ends inside its own column.
// The name itself is cut, SkyUI adds the count of a stack after it, so "(37)" stays on a row of arrows
Equipment.makeRoom = function(row, field, entry, state)
{
   var end = Equipment.columnEnd(field);
   var full = entry.text;
   var keep = full.length;
   var room = Equipment.markShown(entry) ? Equipment.MARK_ROOM : 0;
   if(end == undefined || room == 0 && !Equipment.junkShown(entry) && !Equipment.questShown(entry))
   {
      return undefined;
   }
   while(Equipment.afterIcons(row, field, entry, state) + room > end && keep > Equipment.SHORTEST_NAME)
   {
      keep -= Equipment.NAME_CUT;
      entry.text = full.substr(0, keep) + "...";
      row.equipmentFormatName(field, entry, state);
   }
   entry.text = full;
};

// a row that can be compared with what's worn says how it compares, an equal one with a grey sign
Equipment.showDifference = function(row, field, entry, state)
{
   if(!Equipment.markShown(entry))
   {
      row.equipmentMark._visible = false;
      return undefined;
   }
   var amount = Math.round(entry.equipmentDifference);
   var mark = row.equipmentMark;
   if(mark == undefined)
   {
      mark = Equipment.createMark(row);
   }
   var better = amount > 0;
   var color = amount == 0 ? Equipment.inkOf("equal") : (better ? Equipment.inkOf("better") : Equipment.inkOf("worse"));
   mark.arrow.clear();
   if(amount != 0)
   {
      Equipment.drawArrow(mark.arrow, better, color);
   }
   mark.amount._x = amount == 0 ? 0 : Equipment.ARROW_WIDTH + 3;
   mark.amount.text = amount == 0 ? "=" : (better ? "+" : "") + amount;
   mark.amount.textColor = color;
   mark._x = Equipment.afterIcons(row, field, entry, state);
   mark._y = field._y + field._height / 2;
   mark._visible = true;
};

// the arrow goes after the game's icons, the quest sign and the junk sign
Equipment.afterIcons = function(row, field, entry, state)
{
   return Equipment.questEnd(row, field, entry, state) + (Equipment.junkShown(entry) ? Equipment.JUNK_SIGN + Equipment.ICON_GAP : 0);
};

// Where the name of a row ends with the signs and the arrow after it, in the panel's units. 0 for a row with no clip
// in sight, the menu of a row stands past it
Equipment.nameEnd = function(entry)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var row = entry != undefined && entry.clipIndex != undefined ? list.getClipByIndex(entry.clipIndex) : undefined;
   var field = row != undefined && row.itemIndex == entry.itemIndex ? row.equipmentNameField : undefined;
   var mark;
   var point;
   if(field == undefined)
   {
      return 0;
   }
   mark = row.equipmentMark;
   point = {x:mark != undefined && mark._visible ? mark._x + mark._width : Equipment.afterIcons(row, field, entry, list.listState), y:0};
   row.localToGlobal(point);
   return Equipment.number(Equipment.toPanel(point).x);
};

// the junk sign goes after the game's icons and the quest sign
Equipment.questEnd = function(row, field, entry, state)
{
   return Equipment.iconsEnd(row, field, entry, state) + (Equipment.questShown(entry) ? Equipment.QUEST_SIGN + Equipment.ICON_GAP : 0);
};

// the row puts its own icons after the name one by one
Equipment.iconsEnd = function(row, field, entry, state)
{
   var shown = 0;
   var flags = [entry.bestInClass, entry.favorite, entry.isPoisoned, (entry.isStolen == true || entry.isStealing == true) && state.showStolenIcon == true, entry.isEnchanted, entry.isRead];
   // a row clip of a skin with no best icon has no width to step by, the marks went to x = NaN
   var step = row.bestIcon._width > 0 ? row.bestIcon._width * Equipment.ICON_STEP : field._height;
   var i = 0;
   while(i < flags.length)
   {
      if(flags[i] == true)
      {
         shown++;
      }
      i++;
   }
   return field._x + field._width + Equipment.ICON_GAP + shown * step;
};

Equipment.createMark = function(row)
{
   var mark = row.createEmptyMovieClip("equipmentMark", row.getNextHighestDepth());
   mark.createEmptyMovieClip("arrow", 1);
   mark.createTextField("amount", 2, Equipment.ARROW_WIDTH + 3, -10, 60, 20);
   mark.amount.selectable = false;
   mark.amount.setNewTextFormat(new TextFormat("$EverywhereMediumFont", 13, Equipment.BETTER));
   return mark;
};

Equipment.drawArrow = function(arrow, up, color)
{
   var tip = up ? -Equipment.ARROW_HEIGHT / 2 : Equipment.ARROW_HEIGHT / 2;
   arrow.clear();
   arrow.beginFill(color, 100);
   arrow.moveTo(0, -tip);
   arrow.lineTo(Equipment.ARROW_WIDTH / 2, tip);
   arrow.lineTo(Equipment.ARROW_WIDTH, -tip);
   arrow.lineTo(0, -tip);
   arrow.endFill();
};

// A row of something wearable that isn't worn shows the slot it would go to
// The badge sits where a worn row has its mark, in the first column of the list
Equipment.showBadge = function(row, entry, state)
{
   var slot = entry.equipState > 0 || !Equipment.settings.badges ? undefined : Equipment.slotOf(entry);
   var place = Equipment.equipColumn(state);
   var badge = row.equipmentBadge;
   if(slot == undefined || place == undefined)
   {
      badge._visible = false;
      return undefined;
   }
   if(badge == undefined)
   {
      badge = row.createEmptyMovieClip("equipmentBadge", row.getNextHighestDepth());
   }
   var size = Math.min(place.width, place.height);
   badge._x = place.x;
   badge._y = place.y;
   badge._visible = true;
   if(badge.slot == slot && badge.size == size)
   {
      return undefined;
   }
   badge.slot = slot;
   badge.size = size;
   badge.clear();
   badge.lineStyle(1, Equipment.inkOf("edge"), 30, true);
   badge.beginFill(Equipment.paper ? Equipment.WHITE : 0, Equipment.paper ? 18 : 30);
   Equipment.roundBox(badge, 0, 0, size, size, 3);
   badge.endFill();
   Equipment.drawIcon(badge, slot.icon, Equipment.BADGE_INSET, Equipment.BADGE_INSET, size - 2 * Equipment.BADGE_INSET, Equipment.paper ? Equipment.inkOf("chip") : Equipment.PARCHMENT, 75);
};

Equipment.equipColumn = function(state)
{
   var columns = state.list.layout.columnLayoutData;
   var i = 0;
   while(i < columns.length)
   {
      if(columns[i].type == Equipment.EQUIP_COLUMN)
      {
         return columns[i];
      }
      i++;
   }
   return undefined;
};

// For the test stand: the first rows of the list as it stands now, with what the comparison and the sorting made
// of each. The lines go to the log of the script extender
Equipment.rowsReport = function(count)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var rows = list.listEnumeration;
   var entry;
   var i = 0;
   skse.Log("EquipmentPanel rows: " + rows.size() + " rows, column " + list.layout.activeColumnIndex + " state " + list.layout.activeColumnState);
   while(i < rows.size() && i < count)
   {
      entry = rows.at(i);
      skse.Log("EquipmentPanel row " + i + ": " + entry.text + " x" + entry.count + ", type " + entry.formType + " sub " + entry.subType + " (" + entry.subTypeDisplay + ") class " + entry.weightClass + ", icon " + entry.iconLabel + ", slots " + entry.partMask + ", worn " + entry.equipState + ", armor " + entry.infoArmor + ", damage " + entry.infoDamage + ", difference " + entry.equipmentDifference + ", weight " + entry.infoWeight + ", stack " + entry.infoStackWeight + ", added " + entry.equipmentAdded + ", new " + Equipment.isNew(entry) + ", junk " + entry.equipmentJunk + ", quest " + entry.equipmentQuest + ", stolen " + entry.equipmentStolen + " (" + entry.isStolen + "), carried " + entry.equipmentCarried + ", from " + entry.equipmentOrigin + ", weight colour " + entry.equipmentWeightColor);
      i++;
   }
   Equipment.rowsReported = rows.size();
};

// For the test stand: how long the list lays its rows out the way SkyUI does, with the panel's marks of rows and with
// all the panel's work. A list that scrolls smoothly lays them out every 16 ms while it moves. Three rounds in turn,
// the shortest of each counts, a stall of the game in one round would read as the panel's cost
Equipment.listUpdateReport = function(times)
{
   var plugin = Equipment.plugin();
   var list = Equipment.menu.inventoryLists.itemList;
   var row = _global.InventoryListEntry.prototype;
   var ours = row.formatName;
   var count = Number(times);
   var least = [];
   var told = [];
   var took;
   var round = 0;
   var way;
   if(plugin == undefined || plugin.Clock == undefined || list.equipmentUpdateList == undefined)
   {
      return undefined;
   }
   while(round < 3)
   {
      way = 0;
      while(way < 3)
      {
         row.formatName = way == 0 ? row.equipmentFormatName : ours;
         took = Equipment.timeLayouts(plugin, list, count, way == 2);
         least[way] = least[way] == undefined ? took : Math.min(least[way], took);
         told.push(Math.round(took));
         way++;
      }
      round++;
   }
   row.formatName = ours;
   skse.Log("PANELTEST list laid out " + count + " times with " + list._listIndex + " rows: as SkyUI " + Math.round(least[0]) + " ms, with the marks of rows " + Math.round(least[1]) + " ms, with all the panel " + Math.round(least[2]) + " ms (rounds " + told.join("/") + ")");
};

Equipment.timeLayouts = function(plugin, list, count, all)
{
   var start = plugin.Clock();
   var i = 0;
   while(i < count)
   {
      if(all)
      {
         list.UpdateList();
      }
      else
      {
         list.equipmentUpdateList();
      }
      i++;
   }
   return plugin.Clock() - start;
};
