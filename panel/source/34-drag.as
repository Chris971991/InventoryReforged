Equipment.DRAG_START = 7;
Equipment.GHOST_ICON = 22;
Equipment.GHOST_PAD = 6;
Equipment.GHOST_AWAY = 14;
Equipment.HOLD_MS = 4000;

// A row of the list can be dragged onto a slot or onto the doll to put it on. The game equips on the press
// of a row (AttemptEquip), so a press is held back until the button goes up: a press that didn't move
// equips as before, one that became a drag equips only where it's dropped
Equipment.buildDrag = function()
{
   var menu = Equipment.menu;
   Equipment.mouse = {};
   Equipment.mouse.onMouseDown = function()
   {
      // a row the pointer crossed on its way to the lens and was pressed on is the one the press is for
      Equipment.endLensWait();
      Equipment.mouseHeld = true;
      Equipment.pressedRow();
   };
   Equipment.mouse.onMouseUp = function()
   {
      Equipment.mouseHeld = false;
      Equipment.released();
   };
   Equipment.mouse.onMouseMove = function()
   {
      Equipment.dragMove();
      Equipment.lightEye();
      Equipment.lightClose();
      Equipment.followLensAim();
      Equipment.leaveRows();
   };
   Mouse.addListener(Equipment.mouse);

   menu.equipmentAttemptEquip = menu.AttemptEquip;
   menu.AttemptEquip = function(a_slot, a_checkOverList)
   {
      if(Equipment.pressUsed() || Equipment.padWears(this.inventoryLists.itemList.selectedEntry))
      {
         return undefined;
      }
      Equipment.endLensWait();
      if(!Equipment.settings.dragAndDrop)
      {
         if(!this.shouldProcessItemsListInput(a_checkOverList == undefined ? true : a_checkOverList) || !this.confirmSelectedEntry() || !Equipment.swapTwin(this.inventoryLists.itemList.selectedEntry))
         {
            this.equipmentAttemptEquip(a_slot, a_checkOverList);
         }
         return undefined;
      }
      // SkyUI checks the pointer is on the selected row, that holds at the press and maybe not at the release,
      // so the row is checked and kept now and equipped later as it was
      if(!this.shouldProcessItemsListInput(a_checkOverList == undefined ? true : a_checkOverList) || !this.confirmSelectedEntry())
      {
         return undefined;
      }
      var list = this.inventoryLists.itemList;
      Equipment.heldEquip = {slot:a_slot, index:list._selectedIndex, entry:list.selectedEntry, frames:0, at:getTimer()};
   };
};

// A gamepad has no drag. In a follower's trade the list a slot of theirs filters is what can go into that slot, so A on
// a row puts it there, given first when it's the player's. Elsewhere A moves the row as the game does
Equipment.padWears = function(entry)
{
   if(Equipment.menu._platform == 0 || !Equipment.dressesOther() || Equipment.activeSlot == undefined || entry == undefined || !Equipment.fits(entry, Equipment.activeSlot))
   {
      return false;
   }
   Equipment.putOn(entry, Equipment.activeSlot);
   return true;
};

// a held back equip goes on once the button is up, a drag drops it
Equipment.dragFrame = function()
{
   var held = Equipment.heldEquip;
   if(held == undefined)
   {
      return undefined;
   }
   held.frames++;
   if(Equipment.dragging)
   {
      Equipment.heldEquip = undefined;
      return undefined;
   }
   // A mouse event may come a frame after the game's call, so the first frame always waits. After that only
   // a press held on a row waits, and never longer than a few seconds: a lost release must not lose the equip
   if(held.frames < 2 || Equipment.mouseHeld && Equipment.dragFrom != undefined && getTimer() - held.at < Equipment.HOLD_MS)
   {
      return undefined;
   }
   Equipment.heldEquip = undefined;
   Equipment.equipHeld(held);
};

// the menu's own equip, or its take in a container, for the row that was pressed. The pointer was checked at the press
Equipment.equipHeld = function(held)
{
   var menu = Equipment.menu;
   var list = menu.inventoryLists.itemList;
   if(list.entryList[held.index] != held.entry || Equipment.swapTwin(held.entry))
   {
      return undefined;
   }
   list._selectedIndex = held.index;
   menu.confirmSelectedEntry = function()
   {
      return true;
   };
   menu.equipmentAttemptEquip(held.slot, false);
   delete menu.confirmSelectedEntry;
};

// The row under the pointer, if it's a row of the item list. A list that scrolls smoothly (Smooth Scrolling for SkyUI
// and the like) keeps its rows in a clip of its own under a mask, a row may lie deeper in the list than its child
Equipment.rowUnderMouse = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   var top = Mouse.getTopMostEntity();
   while(top != undefined)
   {
      if(top.itemIndex != undefined && top._visible && Equipment.inside(top._parent, list))
      {
         return list.entryList[top.itemIndex];
      }
      top = top._parent;
   }
   return undefined;
};

Equipment.inside = function(clip, outer)
{
   while(clip != undefined)
   {
      if(clip == outer)
      {
         return true;
      }
      clip = clip._parent;
   }
   return false;
};

Equipment.pressedRow = function()
{
   var entry;
   Equipment.dragFrom = undefined;
   if(!Equipment.settings.dragAndDrop || !Equipment.listsAnswer() || Equipment.anchorSlot != undefined || Equipment.lookOnly())
   {
      return undefined;
   }
   entry = Equipment.rowUnderMouse();
   if(entry == undefined || Equipment.slotOf(entry) == undefined)
   {
      return undefined;
   }
   Equipment.dragFrom = {entry:entry, x:_root._xmouse, y:_root._ymouse};
};

Equipment.dragMove = function()
{
   var from = Equipment.dragFrom;
   var dx;
   var dy;
   if(from == undefined)
   {
      return undefined;
   }
   if(!Equipment.dragging)
   {
      dx = (_root._xmouse - from.x) * Equipment.squash;
      dy = _root._ymouse - from.y;
      if(!Equipment.mouseHeld || dx * dx + dy * dy < Equipment.DRAG_START * Equipment.DRAG_START)
      {
         return undefined;
      }
      Equipment.startDrag(from.entry);
   }
   Equipment.moveGhost();
   Equipment.markTargets();
};

Equipment.startDrag = function(entry)
{
   var ghost = Equipment.over.createEmptyMovieClip("equipmentGhost", Equipment.over.getNextHighestDepth());
   var slot = Equipment.slotOf(entry);
   var words = Equipment.writeLine(ghost, "words", 2, Equipment.gameName(entry.text), 14, Equipment.WHITE);
   var size = Equipment.GHOST_ICON;
   var pad = Equipment.GHOST_PAD;
   var wide = size + 3 * pad + words._width;
   var high = size + 2 * pad;
   Equipment.dragging = true;
   Equipment.dragEntry = entry;
   Equipment.closeContext();
   ghost.hitTestDisable = true;
   Equipment.panelBox(ghost, wide, high, 85, 60);
   Equipment.drawIcon(ghost, slot.icon, pad, pad, size, Equipment.GOLD, 100);
   words._x = size + 2 * pad;
   words._y = (high - words._height) / 2;
   ghost._alpha = 92;
   Equipment.ghost = ghost;
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

Equipment.moveGhost = function()
{
   Equipment.placeGhost(_root._xmouse, _root._ymouse);
};

// the ghost hangs right of the pointer, near the right edge of the screen it hangs left of it
Equipment.placeGhost = function(x, y)
{
   var ghost = Equipment.ghost;
   var screen = Equipment.screenRect();
   var point = Equipment.toPanel({x:x, y:y});
   var left = point.x + Equipment.GHOST_AWAY + ghost._width > screen.x + screen.width - Equipment.MARGIN;
   ghost._x = left ? point.x - Equipment.GHOST_AWAY - ghost._width : point.x + Equipment.GHOST_AWAY;
   ghost._y = point.y + 10;
};

// the slot the pointer is over, the stage of the doll counts as the slot the item goes to by itself
Equipment.slotUnderMouse = function()
{
   var slot = Equipment.slotAt(_root._xmouse, _root._ymouse);
   if(slot != undefined)
   {
      return slot;
   }
   if(Equipment.stage.hitTest(_root._xmouse, _root._ymouse, false) && Equipment.dragEntry != undefined)
   {
      return Equipment.slotOf(Equipment.dragEntry);
   }
   return undefined;
};

// slots that take the dragged item light up, the one under the pointer brightest
Equipment.markTargets = function()
{
   var under = Equipment.slotUnderMouse();
   var cells = Equipment.cells();
   var i = 0;
   var slot;
   var takes;
   while(i < cells.length)
   {
      slot = cells[i];
      takes = Equipment.dragging && Equipment.fits(Equipment.dragEntry, slot);
      if(takes != slot.takes || (slot == under) != slot.hovered)
      {
         slot.takes = takes;
         slot.hovered = slot == under;
         Equipment.drawFrame(slot);
      }
      i++;
   }
};

Equipment.released = function()
{
   var slot;
   if(!Equipment.dragging)
   {
      Equipment.dragFrom = undefined;
      return undefined;
   }
   slot = Equipment.slotUnderMouse();
   if(slot != undefined && Equipment.fits(Equipment.dragEntry, slot))
   {
      Equipment.putOn(Equipment.dragEntry, slot);
   }
   Equipment.endDrag();
};

Equipment.endDrag = function()
{
   var cells = Equipment.cells();
   var i = 0;
   Equipment.dragging = false;
   Equipment.dragFrom = undefined;
   Equipment.dragEntry = undefined;
   Equipment.ghost.removeMovieClip();
   Equipment.ghost = undefined;
   while(i < cells.length)
   {
      cells[i].takes = false;
      cells[i].hovered = false;
      Equipment.drawFrame(cells[i]);
      i++;
   }
};

// for the test stand: a drag of the item with this form id onto a slot, as a hand would do it
Equipment.dropOn = function(told)
{
   var parts = String(told).split(",");
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var i = 0;
   while(i < entries.length && entries[i].formId != Number(parts[0]))
   {
      i++;
   }
   Equipment.dropped = i < entries.length && Equipment.putOn(entries[i], Equipment.slotNamed(parts[1]));
};

// for the test stand: the same drop for the row with this text, for twins that share a form id
Equipment.dropNamed = function(told)
{
   var parts = String(told).split(",");
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var i = 0;
   while(i < entries.length && entries[i].text != parts[0])
   {
      i++;
   }
   Equipment.dropped = i < entries.length && Equipment.putOn(entries[i], Equipment.slotNamed(parts[1]));
};

// for the stand pictures: a row dragged and held over a slot. The stand can't move the pointer of the menu,
// so the ghost and the lit slots go where a hand would have put them
Equipment.showDrag = function(told)
{
   var parts = String(told).split(",");
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var over = Equipment.slotNamed(parts[1]);
   var size = Equipment.SLOT_SIZE;
   var point = {x:over.box._x + size * 0.55, y:over.box._y + size * 0.6};
   var i = 0;
   var slot;
   while(i < entries.length && entries[i].formId != Number(parts[0]))
   {
      i++;
   }
   if(i == entries.length || over == undefined)
   {
      return undefined;
   }
   if(Equipment.dragging)
   {
      Equipment.endDrag();
   }
   Equipment.startDrag(entries[i]);
   Equipment.panel.localToGlobal(point);
   Equipment.placeGhost(point.x, point.y);
   i = 0;
   while(i < Equipment.SLOTS.length)
   {
      slot = Equipment.SLOTS[i];
      slot.takes = Equipment.fits(Equipment.dragEntry, slot);
      slot.hovered = slot == over;
      Equipment.drawFrame(slot);
      i++;
   }
};
