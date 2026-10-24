Equipment.COMPANIONS_DEPTH = 16;
Equipment.COMPANION_GAP = 6;
Equipment.COMPANION_PAD = 10;
// names are cut no shorter than this, past that the farthest companions lose their tabs
Equipment.COMPANION_LEAST = 48;
// the tabs stand this much higher than the slots' top, nearer the screen's edge but not on it. The doll keeps its room
Equipment.COMPANION_LIFT = 14;

// Tabs over the doll for the player and each companion near enough to trade with, nearest first, the one the
// panel shows lit. A click shuts this menu of items and opens the other one: that companion's trade or the player's
// own inventory. Without a companion near there are no tabs. The tabs have a strip of their own at the top of the
// doll's room, the doll stands under it
Equipment.readCompanions = function()
{
   var plugin = Equipment.plugin();
   var told = plugin != undefined && plugin.Companions != undefined ? plugin.Companions() : undefined;
   var people = [{formId:0, name:Equipment.say("You"), player:true}];
   var i = 0;
   while(told != undefined && i < told.length)
   {
      people.push({formId:Number(told[i].formId), name:String(told[i].name), player:false});
      i++;
   }
   Equipment.companionPeople = people.length > 1 ? people : undefined;
   Equipment.companionsRoom = people.length > 1 ? Equipment.HERO_TAB_HIGH + 2 * Equipment.COMPANION_GAP : 0;
};

// the top of the doll's room, under the strip of the tabs when there are any
Equipment.dollTop = function()
{
   return Equipment.frame.top + Equipment.number(Equipment.companionsRoom);
};

Equipment.buildCompanions = function()
{
   var strip = Equipment.panel.createEmptyMovieClip("companions", Equipment.COMPANIONS_DEPTH);
   var people = Equipment.companionPeople;
   var i = 0;
   Equipment.companions = strip;
   strip._visible = false;
   if(people == undefined)
   {
      return undefined;
   }
   while(i < people.length)
   {
      Equipment.companionTab(strip, i, people[i]);
      i++;
   }
   strip.people = people;
   strip._visible = true;
   Equipment.placeCompanions();
};

Equipment.companionTab = function(strip, index, person)
{
   var tab = strip.createEmptyMovieClip("tab" + index, index);
   tab.createEmptyMovieClip("look", 0);
   tab.createTextField("touch", 1, 0, 0, 10, 10);
   tab.touch.selectable = false;
   Equipment.writeLine(tab, "label", 2, Equipment.shortName(person.name).toUpperCase(), Equipment.HERO_TAB_TEXT, Equipment.PARCHMENT);
   tab.name = tab.label.text;
   tab.nameWide = tab.label._width;
   tab.person = person;
   tab.shown = Equipment.companionShown(person);
   tab.onRollOver = function()
   {
      this.lit = true;
      Equipment.drawCompanionTab(this);
   };
   tab.onRollOut = tab.onReleaseOutside = function()
   {
      this.lit = false;
      Equipment.drawCompanionTab(this);
   };
   tab.onRelease = function()
   {
      Equipment.switchCompanion(this.person);
   };
   return tab;
};

// the one the panel shows now: the player in the inventory, the one traded with in a trade
Equipment.companionShown = function(person)
{
   var subject = Equipment.subject;
   if(person.player)
   {
      return subject == undefined || subject.player != false;
   }
   return subject != undefined && subject.player == false && Number(subject.formId) == person.formId;
};

// The inventory leaves the way SkyUI leaves it for the magic menu: the list's place kept for the next time and the
// Tab menu under it shut, the trade would close back into it
Equipment.switchCompanion = function(person)
{
   var plugin = Equipment.plugin();
   var inventory = Equipment.kind == "inventory";
   if(Equipment.companionShown(person) || plugin == undefined || plugin.SwitchTo == undefined)
   {
      return undefined;
   }
   if(inventory)
   {
      Equipment.menu.saveIndices();
   }
   if(plugin.SwitchTo(person.formId, inventory) == true && inventory)
   {
      gfx.io.GameDelegate.call("CloseTweenMenu", []);
   }
};

// A row in the middle of its strip over the doll, no wider than the doll's room so it never lies over the slots.
// Long names are cut first, then the farthest companions lose their tabs. The player's tab always stays
Equipment.placeCompanions = function()
{
   var strip = Equipment.companions;
   var stage = Equipment.stage;
   var widths = [];
   var most;
   var x = 0;
   var i = 0;
   var tab;
   if(strip == undefined || !strip._visible)
   {
      return undefined;
   }
   while(strip["tab" + i] != undefined)
   {
      widths.push(strip["tab" + i].nameWide);
      i++;
   }
   most = Equipment.namesRoom(widths, stage.touch._width);
   while(widths.length > 2 && most < Equipment.COMPANION_LEAST)
   {
      widths.pop();
      most = Equipment.namesRoom(widths, stage.touch._width);
   }
   i = 0;
   while(strip["tab" + i] != undefined)
   {
      tab = strip["tab" + i];
      tab._visible = i < widths.length;
      Equipment.nameTab(tab, most);
      Equipment.drawCompanionTab(tab);
      tab._x = x;
      x += tab._visible ? tab.wide + Equipment.COMPANION_GAP : 0;
      i++;
   }
   strip._x = Math.round(stage._x + (stage.touch._width - (x - Equipment.COMPANION_GAP)) / 2);
   strip._y = Math.round(Equipment.frame.top + Equipment.COMPANION_GAP - Equipment.COMPANION_LIFT);
};

// How wide a name may be on each tab for the row to fit in room: the short names keep their width, the long ones
// share what's left. No limit when they all fit
Equipment.namesRoom = function(widths, room)
{
   var sorted = widths.slice().sort(function(a, b)
   {
      return a - b;
   });
   var free = room + Equipment.COMPANION_GAP - sorted.length * (2 * Equipment.COMPANION_PAD + Equipment.COMPANION_GAP);
   var i = 0;
   while(i < sorted.length)
   {
      if(sorted[i] * (sorted.length - i) > free)
      {
         return free / (sorted.length - i);
      }
      free -= sorted[i];
      i++;
   }
   return Infinity;
};

Equipment.nameTab = function(tab, most)
{
   tab.label.text = Equipment.cutToFit(tab.name, most, function(words)
   {
      tab.label.text = words;
      return tab.label._width;
   });
};

Equipment.drawCompanionTab = function(tab)
{
   var high = Equipment.HERO_TAB_HIGH;
   var lit = tab.lit || tab.shown;
   tab.wide = Math.round(tab.label._width + 2 * Equipment.COMPANION_PAD);
   tab.touch._width = tab.wide;
   tab.touch._height = high;
   tab.label._x = Equipment.COMPANION_PAD;
   tab.label._y = Math.round((high - tab.label._height) / 2);
   tab.label.textColor = lit ? Equipment.GOLD : Equipment.PARCHMENT;
   tab.look.clear();
   tab.look.lineStyle(1, Equipment.GOLD, tab.shown ? 100 : (tab.lit ? 90 : 60), true);
   tab.look.beginFill(0, tab.shown ? 70 : (tab.lit ? 62 : 42));
   Equipment.roundBox(tab.look, 0, 0, tab.wide, high, 5);
   tab.look.endFill();
};

// for the test stand: the pointer's place over the tab of this actor, -1 when there's none
Equipment.aimCompanion = function(formId)
{
   var strip = Equipment.companions;
   var i = 0;
   Equipment.aimedX = Equipment.aimedY = -1;
   while(strip != undefined && strip["tab" + i] != undefined)
   {
      if(strip["tab" + i].person.formId == Number(formId))
      {
         Equipment.aimClip("equipmentPanel.companions.tab" + i);
         return undefined;
      }
      i++;
   }
};

// for the test stand: the tabs and which one is lit
Equipment.companionsReport = function(note)
{
   var strip = Equipment.companions;
   var told = [];
   var i = 0;
   while(strip != undefined && strip["tab" + i] != undefined)
   {
      told.push(strip["tab" + i].label.text + (strip["tab" + i].shown ? " (shown)" : "") + (strip["tab" + i]._visible ? "" : " (no room)") + " " + strip["tab" + i].person.formId);
      i++;
   }
   skse.Log("PANELTEST companions " + note + ": " + (strip != undefined && strip._visible ? told.join(", ") : "no tabs"));
};
