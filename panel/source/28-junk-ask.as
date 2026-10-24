Equipment.JUNK_ASK_WIDE = 460;
Equipment.JUNK_ASK_PAD = 14;
Equipment.JUNK_ASK_LINE = 24;
Equipment.JUNK_ASK_SHOWN = 12;
Equipment.JUNK_ASK_TEXT = 15;
Equipment.JUNK_ASK_BUTTON = 30;
Equipment.JUNK_ASK_SHADE = 35;

// Selling or storing all junk at once asks first. Every row that goes is listed with how many and what the shop pays
// for them or what they weigh, with the sum under them. The button that moves them, Enter or the accept button of a
// pad go ahead; the other button, Escape, Tab, the pad's cancel or a press outside leave everything as it is. A long
// list scrolls with the wheel. The rows wait behind it
Equipment.askJunk = function(verb, rows)
{
   var ask = Equipment.junkAsk;
   if(rows.length == 0)
   {
      return undefined;
   }
   if(ask == undefined)
   {
      ask = Equipment.junkAsk = Equipment.over.createEmptyMovieClip("equipmentJunkAsk", Equipment.over.getNextHighestDepth());
      Equipment.junkWheel = {};
      Equipment.junkWheel.onMouseWheel = function(delta)
      {
         if(Equipment.junkAsk._visible)
         {
            Equipment.scrollJunkAsk(delta > 0 ? -1 : 1);
         }
      };
      Mouse.addListener(Equipment.junkWheel);
   }
   Equipment.closeContext();
   Equipment.closePopup();
   Equipment.junkAsked = {verb:verb, rows:rows, from:0};
   Equipment.drawJunkAsk();
   ask._visible = true;
   Equipment.holdRows();
   // the models are 3D drawn over the whole menu, the preview of the row last under the pointer stood over the window
   if(Equipment.plugin() != undefined)
   {
      Equipment.plugin().HideModels(true);
   }
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

// what a row comes to in the shop or in the chest: gold for all its copies, or their weight
Equipment.junkWorth = function(verb, entry)
{
   var count = Math.max(Equipment.number(entry.count), 1);
   var one = verb == "sell" ? Equipment.number(entry.infoValue != undefined ? entry.infoValue : entry.value) : Equipment.number(entry.infoWeight);
   return one * count;
};

Equipment.junkWorthWords = function(verb, worth)
{
   return verb == "sell" ? String(Math.round(worth)) : String(Math.round(worth * 10) / 10);
};

Equipment.drawJunkAsk = function()
{
   var ask = Equipment.junkAsk;
   var asked = Equipment.junkAsked;
   var screen = Equipment.screenRect();
   var pad = Equipment.JUNK_ASK_PAD;
   var wide = Equipment.JUNK_ASK_WIDE;
   var shown = Math.min(asked.rows.length, Equipment.JUNK_ASK_SHOWN);
   var listTop;
   var high;
   var total = 0;
   var i = 0;
   for(var name in ask)
   {
      if(ask[name] instanceof MovieClip)
      {
         ask[name].removeMovieClip();
      }
   }
   ask.clear();
   while(i < asked.rows.length)
   {
      total += Equipment.junkWorth(asked.verb, asked.rows[i]);
      i++;
   }
   // A dim sheet over the whole screen takes every press, one outside the window calls it off. The window itself has no
   // press of its own: in ActionScript 2 a clip that takes presses takes them from the clips inside it too, and its
   // buttons never got a click
   ask.createEmptyMovieClip("backdrop", 1);
   ask.backdrop.beginFill(0, Equipment.JUNK_ASK_SHADE);
   ask.backdrop.moveTo(screen.x, screen.y);
   ask.backdrop.lineTo(screen.x + screen.width, screen.y);
   ask.backdrop.lineTo(screen.x + screen.width, screen.y + screen.height);
   ask.backdrop.lineTo(screen.x, screen.y + screen.height);
   ask.backdrop.lineTo(screen.x, screen.y);
   ask.backdrop.endFill();
   ask.backdrop.onPress = function()
   {
      if(!Equipment.junkAsk.sheet.hitTest(_root._xmouse, _root._ymouse, true))
      {
         Equipment.answerJunk(false);
      }
   };
   ask.backdrop.useHandCursor = false;
   ask.createEmptyMovieClip("sheet", 2);
   Equipment.writeLine(ask.sheet, "title", 1, asked.verb == "sell" ? Equipment.say("Sell all junk?") : (asked.verb == "take" ? Equipment.say("Take all junk?") : Equipment.say("Store all junk?")), 20, Equipment.accentInk());
   ask.sheet.title._x = pad;
   ask.sheet.title._y = pad;
   listTop = ask.sheet.title._y + ask.sheet.title._height + 8;
   ask.sheet.createEmptyMovieClip("lines", 2);
   ask.sheet.lines._y = listTop;
   high = listTop + shown * Equipment.JUNK_ASK_LINE + 8;
   Equipment.writeLine(ask.sheet, "total", 3, Equipment.say("Total"), Equipment.JUNK_ASK_TEXT, Equipment.inkOf("value"));
   ask.sheet.total._x = pad;
   ask.sheet.total._y = high;
   Equipment.writeLine(ask.sheet, "sum", 4, Equipment.junkWorthWords(asked.verb, total), Equipment.JUNK_ASK_TEXT, Equipment.accentInk());
   ask.sheet.sum._x = wide - pad - ask.sheet.sum._width;
   ask.sheet.sum._y = high;
   high += ask.sheet.total._height + 12;
   Equipment.junkAskButton(ask.sheet, "yes", 5, Equipment.junkWords(asked.verb, Equipment.pieces(asked.rows)), pad, high, (wide - 3 * pad) / 2, true);
   Equipment.junkAskButton(ask.sheet, "no", 6, Equipment.say("Cancel"), 2 * pad + (wide - 3 * pad) / 2, high, (wide - 3 * pad) / 2, false);
   high += Equipment.JUNK_ASK_BUTTON + pad;
   Equipment.panelBox(ask.sheet, wide, high, 96, 45);
   ask.sheet._x = Math.round(screen.x + (screen.width - wide) / 2);
   ask.sheet._y = Math.round(screen.y + (screen.height - high) / 2);
   Equipment.drawJunkLines();
};

// the rows in sight from asked.from on, a scroll mark at the right when there are more than fit
Equipment.drawJunkLines = function()
{
   var asked = Equipment.junkAsked;
   var lines = Equipment.junkAsk.sheet.lines;
   var pad = Equipment.JUNK_ASK_PAD;
   var wide = Equipment.JUNK_ASK_WIDE;
   var shown = Math.min(asked.rows.length, Equipment.JUNK_ASK_SHOWN);
   var measure;
   var line;
   var entry;
   var count;
   var i = 0;
   for(var name in lines)
   {
      if(lines[name] instanceof MovieClip)
      {
         lines[name].removeMovieClip();
      }
   }
   lines.clear();
   while(i < shown)
   {
      entry = asked.rows[asked.from + i];
      line = lines.createEmptyMovieClip("line" + i, i + 1);
      line._y = i * Equipment.JUNK_ASK_LINE;
      count = Math.max(Equipment.number(entry.count), 1);
      Equipment.writeLine(line, "worth", 2, Equipment.junkWorthWords(asked.verb, Equipment.junkWorth(asked.verb, entry)), Equipment.JUNK_ASK_TEXT, Equipment.inkOf("value"));
      line.worth._x = wide - pad - line.worth._width;
      Equipment.writeLine(line, "count", 3, count > 1 ? "x" + count : "", Equipment.JUNK_ASK_TEXT, Equipment.inkOf("quiet"));
      line.count._x = wide - pad - 70 - line.count._width;
      Equipment.writeLine(line, "words", 1, "", Equipment.JUNK_ASK_TEXT, Equipment.WHITE);
      measure = function(words)
      {
         line.words.text = words;
         return line.words.textWidth;
      };
      line.words.text = Equipment.cutToFit(String(entry.text), wide - 2 * pad - 90 - line.count._width, measure);
      line.words._x = pad;
      i++;
   }
   if(asked.rows.length > shown)
   {
      lines.lineStyle(2, Equipment.accentInk(), 70, true);
      lines.moveTo(wide - 4, shown * Equipment.JUNK_ASK_LINE * asked.from / asked.rows.length);
      lines.lineTo(wide - 4, shown * Equipment.JUNK_ASK_LINE * (asked.from + shown) / asked.rows.length);
      lines.lineStyle();
   }
};

Equipment.junkAskButton = function(sheet, name, depth, words, x, y, wide, lit)
{
   var button = sheet.createEmptyMovieClip(name, depth);
   button._x = x;
   button._y = y;
   button.wide = wide;
   button.lit = lit;
   Equipment.writeLine(button, "words", 2, words.toUpperCase(), Equipment.JUNK_ASK_TEXT, lit ? Equipment.accentInk() : Equipment.WHITE);
   button.words._x = (wide - button.words._width) / 2;
   button.words._y = (Equipment.JUNK_ASK_BUTTON - button.words._height) / 2;
   Equipment.drawJunkAskButton(button, false);
   button.onRollOver = function()
   {
      Equipment.drawJunkAskButton(this, true);
   };
   button.onRollOut = button.onReleaseOutside = function()
   {
      Equipment.drawJunkAskButton(this, false);
   };
   button.onPress = function()
   {
      Equipment.answerJunk(this._name == "yes");
   };
};

Equipment.drawJunkAskButton = function(button, over)
{
   button.clear();
   button.lineStyle(1, button.lit ? Equipment.accentInk() : Equipment.WHITE, over ? 90 : 40, true);
   button.beginFill(button.lit ? Equipment.GOLD : 1710618, over ? 30 : (button.lit ? 15 : 80));
   Equipment.roundBox(button, 0, 0, button.wide, Equipment.JUNK_ASK_BUTTON, 4);
   button.endFill();
};

Equipment.scrollJunkAsk = function(step)
{
   var asked = Equipment.junkAsked;
   var from = Math.max(0, Math.min(asked.from + step, asked.rows.length - Equipment.JUNK_ASK_SHOWN));
   if(from != asked.from)
   {
      asked.from = from;
      Equipment.drawJunkLines();
   }
};

// The window goes either way. Yes moves the junk the list holds now, the same rows unless something else moved
Equipment.answerJunk = function(yes)
{
   var asked = Equipment.junkAsked;
   if(Equipment.junkAsk == undefined || !Equipment.junkAsk._visible)
   {
      return undefined;
   }
   Equipment.junkAsk._visible = false;
   Equipment.junkAsked = undefined;
   Equipment.holdRows();
   if(Equipment.plugin() != undefined)
   {
      Equipment.plugin().HideModels(false);
   }
   gfx.io.GameDelegate.call("PlaySound", [yes ? "UIMenuOK" : "UIMenuCancel"]);
   skse.Log("EquipmentPanel junk " + asked.verb + " " + (yes ? "agreed" : "called off") + ": " + asked.rows.length + " rows");
   if(yes)
   {
      Equipment.moveJunkNow(asked.verb);
   }
};

// keys while the window is up: it takes them all, the list behind waits
Equipment.junkAskKey = function(details)
{
   var code = details.navEquivalent;
   if(Equipment.junkAsk == undefined || !Equipment.junkAsk._visible)
   {
      return false;
   }
   if(Shared.GlobalFunc.IsKeyPressed(details))
   {
      if(code == gfx.ui.NavigationCode.ENTER)
      {
         Equipment.answerJunk(true);
      }
      else if(code == gfx.ui.NavigationCode.ESCAPE || code == gfx.ui.NavigationCode.TAB)
      {
         Equipment.answerJunk(false);
      }
      else if(code == gfx.ui.NavigationCode.UP || code == gfx.ui.NavigationCode.DOWN)
      {
         Equipment.scrollJunkAsk(code == gfx.ui.NavigationCode.UP ? -1 : 1);
      }
   }
   return true;
};

// for the test stand: what the window lists and its sum, as the log line of a sale
// for the test stand: the middle of a button of the window, "yes" or "no", as aimedX, aimedY
Equipment.aimJunkAsk = function(name)
{
   var button = Equipment.junkAsk.sheet[String(name).toLowerCase()];
   var point = {x:button.wide / 2, y:Equipment.JUNK_ASK_BUTTON / 2};
   var share;
   button.localToGlobal(point);
   share = Equipment.screenShare(point.x, point.y);
   Equipment.aimedX = share.x;
   Equipment.aimedY = share.y;
};

Equipment.junkAskReport = function()
{
   var asked = Equipment.junkAsked;
   var lines = Equipment.junkAsk.sheet.lines;
   var told = [];
   var i = 0;
   if(asked == undefined)
   {
      skse.Log("PANELTEST junk ask: none");
      return undefined;
   }
   while(lines["line" + i] != undefined)
   {
      told.push(lines["line" + i].words.text + " " + lines["line" + i].count.text + " " + lines["line" + i].worth.text);
      i++;
   }
   skse.Log("PANELTEST junk ask " + asked.verb + ": " + asked.rows.length + " rows, sum " + Equipment.junkAsk.sheet.sum.text + ", shown " + told.join(" | "));
};
