Equipment.STRIP_HIGH = 30;
Equipment.CHIP_TEXT = 15;
Equipment.CHIP_ICON = 19;
Equipment.ICON_TO_WORD = 5;
Equipment.CHIP_GAP = 18;
Equipment.CHIP_DIM = 45;
Equipment.CHIP_LIT = 80;
Equipment.UNDERLINE_BELOW = 3;
Equipment.SEPARATOR = 0.36;
Equipment.GROUP_SEPARATOR = 0.6;

// Subcategories sit in a strip of their own under the category icons, written like the column names of
// SkyUI. A group of them is one choice, groups add up: Heavy and Head together leave heavy helmets.
// One pressed again lets its group go. A chip that's alone is a kind the other groups don't split, a shield has no
// head or feet, so it lets the other groups go and a press in them lets it go
Equipment.CHIP_ROWS = {
   weapons:[
      [
         {key:"oneHanded", words:"$One-Handed", fallback:"One-Handed", icon:"sword"},
         {key:"twoHanded", words:"$Two-Handed", fallback:"Two-Handed", icon:"battleaxe"},
         {key:"ranged", words:"$Bows", fallback:"Bows", icon:"bow"},
         {key:"staff", words:"$Staves", fallback:"Staves", icon:"staff"},
         {key:"ammo", words:"$Ammo", fallback:"Ammo", icon:"arrows"}
      ]
   ],
   armor:[
      [
         {key:"light", words:"$Light", fallback:"Light", icon:"jerkin"},
         {key:"heavy", words:"$Heavy", fallback:"Heavy", icon:"plate"},
         {key:"clothing", words:"$Clothing", fallback:"Clothing", icon:"robe"},
         {key:"jewelry", words:"$Jewelry", fallback:"Jewelry", icon:"ring"},
         {key:"shield", words:"$Shield", fallback:"Shields", icon:"shield", alone:true}
      ],
      [
         {key:"head", words:"$Head", fallback:"Head", icon:"helmet"},
         {key:"body", words:"$Body", fallback:"Body", icon:"cuirass"},
         {key:"hands", words:"$Hands", fallback:"Hands", icon:"gauntlets"},
         {key:"feet", words:"$Feet", fallback:"Feet", icon:"boots"},
         {key:"other", words:"$Other", fallback:"Other", icon:"cloak"}
      ]
   ],
   // potions get the colours of the bars they fill
   potions:[
      [
         {key:"health", words:"$Health", fallback:"Health", icon:"potion", tint:0xD4483C},
         {key:"magicka", words:"$Magicka", fallback:"Magicka", icon:"potion", tint:0x4F8BE6},
         {key:"stamina", words:"$Stamina", fallback:"Stamina", icon:"potion", tint:0x62BE52},
         {key:"poison", words:"$Poisons", fallback:"Poisons", icon:"poison", tint:0xA864D2},
         {key:"otherPotion", words:"$Other", fallback:"Other", icon:"potion", tint:0xE2B456}
      ]
   ],
   food:[
      [
         {key:"meal", words:"$Food", fallback:"Food", icon:"food"},
         {key:"drink", words:"$Drinks", fallback:"Drinks", icon:"mug"}
      ]
   ],
   books:[
      [
         {key:"book", words:"$Books", fallback:"Books", icon:"book"},
         {key:"note", words:"$Notes", fallback:"Notes", icon:"note"},
         {key:"map", words:"$Maps", fallback:"Maps", icon:"map"},
         {key:"spellTome", words:"$Spell Tomes", fallback:"Spell Tomes", icon:"tome"}
      ]
   ],
   misc:[
      [
         {key:"ore", words:"$Ore", fallback:"Ore", icon:"ore"},
         {key:"ingot", words:"$Ingots", fallback:"Ingots", icon:"ingot"},
         {key:"gem", words:"$Gems", fallback:"Gems", icon:"gem"},
         {key:"soulGem", words:"$Soul Gems", fallback:"Soul Gems", icon:"soulgem"},
         {key:"pelt", words:"$Pelts", fallback:"Pelts", icon:"pelt"},
         {key:"otherMisc", words:"$Other", fallback:"Other", icon:"sack"}
      ]
   ]
};

// what SkyUI calls the kinds of potions, food, books and things it knows (skyui.defines.Item)
Equipment.HEALTH_POTIONS = [0, 1, 2];
Equipment.MAGICKA_POTIONS = [3, 4, 5];
Equipment.STAMINA_POTIONS = [6, 7, 8];
Equipment.DRINK = 13;
Equipment.MEAL = 14;
Equipment.POISON_POTION = 15;
Equipment.SPELL_TOME = 0;
Equipment.NOTES = [1, 2];
Equipment.MAP_BOOK = 3;
Equipment.MISC = 32;
Equipment.SOUL_GEM = 52;
Equipment.MISC_GEM = 0;
Equipment.MISC_INGOT = 7;
Equipment.MISC_ORE = 31;
Equipment.MISC_PELTS = [3, 4, 5, 23];

// SkyUI 5.2 knows ore and treasure maps by these forms of the game. Menu files built on an older SkyUI
// (Nordic UI, Edge UI) call ore an ingot and a map a note, so the chips look at the forms themselves
Equipment.ORE_FORMS = [371931, 371932, 371933, 371934, 371935, 371936, 371937, 371938, 373471, 466163, 67285099, 67204937, 67212713];
Equipment.MAP_FORMS = [909051, 979066, 996301, 996302, 996303, 996304, 996305, 996306, 996307, 996308, 996309, 996320];

// the kinds of potions, books and things that have a word of their own, so the rest is Other
Equipment.KIND_KEYS = {
   potions:["health", "magicka", "stamina", "poison"],
   misc:["ore", "ingot", "gem", "soulGem", "pelt"],
   books:["note", "map", "spellTome"]
};

// The mod's own file first when it has the words in another language, someone translated the mod. SkyUI's words
// after it, so the chips speak the language of the game with no translation of the mod. Any mod can put "$Bows" in
// English into the game's words, a player's own translation stayed English under it
Equipment.translated = function(chip)
{
   var own = Equipment.say(chip.fallback);
   if(own != chip.fallback)
   {
      return own;
   }
   var words = skyui.util.Translator.translate(chip.words);
   return words == undefined || words == "" || words.charAt(0) == "$" ? own : words;
};

// whether an item is in the part of a category a chip names
Equipment.chipHolds = function(key, entry)
{
   var weapon = entry.formType == Equipment.WEAPON;
   var armor = entry.formType == Equipment.ARMOR;
   var slot;
   switch(key)
   {
      case "oneHanded":
         return weapon && Equipment.oneHanded(entry) && entry.subType != Equipment.STAFF;
      case "twoHanded":
         return Equipment.twoHanded(entry);
      case "ranged":
         return Equipment.ranged(entry);
      case "staff":
         return weapon && entry.subType == Equipment.STAFF;
      case "ammo":
         return entry.formType == Equipment.AMMO;
      case "light":
         return armor && entry.weightClass == 0 && !Equipment.isShield(entry);
      case "heavy":
         return armor && entry.weightClass == 1 && !Equipment.isShield(entry);
      case "clothing":
         return armor && (entry.weightClass == 3 || Equipment.unclassed(entry) && !Equipment.jewelryParts(entry));
      case "jewelry":
         return armor && (entry.weightClass == 4 || Equipment.unclassed(entry) && Equipment.jewelryParts(entry));
      case "shield":
         return Equipment.isShield(entry);
   }
   // Other of Misc takes whatever lands in Misc with no word of its own, torches are lights there
   if(entry.formType == Equipment.POTION || entry.formType == Equipment.BOOK || entry.formType == Equipment.MISC || entry.formType == Equipment.SOUL_GEM || key == "otherMisc")
   {
      return Equipment.kindHolds(key, entry);
   }
   if(!armor || Equipment.isShield(entry))
   {
      return false;
   }
   slot = Equipment.armorSlot(entry).name;
   if(key == "other")
   {
      return slot == "other" || slot == "amulet" || slot == "ring";
   }
   return slot == key;
};

Equipment.kindHolds = function(key, entry)
{
   var potion = entry.formType == Equipment.POTION;
   var book = entry.formType == Equipment.BOOK;
   var misc = entry.formType == Equipment.MISC;
   var i = 0;
   switch(key)
   {
      case "health":
         return potion && Equipment.has(Equipment.HEALTH_POTIONS, entry.subType);
      case "magicka":
         return potion && Equipment.has(Equipment.MAGICKA_POTIONS, entry.subType);
      case "stamina":
         return potion && Equipment.has(Equipment.STAMINA_POTIONS, entry.subType);
      case "poison":
         return potion && entry.subType == Equipment.POISON_POTION;
      case "meal":
         return potion && entry.subType == Equipment.MEAL;
      case "drink":
         return potion && entry.subType == Equipment.DRINK;
      case "note":
         return book && Equipment.has(Equipment.NOTES, entry.subType) && !Equipment.has(Equipment.MAP_FORMS, entry.formId);
      case "map":
         return book && (entry.subType == Equipment.MAP_BOOK || Equipment.has(Equipment.MAP_FORMS, entry.formId));
      case "spellTome":
         return book && entry.subType == Equipment.SPELL_TOME;
      case "ore":
         return misc && (entry.subType == Equipment.MISC_ORE || Equipment.has(Equipment.ORE_FORMS, entry.formId));
      case "ingot":
         return misc && entry.subType == Equipment.MISC_INGOT && !Equipment.has(Equipment.ORE_FORMS, entry.formId);
      case "gem":
         return misc && entry.subType == Equipment.MISC_GEM;
      case "soulGem":
         return entry.formType == Equipment.SOUL_GEM;
      case "pelt":
         return misc && Equipment.has(Equipment.MISC_PELTS, entry.subType);
   }
   // the Other of a family is what none of its named kinds holds
   var named = key == "otherPotion" ? Equipment.KIND_KEYS.potions : (key == "otherMisc" ? Equipment.KIND_KEYS.misc : (key == "book" ? Equipment.KIND_KEYS.books : undefined));
   if(named == undefined || key == "otherPotion" && !potion || key == "book" && !book)
   {
      return false;
   }
   while(i < named.length)
   {
      if(Equipment.kindHolds(named[i], entry))
      {
         return false;
      }
      i++;
   }
   return true;
};

// SkyUI names clothing and jewelry by keywords of the game, a mod's piece without them has no class at all
Equipment.unclassed = function(entry)
{
   return entry.weightClass != 0 && entry.weightClass != 1 && entry.weightClass != 3 && entry.weightClass != 4 && !Equipment.isShield(entry);
};

// a piece that sits only where jewelry sits: neck, finger, circlet
Equipment.jewelryParts = function(entry)
{
   return entry.partMask != undefined && entry.partMask != 0 && (entry.partMask & ~Equipment.JEWELRY_PARTS) == 0;
};
Equipment.JEWELRY_PARTS = 0x20 | 0x40 | 0x1000;

// which set of chips a category flag has, nothing for the categories without them. The flags of the other
// side of a container or a shop are the inventory ones moved up by ten bits
Equipment.FAMILIES = {f2:"weapons", f4:"armor", f8:"potions", f32:"food", f128:"books", f512:"misc"};
Equipment.chipFamily = function(flag)
{
   var own = flag >= 1024 ? flag >> 10 : flag;
   return Equipment.FAMILIES["f" + own];
};

// Under the filter of a slot the subcategories chosen there are its own. The ones chosen before wait
// aside and come back when the filter goes
Equipment.chosenNow = function()
{
   return Equipment.activeSlot != undefined ? Equipment.slotChips : Equipment.chosenChips;
};

Equipment.chipFilter = {};
Equipment.chipFilter.applyFilter = function(entries)
{
   var chosen = Equipment.chosenNow();
   var i = 0;
   var keep;
   var r;
   if(chosen == undefined || chosen.length == 0)
   {
      return undefined;
   }
   while(i < entries.length)
   {
      keep = true;
      r = 0;
      while(r < chosen.length && keep)
      {
         keep = chosen[r] == undefined || Equipment.chipHolds(chosen[r], entries[i]);
         r++;
      }
      if(keep)
      {
         i++;
      }
      else
      {
         entries.splice(i, 1);
      }
   }
};

// The subcategories shown when a menu closes come back with their category the next time a menu opens on it, the way
// the game keeps the category. The script is loaded anew with every menu, the plugin keeps the family and its chips.
// A category picked inside a menu starts clean: chips that came back on every switch looked stuck
Equipment.keepChips = function()
{
   var plugin = Equipment.plugin();
   var keys = [Equipment.chipsFamily];
   var r = 0;
   if(plugin == undefined || plugin.Remember == undefined || Equipment.chipsFamily == undefined || Equipment.activeSlot != undefined)
   {
      return undefined;
   }
   while(r < Equipment.chosenChips.length)
   {
      keys.push(Equipment.chosenChips[r] != undefined ? String(Equipment.chosenChips[r]) : "");
      r++;
   }
   plugin.Remember("chips", keys.join("|"));
};

// the chips kept for a family when the last menu left it, those its rows still have
Equipment.keptChips = function(family)
{
   var plugin = Equipment.plugin();
   var rows = family != undefined ? Equipment.CHIP_ROWS[family] : undefined;
   var keys = rows != undefined && plugin != undefined && plugin.Recall != undefined ? String(plugin.Recall("chips")).split("|") : [];
   var chosen = [];
   var r = 0;
   var c;
   if(keys[0] != family)
   {
      return chosen;
   }
   keys.shift();
   while(r < keys.length && r < rows.length)
   {
      c = 0;
      while(keys[r] != "" && c < rows[r].length)
      {
         if(rows[r][c].key == keys[r])
         {
            chosen[r] = keys[r];
         }
         c++;
      }
      r++;
   }
   return chosen;
};

// whether any row of the category passes the chips chosen now
Equipment.chipsLeaveRows = function()
{
   var lists = Equipment.menu.inventoryLists;
   var flag = lists.categoryList.selectedEntry.flag;
   var entries = lists.itemList.entryList;
   var rows = [];
   var i = 0;
   while(i < entries.length)
   {
      if(lists._typeFilter.isMatch(entries[i], flag))
      {
         rows.push(entries[i]);
      }
      i++;
   }
   Equipment.chipFilter.applyFilter(rows);
   return rows.length > 0;
};

Equipment.startRows = function(config)
{
   var lists = Equipment.menu.inventoryLists;
   Equipment.chosenChips = lists.categoryList.selectedEntry != undefined ? Equipment.keptChips(Equipment.chipFamily(lists.categoryList.selectedEntry.flag)) : [];
   Equipment.slotChips = [];
   // a new category gets its list in showItemsList, its subcategories follow it
   lists.equipmentShowItemsList = lists.showItemsList;
   lists.showItemsList = function()
   {
      Equipment.categoryShown();
      this.equipmentShowItemsList();
      Equipment.onCategoryChange();
   };
   // with subcategories off the place still holds the chip of a slot filter
   Equipment.chips = lists.panelContainer.createEmptyMovieClip("equipmentChips", lists.panelContainer.getNextHighestDepth());
   lists.addEventListener("categoryChange", Equipment, "onCategoryChange");
   lists.addEventListener("itemHighlightChange", Equipment, "passNew");
   if(Equipment.settings.subcategories)
   {
      lists.itemList.listEnumeration.addFilter(Equipment.chipFilter);
   }
   Equipment.showChips();
};

// a slot filter of the panel gives way to the category a player picks, the rows of chips start empty
Equipment.categoryShown = function()
{
};

Equipment.onCategoryChange = function(event)
{
   var family = Equipment.chipFamily(Equipment.menu.inventoryLists.categoryList.selectedEntry.flag);
   var changed = family != Equipment.chipsFamily;
   if(changed)
   {
      Equipment.chosenChips = [];
   }
   Equipment.dropKeyChip();
   Equipment.showChips();
   if(changed)
   {
      Equipment.keepChips();
   }
};

// The strip pushes the list down by its height and SkyUI fits the list between it and the bottom bar again.
// It opens for a category that has subcategories and for the filter of a slot, and is gone otherwise. It's two lines
// high when the junk button has a line of its own under the chips
Equipment.openStrip = function(open, lines)
{
   var list = Equipment.menu.inventoryLists.itemList;
   // the list opens with no room for the strip. Taken for a change, the first closing gave the list 30 px it never
   // lost, and a skin with no call that fits the list (Dragonborn UI, Vel'dun UI) drew a row over its bottom bar
   var had = Equipment.number(Equipment.roomHigh);
   var high = open ? Equipment.STRIP_HIGH * (lines > 1 ? lines : 1) : 0;
   var top = list._y - had;
   if(high == had)
   {
      return top;
   }
   Equipment.roomHigh = high;
   list._y = top + high;
   // menus made on SkyUI 5 like Vel'dun UI have no call that fits the list again, the list is cut by hand there
   if(Equipment.menu.updateDynamicListHeight != undefined)
   {
      Equipment.menu.updateDynamicListHeight();
   }
   else
   {
      list.listHeight += had - high;
   }
   Equipment.fitRows(list);
   return top;
};

// SkyUI counts the rows that fit only when the columns are laid out, a new height alone keeps the old count. Once the
// columns were laid out before the strip opened, the last row hung under the bottom bar
Equipment.fitRows = function(list)
{
   if(list.entryHeight > 0)
   {
      list._maxListIndex = Math.floor(list.listHeight / list.entryHeight + 0.05);
   }
   list.requestUpdate();
};

// a slot filter shows in the strip as a word of its own, a press on it lets the filter go
Equipment.showChips = function()
{
   var lists = Equipment.menu.inventoryLists;
   var chips = Equipment.chips;
   var family = Equipment.chipFamily(lists.categoryList.selectedEntry.flag);
   var rows = family == undefined || !Equipment.settings.subcategories ? [] : Equipment.CHIP_ROWS[family];
   var label = lists.categoryLabel;
   var left = label._x + label.textField._x;
   var right = lists.categoryList._x + lists.categoryList._width;
   var junk = Equipment.junkButtonWanted();
   var chipped = rows.length > 0 || Equipment.activeSlot != undefined;
   var top;
   var made;
   if(chips == undefined)
   {
      return undefined;
   }
   Equipment.clearChips();
   Equipment.chipsFamily = family;
   top = Equipment.openStrip(chipped || junk, chipped && junk ? 2 : 1);
   chips._xscale = chips._yscale = 100;
   chips._x = left;
   // the column names stand above the top of the list, the strip takes the place they had before
   top += lists.itemList.header.getBounds(lists.itemList).yMin;
   // The junk button has a line of its own under the chips, beside them it took their room and made them small. In a
   // category with no chips it's alone on the line
   Equipment.placeJunkButton(junk, chipped ? top + Equipment.STRIP_HIGH : top, right);
   // the widest strip is fitted once to the width of the list and every category uses its size,
   // so the words keep one size from tab to tab
   if(Equipment.stripScale == undefined)
   {
      Equipment.layChips(Equipment.CHIP_ROWS.armor, 0, false);
      Equipment.stripScale = chips._width > right - left ? (right - left) / chips._width * 100 : 100;
      Equipment.clearChips();
   }
   if(Equipment.activeSlot != undefined)
   {
      // the filter of the slot leads, the subcategories that split what fits it follow
      made = Equipment.makeChip("slot", Equipment.activeSlot.title + "   ×", Equipment.activeSlot.icon, 0, true, -1, undefined);
      Equipment.layChips(rows, made.wide + Equipment.CHIP_GAP, true);
   }
   else
   {
      Equipment.layChips(rows, 0, false);
   }
   chips._xscale = chips._yscale = Math.min(Equipment.stripScale, chips._width > right - left ? (right - left) / chips._width * 100 : 100);
   chips._y = top + Equipment.STRIP_HIGH * (1 - chips._yscale / 100) / 2;
   // a menu that centres the name of the category (Nordic UI) gets the strip centred under it too
   if(label.textField.getTextFormat().align == "center")
   {
      chips._x = Math.max(left, Math.min(right - chips._width, left + (label.textField._width * label._xscale / 100 - chips._width) / 2));
   }
   Equipment.markChips();
};

Equipment.clearChips = function()
{
   var chips = Equipment.chips;
   for(var name in chips)
   {
      if(chips[name] instanceof MovieClip)
      {
         chips[name].removeMovieClip();
      }
   }
   chips.clear();
   Equipment.chipTip._visible = false;
};

// the words of the groups from x on, under a slot filter only those that split what fits the slot
Equipment.layChips = function(rows, x, splitting)
{
   var chosen = Equipment.chosenNow();
   var r = 0;
   var c;
   var made;
   var first;
   var chip;
   while(r < rows.length)
   {
      c = 0;
      first = true;
      while(c < rows[r].length)
      {
         chip = rows[r][c];
         c++;
         if(splitting && !Equipment.splitsSlot(chip.key))
         {
            continue;
         }
         if(x > 0)
         {
            Equipment.separator(Equipment.chips, x - Equipment.CHIP_GAP / 2, first ? Equipment.GROUP_SEPARATOR : Equipment.SEPARATOR);
         }
         made = Equipment.makeChip(chip.key, Equipment.translated(chip), chip.icon, x, chosen[r] == chip.key, r, chip.key, chip.tint);
         x += made.wide + Equipment.CHIP_GAP;
         first = false;
      }
      r++;
   }
};

// a subcategory splits what fits the slot when it holds some of it and not all of it
Equipment.splitsSlot = function(key)
{
   var lists = Equipment.menu.inventoryLists;
   var entries = lists.itemList.entryList;
   var flag = lists.categoryList.selectedEntry.flag;
   var fit = 0;
   var held = 0;
   var i = 0;
   while(i < entries.length)
   {
      if(lists._typeFilter.isMatch(entries[i], flag) && Equipment.fits(entries[i], Equipment.activeSlot))
      {
         fit++;
         held += Equipment.chipHolds(key, entries[i]) ? 1 : 0;
      }
      i++;
   }
   return held > 0 && held < fit;
};

// a thin upright line between two subcategories, taller and lighter between two groups
Equipment.separator = function(chips, x, share)
{
   var high = Equipment.STRIP_HIGH * share;
   chips.lineStyle(1, Equipment.inkOf("chip"), share == Equipment.GROUP_SEPARATOR ? 45 : 22, true);
   chips.moveTo(x, (Equipment.STRIP_HIGH - high) / 2);
   chips.lineTo(x, (Equipment.STRIP_HIGH + high) / 2);
   chips.lineStyle();
};

Equipment.makeChip = function(name, words, icon, x, chosen, row, key, tint)
{
   var chip = Equipment.chips.createEmptyMovieClip("chip_" + name + "_" + row, Equipment.chips.getNextHighestDepth());
   var slot = row < 0;
   var color = slot ? Equipment.accentInk() : Equipment.inkOf("chip");
   var sign = chip.createEmptyMovieClip("sign", 2);
   var text = Equipment.writeLine(chip, "words", 1, words.toUpperCase(), Equipment.CHIP_TEXT, color);
   Equipment.drawIcon(sign, icon, 0, (Equipment.STRIP_HIGH - Equipment.CHIP_ICON) / 2, Equipment.CHIP_ICON, tint != undefined ? tint : color, 100);
   chip.tinted = tint != undefined;
   text._x = Equipment.CHIP_ICON + Equipment.ICON_TO_WORD;
   chip._x = x;
   chip.wide = text._x + text._width;
   chip.chosen = chosen;
   chip.row = row;
   chip.key = key;
   chip.label = words;
   chip.color = color;
   text._y = (Equipment.STRIP_HIGH - text._height) / 2;
   Equipment.drawChip(chip, false);
   chip.onRollOver = function()
   {
      Equipment.drawChip(this, true);
      Equipment.showChipWeight(this);
   };
   chip.onRollOut = chip.onReleaseOutside = function()
   {
      Equipment.drawChip(this, false);
      Equipment.hideChipWeight();
   };
   chip.onPress = function()
   {
      Equipment.pressChip(this);
   };
   return chip;
};

// dim words, lighter under the pointer, the chosen one bright and underlined. The clear box takes the presses
Equipment.drawChip = function(chip, lit)
{
   var line = chip.words._y + chip.words._height - Equipment.UNDERLINE_BELOW;
   chip.clear();
   chip.beginFill(0, 0);
   chip.moveTo(0, 0);
   chip.lineTo(chip.wide, 0);
   chip.lineTo(chip.wide, Equipment.STRIP_HIGH);
   chip.lineTo(0, Equipment.STRIP_HIGH);
   chip.lineTo(0, 0);
   chip.endFill();
   chip.words._alpha = chip.chosen ? 100 : (lit ? Equipment.CHIP_LIT : Equipment.CHIP_DIM);
   // a coloured sign keeps more of its colour while dim, the colour is what it says
   chip.sign._alpha = chip.chosen || lit ? 100 : (chip.tinted ? Equipment.CHIP_LIT : Equipment.CHIP_DIM);
   if(chip.chosen)
   {
      chip.lineStyle(2, chip.color, 85, true);
      chip.moveTo(chip.words._x + 1, line);
      chip.lineTo(chip.wide - 1, line);
      chip.lineStyle();
   }
};

// The game brings the list up to date through InvalidateListData of the lists, after an equip and the like. That call
// runs the same InvalidateData of the list, counts the categories again and tells the menu. A player's subcategories
// took effect only once he equipped something, so some menu files of other UI mods seem to redraw their rows on that
// call alone. A filter of the panel goes the same way
Equipment.refilter = function()
{
   var lists = Equipment.menu.inventoryLists;
   if(lists.InvalidateListData != undefined)
   {
      lists.InvalidateListData();
      return undefined;
   }
   lists.itemList.InvalidateData();
};

Equipment.pressChip = function(chip)
{
   var lists = Equipment.menu.inventoryLists;
   if(lists.itemList.disableInput || lists.categoryList.disableInput)
   {
      return undefined;
   }
   if(chip.row < 0)
   {
      Equipment.dropFilter();
   }
   else
   {
      Equipment.pickChip(Equipment.chosenNow(), Equipment.CHIP_ROWS[Equipment.chipsFamily], chip.row, chip.key);
      Equipment.leaveRowsOpen(chip.row);
   }
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
   Equipment.showChips();
   Equipment.keepChips();
   lists.itemList.selectedIndex = -1;
   lists.itemList.scrollPosition = 0;
   Equipment.refilter();
};

// A press on a chip of a group: the chosen one again lets the group go, another takes its place. A chip that's alone
// lets the other groups go, and goes itself when another group gets a chip
Equipment.pickChip = function(chosen, rows, row, key)
{
   var alone;
   var r = 0;
   chosen[row] = chosen[row] == key ? undefined : key;
   if(chosen[row] == undefined)
   {
      return undefined;
   }
   alone = Equipment.chipAlone(rows, row, key);
   while(rows != undefined && r < rows.length)
   {
      if(r != row && (alone || Equipment.chipAlone(rows, r, chosen[r])))
      {
         chosen[r] = undefined;
      }
      r++;
   }
};

// Groups add up, and a pick that leaves no row at all with the other groups lets them go: Clothing chosen and then
// Other of the slots left an empty list, and Clothing looked stuck
Equipment.leaveRowsOpen = function(row)
{
   var chosen = Equipment.chosenNow();
   var r = 0;
   if(Equipment.activeSlot != undefined || chosen[row] == undefined || Equipment.chipsLeaveRows())
   {
      return undefined;
   }
   while(r < chosen.length)
   {
      if(r != row)
      {
         chosen[r] = undefined;
      }
      r++;
   }
};

Equipment.chipAlone = function(rows, row, key)
{
   var group = rows != undefined ? rows[row] : undefined;
   var c = 0;
   while(key != undefined && group != undefined && c < group.length)
   {
      if(group[c].key == key)
      {
         return group[c].alone == true;
      }
      c++;
   }
   return false;
};

// for the test stand: chooses a chip of a row by its key the way a press does, "" lets the row go
Equipment.chooseChip = function(told)
{
   var parts = String(told).split(",");
   var row = Number(parts[0]);
   Equipment.chosenNow()[row] = parts[1] == "" ? undefined : parts[1];
   Equipment.showChips();
   Equipment.keepChips();
   Equipment.refilter();
};

// Keys walk the subcategories too. Up on the list's first row, or with no row picked, goes up to the strip, Left and
// Right walk its chips, Enter chooses the lit one the way a click does, Down or Escape go back to the rows. Tab still
// closes the menu. A pad has its own way there (39-gamepad.as)
Equipment.chipKeys = function(details)
{
   var nav = details.navEquivalent;
   var codes = gfx.ui.NavigationCode;
   var list = Equipment.menu.inventoryLists.itemList;
   var chips;
   if(Equipment.menu._platform != 0 || !Shared.GlobalFunc.IsKeyPressed(details) || !Equipment.listsAnswer())
   {
      return false;
   }
   chips = Equipment.padChips();
   if(Equipment.keyChip == undefined)
   {
      if(nav != codes.UP || chips.length == 0 || !(list.selectedIndex < 0 || list.getListEnumIndex(list.selectedIndex) == 0))
      {
         return false;
      }
      Equipment.lightKeyChip(Equipment.firstKeyChip(chips));
      gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
      return true;
   }
   if(nav == codes.LEFT || nav == codes.RIGHT)
   {
      Equipment.lightKeyChip(Math.max(0, Math.min(chips.length - 1, Equipment.keyChip + (nav == codes.LEFT ? -1 : 1))));
      gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
      return true;
   }
   if(nav == codes.ENTER)
   {
      if(chips[Equipment.keyChip] != undefined)
      {
         Equipment.pressChip(chips[Equipment.keyChip]);
         Equipment.lightKeyChip(Math.min(Equipment.keyChip, Equipment.padChips().length - 1));
      }
      return true;
   }
   if(nav == codes.UP)
   {
      return true;
   }
   Equipment.dropKeyChip();
   // Down steps back onto the rows and nothing more, Escape only leaves the strip, other keys go on as always
   return nav == codes.DOWN || nav == codes.ESCAPE;
};

// the chip the keys start on: the first one chosen, or the first one
Equipment.firstKeyChip = function(chips)
{
   var i = 0;
   while(i < chips.length)
   {
      if(chips[i].chosen)
      {
         return i;
      }
      i++;
   }
   return 0;
};

Equipment.lightKeyChip = function(index)
{
   var chips = Equipment.padChips();
   var old = Equipment.keyChip != undefined ? chips[Equipment.keyChip] : undefined;
   var chip = chips[index];
   if(old != undefined)
   {
      Equipment.drawChip(old, false);
   }
   Equipment.keyChip = chip != undefined ? index : undefined;
   if(Equipment.padRing != undefined)
   {
      Equipment.padRing.clear();
   }
   if(chip != undefined)
   {
      Equipment.drawChip(chip, true);
      if(Equipment.padRing != undefined)
      {
         Equipment.padRingAround(chip, chip.wide, Equipment.STRIP_HIGH);
      }
   }
};

Equipment.dropKeyChip = function()
{
   var chip = Equipment.keyChip != undefined ? Equipment.padChips()[Equipment.keyChip] : undefined;
   if(chip != undefined)
   {
      Equipment.drawChip(chip, false);
   }
   Equipment.keyChip = undefined;
   if(Equipment.padRing != undefined && Equipment.menu._platform == 0)
   {
      Equipment.padRing.clear();
   }
};

// for the test stand: a key handed to the menu the way SkyUI's input hands it, by the name of its navigation code
// ("UP", "ENTER"). The stand's keys reach the menus only with a name of the game's controls, arrows have none there
Equipment.pressNav = function(name)
{
   Equipment.menu.handleInput({value:"keyDown", navEquivalent:gfx.ui.NavigationCode[String(name).toUpperCase()], code:0}, []);
};

// the chip of a slot filter has no key of its own, the stand calls it "slot"
Equipment.chipKey = function(chip)
{
   return chip.key != undefined ? chip.key : "slot";
};

// for the test stand: the middle of the chip with this key as aimedX, aimedY, and what a press there would land on.
// Papyrus hands strings over in the case the game met them first ("Head" for "head"), so case doesn't count
Equipment.aimChip = function(key)
{
   var name;
   var chip;
   var point;
   var share;
   Equipment.aimedX = Equipment.aimedY = -1;
   for(name in Equipment.chips)
   {
      chip = Equipment.chips[name];
      if(chip instanceof MovieClip && chip.wide != undefined && Equipment.chipKey(chip).toLowerCase() == String(key).toLowerCase())
      {
         point = {x:chip.wide / 2, y:Equipment.STRIP_HIGH / 2};
         chip.localToGlobal(point);
         share = Equipment.screenShare(point.x, point.y);
         Equipment.aimedX = share.x;
         Equipment.aimedY = share.y;
         Equipment.aimedTop = String(Mouse.getTopMostEntity(point.x, point.y, true));
         return undefined;
      }
   }
};

// for the test stand: every chip in sight with its box in shares of the screen, chosen or not, and whether a press on
// its middle reaches it or something of another menu file lies over it, then the rows of the list
Equipment.chipsReport = function(note)
{
   var lines = [];
   var name;
   var chip;
   var from;
   var to;
   var top;
   for(name in Equipment.chips)
   {
      chip = Equipment.chips[name];
      if(chip instanceof MovieClip && chip.wide != undefined)
      {
         from = {x:0, y:0};
         to = {x:chip.wide, y:Equipment.STRIP_HIGH};
         chip.localToGlobal(from);
         chip.localToGlobal(to);
         top = String(Mouse.getTopMostEntity((from.x + to.x) / 2, (from.y + to.y) / 2, true));
         lines.push(Equipment.chipKey(chip) + (chip.chosen ? "*" : "") + " " + Equipment.tenth(Equipment.screenShare(from.x, from.y).x * 100) + "-" + Equipment.tenth(Equipment.screenShare(to.x, to.y).x * 100) + "%" + (top.indexOf(String(chip)) == 0 ? "" : " UNDER " + top));
      }
   }
   lines.reverse();
   skse.Log("PANELTEST chips " + note + ": " + lines.join(", ") + "; strip " + (Equipment.chips._visible ? "shown" : "hidden") + ", rows " + Equipment.menu.inventoryLists.itemList.listEnumeration.size() + ", category " + Equipment.menu.inventoryLists.categoryList.selectedEntry.text);
};

// for the test stand: how many rows of the open category each of its chips would keep, nothing chosen changes
Equipment.chipCounts = function(note)
{
   var lists = Equipment.menu.inventoryLists;
   var entries = lists.itemList.entryList;
   var flag = lists.categoryList.selectedEntry.flag;
   var rows = Equipment.CHIP_ROWS[Equipment.chipFamily(flag)];
   var counts = [];
   var r = 0;
   var c;
   var i;
   var held;
   while(rows != undefined && r < rows.length)
   {
      c = 0;
      while(c < rows[r].length)
      {
         held = 0;
         i = 0;
         while(i < entries.length)
         {
            held += lists._typeFilter.isMatch(entries[i], flag) && Equipment.chipHolds(rows[r][c].key, entries[i]) ? 1 : 0;
            i++;
         }
         counts.push(rows[r][c].key + " " + held);
         c++;
      }
      r++;
   }
   skse.Log("PANELTEST counts " + note + " (" + lists.categoryList.selectedEntry.text + "): " + counts.join(", "));
};

// for the test stand: the list scrolled as far down as it goes
Equipment.scrollListEnd = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   list.scrollPosition = list.maxScrollPosition;
};

// for the test stand: how many rows the list draws for its height, and where the last one ends against the top of
// what the bottom bar shows
Equipment.listFitReport = function(note)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var last = list.getClipByIndex(list._listIndex - 1);
   var end = {x:0, y:last != undefined ? last._height : 0};
   var bar = {x:0, y:Equipment.barTop()};
   if(last != undefined)
   {
      last.localToGlobal(end);
   }
   Equipment.menu.localToGlobal(bar);
   skse.Log("PANELTEST list fit " + note + ": height " + Math.round(list.listHeight) + ", row " + list.entryHeight + ", fit " + list._maxListIndex + ", drawn " + list._listIndex + ", last row ends " + Math.round(end.y) + ", bottom bar at " + Math.round(bar.y) + (end.y > bar.y + 1 ? " UNDER THE BAR" : ""));
};

// for the test stand: the category with this flag is pressed, the sides of a chest or a shop have flags of their own
Equipment.chooseFlag = function(flag)
{
   var index = Equipment.categoryWith(Number(flag));
   skse.Log("PANELTEST category flag " + flag + " is at " + index);
   if(index >= 0)
   {
      Equipment.chooseCategory(index);
   }
};

// for the test stand: a category of the list is pressed by its place in the bar
Equipment.chooseCategory = function(index)
{
   Equipment.menu.inventoryLists.categoryList.onItemPress(Number(index), 0);
};
