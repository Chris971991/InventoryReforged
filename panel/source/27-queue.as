// Moving many rows, one after another, each through the menu's own move of a whole stack as the menu of a row does
// it, so the game checks every one the way it checks a click: stolen things, what a quest keeps, room in a chest.
// A row moves, the menu lists anew and the next one goes a frame later. A row the game kept is left and counted.
// A question of the menu (a merchant short of gold asks whether to sell for less) stops the rest, the answer is
// the player's
Equipment.QUEUE_WAIT = 1500;

// rows: the entries to move, all from one side; done(moved, kept, stopped) is called at the end
Equipment.startQueue = function(rows, done)
{
   var keys = [];
   var key;
   var i = 0;
   var menu = Equipment.menu;
   if(Equipment.queue != undefined || rows.length == 0)
   {
      return false;
   }
   while(i < rows.length)
   {
      key = Equipment.rowKey(rows[i]);
      key.locked = Equipment.locked(rows[i]);
      keys.push(key);
      i++;
   }
   Equipment.queue = {keys:keys, own:Equipment.own(rows[0], Equipment.ownFlags()), moved:0, kept:0, done:done, step:"pick"};
   if(!Equipment.queueHeard)
   {
      Equipment.queueHeard = true;
      menu.itemCard.addEventListener("subMenuAction", Equipment, "queueAsked");
   }
   if(Equipment.queueClock == undefined)
   {
      Equipment.queueClock = menu.createEmptyMovieClip("equipmentQueue", menu.getNextHighestDepth());
   }
   Equipment.queueClock.onEnterFrame = function()
   {
      Equipment.queueTick();
   };
   Equipment.closeContext();
   return true;
};

// A worn copy lists as a row of its own under the same name as the spare, so a key keeps whether its row was
// locked too. Selling the spare helmet never takes the worn one
Equipment.queueMatches = function(entry, key, flags)
{
   return Equipment.isRow(entry, key) && Equipment.locked(entry) == key.locked && Equipment.own(entry, flags) == Equipment.queue.own;
};

// the row of a key on the side the queue moves from, the rows are new objects after every move
Equipment.queueRow = function(key)
{
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var flags = Equipment.ownFlags();
   var i = 0;
   while(i < entries.length)
   {
      if(Equipment.queueMatches(entries[i], key, flags))
      {
         return entries[i];
      }
      i++;
   }
   return undefined;
};

// how many all rows of a key hold, two rows of one name can't be told apart after a move
Equipment.queueHeld = function(key)
{
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var flags = Equipment.ownFlags();
   var held = 0;
   var i = 0;
   while(i < entries.length)
   {
      if(Equipment.queueMatches(entries[i], key, flags))
      {
         held += Math.max(Equipment.number(entries[i].count), 1);
      }
      i++;
   }
   return held;
};

// SkyUI selects only a row its filters let through, but all junk or a whole subcategory also holds rows the
// category, the other chips or the search hide. Those get selected past the filters like the row of a slot window
Equipment.queuePick = function(entry)
{
   if(Equipment.pickRow(entry))
   {
      return true;
   }
   if(!Equipment.listShows(entry))
   {
      Equipment.selectApart(entry);
   }
   return Equipment.menu.inventoryLists.itemList.selectedEntry == entry;
};

// a frame picks the next row, the next frame moves it, then the queue waits for the menu to list anew
Equipment.queueTick = function()
{
   var queue = Equipment.queue;
   var entry;
   if(queue == undefined)
   {
      Equipment.queueClock.onEnterFrame = undefined;
      return undefined;
   }
   if(queue.step == "wait")
   {
      // nothing listed anew: the game kept the row
      if(getTimer() - queue.sentAt > Equipment.QUEUE_WAIT)
      {
         queue.kept++;
         queue.step = "pick";
      }
      return undefined;
   }
   if(queue.step == "move")
   {
      entry = Equipment.queueRow(queue.current);
      if(entry == undefined)
      {
         queue.step = "pick";
         return undefined;
      }
      // a list that takes no selection now leaves the row where it is, it counts as kept
      if(!Equipment.queuePick(entry))
      {
         queue.kept++;
         queue.step = "pick";
         return undefined;
      }
      queue.count = Equipment.number(entry.count);
      queue.held = Equipment.queueHeld(queue.current);
      queue.sentAt = getTimer();
      queue.step = "wait";
      Equipment.menu._equipHand = undefined;
      Equipment.menu.onQuantityMenuSelect({amount:queue.count});
      return undefined;
   }
   while(queue.keys.length > 0)
   {
      queue.current = queue.keys.shift();
      entry = Equipment.queueRow(queue.current);
      if(entry != undefined)
      {
         Equipment.queuePick(entry);
         queue.step = "move";
         return undefined;
      }
   }
   Equipment.endQueue(false);
};

// the menu listed anew after a move: the row is gone or holds less, or the game kept it
Equipment.queueListed = function()
{
   var queue = Equipment.queue;
   if(queue == undefined || queue.step != "wait")
   {
      return undefined;
   }
   if(Equipment.queueHeld(queue.current) < queue.held)
   {
      queue.moved++;
   }
   else
   {
      queue.kept++;
   }
   queue.step = "pick";
};

// the card asks a question in the middle of the queue, the rest waits for another press
Equipment.queueAsked = function(event)
{
   if(Equipment.queue != undefined && event.opening)
   {
      Equipment.endQueue(true);
   }
};

Equipment.endQueue = function(stopped)
{
   var queue = Equipment.queue;
   Equipment.queue = undefined;
   Equipment.queueClock.onEnterFrame = undefined;
   if(queue.done != undefined)
   {
      queue.done(queue.moved, queue.kept + (stopped ? queue.keys.length + 1 : 0), stopped);
   }
};

// for the test stand: whether a queue still runs and what it did so far
Equipment.queueReport = function(note)
{
   var queue = Equipment.queue;
   skse.Log("PANELTEST queue " + note + ": " + (queue == undefined ? "none" : queue.step + ", moved " + queue.moved + ", kept " + queue.kept + ", left " + queue.keys.length) + ", rows " + Equipment.menu.inventoryLists.itemList.entryList.length);
};
