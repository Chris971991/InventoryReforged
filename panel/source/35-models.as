Equipment.MODEL_INSET = 5;
Equipment.FOLLOW_PERIOD = 200;
Equipment.FOLLOW_TIMES = 25;
// slots whose piece is shown as it sits on the body. A ring on a finger is too small to see, it shows its own model like weapons and ammo
Equipment.WORN_LOOK = ["amulet", "head", "body", "hands", "feet", "other"];

// a place in the panel's units as shares of the screen, the form the plugin takes
Equipment.share = function(x, y)
{
   var screen = Equipment.screenRect();
   return {x:(x - screen.x) / screen.width, y:(y - screen.y) / screen.height};
};

// the same for a place of the stage, what localToGlobal gives or a place of the skin's own clips in the menu
Equipment.screenShare = function(x, y)
{
   var screen = Stage.visibleRect;
   return {x:(x - screen.x) / screen.width, y:(y - screen.y) / screen.height};
};

// Item models of the slots are built again only when what's worn changed
// The player's copy is built with them and a few more times after, because the game
// puts a changed item on the player's model some frames later
// The small slots of a strip that's out get models of their own, they go with the strip. A strip going out
// or in changes nothing on the body, the doll isn't followed then
Equipment.showModels = function(stripOnly)
{
   var plugin = Equipment.plugin();
   var cells = Equipment.cells();
   var items = [];
   var middle;
   var slot;
   var size;
   var inset;
   var i = 0;
   // the item browser stands where the slots are, its own model is the only one then
   if(plugin == undefined || Equipment.browsing)
   {
      return undefined;
   }
   while(i < cells.length)
   {
      slot = cells[i];
      size = Equipment.cellSize(slot);
      inset = slot.parent != undefined ? Equipment.MINI_INSET : Equipment.MODEL_INSET;
      // a hidden piece isn't on the body to be copied from, it shows its own model. A spell has none
      if(slot.entries.length > 0 && !slot.taken && !Equipment.isSpell(slot.entries[0]))
      {
         middle = Equipment.share(slot.box._x + size / 2, slot.box._y + size / 2);
         items.push(Equipment.modelIndex(slot), slot.entries[0].formId, middle.x, middle.y, Equipment.has(Equipment.WORN_LOOK, slot.name), (size - 2 * inset) / Stage.visibleRect.height);
      }
      i++;
   }
   if(items.join(",") == Equipment.wearing)
   {
      return undefined;
   }
   Equipment.wearing = items.join(",");
   Equipment.modelled = plugin.ShowSlots(items);
   i = 0;
   while(i < cells.length)
   {
      Equipment.showIcon(cells[i]);
      i++;
   }
   if(!stripOnly)
   {
      Equipment.followPlayerAgain();
   }
};

// The plugin tells of the game dressing the player's body again with nothing equipped, a mod that tries a
// piece on or dresses the body its own way. The doll follows the body for a while, as after an equip
Equipment.bodyRebuilt = function()
{
   Equipment.followPlayerAgain();
};

Equipment.followPlayerAgain = function()
{
   Equipment.showPlayer();
   Equipment.followed = 0;
   clearInterval(Equipment.following);
   Equipment.following = setInterval(Equipment.followPlayer, Equipment.FOLLOW_PERIOD);
};

Equipment.followPlayer = function()
{
   Equipment.showPlayer();
   Equipment.followed++;
   if(Equipment.followed >= Equipment.FOLLOW_TIMES)
   {
      clearInterval(Equipment.following);
   }
};

// the player gets the room between the two columns of slots, the plugin fits the body into it. The room is told in
// heights of the screen both ways, the panel's units are square on screen
Equipment.showPlayer = function()
{
   var frame = Equipment.frame;
   var high = Stage.visibleRect.height;
   var left = frame.left + Equipment.SLOT_SIZE + Equipment.PLAYER_GAP;
   var right = frame.right - Equipment.SLOT_SIZE - Equipment.PLAYER_GAP;
   var bottom = frame.bottom + Equipment.LABEL_HEIGHT;
   var top = Equipment.dollTop();
   var middle = Equipment.share((left + right) / 2, (top + bottom) / 2);
   if(Equipment.plugin() != undefined && !Equipment.browsing)
   {
      Equipment.plugin().ShowPlayer(middle.x, middle.y, (right - left) / high, (bottom - top) / high);
   }
};
