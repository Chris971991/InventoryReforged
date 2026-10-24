Equipment.CONTEXT_WIDE = 176;
Equipment.CONTEXT_SETTLE = 250;
Equipment.DIVIDER_ROOM = 9;

// A right click on a slot or a row opens a small menu at the pointer. A click anywhere else closes it
Equipment.buildContext = function()
{
   var context = Equipment.over.createEmptyMovieClip("equipmentContext", Equipment.over.getNextHighestDepth());
   context._visible = false;
   Equipment.context = context;
   Equipment.contextCatcher = {};
   Equipment.contextCatcher.onMouseDown = function()
   {
      // a press inside the menu is the line's own, any other press closes it after the line had its turn
      // The right press that opened the menu comes here a moment later, so the first moment is left alone
      if(getTimer() - Equipment.contextOpenedAt < Equipment.CONTEXT_SETTLE)
      {
         return undefined;
      }
      if(Equipment.context._visible && !Equipment.context.hitTest(_root._xmouse, _root._ymouse, true))
      {
         Equipment.closeContext();
      }
   };
   Mouse.addListener(Equipment.contextCatcher);
};

Equipment.contextLines = function(slot)
{
   var entry = slot.entries[0];
   var lines = [];
   if(entry != undefined && entry.outfit == true)
   {
      lines.push({key:"details", words:Equipment.say("Details"), icon:"details"});
      if(Equipment.inspectable(entry))
      {
         lines.push({key:"inspect", words:Equipment.say("Inspect"), icon:"lens"});
      }
      if(Equipment.canTakeOff(slot))
      {
         lines.push({key:"takeOff", words:Equipment.say("Take off"), icon:"unequip"});
      }
      lines.push({key:"outfit", words:Equipment.say("Part of the outfit"), icon:slot.icon});
   }
   else if(entry != undefined && !Equipment.isSpell(entry))
   {
      lines.push({key:"details", words:Equipment.say("Details"), icon:"details"});
      if(Equipment.inspectable(entry))
      {
         lines.push({key:"inspect", words:Equipment.say("Inspect"), icon:"lens"});
      }
      lines.push({key:"select", words:Equipment.say("Find in list"), icon:slot.icon});
      if(Equipment.canTakeOff(slot.taken ? Equipment.slotNamed("right") : slot))
      {
         lines.push({key:"takeOff", words:Equipment.say("Take off"), icon:"unequip"});
      }
      // a mod asked the panel to leave this follower's gear as it is, the menu says why nothing comes off
      if(Equipment.lookOnly())
      {
         lines.push({key:"readOnly", words:Equipment.say("Kept as is by another mod"), icon:slot.icon});
      }
   }
   else
   {
      lines.push({key:"filter", words:Equipment.activeSlot == Equipment.ownerOf(slot) ? Equipment.say("Show everything") : Equipment.say("Show what fits"), icon:slot.icon});
   }
   if(Equipment.canToggle(slot))
   {
      lines.push({key:"hide", words:Equipment.looksHidden(slot) ? Equipment.say("Show on character") : Equipment.say("Hide on character"), icon:Equipment.looksHidden(slot) ? "eye" : "eyeShut"});
   }
   return lines;
};

Equipment.openContext = function(slot)
{
   if(Equipment.listsAnswer())
   {
      Equipment.showContext(Equipment.contextLines(slot), slot, undefined);
   }
};

// The menu at the pointer, for a slot or for a row of the list. Its lines act on the one it was opened for.
// A line with the key "-" is a thin divider between two groups. The menu is as wide as its longest line
Equipment.showContext = function(lines, slot, entry)
{
   var context = Equipment.context;
   var pad = Equipment.CONTEXT_PAD;
   var wide = Equipment.CONTEXT_WIDE;
   var high;
   var y = pad;
   var i = 0;
   var line;
   var menuPoint = {x:_root._xmouse, y:_root._ymouse};
   var made = [];
   var bottom;
   var byPieces;
   var list = Equipment.menu.inventoryLists.itemList;
   Equipment.closePopup();
   Equipment.closeContext();
   // The row of the menu stays picked and the list stays still while the menu is up: the pointer crossing other rows on
   // its way to a line picked them and brought their windows up
   if(entry != undefined && entry.itemIndex != undefined)
   {
      if(list.selectedIndex != entry.itemIndex)
      {
         list.selectedIndex = entry.itemIndex;
      }
      Equipment.contextHoldsRows = true;
      Equipment.holdRows();
   }
   for(var name in context)
   {
      if(context[name] instanceof MovieClip)
      {
         context[name].removeMovieClip();
      }
   }
   context.clear();
   while(i < lines.length)
   {
      if(lines[i].key == "-")
      {
         made.push({divider:y + Equipment.DIVIDER_ROOM / 2});
         y += Equipment.DIVIDER_ROOM;
         i++;
         continue;
      }
      line = context.createEmptyMovieClip("line" + i, i + 1);
      line._x = pad;
      line._y = y;
      line.what = lines[i];
      line.slot = slot;
      line.entry = entry;
      line.createTextField("touch", 0, 0, 0, 10, Equipment.CONTEXT_LINE);
      line.touch.selectable = false;
      line.createEmptyMovieClip("look", 1);
      Equipment.writeMenuWords(line, lines[i].words, false);
      wide = Math.max(wide, line.words._x + line.words._width + 4 * pad);
      made.push(line);
      y += Equipment.CONTEXT_LINE;
      i++;
   }
   high = y + pad;
   context.wide = wide;
   context.made = made;
   // a slot label under the menu showed through a see-through one
   Equipment.panelBox(context, wide, high, 100, 40);
   i = 0;
   while(i < made.length)
   {
      line = made[i];
      i++;
      if(line.divider != undefined)
      {
         context.lineStyle(1, Equipment.WHITE, 20, true);
         context.moveTo(2 * pad, line.divider);
         context.lineTo(wide - 2 * pad, line.divider);
         context.lineStyle();
         continue;
      }
      line.touch._width = wide - 2 * pad;
      Equipment.drawContextLine(line, false);
      line.onRollOver = function()
      {
         Equipment.drawContextLine(this, true);
      };
      line.onRollOut = line.onReleaseOutside = function()
      {
         Equipment.drawContextLine(this, false);
      };
      line.onPress = function()
      {
         var what = this.what.key;
         var slot = this.slot;
         var entry = this.entry;
         var run = this.what.run;
         Equipment.usePress();
         Equipment.closeContext();
         if(run != undefined)
         {
            run();
         }
         else if(entry != undefined)
         {
            Equipment.rowAct(what, entry);
         }
         else
         {
            Equipment.act(what, slot);
         }
      };
   }
   Equipment.toPanel(menuPoint);
   context.high = high;
   // the menu of a piece in a strip goes over or under the strip like its window
   bottom = (slot != undefined ? Equipment.menu.bottomBar._y : Equipment.barTop()) - Equipment.MARGIN;
   byPieces = Equipment.placeByPieces(slot, context, bottom);
   if(!byPieces)
   {
      context._x = Equipment.contextLeft(menuPoint.x, wide, slot, entry);
      context._y = Math.min(menuPoint.y, bottom - high);
   }
   context._visible = true;
   Equipment.contextSlot = slot;
   Equipment.contextEntry = entry;
   Equipment.contextOpenedAt = getTimer();
   Equipment.contextOverlaid = Equipment.overlayContext();
   if(!Equipment.contextOverlaid && !byPieces && entry != undefined)
   {
      Equipment.clearOfModel(context, wide, high, bottom);
   }
   Equipment.coverWindow();
   Equipment.contextHidesModel = Equipment.contextOverWindow() && !Equipment.contextOverlaid && Equipment.plugin() != undefined;
   if(Equipment.contextHidesModel)
   {
      Equipment.plugin().ShowPreview(false);
   }
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

// The menu of a slot opens right at the slot's edge toward the doll, past the names of the column it stood far off.
// The one of a row opens at the pointer past the row's name, it covered the name standing left of the pointer
Equipment.contextLeft = function(pointerX, wide, slot, entry)
{
   var list = Equipment.rowsEdge();
   var listLeft = list.right - list.wide + Equipment.MARGIN;
   var screen = Equipment.screenRect();
   var screenRight = screen.x + screen.width - Equipment.MARGIN;
   if(slot != undefined)
   {
      return slot.column == 0 ? slot.box._x + Equipment.cellSize(slot) + Equipment.POPUP_GAP : slot.box._x - Equipment.POPUP_GAP - wide;
   }
   if(entry != undefined)
   {
      return Math.max(listLeft, Math.min(Math.max(pointerX, Equipment.nameEnd(entry) + Equipment.POPUP_GAP), list.right - wide));
   }
   return Math.min(pointerX, screenRight - wide);
};

// the part of the item window its model stands in, in the panel's units
Equipment.modelBox = function()
{
   var window = Equipment.window;
   return {left:window._x, right:window._x + Equipment.windowWide(), top:window._y + Equipment.number(window.previewTop), bottom:window._y + Equipment.number(window.preview)};
};

Equipment.meetsBox = function(x, y, wide, high, box)
{
   return x < box.right && box.left < x + wide && y < box.bottom && box.top < y + high;
};

// The slot models and the model of the item window are 3D drawn over the menu. A copy of the open menu goes into the
// movie of the eyes, which the plugin draws over the 3D, so the menu lies on a model like on anything else and stays
// at its row. The menu itself stays under it for the pointer. Four values a line: lit (2), divider (1) or plain (0),
// its top, its words and its icon. False when that movie isn't there
Equipment.overlayContext = function()
{
   var context = Equipment.context;
   var plugin = Equipment.plugin();
   var told = [];
   var at;
   var line;
   var i = 0;
   if(plugin == undefined || plugin.ContextOver == undefined)
   {
      return false;
   }
   if(context._visible)
   {
      at = Equipment.share(context._x, context._y);
      told = [at.x, at.y, Equipment.share(context._x, context._y + 1).y - at.y, context.wide, context.high, Equipment.GOLD, Equipment.PARCHMENT, Equipment.WHITE];
      while(context.made != undefined && i < context.made.length)
      {
         line = context.made[i];
         if(line.divider != undefined)
         {
            told.push(1, line.divider, "", "");
         }
         else
         {
            told.push(line.lit == true ? 2 : 0, line._y, String(line.what.words), line.what.icon != undefined ? String(line.what.icon) : "");
         }
         i++;
      }
   }
   return plugin.ContextOver(told) == true;
};

// Where the copy over the 3D can't be drawn the menu keeps off the window's model: it goes down under the model over
// the window's card, or up over it, and only where neither fits the model steps out while the menu is up
Equipment.clearOfModel = function(context, wide, high, bottom)
{
   var window = Equipment.window;
   var box;
   if(window == undefined || !window._visible)
   {
      return undefined;
   }
   box = Equipment.modelBox();
   if(!Equipment.meetsBox(context._x, context._y, wide, high, box))
   {
      return undefined;
   }
   if(box.bottom + Equipment.WINDOW_GAP + high <= bottom)
   {
      context._y = box.bottom + Equipment.WINDOW_GAP;
   }
   else if(box.top - Equipment.WINDOW_GAP - high >= Equipment.frame.top)
   {
      context._y = box.top - Equipment.WINDOW_GAP - high;
   }
};

// the model of the item window would stand over a row's menu lying on the window's model
Equipment.contextOverWindow = function()
{
   var window = Equipment.window;
   var context = Equipment.context;
   if(window == undefined || !window._visible || !context._visible || Equipment.contextEntry == undefined)
   {
      return false;
   }
   return Equipment.meetsBox(context._x, context._y, context.wide, context.high, Equipment.modelBox());
};

Equipment.drawContextLine = function(line, lit)
{
   Equipment.drawMenuLine(line.look, line._parent.wide - 2 * Equipment.CONTEXT_PAD, lit, line.what.icon);
   line.words.textColor = lit ? Equipment.GOLD : Equipment.WHITE;
   line.lit = lit;
   if(Equipment.contextOverlaid && line._parent._visible)
   {
      Equipment.overlayContext();
   }
};

Equipment.closeContext = function()
{
   if(Equipment.context._visible)
   {
      if(Equipment.ownerOf(Equipment.contextSlot) == Equipment.pieceStrip.slot)
      {
         Equipment.piecesWindowClosedAt = getTimer();
      }
      Equipment.context._visible = false;
      Equipment.contextSlot = undefined;
      Equipment.contextEntry = undefined;
      if(Equipment.contextOverlaid)
      {
         Equipment.overlayContext();
      }
      Equipment.contextOverlaid = false;
      Equipment.coverWindow();
      if(Equipment.contextHidesModel && Equipment.plugin() != undefined)
      {
         Equipment.plugin().ShowPreview(Equipment.window._visible);
      }
      Equipment.contextHidesModel = false;
   }
   // the list answers again before a line of the menu acts on it
   if(Equipment.contextHoldsRows)
   {
      Equipment.contextHoldsRows = false;
      Equipment.holdRows();
   }
};

// for the test stand: the line of the open menu with this key is pressed
Equipment.pressContext = function(key)
{
   var slot = Equipment.contextSlot;
   var entry = Equipment.contextEntry;
   var run;
   for(var name in Equipment.context)
   {
      if(Equipment.context[name].what.key == String(key))
      {
         run = Equipment.context[name].what.run;
      }
   }
   Equipment.closeContext();
   if(run != undefined)
   {
      run();
   }
   else if(entry != undefined)
   {
      Equipment.rowAct(String(key), entry);
   }
   else
   {
      Equipment.act(String(key), slot);
   }
};

// for the test stand: the middle of a line of the open slot menu, as aimedX, aimedY
Equipment.aimContext = function(index)
{
   var line = Equipment.context["line" + Number(index)];
   var point;
   Equipment.aimedX = Equipment.aimedY = -1;
   if(line == undefined || !Equipment.context._visible)
   {
      return undefined;
   }
   point = {x:Equipment.context._x + line._x + 40, y:Equipment.context._y + line._y + Equipment.CONTEXT_LINE / 2};
   var share = Equipment.share(point.x, point.y);
   Equipment.aimedX = share.x;
   Equipment.aimedY = share.y;
};
