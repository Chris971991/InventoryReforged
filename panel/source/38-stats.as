Equipment.STATS_LINE = 18;
Equipment.STOLEN_TEXT = 14;
Equipment.STOLEN_GAP = 4;
Equipment.BODY_SLOTS_TEXT = 12;
Equipment.BODY_SLOTS_COLOR = 0x9C978B;
Equipment.FIRST_BODY_SLOT = 30;

// Base timings of the game's own attack animations at speed 1, read from its behavior files:
// a one handed swing lands 0.70 s after the press and the next one may start at 1.20 s,
// a two handed one lands at 0.80 s and chains at 1.13 s. A bow is drawn full in about a second and
// takes 0.8 s to loose and nock again, a crossbow about 2.5 s a shot. The weapon's speed and the player's
// attack speed divide them. Animation mods change these, so the numbers are a guide
Equipment.SWING = {oneHanded:{hit:0.7, cycle:1.2}, twoHanded:{hit:0.8, cycle:1.13}, bow:{hit:1.0, cycle:1.8}, crossbow:{hit:1.0, cycle:2.5}};

// the numbers block lives under the card, two columns of names and values
Equipment.buildStats = function(window)
{
   window.createTextField("statNames", 2, 0, 0, 10, 10);
   window.createTextField("statValues", 3, 0, 0, 10, 10);
   Equipment.styleText(window.statNames, Equipment.STATS_TEXT, "left", Equipment.inkOf("name"));
   Equipment.styleText(window.statValues, Equipment.STATS_TEXT, "right", Equipment.inkOf("value"));
   window.statNames.multiline = window.statValues.multiline = true;
   window.statNames.wordWrap = window.statValues.wordWrap = false;
   window.statValues.html = true;
   window.statLines = [];

   // a quiet line of the game's body slots under the numbers, it keeps clear of the lens
   window.createTextField("bodySlots", 4, 0, 0, 10, 10);
   Equipment.styleText(window.bodySlots, Equipment.BODY_SLOTS_TEXT, "left", Equipment.inkOf("quiet"));
   window.bodySlots.multiline = window.bodySlots.wordWrap = true;
   window.bodySlots.autoSize = "left";
   window.bodySlots._width = window.wide - 2 * Equipment.STATS_PAD - Equipment.LENS_SIZE;
   window.bodySlots._visible = false;

   // the stolen note of the game's card, as a line of its own over the quiet ones
   window.createTextField("stolen", 5, 0, 0, 10, 10);
   Equipment.styleText(window.stolen, Equipment.STOLEN_TEXT, "left", Equipment.inkOf("stolen"));
   window.stolen.autoSize = "left";
   window.stolen._visible = false;
};

Equipment.statsHeight = function()
{
   var lines = Equipment.window.statLines.length;
   return lines == 0 ? 0 : lines * Equipment.STATS_LINE + 2 * Equipment.STATS_PAD - 6;
};

Equipment.stolenHeight = function()
{
   var line = Equipment.window.stolen;
   return line != undefined && line._visible ? line._height + Equipment.STOLEN_GAP : 0;
};

// The game marks a whole row stolen, the plugin counts the copies of it that belong to someone else. Without that
// count the game's own mark still says it. With stolen copies in rows of their own a stolen row is all stolen, the
// count is only for a row that still mixes them, a merchant's say
Equipment.stolenLine = function(entry)
{
   var count = Math.max(Equipment.number(entry.count), 1);
   var stolen = Math.min(Equipment.number(entry.equipmentStolen), count);
   if(stolen <= 0)
   {
      return entry.isStolen == true ? Equipment.say("Stolen") : "";
   }
   return stolen == count ? Equipment.say("Stolen") : Equipment.say("Stolen: #1 of #2", stolen, count);
};

// whether a row holds stolen copies, two rows of one item with the same name differ by it
Equipment.stolenRow = function(entry)
{
   return Equipment.number(entry.equipmentStolen) > 0 || entry.isStolen == true;
};

// what finds a row again after the menu listed anew, the rows are new objects then
Equipment.rowKey = function(entry)
{
   return {formId:entry.formId, text:entry.text, stolen:Equipment.stolenRow(entry)};
};

Equipment.isRow = function(entry, key)
{
   return entry.formId == key.formId && entry.text == key.text && Equipment.stolenRow(entry) == key.stolen;
};

Equipment.fillStats = function(wide)
{
   var window = Equipment.window;
   var entry = Equipment.windowEntry();
   var lines = Equipment.settings.stats && entry != undefined ? Equipment.statsOf(entry) : [];
   var names = [];
   var values = [];
   var room = window.wide / 2 - Equipment.STATS_PAD - 4;
   var measure = function(words)
   {
      window.statNames.text = words;
      return window.statNames.textWidth;
   };
   var i = 0;
   window.statLines = lines;
   while(i < lines.length)
   {
      names.push(Equipment.cutToFit(lines[i][0], room, measure));
      values.push(Equipment.valueHtml(lines[i][1], lines[i][2]));
      i++;
   }
   window.statNames.text = names.join("\n");
   window.statValues.htmlText = values.join("<br>");
   window.statValues.setTextFormat(Equipment.lettersOnly(Equipment.STATS_TEXT, "right"));
   window.statNames._visible = window.statValues._visible = lines.length > 0;
   window.stolen.text = entry != undefined ? Equipment.stolenLine(entry) : "";
   window.stolen.textColor = Equipment.inkOf("stolen");
   window.stolen._visible = window.stolen.text != "";
   window.bodySlots.text = entry != undefined ? Equipment.quietLines(entry).join("\n") : "";
   window.bodySlots._visible = window.bodySlots.text != "";
};

// A value of the numbers block as html with its change in brackets after it in the ink of better or worse, or the
// value in that ink when the change has no words of its own
Equipment.valueHtml = function(value, change)
{
   var words = Equipment.htmlSafe(String(value));
   var plain = "<font color='" + Equipment.hex(Equipment.inkOf("value")) + "'>" + words + "</font>";
   if(change == undefined)
   {
      return plain;
   }
   if(change.text == undefined)
   {
      return "<font color='" + Equipment.hex(Equipment.inkOf(change.role)) + "'>" + words + "</font>";
   }
   return plain + " <font color='" + Equipment.hex(Equipment.inkOf(change.role)) + "'>" + Equipment.htmlSafe(Equipment.bracketed(change)) + "</font>";
};

Equipment.bracketed = function(change)
{
   return "(" + change.text + ")";
};

// Words cut short with "..." till they're no wider than wide as measure reads them. A long arrow's name in a line
// pushed the numbers out of sight
Equipment.cutToFit = function(words, wide, measure)
{
   var whole = String(words);
   var now = measure(whole);
   var keep;
   var cut;
   if(now <= wide)
   {
      return whole;
   }
   keep = Math.min(whole.length - 1, Math.ceil(whole.length * wide / now));
   cut = whole.substr(0, keep) + "...";
   while(keep > 1 && measure(cut) > wide)
   {
      keep--;
      cut = whole.substr(0, keep) + "...";
   }
   return cut;
};

// The panel's letters with no colour, html text loses the field's own font and this brings it back over the inks
Equipment.lettersOnly = function(size, align)
{
   var format = new TextFormat("$EverywhereMediumFont", size);
   format.align = align;
   return format;
};

// The quiet lines under the numbers: what a quest wants with the item, its body slots, how many the player has
// already when it lies in a chest or a shop, silver, and the plugin it comes from when that isn't the game itself.
// The quest's line has a switch of its own, the quiet tags are the rest
Equipment.quietLines = function(entry)
{
   var lines = [];
   var tags = Equipment.settings.quietTags;
   var slots = Equipment.settings.slotNumbers ? Equipment.bodySlotsLine(entry) : "";
   var quest = Equipment.settings.questLine ? Equipment.questLine(entry) : "";
   var carried = Equipment.number(entry.equipmentCarried);
   if(quest != "")
   {
      lines.push(quest);
   }
   if(slots != "")
   {
      lines.push(slots);
   }
   if(tags && carried > 0 && !Equipment.own(entry, Equipment.ownFlags()))
   {
      lines.push(Equipment.say("You have #", carried));
   }
   if(tags && Equipment.silver(entry))
   {
      lines.push(Equipment.say("Silver"));
   }
   if(tags && Equipment.settings.modOrigin && entry.equipmentOrigin != undefined && entry.equipmentOrigin != "")
   {
      lines.push(Equipment.say("From #", entry.equipmentOrigin));
   }
   return lines;
};

// for the test stand: the numbers of an item with their changes against the worn ones, and what they're weighed against
Equipment.wornArmorReport = function(worn)
{
   var told = [];
   var i = 0;
   while(worn != undefined && i < worn.armor.length)
   {
      told.push(worn.armor[i].text + " " + worn.armor[i].infoArmor + (worn.armor[i].outfit == true ? " outfit" : ""));
      i++;
   }
   return told.join(", ");
};

Equipment.statsReport = function(formId)
{
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var worn = Equipment.wornNow;
   var lines;
   var told;
   var i = 0;
   var j;
   while(i < entries.length)
   {
      if(entries[i].formId == Number(formId))
      {
         lines = Equipment.statsOf(entries[i]);
         told = [];
         j = 0;
         while(j < lines.length)
         {
            told.push(lines[j][0] + " " + lines[j][1] + (lines[j][2] != undefined ? " " + lines[j][2].role + (lines[j][2].text != undefined ? " " + Equipment.bracketed(lines[j][2]) : "") : ""));
            j++;
         }
         skse.Log("PANELTEST stats " + entries[i].text + " against " + (worn != undefined && worn.right != undefined ? worn.right.text : "nothing") + ": " + told.join(" | ") + (entries[i].formType == Equipment.ARMOR ? " || armor " + entries[i].infoArmor + " worn " + entries[i].equipState + ", the set " + Equipment.wornArmorReport(worn) : ""));
      }
      i++;
   }
};

// for the test stand: the quiet lines of every row of an item, the rows of both sides of a chest each
Equipment.quietReport = function(formId)
{
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var flags = Equipment.ownFlags();
   var i = 0;
   while(i < entries.length)
   {
      if(entries[i].formId == Number(formId))
      {
         skse.Log("PANELTEST quiet " + entries[i].text + (Equipment.own(entries[i], flags) ? " (own)" : " (theirs)") + ": " + Equipment.quietLines(entries[i]).join(" / ") + "; quest " + entries[i].equipmentQuest + " " + entries[i].equipmentQuestName);
      }
      i++;
   }
};

// for the test stand: what the plugin and the game say of the stolen copies of every row of an item, and its line
Equipment.stolenReport = function(formId)
{
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var i = 0;
   while(i < entries.length)
   {
      if(entries[i].formId == Number(formId))
      {
         skse.Log("PANELTEST stolen " + entries[i].text + " x" + entries[i].count + ": plugin " + entries[i].equipmentStolen + ", game " + entries[i].isStolen + ", line \"" + Equipment.stolenLine(entries[i]) + "\"");
      }
      i++;
   }
};

// for the test stand: the list scrolls so the row with this text is in sight, a few rows from the top
Equipment.scrollToRowNamed = function(text)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var i = 0;
   while(i < list.listEnumeration.size() && list.listEnumeration.at(i).text != String(text))
   {
      i++;
   }
   skse.Log("PANELTEST row " + text + " is at " + i + " of " + list.listEnumeration.size());
   if(i < list.listEnumeration.size())
   {
      list.scrollPosition = Math.max(0, Math.min(i - 3, list._maxScrollPosition));
      list.requestUpdate();
   }
};

// For the test stand: the list scrolls so the row of this item is in sight, a few rows from the top. In a follower's
// trade a scroll from here followed by a category switch left SkyUI's card on a row of the other side (a player's
// wheel never did, scene panel-fec scrolls by the wheel since)
Equipment.scrollToForm = function(formId)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var i = 0;
   while(i < list.listEnumeration.size() && list.listEnumeration.at(i).formId != Number(formId))
   {
      i++;
   }
   if(i < list.listEnumeration.size())
   {
      list.scrollPosition = Math.max(0, Math.min(i - 3, list._maxScrollPosition));
      list.requestUpdate();
   }
};

// for the test stand: every row a quest wants something with, and how many rows come from a mod
Equipment.questReport = function(note)
{
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var found = [];
   var mods = 0;
   var i = 0;
   while(i < entries.length)
   {
      if(Equipment.number(entries[i].equipmentQuest) > 0)
      {
         found.push(entries[i].text + " = " + Equipment.questLine(entries[i]));
      }
      mods += entries[i].equipmentOrigin != undefined ? 1 : 0;
      i++;
   }
   skse.Log("PANELTEST quests " + note + ": " + entries.length + " rows, " + mods + " from mods, " + found.length + " a quest wants: " + found.join(" | "));
};

// What the plugin found a quest wants with the item, see Quests.h of the plugin. Every line says first whether the
// item is still needed. A quest with no name of its own is named by the place of what it holds
Equipment.questLine = function(entry)
{
   var name = entry.equipmentQuestName != undefined ? String(entry.equipmentQuestName) : "";
   var place = entry.equipmentQuestPlace == true && name != "";
   switch(Equipment.number(entry.equipmentQuest))
   {
      case 1:
      case 5:
         return place ? Equipment.say("Still needed for a quest at #", name) : (name != "" ? Equipment.say("Still needed for #", name) : Equipment.say("Still needed for a quest"));
      case 2:
         return name != "" ? Equipment.say("Still needed, # follows it", name) : Equipment.say("Still needed, a quest follows it");
      case 3:
         return name != "" ? Equipment.say("Still needed, # asks for it", name) : Equipment.say("Still needed, a quest asks for it");
      case 4:
         return name != "" ? Equipment.say("No longer needed, # is done", name) : Equipment.say("No longer needed, its quest is done");
      case 6:
         return place ? Equipment.say("Needed later for a quest at #", name) : (name != "" ? Equipment.say("Needed later, # hasn't begun", name) : Equipment.say("Needed later for a quest not begun yet"));
      case 7:
         return place ? Equipment.say("No longer needed, the quest at # is over", name) : (name != "" ? Equipment.say("No longer needed, # is over", name) : Equipment.say("No longer needed, its quest is over"));
   }
   return "";
};

// the kinds of quest lines that keep an item, see questLine
Equipment.questNeeds = function(entry)
{
   var quest = Equipment.number(entry.equipmentQuest);
   return quest >= 1 && quest <= 3 || quest == 5 || quest == 6;
};

// silver weapons hurt the undead more, mods name their silver with a material keyword like the game's
Equipment.silver = function(entry)
{
   var word;
   for(var name in entry.keywords)
   {
      word = name.toLowerCase();
      if(word.indexOf("silver") >= 0 && word.indexOf("material") >= 0)
      {
         return true;
      }
   }
   return false;
};

Equipment.bodySlotsHeight = function()
{
   var line = Equipment.window.bodySlots;
   return line._visible ? line._height : 0;
};

Equipment.placeStats = function(window, top)
{
   var pad = Equipment.STATS_PAD;
   var high = window.statLines.length * Equipment.STATS_LINE + 4;
   window.statNames._x = pad;
   window.statValues._x = window.wide / 2;
   window.statNames._y = window.statValues._y = top + pad / 2;
   window.statNames._width = window.wide / 2 - pad;
   window.statValues._width = window.wide / 2 - pad;
   window.statNames._height = window.statValues._height = high;
   window.stolen._x = pad;
   window.stolen._y = top + Equipment.statsHeight() + Equipment.STOLEN_GAP / 2;
   Equipment.placeBodySlots(window, top + Equipment.statsHeight() + Equipment.stolenHeight());
};

// right under the numbers, a window of a row has the strip of the lens there and the line sits in it
Equipment.placeBodySlots = function(window, top)
{
   var line = window.bodySlots;
   var room = Equipment.anchorSlot != undefined ? line._height : Math.max(Equipment.LENS_SIZE, line._height);
   line._x = Equipment.STATS_PAD;
   line._y = top + (room - line._height) / 2;
};

// the game's body slots 30 to 61 a mask holds, bit n is slot 30 + n
Equipment.bodySlots = function(mask)
{
   var bits = Equipment.number(mask);
   var slots = [];
   var i = 0;
   while(i < 32)
   {
      if((bits & (1 << i)) != 0)
      {
         slots.push(Equipment.FIRST_BODY_SLOT + i);
      }
      i++;
   }
   return slots;
};

// Slots 32, 46 for a piece to wear. Anything else you wear or hold says it takes no body slot: weapons, torches,
// arrows and jewelry MARA took the slots off. A potion or a book says nothing
Equipment.bodySlotsLine = function(entry)
{
   var slots = entry.formType == Equipment.ARMOR ? Equipment.bodySlots(entry.partMask) : [];
   if(slots.length > 0)
   {
      return slots.length == 1 ? Equipment.say("Slot #", slots[0]) : Equipment.say("Slots #", slots.join(", "));
   }
   if(entry.formType == Equipment.ARMOR && entry.partMask == undefined)
   {
      return "";
   }
   return Equipment.has([Equipment.ARMOR, Equipment.WEAPON, Equipment.AMMO, Equipment.LIGHT], entry.formType) ? Equipment.say("No body slot") : "";
};

Equipment.fixed = function(value, digits)
{
   var power = Math.pow(10, digits);
   var text = String(Math.round(value * power) / power);
   if(digits > 0 && text.indexOf(".") < 0)
   {
      text += ".";
      while(digits > 0)
      {
         text += "0";
         digits--;
      }
   }
   return text;
};

// a time in seconds, the unit in the words of the language
Equipment.seconds = function(value)
{
   return Equipment.say("# s", value);
};

// a share of 1 as a percent with one digit
Equipment.percent = function(share)
{
   return Equipment.fixed(share * 100, 1) + "%";
};

Equipment.statsOf = function(entry)
{
   var told = Equipment.plugin() != undefined ? Equipment.plugin().Stats(entry.formId) : undefined;
   if(told == undefined)
   {
      return [];
   }
   if(entry.formType == Equipment.WEAPON && told.kind == "weapon")
   {
      return Equipment.weaponStats(entry, told);
   }
   if(entry.formType == Equipment.ARMOR && told.kind == "armor")
   {
      return Equipment.armorStats(entry, told);
   }
   return [];
};

Equipment.swingOf = function(entry)
{
   if(Equipment.isBow(entry))
   {
      return Equipment.SWING.bow;
   }
   if(Equipment.isCrossbow(entry))
   {
      return Equipment.SWING.crossbow;
   }
   return Equipment.twoHanded(entry) ? Equipment.SWING.twoHanded : Equipment.SWING.oneHanded;
};

// Worn by the one the panel shows. A body or a mannequin keeps its worn rows marked worn too, those are only things
// to take
Equipment.wornByShown = function(entry)
{
   return entry.outfit == true || Equipment.wornByPlayer(entry, Equipment.wearerFlags());
};

// The raw numbers of a weapon: when a blow lands, damage a second, reach, stagger and crit damage. A bow or
// crossbow counts the worn ammo it shoots with it. A staff has none
Equipment.weaponNumbers = function(entry, told)
{
   var worn = Equipment.wornNow;
   var swing = Equipment.swingOf(entry);
   var left = entry.equipState == Equipment.LEFT_HAND && Equipment.wornByShown(entry);
   var pace = Math.max(told.speed, 0.05) * Math.max(left ? told.leftSpeedMult : told.speedMult, 0.05);
   var damage = Equipment.number(entry.infoDamage);
   var ammo = worn != undefined && worn.ammo != undefined && Equipment.shoots(entry, worn.ammo) ? worn.ammo : undefined;
   if(entry.subType == Equipment.STAFF)
   {
      return undefined;
   }
   if(ammo != undefined)
   {
      damage += Equipment.number(ammo.infoDamage);
   }
   return {hit:swing.hit / pace, damage:damage, perSecond:damage / (swing.cycle / pace), reach:told.reach, stagger:told.stagger, critDamage:told.critDamage, ammo:ammo};
};

// What a weapon that isn't in the hands is weighed against: the one the row arrow weighs it against
Equipment.weaponAgainst = function(entry)
{
   var worn = Equipment.wornNow;
   var held = worn != undefined && !Equipment.wornByPlayer(entry, Equipment.wearerFlags()) ? Equipment.heldLike(entry, worn) : undefined;
   var told = held != undefined && Equipment.plugin() != undefined ? Equipment.plugin().Stats(held.formId) : undefined;
   return told != undefined && told.kind == "weapon" ? Equipment.weaponNumbers(held, told) : undefined;
};

// Swing time, damage a second and a minute of steady blows, reach, stagger and crit damage, each with how much
// it beats the weapon in the hands
Equipment.weaponStats = function(entry, told)
{
   var lines = [];
   var mine = Equipment.weaponNumbers(entry, told);
   var held;
   var sameShot;
   if(mine == undefined)
   {
      return lines;
   }
   held = Equipment.weaponAgainst(entry);
   // the worn arrows go with a bow only, a bow and a crossbow would count them on one side
   sameShot = held != undefined && (mine.ammo == undefined) == (held.ammo == undefined);
   lines.push([Equipment.ranged(entry) ? Equipment.say("Draw") : Equipment.say("Swing"), Equipment.seconds(Equipment.fixed(mine.hit, 2)), held != undefined ? Equipment.change(mine.hit - held.hit, 2, -1) : undefined]);
   if(mine.damage > 0)
   {
      lines.push([mine.ammo != undefined ? Equipment.say("DPS (with #)", mine.ammo.text) : Equipment.say("DPS"), "~" + Equipment.fixed(mine.perSecond, 1), sameShot ? Equipment.change(mine.perSecond - held.perSecond, 1, 1) : undefined]);
      lines.push([Equipment.say("DPM"), "~" + Math.round(mine.perSecond * 60), sameShot ? Equipment.change(Math.round(mine.perSecond * 60) - Math.round(held.perSecond * 60), 0, 1) : undefined]);
   }
   if(!Equipment.ranged(entry))
   {
      lines.push([Equipment.say("Reach"), Equipment.fixed(mine.reach, 2), held != undefined ? Equipment.change(mine.reach - held.reach, 2, 1) : undefined]);
   }
   lines.push([Equipment.say("Stagger"), Equipment.fixed(mine.stagger, 2), held != undefined ? Equipment.change(mine.stagger - held.stagger, 2, 1) : undefined]);
   if(mine.critDamage > 0 || held != undefined && held.critDamage > 0)
   {
      lines.push([Equipment.say("Crit damage"), String(mine.critDamage), held != undefined ? Equipment.change(mine.critDamage - held.critDamage, 0, 1) : undefined]);
   }
   return lines;
};

// How much a number beats the one of what's worn or held in its place, the third part of a line of numbers. Its
// ink says better or worse, more is better unless better is -1. Too little to show at those digits is equal.
// A change with no text colours the value itself, for a line whose value already is the change
Equipment.change = function(amount, digits, better)
{
   var text = Equipment.fixed(Math.abs(amount), digits);
   if(Number(text) == 0)
   {
      return {text:"=", role:"equal"};
   }
   return {text:(amount > 0 ? "+" : "-") + text, role:amount * better > 0 ? "better" : "worse"};
};

// light or heavy armor, shields too. Clothing and jewelry don't count as pieces of armor
Equipment.armorPiece = function(entry)
{
   return entry.weightClass == 0 || entry.weightClass == 1 ? 1 : 0;
};

// What share of a hit a piece stops. The game turns armor rating into damage stopped at a fixed rate, adds a fixed
// share for every worn piece of armor and stops at a cap, so a piece counts for less once the set is past the cap.
// A worn piece shows what it adds to the set, another one what it would add in place of the worn pieces it shares a slot with
Equipment.armorStats = function(entry, told)
{
   var lines = [];
   var worn = Equipment.wornNow;
   var scale = told.armorScale / 100;
   var base = told.armorBase;
   var cap = told.armorCap / 100;
   var rating = Equipment.number(entry.infoArmor);
   var piece = Equipment.armorPiece(entry);
   var total = 0;
   var pieces = 0;
   var replaced = 0;
   var replacedPieces = 0;
   var i = 0;
   var replacedWeight = 0;
   var before;
   var after;
   var tone;
   var weight = Equipment.number(entry.infoWeight);
   var on = Equipment.wornByShown(entry);
   while(worn != undefined && i < worn.armor.length)
   {
      total += Equipment.number(worn.armor[i].infoArmor);
      pieces += Equipment.armorPiece(worn.armor[i]);
      if(!on && (worn.armor[i].partMask & entry.partMask))
      {
         replaced += Equipment.number(worn.armor[i].infoArmor);
         replacedPieces += Equipment.armorPiece(worn.armor[i]);
         replacedWeight += Equipment.number(worn.armor[i].infoWeight);
      }
      i++;
   }
   if(on)
   {
      before = Math.min((total - rating) * scale + (pieces - piece) * base, cap);
      after = Math.min(total * scale + pieces * base, cap);
   }
   else
   {
      before = Math.min(total * scale + pieces * base, cap);
      after = Math.min((total - replaced + rating) * scale + (pieces - replacedPieces + piece) * base, cap);
   }
   if(rating <= 0 && replaced <= 0)
   {
      return lines;
   }
   // a piece that isn't worn already says its change against the worn ones, its ink says which way
   tone = on ? undefined : {role:Equipment.percent(after - before) == Equipment.percent(0) ? "equal" : (after > before ? "better" : "worse")};
   // a worse piece than the worn one says how much less it would stop
   lines.push([on ? Equipment.say("Stops") : Equipment.say("Would stop"),Equipment.say("# of a hit", Equipment.percent(after - before)), tone]);
   lines.push([Equipment.say("Whole set"), Equipment.say("#1 (cap #2)", Equipment.percent(after), Equipment.percent(cap))]);
   // a mod that lets armor stop every hit would make this endless, the line says nothing then
   if(after < 1)
   {
      lines.push([Equipment.say("Effective health"), (after - before >= 0 ? "+" : "") + Equipment.percent((1 - before) / (1 - after) - 1), tone]);
   }
   if(weight > 0 && rating > 0)
   {
      lines.push([Equipment.say("Armor per weight"), Equipment.fixed(rating / weight, 2), replacedWeight > 0 && replaced > 0 ? Equipment.change(rating / weight - replaced / replacedWeight, 2, 1) : undefined]);
   }
   return lines;
};
