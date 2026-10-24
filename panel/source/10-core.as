var Equipment = {};
_global.Equipment = Equipment;

Equipment.kind = _global.EquipmentPanelMenu == undefined ? "inventory" : _global.EquipmentPanelMenu;
Equipment.ARMOR = 26;
Equipment.LIGHT = 31;
Equipment.WEAPON = 41;
Equipment.AMMO = 42;
Equipment.BOOK = 27;
Equipment.INGREDIENT = 30;
Equipment.POTION = 46;
Equipment.SCROLL = 23;
Equipment.WORN = 1;
Equipment.LEFT_HAND = 2;
Equipment.RIGHT_HAND = 3;
Equipment.BOTH_HANDS = 4;
Equipment.SHIELD = 512;
Equipment.FLAG_ALL = 1023;
Equipment.FLAG_WEAPONS = 2;
Equipment.FLAG_ARMOR = 4;
Equipment.CONTAINER_WEAPONS = 2048;
Equipment.CONTAINER_ARMOR = 4096;
Equipment.MASKED_FIRST = ["body", "head", "hands", "feet", "amulet", "ring"];
// body slot 46, where cloak mods put their cloaks
Equipment.CLOAK = 65536;
Equipment.RING_WORDS = ["ClothingRing", "gearMaraRing"];
Equipment.NECK_WORDS = ["ClothingNecklace", "gearMaraAmulet"];
Equipment.MARA_WORDS = ["gearMaraRing", "gearMaraAmulet"];

// weapon types as SkyUI names them in subType
Equipment.MELEE = 0;
Equipment.SWORD = 1;
Equipment.DAGGER = 2;
Equipment.WARAXE = 3;
Equipment.MACE = 4;
Equipment.GREATSWORD = 5;
Equipment.BATTLEAXE = 6;
Equipment.WARHAMMER = 7;
Equipment.BOW = 8;
Equipment.CROSSBOW = 9;
Equipment.STAFF = 10;
Equipment.PICKAXE = 11;
Equipment.WOODAXE = 12;
Equipment.FISHINGROD = 13;
Equipment.ARROW = 0;
Equipment.BOLT = 1;
Equipment.ONE_HANDED = [0, 1, 2, 3, 4, 10, 11, 12, 13];
Equipment.TWO_HANDED = [5, 6, 7];

// The way the game itself swings a weapon (weaponType of the row, 10-19 are the same ten again). SkyUI builds number
// their subtypes apart: menus made on SkyUI 5 like Dear Diary give spears, halberds, katanas and other keyword kinds
// 13 and up, SkyUI 6 has its fishing rod at 13. So hands, bows and crossbows go by the game's own kind, the subtype
// only when a menu hands no kind over
Equipment.ANIM_ONE_HANDED = [0, 1, 2, 3, 4, 8];
Equipment.ANIM_TWO_HANDED = [5, 6];
Equipment.ANIM_BOW = 7;
Equipment.ANIM_CROSSBOW = 9;

// the plugin that draws 3D models into the menu
Equipment.plugin = function()
{
   return skse.plugins.EquipmentPanel;
};

// Every word the panel shows goes through here. Interface/Translations/InventoryReforged_<LANGUAGE>.txt gives it
// as "$IR_<the English words>" in the language of the game, words it doesn't have stay English. A "#" takes a
// value given after the words, "#1", "#2" take them by number so a language can put them in its own order.
// build.py writes the English file from every say("...") of the parts, so only plain words go in here.
// The plugin reads the file for the panel, the game's translator would hand back HEALTH for Health
Equipment.said = {};
Equipment.say = function(words)
{
   var shown = Equipment.said[words];
   var plugin;
   var pieces;
   var told;
   var next;
   var code;
   var i;
   var at;
   if(shown == undefined)
   {
      if(Equipment.fileWords == undefined)
      {
         plugin = Equipment.plugin();
         Equipment.fileWords = plugin != undefined && plugin.Words != undefined ? plugin.Words() : undefined;
      }
      shown = Equipment.fileWords != undefined ? Equipment.fileWords["$IR_" + words] : undefined;
      if(shown == undefined)
      {
         shown = skyui.util.Translator.translate("$IR_" + words);
      }
      if(shown == undefined || shown == "" || shown.charAt(0) == "$")
      {
         shown = words;
      }
      Equipment.said[words] = shown;
   }
   if(arguments.length < 2)
   {
      return shown;
   }
   // one pass over the pieces between the signs, so a "#" inside a value (a name like "Sword #2") stays as it is
   pieces = shown.split("#");
   told = pieces[0];
   next = 1;
   i = 1;
   while(i < pieces.length)
   {
      // an empty piece (a "#" at the end, "From #") gives NaN, and in AS2 "NaN >= 49" holds, so only < and > here
      code = pieces[i].length > 0 ? pieces[i].charCodeAt(0) : 0;
      at = code > 48 && code < 58 ? code - 48 : 0;
      if(at > 0 && at < arguments.length)
      {
         told += String(arguments[at]) + pieces[i].substring(1);
      }
      else if(at == 0 && next < arguments.length)
      {
         told += String(arguments[next]) + pieces[i];
         next++;
      }
      else
      {
         told += "#" + pieces[i];
      }
      i++;
   }
   return told;
};

// The order of slots is the order the plugin numbers them in for hidden slots, keep the two in step
Equipment.SLOTS = [
   {name:"right", column:0, row:0, icon:"sword", flag:Equipment.FLAG_WEAPONS, title:Equipment.say("Right hand")},
   {name:"left", column:0, row:1, icon:"shield", flag:Equipment.FLAG_ALL, title:Equipment.say("Left hand")},
   {name:"ammo", column:0, row:2, icon:"arrows", flag:Equipment.FLAG_WEAPONS, title:Equipment.say("Ammo")},
   {name:"amulet", column:0, row:3, icon:"amulet", flag:Equipment.FLAG_ARMOR, mask:32, title:Equipment.say("Neck")},
   {name:"ring", column:0, row:4, icon:"ring", flag:Equipment.FLAG_ARMOR, mask:64, title:Equipment.say("Ring")},
   {name:"head", column:1, row:0, icon:"helmet", flag:Equipment.FLAG_ARMOR, mask:14339, title:Equipment.say("Head")},
   {name:"body", column:1, row:1, icon:"cuirass", flag:Equipment.FLAG_ARMOR, mask:4, title:Equipment.say("Body")},
   {name:"hands", column:1, row:2, icon:"gauntlets", flag:Equipment.FLAG_ARMOR, mask:24, title:Equipment.say("Hands")},
   {name:"feet", column:1, row:3, icon:"boots", flag:Equipment.FLAG_ARMOR, mask:384, title:Equipment.say("Feet")},
   {name:"other", column:1, row:4, icon:"cloak", flag:Equipment.FLAG_ARMOR, title:Equipment.say("Other")}
];

Equipment.DEFAULTS = {panel:true, doll:true, turnSpeed:0.5, slotModels:true, wornLook:true, hideIcons:true, popup:true, doubleClickTakesOff:true, dragAndDrop:true, pieceStrips:true, arrows:true, equalSign:true, badges:true, subcategories:true, lastAdded:true, stackWeight:true, rowMenu:true, stackKey:45, newMarks:true, junk:true, junkKey:36, weightWarnings:true, questMarks:true, stats:true, heroStats:true, slotNumbers:false, quietTags:true, modOrigin:true, questLine:true, leaveCard:false, inspectOnZoomKey:true, accent:14069864, text:15130824, dollScale:100, dollRight:0, dollUp:0, slotsOff:0, listWidth:0, windowSize:100, windowShow:0};

Equipment.readSettings = function()
{
   var told = Equipment.plugin() != undefined ? Equipment.plugin().Settings() : undefined;
   var settings = {};
   for(var name in Equipment.DEFAULTS)
   {
      settings[name] = told != undefined && told[name] != undefined ? told[name] : Equipment.DEFAULTS[name];
   }
   Equipment.settings = settings;
   // every part draws with these two at the moment it draws, so the menu's colours hold from the first frame
   Equipment.GOLD = Number(settings.accent);
   Equipment.PARCHMENT = Number(settings.text);
};

// A setting changed in the inventory itself goes into the file the menu of the mod keeps, by its id there
// ("iScale:Doll"), and holds at once. Switches go as 1 and 0
Equipment.changeSetting = function(name, id, value)
{
   Equipment.settings[name] = value;
   Equipment.tellSetting(id, value);
};

Equipment.tellSetting = function(id, value)
{
   if(Equipment.plugin() != undefined)
   {
      Equipment.plugin().SetSetting(id, typeof value == "boolean" ? (value ? 1 : 0) : Number(value));
   }
};

// Some mods name their items by a key of a translation file ("$TNG_TCT"), the game shows the words of the file.
// The panel's fields don't look texts up, the name is translated here
Equipment.gameName = function(text)
{
   var words = String(text);
   var shown;
   if(words.charAt(0) != "$")
   {
      return words;
   }
   shown = skyui.util.Translator.translate(words);
   return shown != undefined && shown != "" ? String(shown) : words;
};

// Skins that lay the list on light paper give its rows dark text in their config, Dear Diary does. The panel's own
// words and signs over the list and in the bottom bar then take dark ink, and the item window becomes a sheet of
// paper, the card of such a skin is written in black. Each role has its ink for a dark skin and for a paper one.
// The menu's config tells which, join reads it
Equipment.PAPER_TEXT = 0.45;
Equipment.paper = false;
Equipment.INKS = {
   kind:[0x9C978B, 0x5C4C38],
   name:[undefined, 0x4A3B2A],
   value:[undefined, 0x1C1814],
   quiet:[0x9C978B, 0x66584A],
   chip:[undefined, 0x2A2420],
   sign:[undefined, 0x2A2420],
   junk:[0x8A8579, 0x5E574E],
   better:[7195502, 0x1F6E2A],
   worse:[14702938, 0x9E2A22],
   equal:[11579568, 0x4C4C4C],
   tooHeavy:[0xD8664F, 0xA0301E],
   heavyStack:[0xE2B456, 0x8A5A00],
   stolen:[0xD8664F, 0xA0301E],
   edge:[undefined, 0x3A2E20],
   sheet:[0, 0xD9CEB6]
};

Equipment.readSkin = function(config)
{
   var colors = config != undefined && config.Appearance != undefined ? config.Appearance.colors : undefined;
   var text = colors != undefined && colors.text != undefined ? colors.text.enabled : undefined;
   var told = Number(text);
   Equipment.paper = text != undefined && !isNaN(told) && Equipment.lightness(told) < Equipment.PAPER_TEXT;
};

Equipment.lightness = function(color)
{
   return (0.299 * ((color >> 16) & 255) + 0.587 * ((color >> 8) & 255) + 0.114 * (color & 255)) / 255;
};

// The ink of a role, the colours a player set stand for the dark skin's words and signs. A part on a dark box of its
// own, the look sheet, asks for the dark skin's ink on any skin
Equipment.inkOf = function(role, onDark)
{
   var inks = Equipment.INKS[role];
   var dark = {name:Equipment.PARCHMENT, value:Equipment.WHITE, chip:Equipment.WHITE, sign:Equipment.PARCHMENT, edge:Equipment.WHITE};
   if(inks == undefined)
   {
      return Equipment.WHITE;
   }
   if(Equipment.paper && !onDark)
   {
      return inks[1];
   }
   return inks[0] != undefined ? inks[0] : dark[role];
};

// the accent on paper, darker so it reads
Equipment.accentInk = function()
{
   return Equipment.paper ? Equipment.blendColor(Equipment.GOLD, 0, 0.45) : Equipment.GOLD;
};

// the fill of a small boxed button: see-through dark on a dark skin, a light wash on paper
Equipment.buttonFill = function(clip, alpha)
{
   clip.beginFill(Equipment.paper ? Equipment.WHITE : 0, Equipment.paper ? Math.round(alpha / 2) : alpha);
};

Equipment.number = function(value)
{
   var told;
   if(value == undefined || value == null)
   {
      return 0;
   }
   told = Number(value);
   return isNaN(told) ? 0 : told;
};

// a count that bounds a loop, read off settings of SkyUI or of another mod: 0 for anything that isn't a number,
// "i <= NaN" stays true in ActionScript 2 and such a loop never ends. A column has a few states, 32 is plenty
Equipment.count = function(value)
{
   var told = Number(value);
   return told > 0 ? Math.min(Math.floor(told), 32) : 0;
};

Equipment.has = function(list, value)
{
   var i = 0;
   while(i < list.length)
   {
      if(list[i] == value)
      {
         return true;
      }
      i++;
   }
   return false;
};

// a script of the game hands a name over in the letter case it met first, HEAD for head
Equipment.slotNamed = function(name)
{
   var wanted = String(name).toLowerCase();
   var i = 0;
   while(i < Equipment.SLOTS.length)
   {
      if(Equipment.SLOTS[i].name == wanted)
      {
         return Equipment.SLOTS[i];
      }
      i++;
   }
   return undefined;
};

Equipment.slotIndex = function(slot)
{
   var i = 0;
   while(i < Equipment.SLOTS.length)
   {
      if(Equipment.SLOTS[i] == slot)
      {
         return i;
      }
      i++;
   }
   return -1;
};

Equipment.weaponAnim = function(entry)
{
   var anim = entry.weaponType;
   return anim == undefined || anim == null || isNaN(anim) ? undefined : Number(anim) % 10;
};

Equipment.oneHanded = function(entry)
{
   var anim = Equipment.weaponAnim(entry);
   return entry.formType == Equipment.WEAPON && (anim != undefined ? Equipment.has(Equipment.ANIM_ONE_HANDED, anim) : Equipment.has(Equipment.ONE_HANDED, entry.subType));
};

Equipment.twoHanded = function(entry)
{
   var anim = Equipment.weaponAnim(entry);
   return entry.formType == Equipment.WEAPON && (anim != undefined ? Equipment.has(Equipment.ANIM_TWO_HANDED, anim) : Equipment.has(Equipment.TWO_HANDED, entry.subType));
};

Equipment.isBow = function(entry)
{
   var anim = Equipment.weaponAnim(entry);
   return entry.formType == Equipment.WEAPON && (anim != undefined ? anim == Equipment.ANIM_BOW : entry.subType == Equipment.BOW);
};

Equipment.isCrossbow = function(entry)
{
   var anim = Equipment.weaponAnim(entry);
   return entry.formType == Equipment.WEAPON && (anim != undefined ? anim == Equipment.ANIM_CROSSBOW : entry.subType == Equipment.CROSSBOW);
};

Equipment.isShield = function(entry)
{
   return entry.formType == Equipment.ARMOR && (entry.partMask & Equipment.SHIELD) != 0;
};

Equipment.armorSlot = function(piece)
{
   if(piece.partMask & Equipment.SHIELD)
   {
      return Equipment.slotNamed("left");
   }
   var i = 0;
   var slot;
   while(i < Equipment.MASKED_FIRST.length)
   {
      slot = Equipment.slotNamed(Equipment.MASKED_FIRST[i]);
      if(piece.partMask & slot.mask)
      {
         return slot;
      }
      i++;
   }
   // A piece with no body slot at all, jewelry MARA made slotless while the game runs, goes by its keywords. One with
   // slots of its own keeps to them: cloak mods give cloaks the necklace keyword so they take necklace enchantments.
   // The plugin sorts the same way for hidden slots
   if(Equipment.number(piece.partMask) == 0 && Equipment.hasWord(piece, Equipment.RING_WORDS))
   {
      return Equipment.slotNamed("ring");
   }
   if(Equipment.number(piece.partMask) == 0 && Equipment.hasWord(piece, Equipment.NECK_WORDS))
   {
      return Equipment.slotNamed("amulet");
   }
   return Equipment.slotNamed("other");
};

Equipment.hasWord = function(entry, words)
{
   var i = 0;
   while(entry.keywords != undefined && i < words.length)
   {
      if(entry.keywords[words[i]] != undefined)
      {
         return true;
      }
      i++;
   }
   return false;
};

// the slot an item goes to when put on, nothing for potions, food, scrolls and the rest that's used up
Equipment.slotOf = function(entry)
{
   if(entry.formType == Equipment.ARMOR)
   {
      return Equipment.armorSlot(entry);
   }
   if(entry.formType == Equipment.WEAPON)
   {
      return Equipment.slotNamed("right");
   }
   if(entry.formType == Equipment.AMMO)
   {
      return Equipment.slotNamed("ammo");
   }
   if(entry.formType == Equipment.LIGHT)
   {
      return Equipment.slotNamed("left");
   }
   return undefined;
};

// a small slot of a strip stands for the slot it came out of
Equipment.ownerOf = function(slot)
{
   return slot.parent != undefined ? slot.parent : slot;
};

// My Precious...es puts rings on fingers the game has no slot for. Those stay unworn for the game, its plugin
// draws them and marks their rows worn, the ring of the game's own finger it marks as such
Equipment.virtualRing = function(entry)
{
   return entry.formType == Equipment.ARMOR && entry.equipState > 0 && entry.myPreciousesVanillaRingSlotEquipped == false;
};

// jewelry another mod draws on the body itself, the eye of the panel can't hide it: rings on the extra fingers
// of My Precious...es, every ring and amulet MARA handles
Equipment.drawnElsewhere = function(entry)
{
   return entry != undefined && (Equipment.virtualRing(entry) || Equipment.hasWord(entry, Equipment.MARA_WORDS));
};

// what may be put into a slot: the left hand takes one handed weapons, shields and torches
Equipment.fits = function(entry, slot)
{
   slot = Equipment.ownerOf(slot);
   if(slot.name == "left" && entry.formType == Equipment.WEAPON)
   {
      return Equipment.oneHanded(entry);
   }
   return Equipment.slotOf(entry) == slot;
};

// category flags of the player's side in a menu that also lists a container or a merchant
Equipment.ownFlags = function()
{
   var categories = Equipment.menu.inventoryLists.categoryList;
   var divider = Equipment.dividerOf(categories);
   if(divider < 0)
   {
      return undefined;
   }
   var flags = [];
   var i = divider + 1;
   while(i < categories.entryList.length)
   {
      flags.push(categories.entryList[i].flag);
      i++;
   }
   return flags;
};

// the index of the divider between the two sides, -1 for a list of one side. A menu of another mod may hand a word
// or nothing at all
Equipment.dividerOf = function(categories)
{
   var told = Number(categories.dividerIndex);
   return categories.dividerIndex == undefined || isNaN(told) || told < 0 ? -1 : Math.floor(told);
};

Equipment.own = function(entry, flags)
{
   if(flags == undefined)
   {
      return true;
   }
   var i = 0;
   while(i < flags.length)
   {
      if(Equipment.menu.inventoryLists._typeFilter.isMatch(entry, flags[i]))
      {
         return true;
      }
      i++;
   }
   return false;
};

// Whom the panel shows, as the plugin tells it: the player, or in a follower's trade the follower ({player:false, name,
// formId}). The panel stands for the follower then, their side of the list is what they wear
Equipment.readSubject = function()
{
   var plugin = Equipment.plugin();
   var told = plugin != undefined && plugin.Subject != undefined ? plugin.Subject() : undefined;
   Equipment.subject = told != undefined ? told : {player:true};
};

Equipment.dressesOther = function()
{
   return Equipment.kind == "container" && Equipment.subject != undefined && Equipment.subject.player == false;
};

// category flags of the side whose worn things fill the slots: the follower's side, before the divider, in their trade
Equipment.wearerFlags = function()
{
   var categories = Equipment.menu.inventoryLists.categoryList;
   var divider;
   var flags = [];
   var i = 0;
   if(!Equipment.dressesOther())
   {
      return Equipment.ownFlags();
   }
   divider = Equipment.dividerOf(categories);
   if(divider < 0)
   {
      return undefined;
   }
   while(i < divider && i < categories.entryList.length)
   {
      flags.push(categories.entryList[i].flag);
      i++;
   }
   return flags;
};

// whether a row lies on the follower's side of their trade
Equipment.wearersRow = function(entry)
{
   return Equipment.dressesOther() && Equipment.own(entry, Equipment.wearerFlags());
};

// a body keeps its armor on and the game marks those rows as worn too, only the side of the one shown counts
Equipment.wornByPlayer = function(entry, flags)
{
   return entry.equipState > 0 && Equipment.own(entry, flags);
};

// what the player wears and holds. Only things with a slot count, a scroll in a hand isn't one of them
Equipment.worn = function(entries, flags)
{
   var worn = {armor:[]};
   var i = 0;
   var entry;
   while(i < entries.length)
   {
      entry = entries[i];
      i++;
      if(!Equipment.wornByPlayer(entry, flags) || Equipment.slotOf(entry) == undefined)
      {
         continue;
      }
      if(entry.formType == Equipment.ARMOR)
      {
         worn.armor.push(entry);
         if(Equipment.isShield(entry))
         {
            worn.shield = entry;
         }
      }
      else if(entry.formType == Equipment.AMMO)
      {
         worn.ammo = entry;
      }
      else if(entry.formType == Equipment.LIGHT)
      {
         worn.left = entry;
      }
      else
      {
         if(entry.equipState != Equipment.LEFT_HAND)
         {
            worn.right = entry;
         }
         if(entry.equipState == Equipment.LEFT_HAND || entry.equipState == Equipment.BOTH_HANDS)
         {
            worn.left = entry;
         }
      }
   }
   return worn;
};

// The game leaves a follower's outfit out of the rows of their trade, their armor would stand on the doll with an empty
// slot beside it. The plugin tells those pieces, they fill the slots where nothing else does and can only be looked at
Equipment.addOutfit = function(worn)
{
   var pieces = Equipment.dressesOther() && Equipment.plugin().Outfit != undefined ? Equipment.plugin().Outfit() : undefined;
   var piece;
   var i = 0;
   while(pieces != undefined && i < pieces.length)
   {
      piece = pieces[i];
      i++;
      // a mod that lists the outfit among the trade's rows (Follower Equip Control and the like) has it here already
      if(Equipment.wornAlready(worn, piece))
      {
         continue;
      }
      if(piece.formType == Equipment.ARMOR)
      {
         worn.armor.push(piece);
         if(Equipment.isShield(piece) && worn.shield == undefined)
         {
            worn.shield = piece;
         }
      }
      else if(piece.formType == Equipment.WEAPON)
      {
         if(piece.equipState != Equipment.LEFT_HAND && worn.right == undefined)
         {
            worn.right = piece;
         }
         if((piece.equipState == Equipment.LEFT_HAND || piece.equipState == Equipment.BOTH_HANDS) && worn.left == undefined)
         {
            worn.left = piece;
         }
      }
      else if(piece.formType == Equipment.AMMO && worn.ammo == undefined)
      {
         worn.ammo = piece;
      }
      else if(piece.formType == Equipment.LIGHT && worn.left == undefined)
      {
         worn.left = piece;
      }
   }
   return worn;
};

Equipment.wornAlready = function(worn, piece)
{
   var i = 0;
   while(i < worn.armor.length)
   {
      if(Equipment.sameForm(worn.armor[i], piece))
      {
         return true;
      }
      i++;
   }
   return Equipment.sameForm(worn.right, piece) || Equipment.sameForm(worn.left, piece) || Equipment.sameForm(worn.ammo, piece);
};

Equipment.sameForm = function(entry, other)
{
   return entry != undefined && entry.formId != undefined && entry.formId == other.formId;
};

// The index of a category of the list by its flag, the first one that has it. A list of two sides (a chest, a shop,
// a follower's trade) gives the other side the player's flags moved up ten bits, so a category is looked for on one
// side only: the side of the entry given, else the side in view. A slot of a follower's trade found the player's
// armor first and the list went over to the player
Equipment.SIDE_SHIFT = 1024;
Equipment.categoryWith = function(flag, entry)
{
   var categories = Equipment.menu.inventoryLists.categoryList;
   var entries = categories.entryList;
   var divider = Equipment.dividerOf(categories);
   var theirs = divider >= 0 && (entry != undefined ? !Equipment.own(entry, Equipment.ownFlags()) : categories.activeSegment == 0);
   var i = divider >= 0 && !theirs ? divider + 1 : 0;
   var end = divider >= 0 && theirs ? divider : entries.length;
   while(i < end)
   {
      if(entries[i].flag == flag || theirs && entries[i].flag == flag * Equipment.SIDE_SHIFT)
      {
         return i;
      }
      i++;
   }
   return -1;
};

// The plugin times the script's builds while the menu comes up, its log line about the opening tells them apart from
// the plugin and the game. getTimer gave builds of hundreds of rows 0 ms on the stand, so the plugin's clock counts.
// A build inside a build isn't counted twice
Equipment.timedInside = 0;
Equipment.timeFrom = function(part)
{
   var plugin = Equipment.plugin();
   Equipment.timedInside++;
   if(Equipment.timedInside == 1 && plugin != undefined && plugin.ScriptStarts != undefined)
   {
      plugin.ScriptStarts(part);
   }
};
Equipment.timeTo = function()
{
   var plugin = Equipment.plugin();
   Equipment.timedInside--;
   if(Equipment.timedInside == 0 && plugin != undefined && plugin.ScriptStops != undefined)
   {
      plugin.ScriptStops();
   }
};

Equipment.processList = function(list)
{
   Equipment.timeFrom("rows");
   var entries = list.entryList;
   var flags = Equipment.ownFlags();
   var wearer = Equipment.wearerFlags();
   var worn = Equipment.addOutfit(Equipment.worn(entries, wearer));
   var i = 0;
   Equipment.wornNow = worn;
   while(i < entries.length)
   {
      entries[i].equipmentDifference = Equipment.difference(entries[i], worn, wearer);
      entries[i].infoStackWeight = Equipment.stackWeight(entries[i]);
      Equipment.fillKind(entries[i]);
      Equipment.fillIcon(entries[i]);
      Equipment.fillStolen(entries[i]);
      i++;
   }
   Equipment.tintWeights(entries, flags);
   Equipment.decorateRows();
   Equipment.listReady(list, worn);
   Equipment.queueListed();
   Equipment.cardNumbersLater();
   // the junk button follows the rows, the strip opens or shuts with it
   if(Equipment.junkButtonShown || Equipment.junkButtonWanted())
   {
      Equipment.junkChanged();
   }
   else
   {
      Equipment.markChips();
   }
   Equipment.timeTo();
};

// SkyUI names a piece by its first body slot and leaves a piece with no slot, or with one it has no word for, without
// a kind: MARA's rings and amulets, cloaks. The panel sorts those anyway, the column of kinds takes its word
Equipment.fillKind = function(entry)
{
   var slot;
   if(entry.formType != Equipment.ARMOR || entry.subTypeDisplay != undefined && entry.subTypeDisplay != "" && entry.subTypeDisplay != "-")
   {
      return undefined;
   }
   slot = Equipment.armorSlot(entry).name;
   if(slot == "ring")
   {
      entry.subTypeDisplay = Equipment.translated({words:"$Ring", fallback:"Ring"});
   }
   else if(slot == "amulet")
   {
      entry.subTypeDisplay = Equipment.translated({words:"$Amulet", fallback:"Amulet"});
   }
   else if((Equipment.number(entry.partMask) & Equipment.CLOAK) != 0)
   {
      entry.subTypeDisplay = Equipment.say("Cloak");
   }
   else
   {
      entry.subTypeDisplay = Equipment.translated({words:"$Other", fallback:"Other"});
   }
};

// The same pieces get SkyUI's fallback icon, a plain shield. They take the ring, necklace or cloak of the icon set
// the player has, an icon some other mod picked stays
Equipment.FILLED_ICONS = {ring:"armor_ring", amulet:"armor_amulet"};
Equipment.fillIcon = function(entry)
{
   var icon;
   if(entry.formType != Equipment.ARMOR || entry.iconLabel != "default_armor")
   {
      return undefined;
   }
   icon = Equipment.FILLED_ICONS[Equipment.armorSlot(entry).name];
   if(icon == undefined && (Equipment.number(entry.partMask) & Equipment.CLOAK) != 0)
   {
      icon = "clothing_cloak";
   }
   if(icon != undefined)
   {
      entry.iconLabel = icon;
   }
};

// On a chest's side the game leaves its stolen mark off, and the stolen copies of a dagger in a row of their own
// looked just like the row beside them. The plugin's count marks them
Equipment.fillStolen = function(entry)
{
   if(Equipment.number(entry.equipmentStolen) > 0)
   {
      entry.isStolen = true;
   }
};

// The menu builds its lists once its settings arrive. The panel script may come before that or after,
// the plugin loads it into the menu while the menu loads its own files
Equipment.attach = function(menu)
{
   Equipment.menu = menu;
   Equipment.readSettings();
   Equipment.readSubject();
   if(menu._config != undefined)
   {
      Equipment.join(menu._config);
      menu.inventoryLists.itemList.InvalidateData();
      return undefined;
   }
   menu.equipmentSetConfig = menu.setConfig;
   // the columns change before SkyUI reads them, the plugin's watcher counts that as the build too, SkyUI's own
   // reading of its settings stays outside
   menu.setConfig = function(config)
   {
      // SkyUI's switch for plain white icons, a sign the panel draws itself follows it
      Equipment.noIconColors = config != undefined && config.Appearance != undefined && config.Appearance.icons.item.noColor == true;
      Equipment.timeFrom("build");
      Equipment.addSorting(config);
      Equipment.timeTo();
      this.equipmentSetConfig(config);
      Equipment.join(config);
   };
};

Equipment.join = function(config)
{
   if(Equipment.attached)
   {
      return undefined;
   }
   Equipment.attached = true;
   Equipment.timeFrom("build");
   Equipment.guardUpdates(Equipment.menu.inventoryLists.itemList);
   Equipment.guardUpdates(Equipment.menu.inventoryLists.categoryList);
   Equipment.readSkin(config);
   Equipment.readSquash();
   Equipment.buildLayers();
   if(Equipment.kind == "magic")
   {
      Equipment.startMagic();
      Equipment.timeTo();
      return undefined;
   }
   Equipment.addSorting(config);
   Equipment.menu.inventoryLists.itemList.addDataProcessor(Equipment);
   Equipment.startRows(config);
   if(Equipment.kind == "inventory" || Equipment.dressesOther())
   {
      Equipment.started(config);
   }
   else
   {
      Equipment.startMoving();
   }
   Equipment.startCharging();
   Equipment.timeTo();
};

// SkyUI's requestUpdate starts a timer on every call and keeps the id of the last one only, so a second call before
// the update ran left the first timer behind, updating the whole list every frame for as long as the menu stayed
// open. New dots, fitted rows and junk marks ask for updates of their own, and with a few thousand rows the timers
// left behind slowed the menu to a stop. A call while an update already waits only says it's wanted
Equipment.guardUpdates = function(list)
{
   if(list == undefined || list.equipmentRequestUpdate != undefined)
   {
      return undefined;
   }
   list.equipmentRequestUpdate = list.requestUpdate;
   list.requestUpdate = function()
   {
      if(this._updateRequestID || this._invalidateRequestID)
      {
         this._bRequestUpdate = true;
         return undefined;
      }
      this.equipmentRequestUpdate();
   };
};

Equipment.started = function(config)
{
};

Equipment.listReady = function(list, worn)
{
};
