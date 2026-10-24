Equipment.MINI_SIZE = 50;
Equipment.MINI_GAP = 6;
Equipment.PIECES_PAD = 7;
Equipment.PIECES_DEPTH = 20;
Equipment.MINI_MODELS = 100;
Equipment.MINI_SPAN = 64;
Equipment.MINI_INSET = 4;
Equipment.TAB_WIDE = 12;
Equipment.TAB_HIGH = 26;
Equipment.TAB_GAP = 2;
Equipment.HANDLE_WIDE = 14;
Equipment.ARROW_WIDE = 18;
Equipment.CLICK_SLACK = 7;
Equipment.PIECES_SAME_PRESS = 60;

// A slot with more than one piece in it (a cloak, a bag and a lamp in Other, rings on several fingers, a hood
// with a circlet) shows the first of them and a small tab on its inner side. A click on the tab pulls a strip of
// small slots out of it toward the doll: the empty one first, it filters the
// list and takes a dropped item, then one for every piece. A small slot does what a slot does for its own piece:
// model, window, menu, eye, double click and drop. The strip is one row that ends before the other column; when
// the pieces don't fit, arrows and the wheel scroll them. It stays out until a click lands outside it, its handle
// at the slot pushes it back in, or Esc
Equipment.hasPieces = function(slot)
{
   return Equipment.settings.pieceStrips && slot.parent == undefined && slot.entries.length > 1 && !slot.taken;
};

// a chevron in a little box beside the slot, toward the doll
Equipment.drawTab = function(slot)
{
   var tab = slot.box.tab;
   var wide = Equipment.TAB_WIDE;
   var high = Equipment.TAB_HIGH;
   if(tab == undefined)
   {
      return undefined;
   }
   tab.clear();
   tab._visible = Equipment.hasPieces(slot) && Equipment.pieceStrip.slot != slot;
   if(!tab._visible)
   {
      return undefined;
   }
   tab._x = slot.column == 0 ? Equipment.SLOT_SIZE + Equipment.TAB_GAP : -Equipment.TAB_GAP - wide;
   tab._y = (Equipment.SLOT_SIZE - high) / 2;
   // the touch reaches back over the gap to the slot, so a press between the two doesn't land on the doll
   if(tab.touch == undefined)
   {
      tab.createTextField("touch", 0, slot.column == 0 ? -Equipment.TAB_GAP : 0, 0, wide + Equipment.TAB_GAP, high);
      tab.touch.selectable = false;
   }
   tab.lineStyle(1, Equipment.GOLD, slot.hovered ? 90 : 50, true);
   tab.beginFill(0, 70);
   Equipment.roundBox(tab, 0, 0, wide, high, 3);
   tab.endFill();
   Equipment.drawChevron(tab, wide, high, slot.column == 0, slot.hovered ? 100 : 70);
};

// a chevron that points right or left in the middle of a box
Equipment.drawChevron = function(clip, wide, high, right, alpha)
{
   var tip = right ? wide / 2 + 3 : wide / 2 - 3;
   var back = right ? wide / 2 - 2.5 : wide / 2 + 2.5;
   clip.lineStyle(1.6, Equipment.GOLD, alpha, true, "normal", "round", "round");
   clip.moveTo(back, high / 2 - 5);
   clip.lineTo(tip, high / 2);
   clip.lineTo(back, high / 2 + 5);
   clip.lineStyle();
};

// a strip made again from a new list leaves the models to the list, the worn things may have changed
Equipment.openPieces = function(slot, fromList)
{
   var first = Equipment.pieceStrip.slot == slot ? Equipment.pieceStrip.first : 0;
   var holder;
   var minis = [];
   var i = 0;
   Equipment.closePieces(fromList);
   holder = Equipment.panel.createEmptyMovieClip("strip", Equipment.PIECES_DEPTH);
   Equipment.pieceStrip = {slot:slot, minis:minis, clip:holder, first:first};
   Equipment.pieceStrip.plus = Equipment.makeMini(slot, -1, holder);
   while(i < slot.entries.length)
   {
      minis.push(Equipment.makeMini(slot, i, holder));
      i++;
   }
   Equipment.buildPiecesControls(holder);
   Equipment.placePieces();
   Equipment.drawFrame(slot);
   Equipment.drawTab(slot);
   if(!fromList)
   {
      Equipment.showModels(true);
      gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
   }
};

// place -1 is the empty small slot
Equipment.makeMini = function(slot, place, holder)
{
   var entry = slot.entries[place];
   var mini = {name:slot.name, title:slot.title, icon:slot.icon, flag:slot.flag, mask:slot.mask, column:slot.column, row:slot.row, parent:slot, place:place};
   mini.entries = entry != undefined ? [entry] : [];
   mini.plus = place < 0;
   mini.modelIndex = Equipment.MINI_MODELS + Equipment.slotIndex(slot) * Equipment.MINI_SPAN + place;
   Equipment.buildCell(holder.createEmptyMovieClip(mini.plus ? "plus" : "mini" + place, place + 2), mini, Equipment.MINI_SIZE);
   return mini;
};

// the handle that pushes the strip back in and the two arrows that scroll it
Equipment.buildPiecesControls = function(holder)
{
   var controls = [["handle", Equipment.HANDLE_WIDE], ["back", Equipment.ARROW_WIDE], ["on", Equipment.ARROW_WIDE]];
   var i = 0;
   var control;
   while(i < controls.length)
   {
      control = holder.createEmptyMovieClip(controls[i][0], 1000 + i);
      control.wide = controls[i][1];
      control.createTextField("touch", 0, 0, 0, control.wide, Equipment.MINI_SIZE);
      control.touch.selectable = false;
      control.createEmptyMovieClip("look", 1);
      control.onRollOver = function()
      {
         this.lit = true;
         Equipment.drawPiecesControl(this);
      };
      control.onRollOut = control.onReleaseOutside = function()
      {
         this.lit = false;
         Equipment.drawPiecesControl(this);
      };
      control.onPressAux = function()
      {
      };
      i++;
   }
   holder.handle.onPress = function()
   {
      Equipment.closePieces();
      gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
   };
   holder.back.onPress = function()
   {
      Equipment.scrollPieces(-1);
   };
   holder.on.onPress = function()
   {
      Equipment.scrollPieces(1);
   };
};

// The strip starts at the inner edge of its slot and is as high as the slot: the handle, the empty small slot, then
// as many pieces as the room up to the other column holds, between two arrows when they don't all fit
Equipment.placePieces = function()
{
   var strip = Equipment.pieceStrip;
   var slot = strip.slot;
   var size = Equipment.MINI_SIZE;
   var gap = Equipment.MINI_GAP;
   var step = size + gap;
   var pad = Equipment.PIECES_PAD;
   var count = strip.minis.length;
   var room = Equipment.stage.touch._width + Equipment.PLAYER_GAP;
   var fit;
   var offset;
   var i = 0;
   var mini;
   if(Equipment.piecesRoomCap > 0)
   {
      room = Math.min(room, Equipment.piecesRoomCap);
   }
   fit = Equipment.piecesFit(room, count);
   strip.scrolls = fit.scrolls;
   strip.shown = fit.shown;
   strip.first = Math.max(0, Math.min(strip.first, count - strip.shown));
   strip.visible = [strip.plus];
   offset = Equipment.HANDLE_WIDE + pad;
   Equipment.putPiece(strip.plus.box, offset, size);
   offset += step;
   Equipment.putPiecesControl(strip.clip.back, offset, strip.scrolls);
   if(strip.scrolls)
   {
      offset += Equipment.ARROW_WIDE + gap;
   }
   while(i < count)
   {
      mini = strip.minis[i];
      mini.box._visible = i >= strip.first && i < strip.first + strip.shown;
      if(mini.box._visible)
      {
         Equipment.putPiece(mini.box, offset, size);
         strip.visible.push(mini);
         offset += step;
      }
      i++;
   }
   Equipment.putPiecesControl(strip.clip.on, offset, strip.scrolls);
   if(strip.scrolls)
   {
      offset += Equipment.ARROW_WIDE + gap;
   }
   strip.wide = offset - gap + pad;
   strip.high = Equipment.SLOT_SIZE;
   strip.left = slot.column == 0 ? slot.box._x + Equipment.SLOT_SIZE : slot.box._x - strip.wide;
   strip.top = slot.box._y;
   Equipment.putPiecesControl(strip.clip.handle, 0, true);
   strip.clip.clear();
   strip.clip.lineStyle(1, Equipment.GOLD, 55, true);
   strip.clip.beginFill(0, 82);
   Equipment.roundBox(strip.clip, strip.left, strip.top, strip.wide, strip.high, 5);
   strip.clip.endFill();
   strip.clip.lineStyle();
   i = 0;
   while(i < strip.visible.length)
   {
      Equipment.showMini(strip.visible[i]);
      i++;
   }
};

// how many pieces a strip shows in this much room beside the handle and the empty small slot, and whether it needs
// the arrows, which take room of their own
Equipment.piecesFit = function(room, count)
{
   var step = Equipment.MINI_SIZE + Equipment.MINI_GAP;
   var fixed = Equipment.HANDLE_WIDE + 2 * Equipment.PIECES_PAD + step;
   var fits = Math.floor((room - fixed) / step);
   if(count <= fits)
   {
      return {shown:count, scrolls:false};
   }
   fits = Math.max(1, Math.floor((room - fixed - 2 * (Equipment.ARROW_WIDE + Equipment.MINI_GAP)) / step));
   return {shown:Math.min(count, fits), scrolls:true};
};

// a clip of the strip so far from the slot's inner edge, the strip runs right of the left column, left of the right one
Equipment.putPiece = function(box, offset, wide)
{
   var slot = Equipment.pieceStrip.slot;
   box._x = slot.column == 0 ? slot.box._x + Equipment.SLOT_SIZE + offset : slot.box._x - offset - wide;
   box._y = slot.box._y + Equipment.PIECES_PAD;
};

Equipment.putPiecesControl = function(control, offset, shown)
{
   control._visible = shown;
   if(shown)
   {
      Equipment.putPiece(control, offset, control.wide);
      Equipment.drawPiecesControl(control);
   }
};

// The handle's chevron points back into the slot. An arrow is dim when there's nothing more that way
Equipment.drawPiecesControl = function(control)
{
   var strip = Equipment.pieceStrip;
   var toDoll = strip.slot.column == 0;
   var high = Equipment.MINI_SIZE;
   var live = true;
   var right;
   if(control._name == "handle")
   {
      right = !toDoll;
   }
   else
   {
      right = control._name == "on" ? toDoll : !toDoll;
      live = control._name == "on" ? strip.first + strip.shown < strip.minis.length : strip.first > 0;
   }
   control.look.clear();
   control.look.beginFill(control.lit && live ? Equipment.GOLD : 0, control.lit && live ? 22 : 40);
   Equipment.roundBox(control.look, 0, 0, control.wide, high, 3);
   control.look.endFill();
   Equipment.drawChevron(control.look, control.wide, high, right, live ? (control.lit ? 100 : 75) : 25);
};

Equipment.scrollPieces = function(by)
{
   var strip = Equipment.pieceStrip;
   var first;
   if(strip == undefined || !strip.scrolls)
   {
      return undefined;
   }
   first = Math.max(0, Math.min(strip.first + by, strip.minis.length - strip.shown));
   if(first == strip.first)
   {
      return undefined;
   }
   strip.first = first;
   if(Equipment.anchorSlot.parent == strip.slot)
   {
      Equipment.closePopup();
   }
   Equipment.placePieces();
   Equipment.showModels(true);
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

Equipment.showMini = function(mini)
{
   Equipment.drawFrame(mini);
   Equipment.showIcon(mini);
   Equipment.drawEye(mini);
};

Equipment.closePieces = function(fromList)
{
   var strip = Equipment.pieceStrip;
   if(strip == undefined)
   {
      return undefined;
   }
   if(Equipment.anchorSlot.parent == strip.slot)
   {
      Equipment.closePopup();
   }
   if(Equipment.contextSlot.parent == strip.slot)
   {
      Equipment.closeContext();
   }
   Equipment.pieceStrip = undefined;
   strip.clip.removeMovieClip();
   Equipment.showSlot(strip.slot);
   if(!fromList)
   {
      Equipment.showModels(true);
   }
};

// After the list was made again the strip keeps its small slots while the pieces stayed the same,
// it's made anew when they changed and goes when one piece or none is left
Equipment.refreshPieces = function()
{
   var strip = Equipment.pieceStrip;
   var slot;
   var i = 0;
   if(strip == undefined)
   {
      return undefined;
   }
   slot = strip.slot;
   if(!Equipment.hasPieces(slot))
   {
      Equipment.closePieces(true);
      return undefined;
   }
   if(!Equipment.samePieces(strip, slot))
   {
      Equipment.openPieces(slot, true);
      return undefined;
   }
   while(i < slot.entries.length)
   {
      strip.minis[i].entries = [slot.entries[i]];
      i++;
   }
   Equipment.placePieces();
   Equipment.drawFrame(slot);
};

Equipment.samePieces = function(strip, slot)
{
   var i = 0;
   var shown;
   if(strip.minis.length != slot.entries.length)
   {
      return false;
   }
   while(i < slot.entries.length)
   {
      shown = strip.minis[i].entries[0];
      if(shown.formId != slot.entries[i].formId || shown.text != slot.entries[i].text)
      {
         return false;
      }
      i++;
   }
   return true;
};

Equipment.inPieces = function(x, y)
{
   var strip = Equipment.pieceStrip;
   var point = {x:x, y:y};
   Equipment.panel.globalToLocal(point);
   return point.x >= strip.left && point.x <= strip.left + strip.wide && point.y >= strip.top && point.y <= strip.top + strip.high;
};

// whether a press at this point belongs to the strip: on it, on its slot, in a window or a menu of it
Equipment.pressOnPieces = function(x, y)
{
   var strip = Equipment.pieceStrip;
   var owned = Equipment.ownerOf(Equipment.anchorSlot) == strip.slot && Equipment.window.hitTest(x, y, true);
   owned = owned || Equipment.context._visible && Equipment.context.hitTest(x, y, true);
   return owned || Equipment.inPieces(x, y) || strip.slot.box.hitTest(x, y, true);
};

// A click outside the strip puts it back, a press that became a drag of a row doesn't: the row may be on its way
// to the empty small slot. A click that closed a window or a menu of the strip leaves the strip out
Equipment.buildPieces = function()
{
   Equipment.piecesMouse = {};
   Equipment.piecesMouse.onMouseDown = function()
   {
      var strip = Equipment.pieceStrip;
      Equipment.piecesPress = undefined;
      if(strip == undefined || Equipment.pressOnPieces(_root._xmouse, _root._ymouse))
      {
         return undefined;
      }
      Equipment.piecesPress = {x:_root._xmouse, y:_root._ymouse, at:getTimer(), slot:strip.slot, owned:Equipment.ownerOf(Equipment.anchorSlot) == strip.slot || Equipment.context._visible && Equipment.ownerOf(Equipment.contextSlot) == strip.slot};
   };
   Equipment.piecesMouse.onMouseUp = function()
   {
      var press = Equipment.piecesPress;
      var dx;
      var dy;
      Equipment.piecesPress = undefined;
      // The press closed a window or a menu of the strip, whichever heard it first. A press on the tab of another
      // slot pulled out that slot's strip, which stays, the listener may hear the press before or after the tab
      if(press == undefined || press.owned || Equipment.pieceStrip == undefined || Equipment.pieceStrip.slot != press.slot || Math.abs(Equipment.piecesWindowClosedAt - press.at) < Equipment.PIECES_SAME_PRESS)
      {
         return undefined;
      }
      dx = (_root._xmouse - press.x) * Equipment.squash;
      dy = _root._ymouse - press.y;
      if(dx * dx + dy * dy <= Equipment.CLICK_SLACK * Equipment.CLICK_SLACK)
      {
         Equipment.closePieces();
      }
   };
   Equipment.piecesMouse.onMouseWheel = function(delta)
   {
      if(Equipment.pieceStrip != undefined && Equipment.inPieces(_root._xmouse, _root._ymouse))
      {
         Equipment.scrollPieces(delta > 0 ? -1 : 1);
      }
   };
   Mouse.addListener(Equipment.piecesMouse);
};

// Called every frame: the strip goes back in while an item is inspected or the panel is away. It comes out only
// on a click of its tab, a pointer resting on the slot leaves it in
Equipment.watchPieces = function()
{
   if(Equipment.panel != undefined && (Equipment.inspecting || !Equipment.panel._visible))
   {
      Equipment.closePieces();
   }
};

// A window of a piece of the strip, or of the slot whose strip is out, opens over or under the strip, where
// there's more room, so the strip stays in sight. Tells whether it placed the window
Equipment.placeByPieces = function(slot, window, bottom)
{
   var strip = Equipment.pieceStrip;
   var x;
   var roomAbove;
   var roomBelow;
   if(strip == undefined || Equipment.ownerOf(slot) != strip.slot)
   {
      return false;
   }
   if(slot.parent != undefined)
   {
      x = slot.column == 0 ? slot.box._x : slot.box._x + Equipment.MINI_SIZE - window.wide;
   }
   else
   {
      x = slot.column == 0 ? strip.left : strip.left + strip.wide - window.wide;
   }
   roomAbove = strip.top - Equipment.POPUP_GAP - Equipment.frame.top;
   roomBelow = bottom - strip.top - strip.high - Equipment.POPUP_GAP;
   window._x = Math.max(Equipment.frame.left, Math.min(x, Equipment.frame.right - window.wide));
   if(window.high <= roomAbove || roomAbove > roomBelow)
   {
      window._y = Math.max(Equipment.frame.top, strip.top - Equipment.POPUP_GAP - window.high);
   }
   else
   {
      window._y = Math.min(strip.top + strip.high + Equipment.POPUP_GAP, bottom - window.high);
   }
   return true;
};

Equipment.piecesReport = function()
{
   var strip = Equipment.pieceStrip;
   var names = [];
   var i = 0;
   if(strip == undefined)
   {
      return "-";
   }
   while(i < strip.minis.length)
   {
      names.push((strip.minis[i].box._visible ? "" : "~") + strip.minis[i].entries[0].text + (Equipment.looksHidden(strip.minis[i]) ? "(hidden)" : ""));
      i++;
   }
   return strip.slot.name + " [+ / " + names.join(" / ") + "] shows " + strip.shown + " from " + strip.first + (strip.scrolls ? " with arrows" : "") + " at " + Math.round(strip.left) + "," + Math.round(strip.top) + " " + Math.round(strip.wide) + "x" + Math.round(strip.high);
};

// for the test stand: the strip of a slot pulled out the way a click on its tab pulls it, "-" puts it back. It skips
// the mouse, a scene that checks the tab itself clicks it (panel-strip-tab)
Equipment.openPiecesOf = function(name)
{
   var slot = Equipment.slotNamed(name);
   if(slot != undefined && Equipment.hasPieces(slot))
   {
      Equipment.openPieces(slot);
   }
   else
   {
      Equipment.closePieces();
   }
};

// for the test stand: the room of a strip held to this many pixels, a narrow screen with a few rings, 0 lets it go
Equipment.capPiecesRoom = function(pixels)
{
   Equipment.piecesRoomCap = Number(pixels);
   if(Equipment.pieceStrip != undefined)
   {
      Equipment.placePieces();
      Equipment.showModels(true);
   }
};

// for the test stand: the strip scrolled by this many pieces, as the arrows do it
Equipment.scrollPiecesBy = function(by)
{
   Equipment.scrollPieces(Number(by));
};

// for the test stand: the small slot of the open strip at this place, "plus" for the empty one
Equipment.miniAt = function(place)
{
   var strip = Equipment.pieceStrip;
   if(strip == undefined)
   {
      return undefined;
   }
   return String(place) == "plus" ? strip.plus : strip.minis[Number(place)];
};

// for the test stand: a place on screen of a small slot or of a control of the strip ("handle", "back", "on"),
// "place,right,down" with shares of it from its middle
Equipment.aimMini = function(told)
{
   var parts = String(told).split(",");
   var mini = Equipment.miniAt(parts[0]);
   var box = mini != undefined ? mini.box : Equipment.pieceStrip.clip[parts[0]];
   var wide = mini != undefined ? Equipment.MINI_SIZE : box.wide;
   var share;
   Equipment.aimedX = Equipment.aimedY = -1;
   if(box == undefined || !box._visible)
   {
      return undefined;
   }
   share = Equipment.share(box._x + wide * (0.5 + Number(parts[1])), box._y + Equipment.MINI_SIZE * (0.5 + Number(parts[2])));
   Equipment.aimedX = share.x;
   Equipment.aimedY = share.y;
};

// for the test stand: an action of a window or a menu on a small slot, told as "key,place"
Equipment.actOnMini = function(told)
{
   var parts = String(told).split(",");
   var mini = Equipment.miniAt(parts[1]);
   if(mini != undefined)
   {
      Equipment.act(parts[0], mini);
   }
};

// for the test stand: a small slot pressed the way a click presses it, the eye of it by "eye,place"
Equipment.clickMini = function(told)
{
   var parts = String(told).split(",");
   var mini = Equipment.miniAt(parts[parts.length - 1]);
   if(mini == undefined)
   {
      return undefined;
   }
   if(parts[0] == "eye")
   {
      Equipment.toggleHidden(mini);
   }
   else
   {
      Equipment.click(mini);
   }
};

// For the test stand: a drop of the row with this text onto a small slot, "text,place". A form id of a mod late
// in the load order doesn't come through Papyrus whole, the text does
Equipment.dropOnMini = function(told)
{
   var parts = String(told).split(",");
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var mini = Equipment.miniAt(parts[1]);
   var i = 0;
   while(i < entries.length && entries[i].text != parts[0])
   {
      i++;
   }
   Equipment.dropped = i < entries.length && mini != undefined && Equipment.putOn(entries[i], mini);
};
