Equipment.TIP_PAD = 6;
Equipment.TIP_TEXT = 13;

// A right click on a subcategory in a chest takes all of it, or stores all of it, or stores only what the chest
// holds already. The rows go one by one through the queue. Worn things, favorites and what a quest wants stay with
// the player, a chest gives everything
Equipment.chipAt = function(x, y)
{
   var name;
   var chip;
   var box;
   for(name in Equipment.chips)
   {
      chip = Equipment.chips[name];
      if(chip instanceof MovieClip && chip.wide != undefined && chip._visible)
      {
         box = chip.getBounds(_root);
         if(x >= box.xMin && x <= box.xMax && y >= box.yMin && y <= box.yMax)
         {
            return chip;
         }
      }
   }
   return undefined;
};

// the rows of a subcategory on one side, in the category in view
Equipment.chipRows = function(chip, own)
{
   var lists = Equipment.menu.inventoryLists;
   var entries = lists.itemList.entryList;
   var flag = lists.categoryList.selectedEntry.flag;
   var flags = Equipment.ownFlags();
   var rows = [];
   var i = 0;
   while(i < entries.length)
   {
      if(lists._typeFilter.isMatch(entries[i], flag) && Equipment.chipHolds(chip.key, entries[i]) && Equipment.own(entries[i], flags) == own && !(own && Equipment.locked(entries[i])))
      {
         rows.push(entries[i]);
      }
      i++;
   }
   return rows;
};

// how many things the rows hold, a stack counts all of its pieces
Equipment.pieces = function(rows)
{
   var count = 0;
   var i = 0;
   while(i < rows.length)
   {
      count += Math.max(Equipment.number(rows[i].count), 1);
      i++;
   }
   return count;
};

Equipment.rowsWeight = function(rows)
{
   var weight = 0;
   var i = 0;
   while(i < rows.length)
   {
      weight += Equipment.number(rows[i].infoWeight) * Math.max(Equipment.number(rows[i].count), 1);
      i++;
   }
   return weight;
};

// what the player could still carry, nothing when the game or a mod set no real limit
Equipment.carryRoom = function()
{
   var carry = Equipment.plugin() != undefined ? Equipment.plugin().Carry() : undefined;
   if(carry == undefined || carry.most >= Equipment.NO_CARRY_LIMIT)
   {
      return undefined;
   }
   return carry.most - carry.carried;
};

Equipment.openChipMenu = function(chip)
{
   var menu = Equipment.menu;
   var taking;
   var rows;
   var held;
   var lines = [];
   if(Equipment.kind != "container" || chip == undefined || chip.key == undefined || !Equipment.listsAnswer() || Equipment.queue != undefined)
   {
      return false;
   }
   taking = menu.isViewingContainer();
   rows = Equipment.chipRows(chip, !taking);
   if(rows.length == 0)
   {
      return false;
   }
   Equipment.hideChipWeight();
   if(taking)
   {
      lines.push({key:"chipAll", words:Equipment.takeAllWords(chip.label, rows), icon:"transfer", run:function()
      {
         Equipment.moveChip(rows);
      }});
   }
   else
   {
      lines.push({key:"chipAll", words:menu.bNPCMode ? Equipment.say("Give all #1 (#2)", chip.label, Equipment.pieces(rows)) : Equipment.say("Store all #1 (#2)", chip.label, Equipment.pieces(rows)), icon:"transfer", run:function()
      {
         Equipment.moveChip(rows);
      }});
      held = Equipment.heldThere(rows);
      if(!menu.bNPCMode && held.length > 0)
      {
         lines.push({key:"chipHeld", words:Equipment.say("Store what this chest holds (#)", Equipment.pieces(held)), icon:"transfer", run:function()
         {
            Equipment.moveChip(held);
         }});
      }
   }
   Equipment.showContext(lines, undefined, undefined);
   return true;
};

Equipment.takeAllWords = function(label, rows)
{
   var words = Equipment.say("Take all #1 (#2)", label, Equipment.pieces(rows));
   var weight = Equipment.rowsWeight(rows);
   var room = Equipment.carryRoom();
   if(!Equipment.settings.weightWarnings || weight <= 0)
   {
      return words;
   }
   words += ", " + Equipment.say("weight #", Equipment.weightText(weight));
   if(room != undefined && weight > room)
   {
      words += ", " + Equipment.say("# over your limit", Equipment.weightText(weight - Math.max(room, 0)));
   }
   return words;
};

// the player's rows of things the chest has some of already
Equipment.heldThere = function(rows)
{
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var flags = Equipment.ownFlags();
   var there = {};
   var held = [];
   var i = 0;
   while(i < entries.length)
   {
      if(!Equipment.own(entries[i], flags))
      {
         there["f" + entries[i].formId] = true;
      }
      i++;
   }
   i = 0;
   while(i < rows.length)
   {
      if(there["f" + rows[i].formId] == true)
      {
         held.push(rows[i]);
      }
      i++;
   }
   return held;
};

Equipment.moveChip = function(rows)
{
   Equipment.startQueue(rows, function(moved, kept, stopped)
   {
      skse.Log("EquipmentPanel subcategory moved " + moved + ", kept " + kept + (stopped ? ", stopped at a question" : ""));
   });
};

// The pointer on a subcategory shows what all of it weighs on the side in view, red in a chest or a shop when it's
// more than the player can still carry
Equipment.showChipWeight = function(chip)
{
   var lists = Equipment.menu.inventoryLists;
   var tip = Equipment.chipTip;
   var own = Equipment.kind == "inventory" || !(Equipment.kind == "container" ? Equipment.menu.isViewingContainer() : Equipment.menu.isViewingVendorItems());
   var rows;
   var weight;
   var room;
   var point;
   if(!Equipment.settings.weightWarnings || chip.key == undefined)
   {
      return undefined;
   }
   rows = Equipment.chipRows(chip, own);
   weight = Equipment.rowsWeight(rows);
   if(weight <= 0)
   {
      return undefined;
   }
   room = own ? undefined : Equipment.carryRoom();
   if(tip == undefined)
   {
      tip = Equipment.chipTip = lists.panelContainer.createEmptyMovieClip("equipmentChipTip", lists.panelContainer.getNextHighestDepth());
      tip.createEmptyMovieClip("sign", 2);
   }
   tip.clear();
   tip.sign.clear();
   if(tip.words != undefined)
   {
      tip.words.removeTextField();
   }
   Equipment.writeLine(tip, "words", 1, Equipment.weightText(weight), Equipment.TIP_TEXT, room != undefined && weight > room ? Equipment.TOO_HEAVY : Equipment.WHITE);
   Equipment.drawIcon(tip.sign, "weight", Equipment.TIP_PAD, Equipment.TIP_PAD, Equipment.TIP_TEXT, Equipment.HERO_DIM, 100);
   tip.words._x = Equipment.TIP_PAD + Equipment.TIP_TEXT + 4;
   tip.words._y = (Equipment.TIP_TEXT + 2 * Equipment.TIP_PAD - tip.words._height) / 2;
   tip.wide = tip.words._x + tip.words._width + Equipment.TIP_PAD;
   Equipment.panelBox(tip, tip.wide, Equipment.TIP_TEXT + 2 * Equipment.TIP_PAD, 90, 45);
   point = {x:chip.wide / 2, y:Equipment.STRIP_HIGH};
   chip.localToGlobal(point);
   lists.panelContainer.globalToLocal(point);
   tip._x = Math.round(point.x - tip.wide / 2);
   tip._y = Math.round(point.y + 2);
   tip._visible = true;
};

Equipment.hideChipWeight = function()
{
   Equipment.chipTip._visible = false;
};
