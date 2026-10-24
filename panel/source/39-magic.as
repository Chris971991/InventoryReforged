// The panel in SkyUI's magic menu: the doll with the two hands and the voice beside it. The rows of the magic list say
// themselves what is in which hand (equipState as in the inventory) and which power or shout is ready, a slot shows
// the row's own icon, from the set a mod gave the row or from SkyUI's. A weapon, shield or torch has no row here, the
// plugin names it and the slot shows its model as in the inventory
Equipment.SPELL = 22;
Equipment.SHOUT = 119;
Equipment.VOICE = 1;
Equipment.MAGIC_ALL = -257;
Equipment.MAGIC_SHOUTS = 64;
Equipment.MAGIC_POWERS = 128;
Equipment.MAGIC_EFFECTS = 256;
Equipment.HANDS_READ_EVERY = 200;
Equipment.HANDS_READ_TIMES = 5;
// the game's numbers of the hands in the menu's equip
Equipment.HAND_NUMBERS = {left:1, right:0};

// place is the plugin's number for the hand or the voice
Equipment.MAGIC_SLOTS = [
   {name:"right", place:0, column:0, row:0, icon:"sword", title:Equipment.say("Right hand")},
   {name:"left", place:1, column:0, row:1, icon:"shield", title:Equipment.say("Left hand")},
   {name:"voice", place:2, column:1, row:0, icon:"dragon", title:Equipment.say("Voice")}
];

Equipment.startMagic = function()
{
   var menu = Equipment.menu;
   var list = menu.inventoryLists.itemList;
   var i = 0;
   Equipment.INVENTORY_SLOTS = Equipment.SLOTS;
   Equipment.SLOTS = Equipment.MAGIC_SLOTS;
   Equipment.slotShown = Equipment.magicSlotShown;
   // the parts of the inventory's slots that mean nothing here: eyes, strips, item windows
   Equipment.canToggle = function()
   {
      return false;
   };
   Equipment.hasPieces = function()
   {
      return false;
   };
   Equipment.showIcon = Equipment.showMagicIcon;
   Equipment.amount = Equipment.spellCost;
   Equipment.click = Equipment.clickMagic;
   Equipment.press = Equipment.pressMagic;
   Equipment.fits = Equipment.fitsMagic;
   Equipment.hover = function(slot, over)
   {
      slot.hovered = over;
      Equipment.drawFrame(slot);
   };
   Equipment.build();
   Equipment.layout();
   // the card of a spell and its art stood over the doll, they go into the item window over the list as in the inventory
   Equipment.buildWindow();
   while(i < Equipment.SLOTS.length)
   {
      Equipment.SLOTS[i].box.onPress = function()
      {
         Equipment.click(this.slot);
      };
      Equipment.SLOTS[i].box.onPressAux = undefined;
      i++;
   }
   // the game hands a picked row to the menu with the hand of the mouse button, a hand slot that narrows the list
   // takes it into its own hand whichever button it was
   menu.equipmentAttemptEquip = menu.AttemptEquip;
   menu.AttemptEquip = function(hand)
   {
      var wanted = Equipment.handOfPick(hand);
      // for the test stand: how many picks came and what each became
      Equipment.attempts = Equipment.number(Equipment.attempts) + 1;
      Equipment.lastAttempt = Equipment.attempts + ": " + String(hand) + ">" + String(wanted);
      this.equipmentAttemptEquip(wanted);
   };
   list.addDataProcessor({processList:function(changed)
   {
      Equipment.readMagic(changed);
   }});
   if(list.entryList != undefined)
   {
      Equipment.readMagic(list);
   }
   Equipment.followPlayerAgain();
   Equipment.startMagicPad();
};

// A gamepad walks the three slots as it walks the inventory's. A narrows the list as a click does, X takes a weapon,
// shield or torch off the hand as the quick second click does. The pad comes to the right hand first
Equipment.startMagicPad = function()
{
   Equipment.padPressWords = function(at)
   {
      var slot = Equipment.padCell(at);
      if(slot == undefined)
      {
         return undefined;
      }
      return Equipment.activeSlot == slot ? Equipment.say("Show everything") : Equipment.say("Show what fits");
   };
   Equipment.padHide = function()
   {
      if(Equipment.handTakesOff(Equipment.padTarget()))
      {
         Equipment.takeOffHand(Equipment.padTarget());
      }
   };
   Equipment.padHideWords = function()
   {
      return Equipment.handTakesOff(Equipment.padTarget()) ? Equipment.say("Take off") : undefined;
   };
   Equipment.buildPad();
   Equipment.padLast = {kind:"slot", column:0, row:0};
};

// The bits of slotsOff go by the inventory's slots, the voice's place there is the ammo's. A hand follows the
// inventory's hand of its name, the voice has no slot to hide it by and always stays
Equipment.magicSlotShown = function(slot)
{
   var off = Equipment.settings != undefined ? Equipment.number(Equipment.settings.slotsOff) : 0;
   var i = 0;
   while(slot != undefined && i < Equipment.INVENTORY_SLOTS.length)
   {
      if(Equipment.INVENTORY_SLOTS[i].name == slot.name)
      {
         return (off & (1 << i)) == 0;
      }
      i++;
   }
   return true;
};

Equipment.readMagic = function(list)
{
   var entries = list.entryList;
   var hands = Equipment.plugin() != undefined ? Equipment.plugin().Hands() : undefined;
   var seen = [];
   var entry;
   var state;
   var slot;
   var i = 0;
   if(!Equipment.filtering && list.listEnumeration != undefined)
   {
      list.listEnumeration.addFilter(Equipment.slotFilter);
      Equipment.filtering = true;
   }
   i = 0;
   while(i < Equipment.SLOTS.length)
   {
      Equipment.SLOTS[i].entries = [];
      Equipment.SLOTS[i].taken = false;
      i++;
   }
   i = 0;
   while(i < entries.length)
   {
      entry = entries[i];
      state = Equipment.number(entry.equipState);
      if(state > 0)
      {
         seen.push(entry.text + ":" + state);
         if(entry.formType == Equipment.SHOUT || state == Equipment.VOICE)
         {
            Equipment.slotNamed("voice").entries = [entry];
         }
         if(state == Equipment.RIGHT_HAND || state == Equipment.BOTH_HANDS)
         {
            Equipment.slotNamed("right").entries = [entry];
         }
         if(state == Equipment.LEFT_HAND || state == Equipment.BOTH_HANDS)
         {
            Equipment.slotNamed("left").entries = [entry];
         }
      }
      i++;
   }
   // what the plugin names fills the rest: things of the inventory, and a spell whose row the list has no more
   i = 0;
   while(hands != undefined && i < Equipment.SLOTS.length)
   {
      slot = Equipment.SLOTS[i];
      if(Number(hands[slot.place].formId) != 0 && (hands[slot.place].item || slot.entries.length == 0))
      {
         slot.entries = [{formId:Number(hands[slot.place].formId), text:String(hands[slot.place].name), item:hands[slot.place].item == true}];
         seen.push(slot.name + " holds " + hands[slot.place].name);
      }
      i++;
   }
   // a weapon for both hands is shown by the right one, the left stands taken
   if(hands != undefined && hands[0].bothHands)
   {
      Equipment.slotNamed("left").entries = [];
      Equipment.slotNamed("left").taken = true;
   }
   // for the test stand: what the panel read
   Equipment.magicSeen = seen.join(" | ");
   i = 0;
   while(i < Equipment.SLOTS.length)
   {
      Equipment.showSlot(Equipment.SLOTS[i]);
      i++;
   }
   Equipment.showMagicModels();
};

// the models of the things in hand, made by the plugin as the inventory's slots make theirs
Equipment.showMagicModels = function()
{
   var plugin = Equipment.plugin();
   var items = [];
   var size = Equipment.SLOT_SIZE;
   var middle;
   var slot;
   var i = 0;
   if(plugin == undefined)
   {
      return undefined;
   }
   while(i < Equipment.SLOTS.length)
   {
      slot = Equipment.SLOTS[i];
      if(slot.entries.length > 0 && slot.entries[0].item)
      {
         middle = Equipment.share(slot.box._x + size / 2, slot.box._y + size / 2);
         items.push(slot.place, slot.entries[0].formId, middle.x, middle.y, false, (size - 2 * Equipment.MODEL_INSET) / Stage.visibleRect.height);
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
   while(i < Equipment.SLOTS.length)
   {
      Equipment.showIcon(Equipment.SLOTS[i]);
      i++;
   }
   Equipment.followPlayerAgain();
};

// The plugin takes a weapon off at once, the game changes no row of the magic list for it. The hands are read again
// a few times while the body catches up
Equipment.readHandsSoon = function()
{
   Equipment.handsRead = 0;
   clearInterval(Equipment.readingHands);
   Equipment.readingHands = setInterval(function()
   {
      Equipment.readMagic(Equipment.menu.inventoryLists.itemList);
      Equipment.handsRead++;
      if(Equipment.handsRead >= Equipment.HANDS_READ_TIMES)
      {
         clearInterval(Equipment.readingHands);
      }
   }, Equipment.HANDS_READ_EVERY);
};

// what a slot can take: a hand any spell cast from the hands, the voice shouts and powers
Equipment.fitsMagic = function(entry, slot)
{
   var flag = Equipment.number(entry.filterFlag);
   if((flag & Equipment.MAGIC_EFFECTS) != 0)
   {
      return false;
   }
   if(slot.name == "voice")
   {
      return (flag & (Equipment.MAGIC_SHOUTS | Equipment.MAGIC_POWERS)) != 0;
   }
   return entry.formType == Equipment.SPELL && (flag & (Equipment.MAGIC_SHOUTS | Equipment.MAGIC_POWERS)) == 0;
};

// a hand slot that narrows the list sends the pick to its own hand, a pick with no slot goes as the game sent it
Equipment.handOfPick = function(hand)
{
   var slot = Equipment.activeSlot;
   if(slot == undefined || slot.name == "voice")
   {
      return hand;
   }
   return Equipment.HAND_NUMBERS[slot.name];
};

// A press narrows the list to what the slot can take and picks the row of what it holds, a second press lets the list
// go. A quick second press on a hand with a weapon, shield or torch takes it off. The game has no way to take a spell
// or a shout off, it only gives way to the next one
Equipment.clickMagic = function(slot)
{
   var now = getTimer();
   var second = Equipment.clicked == slot && now - Equipment.clickedAt < Equipment.DOUBLE_CLICK;
   Equipment.clicked = second ? undefined : slot;
   Equipment.clickedAt = now;
   if(!Equipment.listsAnswer())
   {
      return undefined;
   }
   if(!second)
   {
      Equipment.filterMagic(slot);
   }
   else if(Equipment.settings.doubleClickTakesOff && Equipment.handTakesOff(slot))
   {
      Equipment.takeOffHand(slot);
   }
};

// a press of a gamepad, as the first click
Equipment.pressMagic = function(slot)
{
   if(slot != undefined && Equipment.listsAnswer())
   {
      Equipment.filterMagic(slot);
   }
};

// a weapon, shield or torch can come off the hand, a spell only gives way to the next one
Equipment.handTakesOff = function(slot)
{
   var entry = slot != undefined ? slot.entries[0] : undefined;
   return entry != undefined && entry.item && !slot.taken && Equipment.plugin() != undefined;
};

Equipment.takeOffHand = function(slot)
{
   if(Equipment.plugin().TakeOffHand(slot.place) == true)
   {
      Equipment.readHandsSoon();
   }
};

Equipment.filterMagic = function(slot)
{
   var lists = Equipment.menu.inventoryLists;
   var previous = Equipment.activeSlot;
   var entry = slot.entries[0];
   var category;
   Equipment.activeSlot = previous == slot ? undefined : slot;
   if(Equipment.activeSlot != undefined)
   {
      category = Equipment.categoryWith(Equipment.MAGIC_ALL);
      if(category >= 0 && lists.categoryList.selectedIndex != category)
      {
         lists.categoryList.onItemPress(category, 0);
      }
   }
   lists.itemList.selectedIndex = -1;
   lists.itemList.scrollPosition = 0;
   Equipment.refilter();
   if(Equipment.activeSlot != undefined && entry != undefined && !entry.item && entry.itemIndex != undefined)
   {
      lists.itemList.selectedIndex = entry.itemIndex;
   }
   Equipment.drawFrame(slot);
   if(previous != undefined && previous != slot)
   {
      Equipment.drawFrame(previous);
   }
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

// the number of a spell slot is what a cast costs
Equipment.spellCost = function(entry)
{
   var cost = Math.round(Equipment.number(entry.infoSpellCost));
   return cost > 0 ? String(cost) : "";
};

// An empty slot shows the panel's own sign, a spell or a shout the icon its row has in the list, tinted the same, a
// thing of the inventory its model, or its sign while models are off
Equipment.showMagicIcon = function(slot)
{
   var box = slot.box;
   var entry = slot.entries[0];
   var size = Equipment.ICON_SIZE;
   var inset = (Equipment.SLOT_SIZE - size) / 2;
   box.icon.clear();
   Equipment.hideRowIcon(box);
   if(entry == undefined || slot.taken)
   {
      Equipment.drawIcon(box.icon, slot.icon, inset, inset, size, Equipment.PARCHMENT, slot.taken ? 45 : 20);
      return undefined;
   }
   if(entry.item)
   {
      if(!Equipment.settings.slotModels || Equipment.plugin() == undefined || !Equipment.has(Equipment.modelled, slot.place))
      {
         Equipment.drawIcon(box.icon, slot.icon, inset, inset, size, Equipment.PARCHMENT, 85);
      }
      return undefined;
   }
   if(!Equipment.showRowIcon(box, entry, Equipment.SLOT_SIZE))
   {
      Equipment.drawIcon(box.icon, slot.icon, inset, inset, size, Equipment.PARCHMENT, 85);
   }
};
