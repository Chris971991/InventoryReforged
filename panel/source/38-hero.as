Equipment.HERO_PAD = 12;
Equipment.HERO_ROW = 19;
Equipment.HERO_POOL_ROW = 23;
Equipment.HERO_GUTTER = 20;
Equipment.HERO_TWO_COLUMNS = 400;
Equipment.HERO_STEP = 20;
Equipment.HERO_DEPTH = 15;
Equipment.HERO_WIDE_MOST = 520;
Equipment.HERO_INSET = 4;
Equipment.HERO_SHADE = 84;
Equipment.HERO_TAB_HIGH = 20;
Equipment.HERO_TAB_TEXT = 12;
Equipment.HERO_LEAST = 80;
Equipment.HERO_WHEEL = 2;
Equipment.HERO_BAR = 4;
Equipment.HERO_DIM = 0x9C978B;
Equipment.HERO_FAINT = 0x6E6A62;
Equipment.HERO_HEAVY = 0xD8664F;
Equipment.HEALTH_COLOR = 0xA8342A;
Equipment.MAGICKA_COLOR = 0x2F66AE;
Equipment.STAMINA_COLOR = 0x3F8C45;
Equipment.ARMOR_COLOR = 0x8E9DA6;
// the type the game gives a disease among its spells
Equipment.DISEASE = 1;
Equipment.METRES_OF_UNIT = 0.01428;
Equipment.HERO_ICON = 13;
Equipment.SCHOOLS = ["Alteration", "Conjuration", "Destruction", "Illusion", "Restoration"];
// what a share on a skill does, as the game's own effects word it
Equipment.SKILL_WORDS = [["One-handed", Equipment.say("#% damage")], ["Two-handed", Equipment.say("#% damage")], ["Archery", Equipment.say("#% damage")], ["Block", Equipment.say("#% blocking")], ["Smithing", Equipment.say("#% improving")], ["Pickpocket", Equipment.say("#% to steal")], ["Lockpicking", Equipment.say("#% easier")], ["Sneak", Equipment.say("#% stealth")], ["Alchemy", Equipment.say("#% potions")], ["Speech", Equipment.say("#% prices")], ["Enchanting", Equipment.say("#% enchanting")]];

// The character drawer. A small see-through tab at the foot of the doll pulls the player's numbers up over the doll.
// A click on the tab opens or shuts it, dragging the tab sets how far it comes out, the wheel and the bar at its side
// scroll it. It stays the way the player left it, the next time the menu opens too, until the tab pushes it back in.
// A press on the numbers goes through to the doll, so the doll still turns under them. Nothing is counted while it's in
Equipment.buildHero = function()
{
   var drawer = Equipment.panel.createEmptyMovieClip("hero", Equipment.HERO_DEPTH);
   var told = Equipment.plugin() != undefined ? Equipment.plugin().HeroDrawer() : undefined;
   var bar;
   var tab;
   drawer.createEmptyMovieClip("back", 0);
   drawer.createEmptyMovieClip("rows", 1);
   drawer.createEmptyMovieClip("view", 2);
   drawer.rows.setMask(drawer.view);
   // The bar is only drawn, the mouse listener below takes and lets go of it. As a button of its own it kept the
   // press after a drag, the next press anywhere went to it and the doll didn't turn
   bar = drawer.createEmptyMovieClip("bar", 3);
   bar.createEmptyMovieClip("look", 1);
   tab = drawer.createEmptyMovieClip("tab", 4);
   tab.createTextField("touch", 0, 0, 0, 10, 10);
   tab.touch.selectable = false;
   tab.createEmptyMovieClip("look", 1);
   Equipment.writeLine(tab, "label", 2, Equipment.say("Character").toUpperCase(), Equipment.HERO_TAB_TEXT, Equipment.PARCHMENT);
   tab.onRollOver = function()
   {
      this.lit = true;
      Equipment.drawHeroTab();
   };
   tab.onRollOut = function()
   {
      this.lit = false;
      Equipment.drawHeroTab();
   };
   tab.onPress = function()
   {
      Equipment.grabHero();
   };
   tab.onRelease = function()
   {
      Equipment.letHeroGo();
   };
   tab.onReleaseOutside = function()
   {
      this.lit = false;
      Equipment.letHeroGo();
   };
   tab.onMouseMove = function()
   {
      Equipment.pullHero();
   };
   tab.onPressAux = function()
   {
   };
   Equipment.hero = drawer;
   Equipment.heroState = {open:told.open == true, high:Equipment.number(told.high), first:Equipment.number(told.first)};
   Equipment.heroMouse = {};
   Equipment.heroMouse.onMouseWheel = function(delta)
   {
      var overStrip = Equipment.pieceStrip != undefined && Equipment.inPieces(_root._xmouse, _root._ymouse);
      if(Equipment.hero.high > 0 && !overStrip && Equipment.inHero(_root._xmouse, _root._ymouse))
      {
         Equipment.scrollHero(delta > 0 ? -Equipment.HERO_WHEEL : Equipment.HERO_WHEEL);
      }
   };
   Equipment.heroMouse.onMouseDown = function()
   {
      if(Equipment.onHeroBar(_root._xmouse, _root._ymouse))
      {
         Equipment.heroBarHeld = true;
         Equipment.dragHeroBar();
      }
   };
   Equipment.heroMouse.onMouseMove = function()
   {
      if(Equipment.heroBarHeld)
      {
         Equipment.dragHeroBar();
      }
   };
   Equipment.heroMouse.onMouseUp = function()
   {
      if(Equipment.heroBarHeld)
      {
         Equipment.heroBarHeld = false;
         Equipment.drawHeroBar();
      }
   };
   Mouse.addListener(Equipment.heroMouse);
   // the game tells the menu the player's numbers after a change, an open drawer follows them
   var bottom = Equipment.menu.bottomBar;
   bottom.equipmentUpdatePlayerInfo = bottom.UpdatePlayerInfo;
   bottom.UpdatePlayerInfo = function(a_player, a_item)
   {
      this.equipmentUpdatePlayerInfo(a_player, a_item);
      Equipment.refreshHero();
   };
   Equipment.placeHero();
};

// The drawer stands on the foot of the doll's room, as wide as the room up to a measure where a line still reads
// as one, and comes out at most until its tab meets the top of the room
Equipment.placeHero = function()
{
   var drawer = Equipment.hero;
   var stage = Equipment.stage;
   drawer._visible = Equipment.settings.heroStats && Equipment.plugin() != undefined;
   drawer.wide = Math.round(Math.min(stage.touch._width - 2 * Equipment.HERO_INSET, Equipment.HERO_WIDE_MOST));
   drawer._x = Math.round(stage._x + (stage.touch._width - drawer.wide) / 2);
   drawer._y = Math.round(stage._y + stage.touch._height);
   drawer.room = stage.touch._height - Equipment.HERO_TAB_HIGH - Equipment.HERO_INSET;
   drawer.high = 0;
   if(drawer._visible && Equipment.heroState.open)
   {
      Equipment.showHero();
   }
   else
   {
      Equipment.drawHero(0);
   }
};

Equipment.wearerInfo = function(told)
{
   var worn = Equipment.wornNow;
   var armor = 0;
   var i = 0;
   while(worn != undefined && i < worn.armor.length)
   {
      armor += Equipment.number(worn.armor[i].infoArmor);
      i++;
   }
   return {encumbrance:told.carried, maxEncumbrance:told.most, armor:armor, damage:worn != undefined && worn.right != undefined ? Equipment.number(worn.right.infoDamage) : undefined};
};

Equipment.refreshHero = function()
{
   if(Equipment.hero._visible && Equipment.heroState.open && Equipment.heroGrab == undefined)
   {
      Equipment.showHero();
   }
};

// the numbers counted anew, then the drawer as far out as the player left it
Equipment.showHero = function()
{
   Equipment.countHero();
   Equipment.drawHero(Equipment.heroHigh(Equipment.heroState.high));
};

// The sheet drawn anew: the level over all, the three pools with their bars across, then pairs of groups side by
// side when the drawer is wide enough or one under the other, and what raises the skills when anything does
Equipment.countHero = function()
{
   var drawer = Equipment.hero;
   var told = Equipment.plugin().HeroStats();
   if(told != undefined)
   {
      told.hands = Equipment.heroHands(Equipment.wornNow);
   }
   // the bottom bar counts the player's, a follower's load comes from the plugin and the armor and damage from the
   // numbers the list shows on what they wear
   var info = Equipment.dressesOther() && told != undefined ? Equipment.wearerInfo(told) : Equipment.menu.bottomBar._playerInfoObj;
   var sheet = Equipment.heroSheet(told, info, Equipment.wornNow);
   var page = drawer.rows.createEmptyMovieClip("page", 0);
   var pad = Equipment.HERO_PAD;
   var left = pad;
   var right = drawer.wide - pad - Equipment.HERO_BAR - 2;
   var y = pad - 2;
   var i = 0;
   page.next = 0;
   if(sheet == undefined)
   {
      drawer.full = Equipment.heroLeast();
      return undefined;
   }
   y += Equipment.drawLevel(page, sheet, left, y, right);
   while(i < sheet.pools.length)
   {
      y += Equipment.drawPool(page, sheet.pools[i], left, y, right);
      i++;
   }
   y += Equipment.heroGap(page, left, y, right);
   y += Equipment.heroPair(page, left, y, right, function(x, top, wide)
   {
      return Equipment.drawDefense(page, sheet, x, top, wide);
   }, function(x, top, wide)
   {
      return Equipment.drawRows(page, Equipment.say("Offense"), sheet.offense, x, top, wide);
   });
   y += Equipment.heroGap(page, left, y, right);
   y += Equipment.heroPair(page, left, y, right, function(x, top, wide)
   {
      return Equipment.drawRows(page, Equipment.say("Movement and load"), sheet.movement, x, top, wide);
   }, function(x, top, wide)
   {
      return Equipment.drawRows(page, Equipment.say("Other"), sheet.other, x, top, wide);
   });
   if(sheet.skills.length > 0)
   {
      y += Equipment.heroGap(page, left, y, right);
      y += Equipment.drawSkills(page, sheet.skills, left, y, right);
   }
   if(sheet.spells.length > 0)
   {
      y += Equipment.heroGap(page, left, y, right);
      y += Equipment.drawNotes(page, Equipment.say("Spells in hand"), sheet.spells, left, y, right);
   }
   if(sheet.statuses.length > 0)
   {
      y += Equipment.heroGap(page, left, y, right);
      y += Equipment.drawNotes(page, Equipment.say("Status"), sheet.statuses, left, y, right);
   }
   drawer.full = Math.round(y + pad);
};

// Every note as its name, a few words on the right, and under them what it does in the game's words
Equipment.drawNotes = function(page, head, statuses, left, y, right)
{
   var row = Equipment.HERO_ROW;
   var top = y;
   var status;
   var words;
   var i = 0;
   y += Equipment.heroHead(page, head, left, y, right - left);
   while(i < statuses.length)
   {
      status = statuses[i];
      Equipment.heroText(page, status.name, left, y, row, 14, status.color, "left");
      if(status.aside != "")
      {
         Equipment.heroText(page, status.aside, right, y, row, 13, Equipment.WHITE, "right");
      }
      y += row;
      if(status.text != "")
      {
         words = Equipment.heroWrapped(page, status.text, left + 10, y - 3, right - left - 10, 12, Equipment.HERO_DIM);
         y += words.textHeight;
      }
      y += 6;
      i++;
   }
   return y - top;
};

// text that wraps at a width and is as high as its lines
Equipment.heroWrapped = function(page, words, x, y, wide, size, color)
{
   var field = Equipment.heroField(page, x, y, Equipment.HERO_ROW, size, color);
   field.multiline = field.wordWrap = true;
   field._width = wide;
   field.text = String(words);
   field._x = Math.round(x);
   field._y = Math.round(y);
   return field;
};

// a thin rule between two parts of the sheet with a little air on both sides
Equipment.heroGap = function(page, left, y, right)
{
   Equipment.heroRule(page, left, y + 6, right - left, 14);
   return 15;
};

// Two groups side by side in a wide drawer, the second under the first in a narrow one. Each is drawn by a
// function of its left, top and width that tells how high it came out
Equipment.heroPair = function(page, left, y, right, first, second)
{
   var two = right - left + 2 * Equipment.HERO_PAD >= Equipment.HERO_TWO_COLUMNS;
   var column = two ? (right - left - Equipment.HERO_GUTTER) / 2 : right - left;
   var high = first(left, y, column);
   if(two)
   {
      return Math.max(high, second(left + column + Equipment.HERO_GUTTER, y, column));
   }
   return high + 8 + second(left, y + high + 8, column);
};

// the level in gold, a thin gold bar of the way to the next and how far it is
Equipment.drawLevel = function(page, sheet, left, y, right)
{
   var row = Equipment.HERO_POOL_ROW;
   var level = Equipment.say("Level #", sheet.level);
   // a follower's sheet says whose it is
   var name = Equipment.heroText(page, (Equipment.dressesOther() ? Equipment.gameName(Equipment.subject.name) + "   " + level : level).toUpperCase(), left, y, row, 14, Equipment.GOLD, "left");
   var start = left + Math.max(64, name._width + 10);
   var note;
   if(sheet.toNext == undefined)
   {
      return row;
   }
   note = Equipment.heroText(page, Equipment.say("#1% to #2", Equipment.fixed(sheet.toNext * 100, 0), sheet.level + 1), right, y, row, 12, Equipment.HERO_DIM, "right");
   Equipment.heroBar(page, start, y + (row - 5) / 2, note._x - 10 - start, 5, sheet.toNext, Equipment.GOLD);
   return row + 2;
};

// A pool on one line: its name, a bar in its colour, what's left of the most and how fast it comes back.
// A narrow drawer puts the pace under the bar
Equipment.drawPool = function(page, pool, left, y, right)
{
   var row = Equipment.HERO_POOL_ROW;
   var name = 64;
   var value = 70;
   var note = 150;
   var under = right - left - name - value - note - 24 < 80;
   var barRight = right - value - 10 - (under ? 0 : note + 6);
   Equipment.heroText(page, pool.name, left, y, row, 14, Equipment.PARCHMENT, "left");
   Equipment.heroBar(page, left + name, y + (row - 8) / 2, barRight - left - name, 8, pool.now / pool.most, pool.color);
   Equipment.heroText(page, Math.round(pool.now) + " / " + Math.round(pool.most), barRight + 10 + value, y, row, 15, Equipment.WHITE, "right");
   if(under)
   {
      Equipment.heroText(page, pool.note, left + name, y + row - 3, Equipment.HERO_ROW, 12, Equipment.HERO_DIM, "left");
      return row + Equipment.HERO_ROW - 3;
   }
   Equipment.heroText(page, pool.note, right - note, y, row, 12, Equipment.HERO_DIM, "left");
   return row;
};

// The armor rating and the share of a hit it stops with a bar up to the game's cap, the six resistances in pairs,
// a zero faint, one past its cap in gold
Equipment.drawDefense = function(page, sheet, left, y, wide)
{
   var row = Equipment.HERO_ROW;
   var top = y;
   var cell = (wide - 14) / 2;
   var share;
   var resist;
   var x;
   var i = 0;
   y += Equipment.heroHead(page, Equipment.say("Defense"), left, y, wide);
   if(sheet.armor != undefined)
   {
      Equipment.heroText(page, Equipment.say("Armor rating"), left, y, row, 14, Equipment.PARCHMENT, "left");
      Equipment.heroText(page, String(sheet.armor.rating), left + wide, y, row, 15, Equipment.WHITE, "right");
      y += row;
      share = sheet.armor.cap > 0 ? sheet.armor.stops / sheet.armor.cap : 0;
      Equipment.heroText(page, Equipment.say("stops # of a hit", Equipment.percent(sheet.armor.stops)), left, y, row, 12, Equipment.HERO_DIM, "left");
      Equipment.heroBar(page, left + wide - Math.min(90, wide * 0.36), y + (row - 5) / 2, Math.min(90, wide * 0.36), 5, share, Equipment.ARMOR_COLOR);
      y += row + 2;
   }
   while(i < sheet.resists.length)
   {
      resist = sheet.resists[i];
      x = left + (i % 2) * (cell + 14);
      Equipment.heroText(page, resist.name, x, y, row, 13, resist.zero ? Equipment.HERO_FAINT : Equipment.PARCHMENT, "left");
      Equipment.heroText(page, (resist.capped ? Equipment.say("# max", resist.value) : resist.value), x + cell, y, row, 14, resist.capped ? Equipment.GOLD : (resist.zero ? Equipment.HERO_FAINT : Equipment.WHITE), "right");
      if(i % 2 == 1)
      {
         y += row;
      }
      i++;
   }
   return y - top;
};

// A group of lines, a name on the left and its value on the right. A minor line is dim, a line can bring its own
// colour for the value
Equipment.drawRows = function(page, head, rows, left, y, wide)
{
   var row = Equipment.HERO_ROW;
   var top = y;
   var line;
   var value;
   var i = 0;
   if(rows.length == 0)
   {
      return 0;
   }
   y += Equipment.heroHead(page, head, left, y, wide);
   while(i < rows.length)
   {
      line = rows[i];
      Equipment.heroText(page, line.name, left, y, row, line.minor ? 13 : 14, line.minor ? Equipment.HERO_DIM : Equipment.PARCHMENT, "left");
      value = Equipment.heroText(page, line.value, left + wide, y, row, line.minor ? 14 : 15, line.color != undefined ? line.color : (line.minor ? Equipment.HERO_DIM : Equipment.WHITE), "right");
      if(line.icon != undefined)
      {
         Equipment.drawIcon(page, line.icon, value._x - Equipment.HERO_ICON - 4, y + (row - Equipment.HERO_ICON) / 2, Equipment.HERO_ICON, Equipment.HERO_DIM, 100);
      }
      y += row;
      i++;
   }
   return y - top;
};

// What raises the skills now, two to a line in a wide drawer
Equipment.drawSkills = function(page, skills, left, y, right)
{
   var row = Equipment.HERO_ROW;
   var top = y;
   var two = right - left + 2 * Equipment.HERO_PAD >= Equipment.HERO_TWO_COLUMNS;
   var cell = two ? (right - left - Equipment.HERO_GUTTER) / 2 : right - left;
   var x;
   var i = 0;
   y += Equipment.heroHead(page, Equipment.say("Skill bonuses"), left, y, right - left);
   while(i < skills.length)
   {
      x = two && i % 2 == 1 ? left + cell + Equipment.HERO_GUTTER : left;
      Equipment.heroText(page, skills[i].name, x, y, row, 14, Equipment.PARCHMENT, "left");
      Equipment.heroText(page, skills[i].value, x + cell, y, row, 14, Equipment.WHITE, "right");
      if(!two || i % 2 == 1 || i == skills.length - 1)
      {
         y += row;
      }
      i++;
   }
   return y - top;
};

// the name of a group in small gold capitals over a thin gold line
Equipment.heroHead = function(page, name, left, y, wide)
{
   Equipment.heroText(page, name.toUpperCase(), left, y, 18, 12, Equipment.GOLD, "left");
   page.lineStyle(1, Equipment.GOLD, 35, true);
   page.moveTo(left, Math.round(y + 18) + 0.5);
   page.lineTo(left + wide, Math.round(y + 18) + 0.5);
   page.lineStyle();
   return 22;
};

Equipment.heroRule = function(page, left, y, wide, alpha)
{
   page.lineStyle(1, Equipment.WHITE, alpha, true);
   page.moveTo(left, Math.round(y) + 0.5);
   page.lineTo(left + wide, Math.round(y) + 0.5);
   page.lineStyle();
};

// one piece of text in the middle of a row, from x to the right, to the left of x or around it
Equipment.heroText = function(page, words, x, y, row, size, color, align)
{
   var field = Equipment.heroField(page, x, y, row, size, color);
   field.text = String(words);
   return Equipment.alignHero(field, x, y, row, align);
};

Equipment.heroField = function(page, x, y, row, size, color)
{
   var name = "t" + page.next;
   page.createTextField(name, page.next, x, y, 10, row);
   page.next++;
   Equipment.styleText(page[name], size, "left", color);
   page[name].autoSize = "left";
   return page[name];
};

Equipment.alignHero = function(field, x, y, row, align)
{
   field._x = Math.round(align == "right" ? x - field._width : (align == "center" ? x - field._width / 2 : x));
   field._y = Math.round(y + (row - field._height) / 2);
   return field;
};

// a dark track with a light edge, the filled part in its colour with a lighter top
Equipment.heroBar = function(page, x, y, wide, high, share, color)
{
   // a pool of 0 of 0 gives a share of NaN, and Math.max and Math.min hand NaN on
   var fill = share > 0 ? Math.min(1, share) * (wide - 2) : 0;
   var turn = new flash.geom.Matrix();
   page.lineStyle(1, Equipment.WHITE, 22, true);
   page.beginFill(0, 55);
   Equipment.roundBox(page, x, y, wide, high, 2);
   page.endFill();
   page.lineStyle();
   if(fill > 2)
   {
      turn.createGradientBox(fill, high - 2, Math.PI / 2, x + 1, y + 1);
      page.beginGradientFill("linear", [Equipment.blendColor(color, Equipment.WHITE, 0.35), color], [100, 100], [0, 255], turn);
      Equipment.roundBox(page, x + 1, y + 1, fill, high - 2, 1.5);
      page.endFill();
   }
};

Equipment.blendColor = function(color, toward, share)
{
   var mix = function(shift)
   {
      var a = (color >> shift) & 255;
      var b = (toward >> shift) & 255;
      return Math.round(a + (b - a) * share) << shift;
   };
   return mix(16) | mix(8) | mix(0);
};

// The drawer out by this many pixels over its foot, 0 leaves only the tab
Equipment.drawHero = function(high)
{
   var drawer = Equipment.hero;
   drawer.high = high;
   drawer.back.clear();
   drawer.view.clear();
   drawer.rows._visible = high > 0;
   if(high > 0)
   {
      drawer.back._y = -high;
      Equipment.panelBox(drawer.back, drawer.wide, high, Equipment.HERO_SHADE, 45);
      drawer.view.beginFill(0, 100);
      Equipment.roundBox(drawer.view, 0, 3 - high, drawer.wide, high - 6, 0);
      drawer.view.endFill();
   }
   Equipment.scrollHero(0);
   Equipment.drawHeroTab();
};

// a little see-through box with a clear edge, its chevron tells which way the drawer goes on a click
Equipment.drawHeroTab = function()
{
   var drawer = Equipment.hero;
   var tab = drawer.tab;
   var label = tab.label;
   var high = Equipment.HERO_TAB_HIGH;
   var wide = Math.round(label._width + 34);
   var out = drawer.high > 0;
   var lit = tab.lit || Equipment.heroGrab != undefined;
   var tip = out ? high / 2 + 3 : high / 2 - 3;
   var back = out ? high / 2 - 2 : high / 2 + 2;
   tab._x = Math.round((drawer.wide - wide) / 2);
   tab._y = Math.round(-drawer.high - high + (out ? 1 : 0));
   tab.touch._width = wide;
   tab.touch._height = high;
   label._x = 24;
   label._y = Math.round((high - label._height) / 2);
   label.textColor = lit ? Equipment.GOLD : Equipment.PARCHMENT;
   tab.look.clear();
   tab.look.lineStyle(1, Equipment.GOLD, lit ? 100 : 80, true);
   tab.look.beginFill(0, lit ? 62 : 42);
   Equipment.roundBox(tab.look, 0, 0, wide, high, 5);
   tab.look.endFill();
   tab.look.lineStyle(1.6, Equipment.GOLD, lit ? 100 : 85, true, "normal", "round", "round");
   tab.look.moveTo(8, back);
   tab.look.lineTo(13, tip);
   tab.look.lineTo(18, back);
   tab.look.lineStyle();
};

// The sheet moves by steps, the bar at the side shows where it is. Only a scroll of the player moves the place
// that's kept, a sheet that comes out shorter for a while shows what fits of it and comes back to the place
Equipment.scrollHero = function(by)
{
   var drawer = Equipment.hero;
   var state = Equipment.heroState;
   if(drawer.high <= 0)
   {
      drawer.bar._visible = false;
      return undefined;
   }
   drawer.most = Math.max(0, Math.ceil((drawer.full - drawer.high) / Equipment.HERO_STEP));
   drawer.first = Math.max(0, Math.min(Math.min(state.first, drawer.most) + by, drawer.most));
   drawer.rows._y = -drawer.high - Math.min(drawer.first * Equipment.HERO_STEP, drawer.full - drawer.high);
   if(by != 0 && drawer.first != state.first)
   {
      state.first = drawer.first;
      Equipment.keepHero();
   }
   Equipment.drawHeroBar();
};

Equipment.drawHeroBar = function()
{
   var drawer = Equipment.hero;
   var bar = drawer.bar;
   var wide = Equipment.HERO_BAR;
   var at;
   bar.look.clear();
   bar._visible = drawer.high > 0 && drawer.most > 0;
   if(!bar._visible)
   {
      return undefined;
   }
   bar.top = Equipment.HERO_PAD - drawer.high;
   bar.track = drawer.high - 2 * Equipment.HERO_PAD;
   bar.thumb = Math.max(16, bar.track * drawer.high / drawer.full);
   bar._x = drawer.wide - wide - 5;
   at = bar.top + (bar.track - bar.thumb) * drawer.first / drawer.most;
   bar.look.beginFill(Equipment.WHITE, 12);
   Equipment.roundBox(bar.look, 0, bar.top, wide, bar.track, 2);
   bar.look.endFill();
   bar.look.beginFill(Equipment.GOLD, Equipment.heroBarHeld ? 95 : 70);
   Equipment.roundBox(bar.look, 0, at, wide, bar.thumb, 2);
   bar.look.endFill();
};

// the thumb of the bar follows the pointer
Equipment.dragHeroBar = function()
{
   var drawer = Equipment.hero;
   var bar = drawer.bar;
   var point = Equipment.heroPointer();
   var share;
   if(drawer.most <= 0)
   {
      return undefined;
   }
   share = (point.y - drawer._y - bar.top - bar.thumb / 2) / (bar.track - bar.thumb);
   Equipment.scrollHero(Math.round(Math.max(0, Math.min(1, share)) * drawer.most) - drawer.first);
   Equipment.drawHeroBar();
};

Equipment.grabHero = function()
{
   var drawer = Equipment.hero;
   if(!Equipment.listsAnswer())
   {
      return undefined;
   }
   // pulled from in, the numbers are counted for the pull
   if(drawer.high <= 0)
   {
      Equipment.countHero();
   }
   Equipment.heroGrab = {y:Equipment.heroPointer().y, from:drawer.high, moved:false};
   Equipment.drawHeroTab();
};

Equipment.pullHero = function()
{
   var grab = Equipment.heroGrab;
   var drawer = Equipment.hero;
   var y;
   if(grab == undefined)
   {
      return undefined;
   }
   y = Equipment.heroPointer().y;
   if(!grab.moved && Math.abs(y - grab.y) <= Equipment.CLICK_SLACK)
   {
      return undefined;
   }
   grab.moved = true;
   Equipment.drawHero(Math.max(0, Math.min(grab.from + grab.y - y, drawer.room, drawer.full)));
};

// A click opens the drawer as far as it was out last or pushes it in. A pull leaves it where it's let go, and pulled
// down to less than half its least it goes in
Equipment.letHeroGo = function()
{
   var grab = Equipment.heroGrab;
   var state = Equipment.heroState;
   var drawer = Equipment.hero;
   if(grab == undefined)
   {
      return undefined;
   }
   Equipment.heroGrab = undefined;
   if(!grab.moved)
   {
      state.open = !state.open;
   }
   else if(drawer.high < Equipment.heroLeast() / 2)
   {
      state.open = false;
   }
   else
   {
      state.open = true;
      state.high = Math.round(Math.max(drawer.high, Equipment.heroLeast()));
   }
   if(state.open)
   {
      // a strip of pieces lay over the drawer coming out, with its models over the numbers
      Equipment.closePieces();
      Equipment.showHero();
   }
   else
   {
      Equipment.drawHero(0);
   }
   Equipment.keepHero();
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

// the plugin keeps the drawer for the next opening of a menu
Equipment.keepHero = function()
{
   var state = Equipment.heroState;
   if(Equipment.plugin() != undefined)
   {
      Equipment.plugin().SetHeroDrawer(state.open, state.high, state.first);
   }
};

Equipment.heroLeast = function()
{
   return Equipment.HERO_LEAST;
};

// how far out the drawer goes for a wanted height, 0 is all of the sheet, never past the room or the sheet
Equipment.heroHigh = function(wanted)
{
   var drawer = Equipment.hero;
   var most = Math.min(drawer.room, drawer.full);
   return Math.max(Math.min(Equipment.heroLeast(), most), Math.min(wanted > 0 ? wanted : drawer.full, most));
};

Equipment.heroPointer = function()
{
   var point = {x:_root._xmouse, y:_root._ymouse};
   Equipment.panel.globalToLocal(point);
   return point;
};

// the bar with a few pixels on both sides, where a press takes it
Equipment.onHeroBar = function(x, y)
{
   var drawer = Equipment.hero;
   var bar = drawer.bar;
   var point = {x:x, y:y};
   Equipment.panel.globalToLocal(point);
   point.x -= drawer._x + bar._x;
   point.y -= drawer._y;
   return drawer._visible && bar._visible && point.x >= -5 && point.x <= Equipment.HERO_BAR + 5 && point.y >= bar.top && point.y <= bar.top + bar.track;
};

Equipment.inHero = function(x, y)
{
   var drawer = Equipment.hero;
   var point = {x:x, y:y};
   Equipment.panel.globalToLocal(point);
   return drawer._visible && point.x >= drawer._x && point.x <= drawer._x + drawer.wide && point.y >= drawer._y - drawer.high && point.y <= drawer._y;
};

// What the sheet shows, from what the plugin counts and what the menu shows below the list. The armor rating and
// the damage are the menu's, the game counts perks into those. A share of a hit and a resistance stop at their caps
Equipment.heroSheet = function(told, info, worn)
{
   var sheet = {pools:[], resists:[], offense:[]};
   var pieces = 0;
   var i = 0;
   if(told == undefined)
   {
      return undefined;
   }
   sheet.pools.push(Equipment.poolLine(Equipment.say("Health"), told.health, Equipment.HEALTH_COLOR));
   sheet.pools.push(Equipment.poolLine(Equipment.say("Magicka"), told.magicka, Equipment.MAGICKA_COLOR));
   sheet.pools.push(Equipment.poolLine(Equipment.say("Stamina"), told.stamina, Equipment.STAMINA_COLOR));
   while(worn != undefined && i < worn.armor.length)
   {
      pieces += Equipment.armorPiece(worn.armor[i]);
      i++;
   }
   if(info != undefined && info.armor != undefined)
   {
      sheet.armor = {rating:Math.floor(info.armor), stops:Math.min(info.armor * told.armorScale / 100 + pieces * told.armorBase, told.armorCap / 100), cap:told.armorCap / 100};
   }
   sheet.resists.push(Equipment.resistLine(Equipment.say("Magic"), told.magic, told.resistCap));
   sheet.resists.push(Equipment.resistLine(Equipment.say("Fire"), told.fire, told.resistCap));
   sheet.resists.push(Equipment.resistLine(Equipment.say("Frost"), told.frost, told.resistCap));
   sheet.resists.push(Equipment.resistLine(Equipment.say("Shock"), told.shock, told.resistCap));
   sheet.resists.push(Equipment.resistLine(Equipment.say("Poison"), told.poison, told.resistCap));
   sheet.resists.push(Equipment.resistLine(Equipment.say("Disease"), told.disease, told.resistCap));
   sheet.offense = Equipment.offenseLines(told, info);
   sheet.spells = Equipment.spellLines(told);
   sheet.level = told.level;
   sheet.toNext = told.toNextLevel;
   // a share of speed says little on its own, it shows only when something changed it
   sheet.movement = [];
   if(Math.round(told.move) != 100)
   {
      sheet.movement.push({name:Equipment.say("Speed"), value:Equipment.signed(told.move - 100) + "%"});
   }
   if(info != undefined && info.maxEncumbrance != undefined)
   {
      sheet.movement.push({name:Equipment.say("Carry weight"), value:Math.ceil(info.encumbrance) + " / " + Math.floor(info.maxEncumbrance), color:info.encumbrance > info.maxEncumbrance ? Equipment.HERO_HEAVY : undefined, icon:"weight"});
   }
   if(told.worn != undefined)
   {
      sheet.movement.push({name:Equipment.say("Gear weight"), value:Equipment.weightText(told.worn), icon:"weight"});
      // perks like Conditioning take armor out of what slows the player, the game keeps that weight apart
      // two numbers that read the same say nothing, 92.75 and 92.8 both show as 92.8
      if(told.wornForSpeed != undefined && Equipment.weightText(told.wornForSpeed) != Equipment.weightText(told.worn))
      {
         sheet.movement.push({name:Equipment.say("slows you down"), value:Equipment.weightText(told.wornForSpeed), minor:true});
      }
   }
   if(told.runSpeed > 0)
   {
      sheet.movement.push({name:Equipment.say("Run"), value:Equipment.metres(told.runSpeed)});
      sheet.movement.push({name:Equipment.say("Walk"), value:Equipment.metres(told.walkSpeed), minor:true});
   }
   if(told.noise != undefined && Math.round(told.noise * 100) != 100)
   {
      sheet.movement.push({name:Equipment.say("Noise of steps"), value:Equipment.fixed(told.noise * 100, 0) + "%"});
   }
   sheet.other = Equipment.voiceLines(told);
   if(told.absorb > 0)
   {
      sheet.other.push({name:Equipment.say("Spell absorb"), value:Equipment.fixed(told.absorb, 0) + "%"});
   }
   if(told.reflect > 0)
   {
      sheet.other.push({name:Equipment.say("Reflect damage"), value:Equipment.fixed(told.reflect, 0) + "%"});
   }
   if(told.waterBreathing > 0)
   {
      sheet.other.push({name:Equipment.say("Water breathing"), value:Equipment.say("yes")});
   }
   sheet.other = sheet.other.concat(Equipment.bountyLines(told));
   sheet.skills = Equipment.skillLines(told.skills);
   sheet.statuses = Equipment.statusLines(told);
   return sheet;
};

// A spell in a hand and what it costs as the game counts it for the player: a cast and how many the magicka holds
// now, or a second of a stream and how long it lasts. The same spell in both hands is one line
Equipment.spellLines = function(told)
{
   var lines = [];
   var spells = told.spells != undefined ? told.spells : [];
   var spell;
   var value;
   var i = 0;
   while(i < spells.length)
   {
      spell = spells[i];
      i++;
      if(i == 2 && spells[0].name == spell.name)
      {
         continue;
      }
      if(spell.cost <= 0)
      {
         value = Equipment.say("free");
      }
      else
      {
         value = spell.stream ? Equipment.say("# magicka a second", Equipment.fixed(spell.cost, 0)) : Equipment.say("# magicka", Equipment.fixed(spell.cost, 0));
      }
      lines.push({name:spell.name, aside:value, text:spell.text != undefined ? spell.text : "", color:Equipment.PARCHMENT});
   }
   return lines;
};

// what each hold asks for the head of the player
Equipment.bountyLines = function(told)
{
   var lines = [];
   var bounties = told.bounties != undefined ? told.bounties : [];
   var i = 0;
   while(i < bounties.length)
   {
      lines.push({name:Equipment.say("Bounty, #", bounties[i].name), value:Equipment.say("# gold", Math.round(bounties[i].gold)), color:Equipment.HERO_HEAVY});
      i++;
   }
   return lines;
};

// What lies on the player now with the words the game has for it and the time it has left. A disease is in red
Equipment.statusLines = function(told)
{
   var lines = [];
   var statuses = told.statuses != undefined ? told.statuses : [];
   var status;
   var i = 0;
   while(i < statuses.length)
   {
      status = statuses[i];
      lines.push({name:status.name, text:status.text, aside:Equipment.timeLeft(status.left), color:status.type == Equipment.DISEASE ? Equipment.HERO_HEAVY : Equipment.PARCHMENT});
      i++;
   }
   return lines;
};

// seconds as the time a player reads, nothing for what lasts
Equipment.timeLeft = function(seconds)
{
   var told = Number(seconds);
   var hours;
   if(seconds == undefined || !isFinite(told) || told < 0)
   {
      return "";
   }
   if(told >= 3600)
   {
      hours = Math.floor(told / 3600);
      return told % 3600 >= 60 ? Equipment.say("#1 h #2 min", hours, Math.floor((told % 3600) / 60)) : Equipment.say("# h", hours);
   }
   if(told >= 60)
   {
      return Equipment.say("# min", Math.floor(told / 60));
   }
   return Equipment.seconds(Math.ceil(told));
};

// What a blow does and how fast it comes with the weapons in hand, by the same timings as the item window. A change of
// attack speed shows only when something made one, crit chance only when there is any, fists when the right hand
// holds no weapon
Equipment.offenseLines = function(told, info)
{
   var lines = [];
   var hands = told.hands != undefined ? told.hands : [];
   var damage = info != undefined && info.damage != undefined ? Number(info.damage) : undefined;
   var shown = Equipment.say("Damage");
   var armed = false;
   var hand;
   var swing;
   var pace;
   var i = 0;
   if(told.bowDamage > 0)
   {
      damage = told.bowDamage;
      shown = Equipment.say("Damage with #", told.arrow);
   }
   if(damage > 0)
   {
      lines.push({name:shown, value:String(Math.floor(damage))});
   }
   while(i < hands.length)
   {
      hand = hands[i];
      swing = Equipment.swingOf(hand.entry);
      pace = Math.max(hand.speed, 0.05) * Math.max(hand.left ? told.leftSpeedMult : told.speedMult, 0.05);
      if(hand.left)
      {
         lines.push({name:Equipment.say("Left hand swing"), value:Equipment.seconds(Equipment.fixed(swing.hit / pace, 2)), minor:true});
      }
      else
      {
         armed = true;
         lines.push({name:Equipment.ranged(hand.entry) ? Equipment.say("Draw") : Equipment.say("Swing"), value:Equipment.seconds(Equipment.fixed(swing.hit / pace, 2))});
         if(damage > 0)
         {
            lines.push({name:Equipment.say("Damage a second"), value:"~" + Equipment.fixed(damage / (swing.cycle / pace), 1)});
         }
      }
      i++;
   }
   if(Math.round(told.speedMult * 100) != 100)
   {
      lines.push({name:Equipment.say("Attack speed"), value:Equipment.signed((told.speedMult - 1) * 100) + "%"});
   }
   if(Math.round(told.leftSpeedMult * 100) != Math.round(told.speedMult * 100))
   {
      lines.push({name:Equipment.say("Left attack speed"), value:Equipment.signed((told.leftSpeedMult - 1) * 100) + "%", minor:true});
   }
   if(told.crit > 0)
   {
      lines.push({name:Equipment.say("Critical chance"), value:Equipment.fixed(told.crit, 0) + "%"});
   }
   if(!armed && !Equipment.spellInRight(told))
   {
      lines.push({name:Equipment.say("Unarmed damage"), value:Equipment.fixed(told.unarmed, 0)});
   }
   if(told.sneakMult > 0)
   {
      lines.push({name:Equipment.say("Sneak attack"), value:"x" + Equipment.timesText(told.sneakMult) + ", " + Math.round((armed ? damage : told.unarmed) * told.sneakMult)});
   }
   if(told.powerStamina > 0)
   {
      lines.push({name:Equipment.say("Power attack"), value:"x" + Equipment.timesText(told.powerDamage) + ", " + Equipment.say("# stamina", Math.round(told.powerStamina))});
      if(told.stamina != undefined)
      {
         lines.push({name:Equipment.say("in a row from full"), value:String(Math.floor(told.stamina.most / told.powerStamina)), minor:true});
      }
   }
   return lines;
};

// The weapons in hand with their speed, the plugin reads it from the weapon. A two handed weapon or a bow is in
// both hands and counts once, staves don't swing
Equipment.heroHands = function(worn)
{
   var hands = [];
   var sides = worn != undefined ? [[worn.right, false], [worn.left, true]] : [];
   var entry;
   var told;
   var i = 0;
   while(i < sides.length)
   {
      entry = sides[i][0];
      i++;
      if(entry == undefined || entry.formType != Equipment.WEAPON || entry.subType == Equipment.STAFF)
      {
         continue;
      }
      if(sides[i - 1][1] && entry == worn.right && (Equipment.twoHanded(entry) || Equipment.ranged(entry)))
      {
         continue;
      }
      told = Equipment.plugin().Stats(entry.formId);
      if(told != undefined && told.kind == "weapon")
      {
         hands.push({entry:entry, speed:told.speed, left:sides[i - 1][1]});
      }
   }
   return hands;
};

// The shout in hand and the seconds each learned word rests the voice, as the game counts them with what shortens
// them. A change of that time shows on its own only when there's no shout in hand to show it on
Equipment.voiceLines = function(told)
{
   var lines = [];
   var mult = told.shout != undefined ? told.shout : 1;
   var seconds = [];
   var words;
   var i = 0;
   if(told.voice != undefined)
   {
      words = told.voice.words;
      while(words != undefined && i < words.length)
      {
         if(words[i].learned)
         {
            seconds.push(Equipment.fixed(words[i].seconds * mult, 0));
         }
         i++;
      }
      lines.push({name:told.voice.name, value:seconds.length > 0 ? Equipment.seconds(seconds.join(" / ")) : Equipment.say("no word learned")});
   }
   if(told.voiceWait > 0.5)
   {
      lines.push({name:Equipment.say("Voice rests"), value:Equipment.say("# s more", Equipment.fixed(told.voiceWait, 0)), minor:true});
   }
   if(Math.round(mult * 100) != 100)
   {
      lines.push({name:Equipment.say("Shout cooldown"), value:Equipment.signed((mult - 1) * 100) + "%", minor:told.voice != undefined});
   }
   return lines;
};

// a spell in the right hand leaves it no fists to hit with
Equipment.spellInRight = function(told)
{
   var spells = told.spells != undefined ? told.spells : [];
   var i = 0;
   while(i < spells.length)
   {
      if(!spells[i].left)
      {
         return true;
      }
      i++;
   }
   return false;
};

// game units a second as metres a second, a unit of the game is 1.428 cm
Equipment.metres = function(units)
{
   return Equipment.say("# m/s", Equipment.fixed(units * Equipment.METRES_OF_UNIT, 1));
};

// a multiplier with a tenth when it has one, 2 or 2.5
Equipment.timesText = function(value)
{
   return String(Math.round(value * 10) / 10);
};

// a weight the way the item list writes one, a tenth at most
Equipment.weightText = function(weight)
{
   return String(Math.round(weight * 10) / 10);
};

// What raises a skill, as the game keeps it. Enchantments and potions each add a share, for a school of magic the
// first makes its spells cheaper and the second stronger. Some effects add points to the skill itself
Equipment.skillLines = function(skills)
{
   var lines = [];
   var skill;
   var parts;
   var share;
   var i = 0;
   while(skills != undefined && i < skills.length)
   {
      skill = skills[i];
      parts = [];
      if(Equipment.has(Equipment.SCHOOLS, skill.name))
      {
         if(Math.round(skill.modifier) != 0)
         {
            parts.push(Equipment.say("cost #%", Equipment.signed(-skill.modifier)));
         }
         if(Math.round(skill.power) != 0)
         {
            parts.push(Equipment.say("power #%", Equipment.signed(skill.power)));
         }
      }
      else
      {
         share = skill.modifier + skill.power;
         if(Math.round(share) != 0)
         {
            parts.push(Equipment.skillWord(skill.name).split("#").join(Equipment.signed(share)));
         }
      }
      if(Math.round(skill.points) != 0)
      {
         parts.push(Equipment.say("# skill", Equipment.signed(skill.points)));
      }
      if(parts.length > 0)
      {
         lines.push({name:skill.shown != undefined ? skill.shown : skill.name, value:parts.join(", ")});
      }
      i++;
   }
   return lines;
};

Equipment.skillWord = function(name)
{
   var i = 0;
   while(i < Equipment.SKILL_WORDS.length)
   {
      if(Equipment.SKILL_WORDS[i][0] == name)
      {
         return Equipment.SKILL_WORDS[i][1];
      }
      i++;
   }
   return "#%";
};

Equipment.signed = function(value)
{
   var whole = Math.round(value);
   return (whole > 0 ? "+" : "") + whole;
};

// what's left and the most, and how much comes back a second out of a fight and in one when the game slows it there
Equipment.poolLine = function(name, pool, color)
{
   var note = Equipment.say("+#/s", Equipment.fixed(pool.regen, 1));
   if(pool.combat == 0)
   {
      note += ", " + Equipment.say("none in a fight");
   }
   else if(pool.combat > 0 && pool.combat != 1)
   {
      note += ", " + Equipment.say("in a fight +#", Equipment.fixed(pool.regen * pool.combat, 1));
   }
   return {name:name, now:pool.now, most:pool.most, note:note, color:color};
};

Equipment.resistLine = function(name, value, cap)
{
   var shown = Equipment.fixed(Math.min(value, cap), 0);
   return {name:name, value:shown + "%", zero:shown == "0", capped:value > cap};
};

Equipment.heroReport = function()
{
   var drawer = Equipment.hero;
   var state = Equipment.heroState;
   if(!drawer._visible)
   {
      return "off";
   }
   if(drawer.high <= 0)
   {
      return "in" + (state.high > 0 ? " (last out " + Math.round(state.high) + ")" : "");
   }
   return "out " + Math.round(drawer.high) + " of " + Math.round(drawer.full) + " in a room of " + Math.round(drawer.room) + (drawer.most > 0 ? ", scrolled " + drawer.first + " of " + drawer.most : "");
};

// for the test stand: a click on the tab
Equipment.pressHero = function()
{
   Equipment.grabHero();
   Equipment.letHeroGo();
};

// For the test stand: the middle of the tab, of the sheet or of the bar, as aimedX, aimedY. The game's script
// hands the word over in whatever letter case it met first
Equipment.aimHero = function(part)
{
   var drawer = Equipment.hero;
   var wanted = String(part).toLowerCase();
   var point = {x:drawer.wide / 2, y:-drawer.high / 2};
   var share;
   if(wanted == "tab")
   {
      point = {x:drawer.tab._x + drawer.tab.touch._width / 2, y:drawer.tab._y + Equipment.HERO_TAB_HIGH / 2};
   }
   else if(wanted == "bar")
   {
      point = {x:drawer.bar._x + Equipment.HERO_BAR / 2, y:drawer.bar.top + drawer.bar.track / 2};
   }
   drawer.localToGlobal(point);
   share = Equipment.screenShare(point.x, point.y);
   Equipment.aimedX = share.x;
   Equipment.aimedY = share.y;
};

// for the test stand: the numbers the plugin gives the sheet, to set beside what a real swing, shot or run does
Equipment.heroNumbers = function(note)
{
   var told = Equipment.plugin().HeroStats();
   var info = Equipment.menu.bottomBar._playerInfoObj;
   var spells = [];
   var statuses = [];
   var i = 0;
   while(told.spells != undefined && i < told.spells.length)
   {
      spells.push(told.spells[i].name + (told.spells[i].left ? " left " : " right ") + told.spells[i].cost + (told.spells[i].stream ? " a second" : ""));
      i++;
   }
   i = 0;
   while(told.statuses != undefined && i < told.statuses.length)
   {
      statuses.push(told.statuses[i].name + " " + Equipment.timeLeft(told.statuses[i].left) + " [" + told.statuses[i].text + "]");
      i++;
   }
   skse.Log("PANELTEST sheet " + note + ": damage " + info.damage + ", sneak x" + told.sneakMult + ", power x" + told.powerDamage + " for " + told.powerStamina + " stamina, run " + told.runSpeed + ", walk " + told.walkSpeed + ", bow with arrow " + told.bowDamage + " (" + told.arrow + "), stamina " + told.stamina.now + " of " + told.stamina.most + ", magicka " + told.magicka.now + " of " + told.magicka.most + ", gear " + told.worn + " of which slows " + told.wornForSpeed + "; spells " + spells.join(", ") + "; statuses " + statuses.join(", "));
};
