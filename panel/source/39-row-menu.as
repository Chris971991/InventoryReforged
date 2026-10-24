Equipment.CONTAINER_LEFT_HAND = 1;
Equipment.CONTAINER_NO_HAND = -1;
Equipment.SOUL_GEM = 52;
// the kind of card that shows a list, ICT_LIST of SkyUI
Equipment.CARD_LIST = 14;

// A container or a shop has no panel, only the list with the menu of a row and the key of the stack, a follower's
// trade has the whole panel. The game turns a right press there into a move of the row, the one the menu of the row
// took is let go
Equipment.startMoving = function()
{
   var menu = Equipment.menu;
   Equipment.buildContext();
   Equipment.startCardNumbers();
   menu.equipmentHandleInput = menu.handleInput;
   menu.handleInput = function(details, pathToFocus)
   {
      if(Equipment.chargeKey(details) || Equipment.junkAskKey(details))
      {
         return true;
      }
      if(Equipment.chipKeys(details))
      {
         return true;
      }
      if(Equipment.stackKey(details))
      {
         return true;
      }
      if(Equipment.junkKey(details))
      {
         return true;
      }
      if(Equipment.context._visible && Shared.GlobalFunc.IsKeyPressed(details) && details.navEquivalent == gfx.ui.NavigationCode.ESCAPE)
      {
         Equipment.closeContext();
         return true;
      }
      return this.equipmentHandleInput(details, pathToFocus);
   };
   if(menu.AttemptEquip != undefined)
   {
      menu.equipmentAttemptEquip = menu.AttemptEquip;
      menu.AttemptEquip = function(a_slot, a_checkOverList)
      {
         if(!Equipment.pressUsed())
         {
            this.equipmentAttemptEquip(a_slot, a_checkOverList);
         }
      };
   }
};

// A right click on a row opens a menu of what can be done with it. The lines follow the menu: the inventory
// equips, uses and drops, a container takes or stores, a shop buys or sells, one, all or some of a stack.
// Every line goes through the menu's own call for it, so the game checks what it always checks
Equipment.openRowMenu = function(entry)
{
   if(entry == undefined || !Equipment.listsAnswer())
   {
      return false;
   }
   Equipment.showContext(Equipment.rowLines(entry), undefined, entry);
   return true;
};

Equipment.rowLines = function(entry)
{
   var lines = [];
   var menu = Equipment.menu;
   // what the player carries can be put on, used and marked, what a container or a shop has can only move
   var own = Equipment.kind == "inventory" || Equipment.kind == "container" && !menu.isViewingContainer();
   if(own)
   {
      Equipment.useLines(entry, lines);
   }
   else if(Equipment.wearersRow(entry))
   {
      Equipment.wearLines(entry, lines);
   }
   Equipment.chargeLine(entry, lines);
   if(Equipment.kind == "inventory")
   {
      Equipment.moveLines(entry, lines, "drop", "drop");
   }
   else if(Equipment.kind == "container")
   {
      Equipment.moveLines(entry, lines, menu.isViewingContainer() ? "take" : (menu.bNPCMode ? "give" : "store"), "transfer");
   }
   else
   {
      Equipment.moveLines(entry, lines, menu.isViewingVendorItems() ? "buy" : "sell", "coin");
   }
   lines.push({key:"-"});
   if(own)
   {
      lines.push({key:"favorite", words:entry.favorite ? Equipment.say("Remove from favorites") : Equipment.say("Add to favorites"), icon:"star"});
   }
   if(Equipment.settings.junk && Equipment.plugin() != undefined)
   {
      lines.push({key:"junk", words:entry.equipmentJunk == true ? Equipment.say("Not junk") : Equipment.say("Mark as junk"), icon:"junk"});
      Equipment.junkAllLine(lines);
   }
   // the inspect view lives in the inventory
   if(Equipment.viewer != undefined && Equipment.inspectable(entry))
   {
      lines.push({key:"inspect", words:Equipment.say("Inspect"), icon:"lens"});
   }
   if(lines[lines.length - 1].key == "-")
   {
      lines.pop();
   }
   return lines;
};

// putting on, using, reading and recharging, for what the player carries
Equipment.useLines = function(entry, lines)
{
   var slot = Equipment.slotOf(entry);
   var worn = entry.equipState > 0;
   var charge = Equipment.menu.itemCard.itemInfo.charge;
   if(slot != undefined || entry.formType == Equipment.SCROLL)
   {
      lines.push(worn ? {key:"equip", words:Equipment.say("Take off"), icon:"unequip"} : {key:"equip", words:Equipment.say("Equip"), icon:slot != undefined ? slot.icon : "staff"});
      if(Equipment.leftHanded(entry) && entry.equipState != Equipment.LEFT_HAND && entry.equipState != Equipment.BOTH_HANDS)
      {
         lines.push({key:"left", words:Equipment.say("Equip in left hand"), icon:"shield"});
      }
   }
   else if(entry.formType == Equipment.POTION || entry.formType == Equipment.INGREDIENT)
   {
      lines.push({key:"equip", words:entry.formType == Equipment.INGREDIENT ? Equipment.say("Eat") : Equipment.say("Use"), icon:"potion"});
   }
   else if(entry.formType == Equipment.BOOK)
   {
      lines.push({key:"equip", words:Equipment.say("Read"), icon:"book"});
   }
   if(Equipment.kind == "inventory" && entry.formType == Equipment.WEAPON && charge != undefined && charge < 100)
   {
      lines.push({key:"charge", words:Equipment.say("Recharge"), icon:"gem"});
   }
   if(lines.length > 0)
   {
      lines.push({key:"-"});
   }
};

// what the follower of the panel has can go on them or come off
Equipment.wearLines = function(entry, lines)
{
   var slot = Equipment.slotOf(entry);
   if(slot == undefined || Equipment.lookOnly())
   {
      return undefined;
   }
   lines.push(entry.equipState > 0 ? {key:"wear", words:Equipment.say("Take off"), icon:"unequip"} : {key:"wear", words:Equipment.say("Equip"), icon:slot.icon});
   if(entry.formType == Equipment.WEAPON && Equipment.oneHanded(entry) && entry.equipState != Equipment.LEFT_HAND && entry.equipState != Equipment.BOTH_HANDS)
   {
      lines.push({key:"wearLeft", words:Equipment.say("Equip in left hand"), icon:"shield"});
   }
   lines.push({key:"-"});
};

// A teammate's drained weapon or staff gets recharged too, in the group of what goes on them. A soul gem with a soul
// charges a weapon of the one whose inventory is open, the player's or the teammate's in their trade
Equipment.chargeLine = function(entry, lines)
{
   var line;
   if(Equipment.chargeable(entry))
   {
      line = {key:"charge", words:Equipment.say("Recharge"), icon:"gem"};
   }
   else if(Equipment.gemWeapons(entry) != undefined)
   {
      line = {key:"chargeWith", words:Equipment.say("Recharge a weapon"), icon:"gem"};
   }
   else
   {
      return undefined;
   }
   if(lines.length > 0 && lines[lines.length - 1].key == "-")
   {
      lines.splice(lines.length - 1, 0, line);
      return undefined;
   }
   lines.push(line);
   lines.push({key:"-"});
};

// a row of the one traded with in a trade where the game lets the player give and take freely
Equipment.teammatesRow = function(entry)
{
   var menu = Equipment.menu;
   if(Equipment.kind != "container" || menu.bNPCMode != true)
   {
      return false;
   }
   return Equipment.dressesOther() ? Equipment.wearersRow(entry) : menu.isViewingContainer();
};

// The plugin says how full the item is, by the hand's charge while it's worn: the card of a trade shows what the
// item had when it went on
Equipment.chargeable = function(entry)
{
   var plugin = Equipment.plugin();
   var charge;
   if(entry.formType != Equipment.WEAPON || plugin == undefined || plugin.ChargeOf == undefined || !Equipment.teammatesRow(entry))
   {
      return false;
   }
   charge = plugin.ChargeOf(entry.itemIndex, entry.formId);
   return charge >= 0 && charge < 100;
};

// the card's list of the drained weapons the gem of a row can charge, undefined for none
Equipment.gemWeapons = function(entry)
{
   var plugin = Equipment.plugin();
   var here = Equipment.kind == "inventory" || Equipment.kind == "container" && Equipment.menu.bNPCMode == true;
   if(entry.formType != Equipment.SOUL_GEM || !here || plugin == undefined || plugin.Weapons == undefined)
   {
      return undefined;
   }
   return plugin.Weapons(entry.itemIndex, entry.formId);
};

// The game charges the player's weapon in the inventory through a list of soul gems in the item card. The same list
// comes up for a teammate's row, with the gems of both, the teammate's named so
Equipment.chargeTeammate = function(entry)
{
   var plugin = Equipment.plugin();
   var info = plugin != undefined && plugin.SoulGems != undefined ? plugin.SoulGems(entry.itemIndex, entry.formId) : undefined;
   var lines;
   var i = 0;
   if(info == undefined)
   {
      return false;
   }
   lines = info.listItems;
   while(i < lines.length)
   {
      if(lines[i].owner != undefined && lines[i].owner != "")
      {
         lines[i].text = Equipment.say("#2's #1", lines[i].text, lines[i].owner);
      }
      i++;
   }
   Equipment.charging = {weapon:{itemIndex:entry.itemIndex, formId:entry.formId}};
   Equipment.menu.UpdateItemCardInfo(info);
   return true;
};

// The same list the other way: the gem is the row's, the card asks for the weapon
Equipment.chargeWithGem = function(entry)
{
   var info = Equipment.gemWeapons(entry);
   var list;
   if(info == undefined)
   {
      return false;
   }
   info.name = Equipment.say("Choose a weapon to charge");
   Equipment.charging = {gem:{itemIndex:entry.itemIndex, formId:entry.formId}};
   Equipment.menu.UpdateItemCardInfo(info);
   // after the card's own listeners, which set the meter for the one weapon the game's list is for
   list = Equipment.menu.itemCard.ItemList;
   if(list != undefined)
   {
      list.addEventListener("selectionChange", Equipment, "chargeMeter");
      list.addEventListener("listMovedUp", Equipment, "chargeMeter");
      list.addEventListener("listMovedDown", Equipment, "chargeMeter");
   }
   Equipment.chargeMeter();
   return true;
};

// the meter of the card shows the charge of the weapon the list points at and what the gem adds to it
Equipment.chargeMeter = function()
{
   var card = Equipment.menu.itemCard;
   var line = card.ItemList != undefined ? card.ItemList.selectedEntry : undefined;
   var meter = card.ItemCardMeters != undefined ? card.ItemCardMeters[Equipment.CARD_LIST] : undefined;
   if(Equipment.charging == undefined || Equipment.charging.gem == undefined || line == undefined || meter == undefined)
   {
      return undefined;
   }
   meter.SetPercent(line.charge);
   meter.SetDeltaPercent(line.charge + line.chargeAdded);
};

// The lists of the card go through the item menus' own card. The inventory has the game's recharge on that card too,
// it keeps out of a list of the panel's
Equipment.startCharging = function()
{
   var menu = Equipment.menu;
   if(Equipment.kind != "container" && Equipment.kind != "inventory")
   {
      return undefined;
   }
   menu.itemCard.addEventListener("itemPress", Equipment, "lineChosen");
   menu.itemCard.addEventListener("subMenuAction", Equipment, "chargeAsks");
   Equipment.chargeMouse = {};
   Equipment.chargeMouse.onMouseDown = function()
   {
      var list = Equipment.menu.itemCard.ItemList;
      if(!Equipment.chargeListUp() || getTimer() - Equipment.chargeListAt < Equipment.CHARGE_SETTLE)
      {
         return undefined;
      }
      if(list != undefined && list.hitTest(_root._xmouse, _root._ymouse, true))
      {
         return undefined;
      }
      Equipment.usePress();
      Equipment.closeChargeList();
   };
   Mouse.addListener(Equipment.chargeMouse);
   if(menu.onItemCardListPress != undefined)
   {
      menu.chargeItemCardListPress = menu.onItemCardListPress;
      menu.onItemCardListPress = function(event)
      {
         if(Equipment.charging == undefined)
         {
            this.chargeItemCardListPress(event);
         }
      };
   }
};

// a gem picked for a weapon or a weapon for a gem
Equipment.lineChosen = function(event)
{
   var charging = Equipment.charging;
   var list = Equipment.menu.itemCard.ItemList;
   var line = list != undefined ? list.entryList[event.index] : undefined;
   var plugin = Equipment.plugin();
   if(charging == undefined || line == undefined || plugin == undefined)
   {
      return undefined;
   }
   if(charging.weapon != undefined)
   {
      plugin.Recharge(charging.weapon.itemIndex, charging.weapon.formId, line.itemIndex, line.formId);
   }
   else
   {
      plugin.Recharge(line.itemIndex, line.formId, charging.gem.itemIndex, charging.gem.formId);
   }
};

// While the list is up the game holds its own keys of the item menu, once it's gone the card shows the row again. The
// inventory does both for its list itself, the trade asks
Equipment.chargeAsks = function(event)
{
   if(event.menu == "list" && event.opening == true)
   {
      Equipment.chargeListAt = getTimer();
   }
   if(event.menu != "list" || Equipment.charging == undefined)
   {
      return undefined;
   }
   if(Equipment.kind == "container")
   {
      gfx.io.GameDelegate.call("QuantitySliderOpen", [event.opening == true]);
   }
   if(event.opening == true)
   {
      return undefined;
   }
   Equipment.charging = undefined;
   if(Equipment.kind == "container")
   {
      gfx.io.GameDelegate.call("RequestItemCardInfo", [], Equipment.menu, "UpdateItemCardInfo");
   }
};

// The list of a recharge stands on the card and holds the game's keys, the card let it go on its cancel key alone.
// Tab, Esc, a right press or a press off the list close it like the panel's own menus, the press that opened it
// comes a moment later and is left alone
Equipment.CHARGE_SETTLE = 250;
Equipment.chargeListUp = function()
{
   var card = Equipment.menu.itemCard;
   return card != undefined && card.InputHandler != undefined && card.InputHandler == card.HandleListMenuInput;
};

Equipment.closeChargeList = function()
{
   if(!Equipment.chargeListUp())
   {
      return false;
   }
   Equipment.menu.itemCard.handleInput({value:"keyDown", navEquivalent:gfx.ui.NavigationCode.TAB, code:9}, []);
   return true;
};

Equipment.chargeKey = function(details)
{
   var nav = details.navEquivalent;
   if(!Equipment.chargeListUp() || !Shared.GlobalFunc.IsKeyPressed(details))
   {
      return false;
   }
   if(nav == gfx.ui.NavigationCode.TAB || nav == gfx.ui.NavigationCode.SHIFT_TAB || nav == gfx.ui.NavigationCode.ESCAPE)
   {
      Equipment.closeChargeList();
      return true;
   }
   return false;
};

// a one handed weapon, a staff, a scroll or a spell tome in the hand can go to the left hand
Equipment.leftHanded = function(entry)
{
   return entry.formType == Equipment.WEAPON && Equipment.oneHanded(entry) || entry.formType == Equipment.SCROLL;
};

// one of a stack, all of it, or some through the slider of the menu, which is splitting a stack
Equipment.moveLines = function(entry, lines, verb, icon)
{
   var count = Equipment.number(entry.count);
   var words = Equipment.moveWords(verb, count);
   if(count <= 1)
   {
      lines.push({key:"one", words:words[0], icon:icon});
      return undefined;
   }
   lines.push({key:"one", words:words[1], icon:icon});
   lines.push({key:"all", words:words[2], icon:icon});
   if(count > 2)
   {
      lines.push({key:"some", words:words[3], icon:"amount"});
   }
};

// whole phrases for every way a thing moves, a language may say each of them its own way
Equipment.moveWords = function(verb, count)
{
   switch(verb)
   {
      case "take":
         return [Equipment.say("Take"), Equipment.say("Take one"), Equipment.say("Take all (#)", count), Equipment.say("Take some...")];
      case "give":
         return [Equipment.say("Give"), Equipment.say("Give one"), Equipment.say("Give all (#)", count), Equipment.say("Give some...")];
      case "store":
         return [Equipment.say("Store"), Equipment.say("Store one"), Equipment.say("Store all (#)", count), Equipment.say("Store some...")];
      case "buy":
         return [Equipment.say("Buy"), Equipment.say("Buy one"), Equipment.say("Buy all (#)", count), Equipment.say("Buy some...")];
      case "sell":
         return [Equipment.say("Sell"), Equipment.say("Sell one"), Equipment.say("Sell all (#)", count), Equipment.say("Sell some...")];
   }
   return [Equipment.say("Drop"), Equipment.say("Drop one"), Equipment.say("Drop all (#)", count), Equipment.say("Drop some...")];
};

Equipment.rowAct = function(key, entry)
{
   var menu = Equipment.menu;
   var container = Equipment.kind == "container";
   if(!Equipment.pickRow(entry))
   {
      return undefined;
   }
   // a hand left over from an equip of the container would make its next move an equip
   menu._equipHand = undefined;
   switch(key)
   {
      case "equip":
         // a twin of a hidden piece goes on the way a click puts it on
         if(container || !Equipment.swapTwin(entry))
         {
            gfx.io.GameDelegate.call(container ? "EquipItem" : "ItemSelect", container ? [Equipment.CONTAINER_NO_HAND] : []);
         }
         break;
      case "left":
         gfx.io.GameDelegate.call(container ? "EquipItem" : "ItemSelect", container ? [Equipment.CONTAINER_LEFT_HAND] : [Equipment.LEFT_HAND_SLOT]);
         break;
      case "wear":
         Equipment.dressOther(entry, entry.equipState == Equipment.LEFT_HAND, entry.equipState == 0);
         break;
      case "wearLeft":
         Equipment.dressOther(entry, true, true);
         break;
      case "one":
         menu.onQuantityMenuSelect({amount:1});
         break;
      case "all":
         menu.onQuantityMenuSelect({amount:Equipment.number(entry.count)});
         break;
      case "some":
         menu.itemCard.ShowQuantityMenu(Equipment.number(entry.count));
         break;
      case "favorite":
         Equipment.plugin().Favorite();
         break;
      case "charge":
         if(container)
         {
            Equipment.chargeTeammate(entry);
         }
         else
         {
            menu.AttemptChargeItem();
         }
         break;
      case "chargeWith":
         Equipment.chargeWithGem(entry);
         break;
      case "inspect":
         Equipment.inspectEntry(entry, false);
         break;
      case "junk":
         Equipment.toggleJunk(entry);
         break;
      case "junkAll":
         Equipment.moveJunk();
   }
};

// a shop or a chest has a line for all junk at once
Equipment.junkAllLine = function(lines)
{
   var verb = Equipment.junkVerb();
   var count = verb != undefined ? Equipment.pieces(Equipment.junkRows()) : 0;
   if(count > 0)
   {
      lines.push({key:"junkAll", words:Equipment.junkWords(verb, count), icon:verb == "sell" ? "coin" : "transfer"});
   }
};

// The menu's calls act on its selected row. The row the menu was opened on is selected again if the
// pointer went over others meanwhile, so the item card says what the shop counts its price by
Equipment.pickRow = function(entry)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var i = 0;
   if(list.selectedEntry == entry)
   {
      return true;
   }
   while(i < list.entryList.length && list.entryList[i] != entry)
   {
      i++;
   }
   if(i == list.entryList.length)
   {
      return false;
   }
   list.selectedIndex = i;
   return list.selectedEntry == entry;
};

// The key of the menu of the mod moves the whole stack of the row under the pointer in a container or a shop,
// the way the slider does at its end. The inventory has nowhere to move it to. A held key repeats, and the
// next row would go too, so only the press itself counts
Equipment.stackKey = function(details)
{
   var key = Equipment.number(Equipment.settings.stackKey);
   var entry;
   if(key <= 0 || Equipment.kind == "inventory" || details.skseKeycode != key || !Shared.GlobalFunc.IsKeyPressed(details))
   {
      return false;
   }
   entry = Equipment.menu.inventoryLists.itemList.selectedEntry;
   if(!Shared.GlobalFunc.IsKeyPressed(details, false) || Equipment.inspecting || !Equipment.listsAnswer() || entry == undefined)
   {
      return true;
   }
   Equipment.closeContext();
   Equipment.rowAct("all", entry);
   return true;
};

// for the test stand: a line of the row menu acted on the row with this form id, told as "key,formId"
Equipment.rowActOn = function(told)
{
   var parts = String(told).split(",");
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var i = 0;
   while(i < entries.length && entries[i].formId != Number(parts[1]))
   {
      i++;
   }
   if(i < entries.length)
   {
      Equipment.rowAct(parts[0], entries[i]);
   }
};

// for the test stand: the row of this item gets selected, as the pointer on it selects it
Equipment.pickForm = function(formId)
{
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var i = 0;
   while(i < entries.length && entries[i].formId != Number(formId))
   {
      i++;
   }
   skse.Log("EquipmentPanel picked the row of " + formId + ": " + (i < entries.length && Equipment.pickRow(entries[i])));
};

// for the test stand: the menu of the row of this item opens, the way a right click on the row opens it
Equipment.rowMenuOn = function(formId)
{
   Equipment.pickForm(formId);
   Equipment.openRowMenu(Equipment.menu.inventoryLists.itemList.selectedEntry);
};

// for the test stand: the charge of the selected row as the plugin and the card tell it, and the card's list
Equipment.chargeReport = function(note)
{
   var entry = Equipment.menu.inventoryLists.itemList.selectedEntry;
   var card = Equipment.menu.itemCard;
   var plugin = Equipment.plugin();
   var gems = [];
   var i = 0;
   while(card.ItemList != undefined && i < card.ItemList.entryList.length)
   {
      gems.push(card.ItemList.entryList[i].text + " x" + card.ItemList.entryList[i].count + " +" + Math.round(card.ItemList.entryList[i].chargeAdded));
      i++;
   }
   skse.Log("EquipmentPanel charge " + note + ": row " + entry.text + " " + entry.formId + ", plugin " + (plugin.ChargeOf != undefined ? Math.round(plugin.ChargeOf(entry.itemIndex, entry.formId)) : "-") +
      ", card " + Math.round(card.itemInfo.charge) + ", card type " + card.itemInfo.type + ", list up " + (Equipment.charging != undefined) + ", gems " + gems.join(" / "));
};

// for the test stand: the middle of the line with this key in the open menu, as aimedX, aimedY
Equipment.aimContextKey = function(key)
{
   var context = Equipment.context;
   var share;
   var line;
   var i = 0;
   Equipment.aimedX = Equipment.aimedY = -1;
   while(i < 20 && context._visible)
   {
      line = context["line" + i];
      // a script of the game hands a word over in the letter case it met first, Left for left
      if(line != undefined && line.what.key.toLowerCase() == String(key).toLowerCase())
      {
         share = Equipment.share(context._x + line._x + 40, context._y + line._y + Equipment.CONTEXT_LINE / 2);
         Equipment.aimedX = share.x;
         Equipment.aimedY = share.y;
      }
      i++;
   }
   skse.Log("EquipmentPanel aimed at the line " + key + ": " + Equipment.aimedX + ", " + Equipment.aimedY);
};

// for the test stand: the lines of the open menu, into the log of the script extender
Equipment.contextReport = function(note)
{
   var context = Equipment.context;
   var words = [];
   for(var name in context)
   {
      if(context[name] instanceof MovieClip && context[name].what != undefined)
      {
         words.unshift(context[name].what.words);
      }
   }
   skse.Log("EquipmentPanel menu " + note + ": " + (context._visible ? words.join(" | ") : "closed"));
};
