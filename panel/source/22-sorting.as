Equipment.SORT_DOWN = 18;
Equipment.SORT_UP = 16;
Equipment.SORT_BY_NAME = 1;
Equipment.ADDED_LABEL = Equipment.say("LAST ADDED");
Equipment.STACK_LABEL = Equipment.say("SUMW");

// A press on the header of a column moves it to its next state
// A new state is put in at a place of the turn, the states after it move one on
// A column of a UI mod may come with no count of states. In ActionScript 2 "i >= at" reads as "not i < at", so
// with no number it stays true forever and the loop never ends, the bound is made a number first
Equipment.insertState = function(column, at, state)
{
   var states = Equipment.count(column.states);
   var i = states;
   if(states < at - 1)
   {
      return undefined;
   }
   while(i >= at)
   {
      column["state" + (i + 1)] = column["state" + i];
      i--;
   }
   column["state" + at] = state;
   column.states = states + 1;
};

Equipment.weightState = function(label, shown, order)
{
   return {label:{text:label, arrowDown:order == Equipment.SORT_DOWN}, entry:{text:"@" + shown}, colorAttribute:Equipment.WEIGHT_COLOR, sortAttributes:[shown, "text"], sortOptions:[order, Equipment.SORT_BY_NAME]};
};

// what SkyUI's settings call a NAME column (skyui.components.list.ListLayout.COL_TYPE_NAME)
Equipment.NAME_COLUMN = 3;

// Two more ways to sort a list. The layout keeps the column objects of the settings and reads their states
// on every press, so the states can go in before or after the layout is built
// A name column gets a state that lists first what the player got last, the plugin numbers the items for it
// A weight column takes turns between the weight of one item and the weight of its whole stack
// UI mods bring sets of columns of their own (Aura's Inventory Tweaks has gearNameColumn, bookNameColumn,
// itemWtColumn), so the states go into every column that sorts by name or by the weight of one item
Equipment.addSorting = function(config)
{
   var columns = config.ListLayout.columns;
   var key;
   var column;
   if(columns == undefined || Equipment.sortingAdded)
   {
      return undefined;
   }
   Equipment.sortingAdded = true;
   for(key in columns)
   {
      column = columns[key];
      Equipment.tintColumn(column);
      if(column.type == Equipment.NAME_COLUMN && Equipment.plugin() != undefined && Equipment.settings.lastAdded)
      {
         Equipment.insertState(column, 2, {label:{text:Equipment.ADDED_LABEL}, entry:{text:"@text"}, sortAttributes:["equipmentAdded", "text"], sortOptions:[Equipment.SORT_DOWN, Equipment.SORT_BY_NAME]});
      }
      else if(column.states == 2 && column.state1.sortAttributes[0] == "infoWeight" && Equipment.settings.stackWeight)
      {
         Equipment.insertState(column, 2, Equipment.weightState(Equipment.STACK_LABEL, "infoStackWeight", Equipment.SORT_DOWN));
         Equipment.insertState(column, 4, Equipment.weightState(Equipment.STACK_LABEL, "infoStackWeight", Equipment.SORT_UP));
      }
   }
};

// what a whole stack of an item weighs, nothing for a thing without weight
Equipment.stackWeight = function(entry)
{
   if(entry.infoWeight == undefined || entry.infoWeight == null)
   {
      return null;
   }
   return Math.round(entry.infoWeight * (entry.count > 0 ? entry.count : 1) * 100) / 100;
};

// for the test stand: the header of the column whose label holds this word is pressed as a click would, as many
// times as told, "name,1"
Equipment.pressColumn = function(told)
{
   var parts = String(told).split(",");
   var header = Equipment.menu.inventoryLists.itemList.header;
   var times = Number(parts[1]);
   var column;
   var i = 0;
   while(header["Column" + i] != undefined)
   {
      column = header["Column" + i];
      if(column._visible && column.label.text.toUpperCase().indexOf(parts[0].toUpperCase()) >= 0)
      {
         while(times > 0)
         {
            header.columnPress(i);
            times--;
         }
         skse.Log("EquipmentPanel column pressed: " + header["Column" + i].label.text);
         return undefined;
      }
      i++;
   }
   skse.Log("EquipmentPanel no column holds " + parts[0]);
};
