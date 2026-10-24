Equipment.BACKDROP_SHADE = 25;

// A window opened from a slot is modal: a dark sheet under the panel takes every click outside the window
// and closes it, the slots stay above the sheet so another slot opens its own window at once
Equipment.buildPopup = function()
{
   var screen = Equipment.screenRect();
   var backdrop = Equipment.backdrop;
   backdrop.createTextField("touch", 1, screen.x, screen.y, screen.width, screen.height);
   backdrop.touch.selectable = false;
   backdrop.beginFill(0, Equipment.BACKDROP_SHADE);
   backdrop.moveTo(screen.x, screen.y);
   backdrop.lineTo(screen.x + screen.width, screen.y);
   backdrop.lineTo(screen.x + screen.width, screen.y + screen.height);
   backdrop.lineTo(screen.x, screen.y + screen.height);
   backdrop.lineTo(screen.x, screen.y);
   backdrop.endFill();
   backdrop._visible = false;
   backdrop.onPress = function()
   {
      Equipment.usePress();
      Equipment.closePopup();
   };
   backdrop.onPressAux = function()
   {
      Equipment.closePopup();
   };

   Equipment.actions = Equipment.over.createEmptyMovieClip("equipmentActions", Equipment.over.getNextHighestDepth());
   Equipment.actions._visible = false;
};

// the sheet has to lie under the panel and over the list, so it's made before the panel
Equipment.buildBackdrop = function()
{
   Equipment.backdrop = Equipment.under.createEmptyMovieClip("equipmentBackdrop", Equipment.under.getNextHighestDepth());
};

Equipment.openPopup = function(slot)
{
   var previous = Equipment.anchorSlot;
   Equipment.anchorSlot = slot;
   Equipment.backdrop._visible = true;
   Equipment.window.hitTestDisable = false;
   Equipment.windowWanted = true;
   Equipment.makeActions(slot);
   if(previous != undefined && previous != slot)
   {
      Equipment.drawFrame(previous);
   }
   Equipment.drawFrame(slot);
   Equipment.showWindow();
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

Equipment.closePopup = function()
{
   var slot = Equipment.anchorSlot;
   var list = Equipment.menu.inventoryLists.itemList;
   var apart = Equipment.shownApart != undefined || Equipment.shownPiece != undefined;
   if(slot == undefined)
   {
      return undefined;
   }
   if(Equipment.ownerOf(slot) == Equipment.pieceStrip.slot)
   {
      Equipment.piecesWindowClosedAt = getTimer();
   }
   Equipment.anchorSlot = undefined;
   Equipment.shownApart = undefined;
   Equipment.shownPiece = undefined;
   Equipment.backdrop._visible = false;
   Equipment.actions._visible = false;
   Equipment.window.hitTestDisable = true;
   Equipment.windowWanted = false;
   Equipment.drawFrame(slot);
   // The row the window found lets go too, or SkyUI keeps its card and model up where the window was. A row the
   // list hides may have been let go of by the list already, or swapped for another one without a word
   if(apart && list.selectedIndex < 0)
   {
      Equipment.dropApart();
   }
   else if(apart || list.selectedEntry == slot.entries[0])
   {
      list.selectedIndex = -1;
   }
   Equipment.showWindow();
   Equipment.coverWindow();
};

// what a slot window offers: a close look, taking the piece off, hiding it on the character
Equipment.actionsFor = function(slot)
{
   var entry = slot.entries[0];
   var actions = [];
   if(Equipment.inspectable(entry))
   {
      actions.push({key:"inspect", words:Equipment.say("Inspect"), icon:"lens"});
   }
   if(Equipment.canTakeOff(slot.taken ? Equipment.slotNamed("right") : slot))
   {
      actions.push({key:"takeOff", words:Equipment.say("Take off"), icon:"unequip"});
   }
   if(Equipment.canToggle(slot))
   {
      actions.push({key:"hide", words:Equipment.looksHidden(slot) ? Equipment.say("Show") : Equipment.say("Hide"), icon:Equipment.looksHidden(slot) ? "eye" : "eyeShut"});
   }
   return actions;
};

Equipment.makeActions = function(slot)
{
   var holder = Equipment.actions;
   var actions = Equipment.actionsFor(slot);
   var i = 0;
   var button;
   for(var name in holder)
   {
      if(holder[name] instanceof MovieClip)
      {
         holder[name].removeMovieClip();
      }
   }
   while(i < actions.length)
   {
      button = holder.createEmptyMovieClip("action" + i, i + 1);
      button.action = actions[i];
      button.createTextField("touch", 0, 0, 0, 10, Equipment.ACTION_HIGH);
      button.touch.selectable = false;
      button.createEmptyMovieClip("look", 1);
      Equipment.writeLine(button, "words", 2, actions[i].words, 13, Equipment.WHITE);
      button.onRollOver = function()
      {
         this.lit = true;
         Equipment.drawAction(this);
      };
      button.onRollOut = button.onReleaseOutside = function()
      {
         this.lit = false;
         Equipment.drawAction(this);
      };
      button.onPress = function()
      {
         Equipment.act(this.action.key, Equipment.anchorSlot);
      };
      i++;
   }
};

// the buttons share the width of the window under the card and the numbers
Equipment.placeActions = function()
{
   var holder = Equipment.actions;
   var window = Equipment.window;
   var count = 0;
   var i = 0;
   var wide;
   var button;
   holder._visible = Equipment.anchorSlot != undefined && window._visible;
   if(!holder._visible)
   {
      return undefined;
   }
   while(holder["action" + count] != undefined)
   {
      count++;
   }
   wide = (window.wide - Equipment.ACTION_GAP * (count + 1)) / Math.max(count, 1);
   holder._x = window._x;
   holder._y = window._y + window.high - Equipment.ACTION_GAP - Equipment.ACTION_HIGH - Equipment.ACTION_EDGE;
   while(i < count)
   {
      button = holder["action" + i];
      button._x = Equipment.ACTION_GAP + i * (wide + Equipment.ACTION_GAP);
      button.wide = wide;
      button.touch._width = wide;
      Equipment.drawAction(button);
      i++;
   }
};

Equipment.drawAction = function(button)
{
   var look = button.look;
   var size = Equipment.ACTION_ICON;
   var words = button.words;
   var inner = size + 6 + words._width;
   var x = (button.wide - inner) / 2;
   // a lit button is a wash of the accent, on the paper of a light skin its words take the darker accent
   look.clear();
   look.lineStyle(1, button.lit ? Equipment.accentInk() : Equipment.WHITE, button.lit ? 90 : 35, true);
   look.beginFill(button.lit ? Equipment.GOLD : 1710618, button.lit ? 25 : 80);
   Equipment.roundBox(look, 0, 0, button.wide, Equipment.ACTION_HIGH, 4);
   look.endFill();
   Equipment.drawIcon(look, button.action.icon, x, (Equipment.ACTION_HIGH - size) / 2, size, button.lit ? Equipment.accentInk() : Equipment.PARCHMENT, 100);
   words._x = x + size + 6;
   words._y = (Equipment.ACTION_HIGH - words._height) / 2;
   words.textColor = button.lit ? Equipment.accentInk() : Equipment.WHITE;
};

// what the buttons of a slot window and the lines of the slot menu do
Equipment.act = function(key, slot)
{
   if(slot == undefined)
   {
      return undefined;
   }
   switch(key)
   {
      case "details":
         Equipment.showWorn(slot.taken ? Equipment.slotNamed("right") : slot, true);
         break;
      case "inspect":
         Equipment.closePopup();
         Equipment.inspectEntry(slot.entries[0], true);
         break;
      case "takeOff":
         Equipment.closePopup();
         Equipment.takeOff(slot.taken ? Equipment.slotNamed("right") : slot);
         break;
      case "hide":
         Equipment.toggleHidden(slot);
         if(Equipment.anchorSlot == slot)
         {
            Equipment.makeActions(slot);
            Equipment.placeActions();
         }
         break;
      case "filter":
         Equipment.closePopup();
         Equipment.filterSlot(Equipment.ownerOf(slot));
         break;
      case "select":
         Equipment.findWorn(slot.taken ? Equipment.slotNamed("right") : slot);
   }
};

// For the test stand: the slot window, whether the list shows its row, and what the list and the card show now
Equipment.slotWindowReport = function(note)
{
   var lists = Equipment.menu.inventoryLists;
   var list = lists.itemList;
   var slot = Equipment.anchorSlot;
   var entry = slot != undefined ? slot.entries[0] : undefined;
   var card = Equipment.menu.itemCard;
   skse.Log("PANELTEST slot window " + note + ": " + (slot == undefined ? "none" : slot.name + " " + entry.text + (Equipment.listShows(entry) ? " in the list" : " apart from the list")) + ", shown " + Equipment.window._visible + ", selected " + list._selectedIndex + " " + list.selectedEntry.text + ", card " + card.itemInfo.name + " visible " + card._visible + " alpha " + Math.round(card._alpha) + ", category " + lists.categoryList.selectedEntry.text + ", chips " + Equipment.chosenChips.join("/") + ", filter " + (Equipment.activeSlot == undefined ? "-" : Equipment.activeSlot.name) + ", search \"" + lists._nameFilter.filterText + "\", rows " + list.listEnumeration.size() + ", stolen line \"" + Equipment.window.stolen.text + "\"");
};

// for the test stand: an action of a slot window or the slot menu on a slot, told as "key,slot"
Equipment.actOn = function(told)
{
   var parts = String(told).split(",");
   Equipment.act(parts[0], Equipment.slotNamed(parts[1]));
};
