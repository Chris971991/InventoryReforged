Equipment.JUNK_BUTTON_GAP = 18;
Equipment.JUNK_NOTE_TIME = 4000;

// Junk is marked on the item's base form, so every copy of it is junk. The plugin keeps the marks in the save.
// Selling or storing all junk never takes what is worn, a favorite or anything a quest wants
Equipment.locked = function(entry)
{
   return entry.equipState > 0 || entry.favorite == true || Equipment.questNeeds(entry);
};

Equipment.toggleJunk = function(entry)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var junk = entry.equipmentJunk != true;
   var i = 0;
   if(entry == undefined || Equipment.plugin() == undefined || !Equipment.plugin().SetJunk(entry.formId, junk))
   {
      return undefined;
   }
   while(i < list.entryList.length)
   {
      if(list.entryList[i].formId == entry.formId)
      {
         list.entryList[i].equipmentJunk = junk;
      }
      i++;
   }
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
   list.requestUpdate();
   Equipment.junkChanged();
};

// the key of junk marks the row under the pointer, a held key repeats so only the press counts
Equipment.junkKey = function(details)
{
   var key = Equipment.number(Equipment.settings.junkKey);
   var entry;
   if(!Equipment.settings.junk || key <= 0 || details.skseKeycode != key || !Shared.GlobalFunc.IsKeyPressed(details))
   {
      return false;
   }
   entry = Equipment.menu.inventoryLists.itemList.selectedEntry;
   if(Shared.GlobalFunc.IsKeyPressed(details, false) && !Equipment.inspecting && Equipment.listsAnswer() && entry != undefined && Equipment.queue == undefined)
   {
      Equipment.toggleJunk(entry);
   }
   return true;
};

// where all junk can go from here: a shop buys it, a chest takes it, and the chest's own junk comes to the player
// while its side is in view. Nothing in a shop's side or a follower's trade
Equipment.junkVerb = function()
{
   var menu = Equipment.menu;
   if(Equipment.kind == "barter" && !menu.isViewingVendorItems())
   {
      return "sell";
   }
   if(Equipment.kind == "container" && !menu.bNPCMode)
   {
      return menu.isViewingContainer() ? "take" : "store";
   }
   return undefined;
};

// the junk that may go, every row once: the player's, or the chest's when it's taken
Equipment.junkRows = function()
{
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var flags = Equipment.ownFlags();
   var theirs = Equipment.junkVerb() == "take";
   var rows = [];
   var i = 0;
   while(i < entries.length)
   {
      if(entries[i].equipmentJunk == true && !Equipment.locked(entries[i]) && Equipment.own(entries[i], flags) != theirs)
      {
         rows.push(entries[i]);
      }
      i++;
   }
   return rows;
};

Equipment.junkWords = function(verb, count)
{
   if(verb == "take")
   {
      return Equipment.say("Take all junk (#)", count);
   }
   return verb == "sell" ? Equipment.say("Sell all junk (#)", count) : Equipment.say("Store all junk (#)", count);
};

// every way to move all junk asks first with the whole list, see 28-junk-ask.as
Equipment.moveJunk = function()
{
   var verb = Equipment.junkVerb();
   if(verb == undefined || !Equipment.listsAnswer())
   {
      return undefined;
   }
   Equipment.askJunk(verb, Equipment.junkRows());
};

Equipment.moveJunkNow = function(verb)
{
   if(verb != Equipment.junkVerb() || !Equipment.listsAnswer())
   {
      return undefined;
   }
   Equipment.startQueue(Equipment.junkRows(), function(moved, kept, stopped)
   {
      Equipment.junkNote(verb, moved, kept, stopped);
   });
};

// what the last sale or store did stays on the button for a moment
Equipment.junkNote = function(verb, moved, kept, stopped)
{
   var words;
   if(verb == "take")
   {
      words = stopped ? Equipment.say("Took #, the rest waits", moved) : (kept > 0 ? Equipment.say("Took #1, #2 stayed", moved, kept) : Equipment.say("Took #", moved));
   }
   else if(stopped)
   {
      words = verb == "sell" ? Equipment.say("Sold #, the merchant asks about the rest", moved) : Equipment.say("Stored #, the rest waits", moved);
   }
   else if(kept > 0)
   {
      words = verb == "sell" ? Equipment.say("Sold #1, #2 stayed", moved, kept) : Equipment.say("Stored #1, #2 stayed", moved, kept);
   }
   else
   {
      words = verb == "sell" ? Equipment.say("Sold #", moved) : Equipment.say("Stored #", moved);
   }
   skse.Log("EquipmentPanel junk " + verb + ": moved " + moved + ", kept " + kept + (stopped ? ", stopped at a question" : ""));
   Equipment.junkNoteWords = words;
   Equipment.junkNoteUntil = getTimer() + Equipment.JUNK_NOTE_TIME;
   Equipment.junkChanged();
   clearInterval(Equipment.junkNoteTimer);
   Equipment.junkNoteTimer = setInterval(function()
   {
      clearInterval(Equipment.junkNoteTimer);
      Equipment.junkChanged();
   }, Equipment.JUNK_NOTE_TIME + 50);
};

Equipment.junkButtonWanted = function()
{
   var noted = Equipment.junkNoteUntil != undefined && getTimer() < Equipment.junkNoteUntil;
   return Equipment.settings.junk && Equipment.junkVerb() != undefined && (noted || Equipment.junkRows().length > 0);
};

// the button sits at the right end of the strip of subcategories, the chips keep to its left. It's the list's
// size of words, in the accent while it can do something
Equipment.placeJunkButton = function(shown, top, right)
{
   var lists = Equipment.menu.inventoryLists;
   var button = Equipment.junkButton;
   var verb = Equipment.junkVerb();
   var noted = Equipment.junkNoteUntil != undefined && getTimer() < Equipment.junkNoteUntil;
   var count;
   var words;
   Equipment.junkButtonShown = shown;
   if(button == undefined)
   {
      button = Equipment.junkButton = lists.panelContainer.createEmptyMovieClip("equipmentJunkButton", lists.panelContainer.getNextHighestDepth());
      button.onRollOver = function()
      {
         this.lit = true;
         Equipment.drawJunkButton(this);
      };
      button.onRollOut = button.onReleaseOutside = function()
      {
         this.lit = false;
         Equipment.drawJunkButton(this);
      };
      button.onPress = function()
      {
         if(!Equipment.queueBusy())
         {
            Equipment.moveJunk();
         }
      };
   }
   button._visible = shown;
   if(!shown)
   {
      return 0;
   }
   count = Equipment.pieces(Equipment.junkRows());
   words = noted ? Equipment.junkNoteWords : Equipment.junkWords(verb, count);
   button.ready = !noted && count > 0;
   button.clear();
   if(button.words != undefined)
   {
      button.words.removeTextField();
      button.sign.removeMovieClip();
   }
   button.createEmptyMovieClip("sign", 2);
   Equipment.writeLine(button, "words", 1, words.toUpperCase(), Equipment.CHIP_TEXT, button.ready ? Equipment.accentInk() : Equipment.inkOf("chip"));
   button.words._x = Equipment.CHIP_ICON + Equipment.ICON_TO_WORD;
   button.words._y = (Equipment.STRIP_HIGH - button.words._height) / 2;
   Equipment.drawIcon(button.sign, verb == "sell" ? "coin" : "transfer", 0, (Equipment.STRIP_HIGH - Equipment.CHIP_ICON) / 2, Equipment.CHIP_ICON, button.ready ? Equipment.accentInk() : Equipment.inkOf("chip"), 100);
   button.wide = button.words._x + button.words._width;
   button._x = right - button.wide;
   button._y = top;
   Equipment.drawJunkButton(button);
   return button.wide + Equipment.JUNK_BUTTON_GAP;
};

Equipment.drawJunkButton = function(button)
{
   button.clear();
   button.beginFill(0, 0);
   button.moveTo(0, 0);
   button.lineTo(button.wide, 0);
   button.lineTo(button.wide, Equipment.STRIP_HIGH);
   button.lineTo(0, Equipment.STRIP_HIGH);
   button.lineTo(0, 0);
   button.endFill();
   button.words._alpha = button.sign._alpha = button.ready ? (button.lit ? 100 : 85) : Equipment.CHIP_DIM;
};

Equipment.queueBusy = function()
{
   return Equipment.queue != undefined || !Equipment.listsAnswer();
};

// the marks or the rows changed: the strip opens or shuts for the button, or only its words change
Equipment.junkChanged = function()
{
   if(Equipment.chips == undefined)
   {
      return undefined;
   }
   Equipment.showChips();
};

// for the test stand: marks the item of a form id as junk the way the junk key does on its row
Equipment.markJunk = function(formId)
{
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var i = 0;
   while(i < entries.length)
   {
      if(String(entries[i].formId) == String(formId) && entries[i].equipmentJunk != true)
      {
         Equipment.toggleJunk(entries[i]);
         return undefined;
      }
      i++;
   }
};

// for the test stand: where the junk button is as aimedX, aimedY, and what it says
Equipment.aimJunkButton = function()
{
   var button = Equipment.junkButton;
   var point;
   var share;
   Equipment.aimedX = Equipment.aimedY = -1;
   if(button == undefined || !button._visible)
   {
      skse.Log("PANELTEST junk button: none");
      return undefined;
   }
   point = {x:button.wide / 2, y:Equipment.STRIP_HIGH / 2};
   button.localToGlobal(point);
   share = Equipment.screenShare(point.x, point.y);
   Equipment.aimedX = share.x;
   Equipment.aimedY = share.y;
   skse.Log("PANELTEST junk button: " + button.words.text + ", ready " + button.ready + ", top " + String(Mouse.getTopMostEntity(point.x, point.y, true)));
};
