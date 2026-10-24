Equipment.WINDOW_SHARE = 0.48;
// the least the window takes as a share of the screen's height, the most as a share of the list
Equipment.WINDOW_LEAST = 0.4;
Equipment.WINDOW_MOST = 0.8;
Equipment.WINDOW_INSET = 19;
Equipment.WINDOW_GAP = 4;
Equipment.WINDOW_PAD = 8;
Equipment.PREVIEW_HEIGHT = 130;
Equipment.PREVIEW_LEAST = 70;
Equipment.CARD_PAD = 14;
Equipment.TEXT_ROOM = 140;
// in the units of the card, which the window shows at about two thirds
Equipment.KIND_LINE = 26;
Equipment.KIND_TEXT = 19;
Equipment.FIT_SLACK = 0.5;
Equipment.WATCH_FRAMES = 600;
Equipment.POPUP_GAP = 10;
Equipment.STATS_PAD = 12;
Equipment.STATS_TEXT = 13;
Equipment.ACTION_HIGH = 26;
Equipment.ACTION_GAP = 6;
// a slot window's buttons stand clear of the frame's bottom edge and its ornaments
Equipment.ACTION_EDGE = 14;
Equipment.ACTION_ICON = 14;
Equipment.LENS_AIM_MS = 300;
Equipment.LENS_AIM_SLACK = 6;

// The item window is the SkyUI item card with the model of the item above it and the numbers of the panel
// under it. Opened from a row it pops up over the right half of the list, under the selected row or above
// it when there's no room below, and takes no mouse so the rows under it keep working
// Opened from a slot it stands beside the slot, holds buttons, and a click anywhere else closes it
Equipment.buildWindow = function()
{
   var menu = Equipment.menu;
   var lists = menu.inventoryLists;
   var card = menu.itemCard._parent;
   var sheet = Equipment.under.createEmptyMovieClip("equipmentWindowSheet", Equipment.under.getNextHighestDepth());
   var window;

   // The window's sheet lies under the card, the card over it wears its own frame grown to the window, the
   // window's words lie over that frame, and what the panel opens later lies over all of them
   card.swapDepths(menu.getNextHighestDepth());
   Equipment.windowLayer = menu.createEmptyMovieClip("equipmentWindowLayer", menu.getNextHighestDepth());
   Equipment.windowLayer._xscale = 100 / Equipment.squash;
   Equipment.over.swapDepths(menu.getNextHighestDepth());
   window = Equipment.windowLayer.createEmptyMovieClip("equipmentWindow", 1);
   Equipment.window = window;
   window.sheet = sheet;
   sheet._visible = false;
   sheet.hitTestDisable = true;
   window._visible = false;
   window.hitTestDisable = true;
   window.back = sheet;
   window.createTextField("touch", 1, 0, 0, 10, 10);
   window.touch.selectable = false;
   Equipment.sizeWindow();
   Equipment.buildStats(window);
   Equipment.windowWanted = false;
   Equipment.cardKept = false;
   Equipment.watched = 0;
   Equipment.panel.onEnterFrame = Equipment.watchCard;

   // SkyUI makes a long text of a card smaller until it fits its place, unless the place may grow first
   // In the window there's room downwards, so the text grows that way and stays readable
   TextField.prototype.maxHeightExpand = Equipment.TEXT_ROOM;

   // the game sizes a model when it puts it up, so the size has to be told before the first row is selected
   window._x = window.left;
   window._y = Equipment.frame.top;
   Equipment.aimPreview();

   // The model in the window is too small to be turned by hand, and the rows under it have to stay in reach
   // It's turned in the inspect view
   menu.mouseRotationRect._xscale = menu.mouseRotationRect._yscale = 0;

   Equipment.reachCard({opening:false});
   menu.itemCard.addEventListener("subMenuAction", Equipment, "reachCard");
   lists.addEventListener("itemHighlightChange", Equipment, "onHighlight");
   lists.addEventListener("showItemsList", Equipment, "onHighlight");
   lists.addEventListener("hideItemsList", Equipment, "onHighlight");
   lists.itemList.addEventListener("selectionChange", Equipment, "rowChosen");
   Equipment.aimForLens();

   // the card is filled after the row is selected, and a list that scrolls moves the selected row. Several effects
   // of an enchantment each get a line of their own
   menu.equipmentUpdateItemCardInfo = menu.UpdateItemCardInfo;
   menu.UpdateItemCardInfo = function(info)
   {
      if(info != undefined && info.effects != undefined && !Equipment.cardFitsByHand() && !Equipment.leavesCard())
      {
         info.effects = Equipment.effectLines(info.effects);
      }
      this.equipmentUpdateItemCardInfo(info);
      Equipment.cardStamp = Equipment.number(Equipment.cardStamp) + 1;
      Equipment.showWindow();
   };
   Equipment.holdModels();
   lists.itemList.equipmentUpdateList = lists.itemList.UpdateList;
   lists.itemList.UpdateList = function()
   {
      // SkyUI selects the row under the pointer here, a row the pointer crosses on its way to the lens waits
      var mouseDriven = this.isMouseDrivenNav;
      if(Equipment.lensWait != undefined || Equipment.headsForLens())
      {
         this.isMouseDrivenNav = false;
      }
      this.equipmentUpdateList();
      this.isMouseDrivenNav = mouseDriven;
      Equipment.keepApart();
      Equipment.listMoving = true;
      Equipment.showWindow();
      Equipment.listMoving = false;
   };
};

// The game puts up the model of a row the moment the row is selected and draws it a frame before the panel hides it
// again. A window that waits for a pause or never comes flashed the model over the list as the pointer went along
// the rows. Until a window is up for the selected row its model isn't asked for, the window asks when it comes up.
// A slot's window and the question of how many ask as before
Equipment.holdModels = function()
{
   var delegate = gfx.io.GameDelegate;
   if(delegate.equipmentCall != undefined)
   {
      return undefined;
   }
   delegate.equipmentCall = delegate.call;
   delegate.call = function(name, args)
   {
      if(name == "UpdateItem3D" && args[0] == true && Equipment.holdsModel())
      {
         Equipment.modelHeld = true;
         Equipment.modelsHeld = Equipment.number(Equipment.modelsHeld) + 1;
         return undefined;
      }
      if(name == "UpdateItem3D")
      {
         Equipment.modelHeld = false;
         Equipment.modelFor = args[0] == true ? Equipment.menu.inventoryLists.itemList.selectedIndex : undefined;
      }
      return delegate.equipmentCall.apply(delegate, arguments);
   };
};

Equipment.holdsModel = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   var shownFor = Equipment.window._visible && Equipment.modelFor == list.selectedIndex;
   return Equipment.number(Equipment.settings.windowShow) != Equipment.WINDOW_AT_ONCE && Equipment.anchorSlot == undefined && !Equipment.cardAsking && !shownFor;
};

// the window came up over a row whose model was held back
Equipment.askModel = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   if(!Equipment.modelHeld || !Equipment.window._visible || list.selectedIndex < 0)
   {
      return undefined;
   }
   Equipment.modelHeld = false;
   Equipment.modelFor = list.selectedIndex;
   Equipment.modelsAsked = Equipment.number(Equipment.modelsAsked) + 1;
   gfx.io.GameDelegate.equipmentCall("UpdateItem3D", [true]);
};

// for the test stand: how many models of rows were held back and asked for later
Equipment.modelReport = function(note)
{
   skse.Log("PANELTEST models " + note + ": held " + Equipment.number(Equipment.modelsHeld) + ", asked by the window " + Equipment.number(Equipment.modelsAsked) + ", one held now " + (Equipment.modelHeld == true) + ", window " + Equipment.window._visible + ", selected " + Equipment.menu.inventoryLists.itemList.selectedIndex);
};

// How wide the window is in its own units, the skin's units over a row. It takes its share of the list at the
// list's full width, so a narrowed list keeps a card that reads, and never less than its share of the screen's
// height, so does a narrow list of a skin or a menu drawn small on a big screen. The size the player chose goes on
// top of that, up to the most of the list that leaves the names of the rows in sight
Equipment.windowWideFor = function(listWide, screenHigh, size, squash)
{
   var wide = Math.max(listWide * Equipment.WINDOW_SHARE, screenHigh * Equipment.WINDOW_LEAST) * size / 100;
   return Math.round(Math.min(wide, listWide * Equipment.WINDOW_MOST) / squash);
};

// A skin that draws its item card as a picture of its own in a place of its own (Norden UI, under the list) may keep
// it there as it is. The window then holds the model and the numbers, the card stays the skin's with its art, its
// kind line and its effects as the skin lays them. An ini switch for now, [Window] bLeaveCard
Equipment.leavesCard = function()
{
   return Equipment.settings != undefined && Equipment.settings.leaveCard == true;
};

// the card fills the width of the window and its words grow with it
Equipment.sizeWindow = function()
{
   var window = Equipment.window;
   var card = Equipment.menu.itemCard._parent;
   var list = Equipment.listFullWide != undefined ? Equipment.listFullWide : Equipment.rowsEdge().wide;
   var size = Equipment.number(Equipment.settings.windowSize);
   var cardWide = Equipment.cardWidth();
   window.wide = Equipment.windowWideFor(list, Stage.visibleRect.height, size > 0 ? size : 100, Equipment.squash);
   // a card with no width yet would get an endless scale and stay out of sight for the whole menu
   if(!Equipment.leavesCard() && cardWide > 0)
   {
      card._yscale = window.wide / cardWide * 100;
   }
   Equipment.windowUnits(Equipment.squash);
   Equipment.placeWindowLeft();
   if(window.bodySlots != undefined)
   {
      window.bodySlots._width = window.wide - 2 * Equipment.STATS_PAD - Equipment.LENS_SIZE;
   }
};

// The window stands at the list's right edge
Equipment.placeWindowLeft = function()
{
   var window = Equipment.window;
   window.left = Equipment.rowsEdge().right - (Equipment.WINDOW_INSET + window.wide) * Equipment.squash;
};

// Over a row the window and the card in it take the proportions of the list they lie over, the skin's own words
// are drawn so. Beside a slot they're square like the rest of the panel. across is how wide the window's unit is in
// the panel's units, the squash of the menu over a row and 1 beside a slot
Equipment.windowUnits = function(across)
{
   var window = Equipment.window;
   var card = Equipment.menu.itemCard._parent;
   window.across = across;
   window._xscale = across * 100;
   if(!Equipment.leavesCard())
   {
      card._xscale = card._yscale * across / Equipment.squash;
   }
};

// how wide the window is in the panel's units
Equipment.windowWide = function()
{
   var window = Equipment.window;
   return window.wide * (window.across != undefined ? window.across : 1);
};

// what the window shows: the item of the slot it was opened from, the selected row otherwise
Equipment.windowEntry = function()
{
   return Equipment.anchorSlot != undefined ? Equipment.anchorSlot.entries[0] : Equipment.menu.inventoryLists.itemList.selectedEntry;
};

// Every kind of card is laid out inside the frame of the card, which has the same width for all of them
// The holder of the card can be wider than that when another mod has put something of its own into the card
// Menus built on SkyUI may place the card in its holder at a size of their own, Vel'dun UI does at 85%
Equipment.cardWidth = function()
{
   var card = Equipment.menu.itemCard;
   var holder = card._parent;
   var frame = Equipment.frameBox(card);
   return frame != undefined ? (frame.xMax - frame.xMin) * card._xscale / 100 : holder._width * 100 / holder._xscale;
};

// The frame of the card as a viewer sees it, the card itself when none is named, nothing while it has no size.
// A frame drawn out past the card's left edge has that much clear margin on every side, a shadow or the see-through
// edge of a picture: Dear Diary loads a paper 468 wide from 20 left and above the card, the paper itself is 428 like
// SkyUI's frame. A frame drawn round its own middle starts left of the card by half its width, that's no margin
Equipment.frameBox = function(card, viewer)
{
   var frame = card.background;
   var box;
   var margin;
   var from;
   var to;
   if(frame == undefined || !(frame._width > 0))
   {
      return undefined;
   }
   box = frame.getBounds(card);
   margin = Math.max(0, -box.xMin);
   if(!(box.xMax - box.xMin - 2 * margin > 1))
   {
      margin = 0;
   }
   from = {x:box.xMin + margin, y:box.yMin + margin};
   to = {x:box.xMax - margin, y:box.yMax - margin};
   if(viewer != undefined && viewer != card)
   {
      card.localToGlobal(from);
      card.localToGlobal(to);
      viewer.globalToLocal(from);
      viewer.globalToLocal(to);
   }
   return {xMin:from.x, yMin:from.y, xMax:to.x, yMax:to.y};
};

// moreHUD Inventory Edition swaps the card of enchanted things, potions, food and books for a wider one
// It draws that card beside the SkyUI one, at the size and the place the SkyUI card has in a plain menu,
// and stretches the text of the SkyUI card over it. In the item window that card hangs out on both sides
// The swap is one function of the clip moreHUD loads into the menu, called for every new card
// On that clip it's replaced by one that tells moreHUD the card was left alone, so moreHUD hides
// its own card and gives the SkyUI one back its look. Its icons on the card are another function and stay
// Tells whether the clip of moreHUD is in hand
Equipment.keepCard = function()
{
   var other;
   if(Equipment.cardKept)
   {
      return true;
   }
   other = Equipment.otherCard();
   if(other == undefined)
   {
      return false;
   }
   Equipment.cardKept = true;
   if(other._itemCardOverride == true)
   {
      Equipment.remakeCard();
   }
   other.AdjustItemCard = function()
   {
      this._itemCardOverride = false;
   };
   other.AdjustItemCard();
   other._alpha = 0;
   return true;
};

// A card moreHUD has swapped already keeps the stretched text until its kind changes
// Going over the empty card and back makes the SkyUI card lay the text out anew
Equipment.remakeCard = function()
{
   var card = Equipment.menu.itemCard;
   var info = card.itemInfo;
   card.gotoAndStop("Empty");
   card.itemInfo = info;
};

// moreHUD loads its clip some frames into the menu and swaps the first card it meets after that
// So the clip is looked for on every frame from the start, for as long as a slow load can take
Equipment.watchCard = function()
{
   Equipment.watched++;
   Equipment.everyFrame();
   if(Equipment.cardKept || Equipment.watched >= Equipment.WATCH_FRAMES)
   {
      return undefined;
   }
   if(Equipment.keepCard())
   {
      Equipment.showWindow();
   }
};

// The card takes the mouse only while it asks how many to drop or whether to go on
// When a question is called off the card tells of it first and goes back to the item after that,
// so the window is measured once more when the card has settled
Equipment.reachCard = function(event)
{
   Equipment.menu.itemCard._parent.hitTestDisable = !event.opening;
   // the window lies over the card, the card's question takes the mouse through it
   Equipment.window.hitTestDisable = event.opening == true || Equipment.anchorSlot == undefined;
   // only a question of the card itself, the call at the start has none
   if(event.menu != undefined)
   {
      Equipment.cardAsking = event.opening == true;
      Equipment.holdRows(Equipment.cardAsking);
   }
   Equipment.showWindow();
   if(!event.opening && Equipment.settling == undefined)
   {
      Equipment.settling = setInterval(Equipment.cardSettled, 1);
   }
};

Equipment.cardSettled = function()
{
   clearInterval(Equipment.settling);
   delete Equipment.settling;
   Equipment.showWindow();
};

Equipment.onHighlight = function(event)
{
   var selected = Equipment.menu.inventoryLists.itemList.selectedEntry;
   // the window of an outfit's piece has no row, only a row that gets selected takes its card
   var piece = Equipment.shownPiece != undefined && event.index < 0;
   if(Equipment.anchorSlot != undefined && selected != Equipment.anchorSlot.entries[0] && !piece)
   {
      // another row took the card and the model, the window of the slot goes and leaves that row selected
      Equipment.shownApart = undefined;
      Equipment.shownPiece = undefined;
      Equipment.closePopup();
   }
   // the lists' own events come without an index, and in AS2 undefined >= 0 is true
   Equipment.wantWindow(event.index > -1);
   Equipment.showWindow();
};

// How the window comes up over a row the pointer or the keys reach, iShow:Window: at once, once the row has stayed
// selected a moment so skimming the list shows nothing, or never. A row a slot chose and a card that asks how many
// bring it up anyway
Equipment.WINDOW_AT_ONCE = 0;
Equipment.WINDOW_AFTER_REST = 1;
Equipment.WINDOW_NEVER = 2;
Equipment.WINDOW_REST_MS = 400;

Equipment.wantWindow = function(row)
{
   var show = Equipment.number(Equipment.settings.windowShow);
   Equipment.windowWanted = row && show == Equipment.WINDOW_AT_ONCE;
   Equipment.restSince = row && show == Equipment.WINDOW_AFTER_REST ? getTimer() : undefined;
};

Equipment.windowRestFrame = function()
{
   if(Equipment.restSince == undefined || getTimer() - Equipment.restSince < Equipment.WINDOW_REST_MS)
   {
      return undefined;
   }
   Equipment.restSince = undefined;
   Equipment.windowWanted = Equipment.menu.inventoryLists.itemList.selectedIndex >= 0;
   Equipment.showWindow();
};

// SkyUI keeps the last row selected when the pointer leaves the list, so its window stayed up with the row
// the pointer was last on. Off the rows the selection goes the way SkyUI drops it itself, and the card and the
// model go with it. The lens of the window still counts as on the row. Only a row the pointer chose on the rows goes
// so, a row the keys or a slot chose stays while the pointer rests or nudges somewhere else. SkyUI's first key press
// on a list with nothing selected leaves the list as driven by the mouse, so the way of each choice is kept from the
// list's own word on it. Called on every move of the mouse
Equipment.leaveRows = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   var lens = Equipment.lens;
   if(list.selectedIndex < 0 || Equipment.rowByKeys || !list.isMouseDrivenNav || Equipment.menu._platform != 0 || Equipment.anchorSlot != undefined || Equipment.context._visible || Equipment.dragging || Equipment.inspecting || !Equipment.listsAnswer())
   {
      return undefined;
   }
   if(Equipment.rowUnderMouse() != undefined || lens._visible && lens.hitTest(_root._xmouse, _root._ymouse, true))
   {
      return undefined;
   }
   list.selectedIndex = -1;
};

// SkyUI tells with every new selection whether keys made it (SELECT_KEYBOARD) or the pointer
Equipment.SELECT_KEYBOARD = 1;
Equipment.rowChosen = function(event)
{
   Equipment.rowByKeys = event.keyboardOrMouse == Equipment.SELECT_KEYBOARD;
};

// The lens sits under the right end of the selected row, and a pointer going for it from the name crosses the
// next row on the way. That row would take the window and the lens away. While the pointer heads for the lens,
// a row it crosses waits a moment before it's selected, the way menus with side menus wait for the pointer
Equipment.aimForLens = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   list.equipmentRollOver = list.onItemRollOver;
   list.onItemRollOver = function(index)
   {
      if(Equipment.headsForLens())
      {
         Equipment.lensWait = {index:index, at:getTimer()};
         return undefined;
      }
      Equipment.lensWait = undefined;
      this.equipmentRollOver(index);
      Equipment.lensFrom = {x:_root._xmouse, y:_root._ymouse};
   };
};

// whether the pointer goes toward the lens from where it last was on the selected row: the line from there through
// the pointer meets the lens at the pointer or further on
Equipment.headsForLens = function()
{
   var from = Equipment.lensFrom;
   var lens = Equipment.lens;
   var slack = Equipment.LENS_AIM_SLACK;
   var box;
   if(from == undefined || !lens._visible || Equipment.menu._platform != 0)
   {
      return false;
   }
   box = lens.getBounds(_root);
   box = {xMin:box.xMin - slack, yMin:box.yMin - slack, xMax:box.xMax + slack, yMax:box.yMax + slack};
   return Equipment.stepsToBox(from.x, from.y, _root._xmouse - from.x, _root._ymouse - from.y, box) >= 1;
};

// how many steps a ray from a point takes to leave a box, -1 when it misses it
Equipment.stepsToBox = function(x, y, dx, dy, box)
{
   var near = 0;
   var far = Number.MAX_VALUE;
   var axes = [[x, dx, box.xMin, box.xMax], [y, dy, box.yMin, box.yMax]];
   var i = 0;
   var a;
   var b;
   while(i < axes.length)
   {
      if(axes[i][1] == 0)
      {
         if(axes[i][0] < axes[i][2] || axes[i][0] > axes[i][3])
         {
            return -1;
         }
      }
      else
      {
         a = (axes[i][2] - axes[i][0]) / axes[i][1];
         b = (axes[i][3] - axes[i][0]) / axes[i][1];
         near = Math.max(near, Math.min(a, b));
         far = Math.min(far, Math.max(a, b));
      }
      i++;
   }
   return near <= far ? far : -1;
};

// On every move of the mouse: where the pointer is on the selected row, and whether a waiting row gets its turn.
// It gets it once the pointer turns away from the lens, and never when the pointer reached the lens
Equipment.followLensAim = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   var lens = Equipment.lens;
   if(Equipment.lensWait == undefined)
   {
      if(Equipment.rowUnderMouse() == list.selectedEntry && list.selectedEntry != undefined)
      {
         Equipment.lensFrom = {x:_root._xmouse, y:_root._ymouse};
      }
   }
   else if(lens._visible && lens.hitTest(_root._xmouse, _root._ymouse, true))
   {
      Equipment.lensWait = undefined;
   }
   else if(!Equipment.headsForLens())
   {
      Equipment.endLensWait();
   }
   else
   {
      // still on its way, a slow hand gets its moment from the last move
      Equipment.lensWait.at = getTimer();
   }
};

Equipment.lensWaitFrame = function()
{
   if(Equipment.lensWait != undefined && getTimer() - Equipment.lensWait.at >= Equipment.LENS_AIM_MS)
   {
      Equipment.endLensWait();
   }
};

// the waiting row is selected if the pointer is still on it
Equipment.endLensWait = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   var wait = Equipment.lensWait;
   Equipment.lensWait = undefined;
   if(wait != undefined && list.entryList[wait.index] != undefined && list.entryList[wait.index] == Equipment.rowUnderMouse())
   {
      list.equipmentRollOver(wait.index);
      Equipment.lensFrom = {x:_root._xmouse, y:_root._ymouse};
   }
};

// The window is up as long as the menu keeps the card and the model of an item up
// A selected row can be out of sight, scrolled away by the bar of the list, or gone for a moment
// while the list is put together again. The window then stays where it was, with the card and the model
Equipment.showWindow = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   var entry = list.selectedEntry;
   var row = entry == undefined ? undefined : list.getClipByIndex(entry.clipIndex);
   var popup = Equipment.anchorSlot != undefined;
   var wanted = Equipment.windowWanted || Equipment.cardAsking;
   Equipment.keepCard();
   Equipment.window._visible = wanted || popup;
   Equipment.window.sheet._visible = Equipment.window._visible;
   // SkyUI keeps the card of a selected row up, the window it lies on decides here
   if(!Equipment.leavesCard())
   {
      Equipment.menu.itemCard._parent._visible = Equipment.window._visible;
   }
   if(popup)
   {
      Equipment.placeWindow(undefined);
   }
   else if(wanted && row != undefined)
   {
      Equipment.placeWindow(row);
   }
   Equipment.showLens(Equipment.window._visible && !popup);
   Equipment.askModel();
   // the window of the row whose menu is up may come after the menu. The menu's copy lies over the model, without one
   // the menu steps off its model, or where it can't the model steps out under the menu
   if(Equipment.context._visible && Equipment.contextEntry != undefined && Equipment.window._visible && !Equipment.contextOverlaid)
   {
      Equipment.clearOfModel(Equipment.context, Equipment.context.wide, Equipment.context.high, Equipment.barTop() - Equipment.MARGIN);
      Equipment.coverWindow();
   }
   if(Equipment.contextOverWindow() && !Equipment.contextOverlaid)
   {
      Equipment.contextHidesModel = true;
   }
   if(Equipment.plugin() != undefined)
   {
      Equipment.plugin().ShowPreview(Equipment.window._visible && Equipment.contextHidesModel != true);
   }
};

// The card keeps every part it can ever show and hides the ones the item has no use for
// These are the parts it shows now
Equipment.shownParts = function()
{
   var card = Equipment.menu.itemCard;
   var parts = [];
   var part;
   for(var name in card)
   {
      part = card[name];
      // the numbers of a chest's card and the pieces of its longer frame live inside it and aren't the card's own
      if(part._parent == card && part._name == name && part._visible && part._alpha > 0 && name.indexOf("equipment") != 0)
      {
         parts.push(part);
      }
   }
   return parts;
};

// The edges of a clip or a text field as another clip sees them. A text field ends where its text does: SkyUI's
// field of a potion's effects stands several lines high, and a poison of one short line left an empty strip under it
Equipment.partBox = function(part, viewer)
{
   if(part.getBounds != undefined)
   {
      return part.getBounds(viewer);
   }
   var high = part.textHeight > 0 ? Math.min(part._height, (part.textHeight + 4) * part._yscale / 100) : part._height;
   var from = {x:part._x, y:part._y};
   var to = {x:part._x + part._width, y:part._y + high};
   part._parent.localToGlobal(from);
   part._parent.localToGlobal(to);
   viewer.globalToLocal(from);
   viewer.globalToLocal(to);
   return {xMin:from.x, yMin:from.y, xMax:to.x, yMax:to.y};
};

// The card starts at its own top line, where all its kinds start, and ends under the lowest part it shows now
// Both are measured from where the card is placed
Equipment.cardSpan = function()
{
   if(Equipment.leavesCard())
   {
      return {top:0, bottom:0};
   }
   var card = Equipment.menu.itemCard;
   var scale = card._parent._yscale / 100;
   var parts = Equipment.shownParts();
   var bottom = 0;
   var i = 0;
   while(i < parts.length)
   {
      bottom = Math.max(bottom, Equipment.partBox(parts[i], card).yMax);
      i++;
   }
   return {top:card._y * scale, bottom:(card._y + (bottom + Equipment.CARD_PAD) * card._yscale / 100) * scale};
};

Equipment.drawWindow = function(high)
{
   var window = Equipment.window;
   window.high = high;
   window.touch._width = window.wide;
   window.touch._height = high;
};

// Under a window that wears the card's frame the sheet fills the middle the copies of the frame leave clear, in
// the frame's own see-through, so the rows show through the whole window as the world does through SkyUI's card.
// A frame the window can't wear leaves the sheet as the window's own frame
Equipment.drawSheet = function(framed)
{
   var window = Equipment.window;
   var sheet = window.sheet;
   var popup = Equipment.anchorSlot != undefined;
   var hole = framed ? window.hole.left + "," + window.hole.top + "," + window.hole.right + "," + window.hole.bottom + "," + window.see : "";
   sheet._x = window._x;
   sheet._y = window._y;
   sheet._xscale = window._xscale;
   if(window.high == sheet.high && popup == sheet.popup && window.wide == sheet.wide && framed == sheet.framed && hole == sheet.hole)
   {
      return undefined;
   }
   sheet.high = window.high;
   sheet.popup = popup;
   sheet.wide = window.wide;
   sheet.framed = framed;
   sheet.hole = hole;
   sheet.clear();
   if(framed)
   {
      Equipment.seeThroughSheet(sheet, window);
   }
   else
   {
      sheet.lineStyle(1, popup ? Equipment.accentInk() : Equipment.inkOf("edge"), popup ? 55 : 30, true);
      sheet.beginFill(Equipment.inkOf("sheet"), popup && !Equipment.paper ? 96 : 100);
      Equipment.roundBox(sheet, 0, 0, window.wide, window.high, 4);
   }
   sheet.endFill();
};

// The whole window as dark as WINDOW_OPACITY. The copies of the frame are as see-through as SkyUI's card along the
// edges and sides, a layer under the whole window makes up the rest there, and the middle the copies leave clear
// gets a layer of the frame's own see-through on top of it, so the two add up the same everywhere
Equipment.WINDOW_OPACITY = 85;
Equipment.seeThroughSheet = function(sheet, window)
{
   var wanted = Equipment.WINDOW_OPACITY / 100;
   var frame = window.see / 100;
   var under = frame < wanted ? 1 - (1 - wanted) / (1 - frame) : 0;
   var hole = window.hole;
   if(under > 0)
   {
      sheet.beginFill(Equipment.inkOf("sheet"), under * 100);
      Equipment.roundBox(sheet, 0, 0, window.wide, window.high, 4);
      sheet.endFill();
   }
   sheet.beginFill(Equipment.inkOf("sheet"), Math.min(frame, wanted) * 100);
   Equipment.roundBox(sheet, hole.left, hole.top, hole.right - hole.left, hole.bottom - hole.top, 0);
};

// the height of everything under the card: the numbers of the panel, the body slots and, in a slot window, its buttons
Equipment.underCard = function(above)
{
   var high = Equipment.statsHeight() + Equipment.stolenHeight();
   var slots = Equipment.bodySlotsHeight();
   if(Equipment.anchorSlot != undefined)
   {
      high += slots + Equipment.ACTION_HIGH + 2 * Equipment.ACTION_GAP + Equipment.ACTION_EDGE;
   }
   else
   {
      // a window above its row has the lens in its lower corner, the numbers stay clear of it and the body slots share
      // that strip with it. Below its row the lens stands at the row's end, a strip kept for it there was an empty
      // band under a short card. Under SkyUI's frame the strip stands over its bottom edge, the quiet lines (a quest,
      // the mod an item is from) lay on the frame's line there
      high += Math.max(above == true ? Equipment.LENS_SIZE : 0, slots) + Equipment.frameEdge();
   }
   return high;
};

// How high the bottom edge of SkyUI's frame is in the window's units, the ornaments and the line, as the window
// wears it. Nothing for a window that keeps its own sheet (a skin's frame drawn as a picture)
Equipment.frameEdge = function()
{
   if(Equipment.leavesCard())
   {
      return 0;
   }
   var window = Equipment.window;
   var card = Equipment.menu.itemCard;
   var art = card != undefined && card.background != undefined ? card.background.getBounds(card) : undefined;
   var top = {x:0, y:0};
   var low;
   if(art == undefined || window.sheet.framed == false || !(art.yMax - art.yMin > 0))
   {
      return 0;
   }
   low = {x:0, y:Math.min(Equipment.WINDOW_FRAME_KEEP, (art.yMax - art.yMin) / 3)};
   card.localToGlobal(top);
   card.localToGlobal(low);
   window.globalToLocal(top);
   window.globalToLocal(low);
   return Math.max(0, low.y - top.y);
};

Equipment.placeWindow = function(row)
{
   var menu = Equipment.menu;
   var window = Equipment.window;
   var card = menu.itemCard._parent;
   var slot = Equipment.anchorSlot;
   var bottom = (slot != undefined ? menu.bottomBar._y : Equipment.barTop()) - Equipment.MARGIN;
   var laid = Equipment.windowLaid;
   // A list that scrolls smoothly lays its rows out every 16 ms while it moves, the window of the same card only
   // goes along with its row then. Anything new on the card lays it all out again
   var along = Equipment.listMoving == true && slot == undefined && laid != undefined && laid.entry == Equipment.windowEntry() && laid.stamp == Equipment.cardStamp && laid.wide == window.wide && laid.squash == Equipment.squash;
   var span;
   var body;
   var bodyAbove;
   var framed;
   var below;
   var at;
   var rowTop;
   var rowBottom;
   var roomAbove;
   var roomBelow;

   // The frame the card draws for itself has a height of its own for each kind of card and knows nothing
   // of a text that has grown. The window is drawn to the measure of the card and is its frame. The card's own
   // stolen note shrinks with the card into its corner, the window has a line of its own for it
   if(along)
   {
      span = laid.span;
      body = laid.body;
   }
   else
   {
      if(!Equipment.leavesCard())
      {
         menu.itemCard.background._visible = false;
         menu.itemCard.StolenTextInstance._visible = false;
      }
      Equipment.windowUnits(slot != undefined ? 1 : Equipment.squash);
      if(!Equipment.leavesCard())
      {
         Equipment.layKind();
      }
      span = Equipment.cardSpan();
      Equipment.fillStats(window.wide);
      body = span.bottom - span.top + Equipment.underCard(false);
      window.edge = slot == undefined ? Equipment.frameEdge() : 0;
   }
   // a window over its row keeps a strip for the lens, one under it doesn't
   bodyAbove = body + (along ? 0 : Equipment.underCard(true) - Equipment.underCard(false));

   if(slot != undefined)
   {
      // beside the slot, toward the doll, level with the slot as far as the screen lets it
      window.preview = Equipment.PREVIEW_HEIGHT;
      Equipment.drawWindow(window.preview + body);
      if(!Equipment.placeByPieces(slot, window, bottom))
      {
         window._x = Equipment.besideSlot(slot, window.wide);
         window._y = Math.max(Equipment.frame.top, Math.min(slot.box._y, bottom - window.high));
      }
      window.above = false;
      window.clamped = false;
   }
   else
   {
      rowTop = {x:0, y:row._y};
      rowBottom = {x:0, y:row._y + Equipment.menu.inventoryLists.itemList.layout.entryHeight};
      row._parent.localToGlobal(rowTop);
      row._parent.localToGlobal(rowBottom);
      Equipment.toPanel(rowTop);
      Equipment.toPanel(rowBottom);
      below = rowBottom.y + Equipment.WINDOW_GAP;
      roomBelow = bottom - below;
      roomAbove = rowTop.y - Equipment.WINDOW_GAP - Equipment.frame.top;
      window.above = Equipment.PREVIEW_HEIGHT + body > roomBelow && (Equipment.PREVIEW_HEIGHT + bodyAbove <= roomAbove || roomAbove > roomBelow);
      body = window.above ? bodyAbove : body;
      // a long card fits on neither side of its row, the model gives up part of its place first
      window.preview = Math.max(Equipment.PREVIEW_LEAST, Math.min(Equipment.PREVIEW_HEIGHT, (window.above ? roomAbove : roomBelow) - body));
      Equipment.drawWindow(window.preview + body);
      // and when even that is not enough the window lies over its row, the lens then sits at the end of the row
      window.clamped = window.high > (window.above ? roomAbove : roomBelow);
      window.rowEdge = window.above ? rowTop.y : rowBottom.y;
      window.rowMiddle = (rowTop.y + rowBottom.y) / 2;
      // the edge of the rows is measured anew, while the panel is built the list hasn't its full width yet
      Equipment.placeWindowLeft();
      window._x = window.left;
      window._y = window.above ? Math.max(Equipment.frame.top, rowTop.y - Equipment.WINDOW_GAP - window.high) : Math.min(below, bottom - window.high);
   }
   at = Equipment.toStage({x:window._x, y:window._y + window.preview - span.top});
   menu.globalToLocal(at);
   if(!Equipment.leavesCard())
   {
      card._x = at.x;
      card._y = at.y;
   }
   if(along && window.high == laid.high && window.preview == laid.preview)
   {
      Equipment.drawSheet(laid.framed);
   }
   else
   {
      framed = Equipment.frameWindow();
      Equipment.drawSheet(framed);
      Equipment.placeStats(window, window.preview + span.bottom - span.top);
      Equipment.placeActions();
      Equipment.windowLaid = slot == undefined ? {entry:Equipment.windowEntry(), stamp:Equipment.cardStamp, wide:window.wide, squash:Equipment.squash, span:span, body:body, high:window.high, preview:window.preview, framed:framed} : undefined;
   }
   Equipment.aimPreview();
   Equipment.coverWindow();
};

// The window wears the card's own frame, SkyUI's or a skin's drawn like it: the top edge with its ornaments goes up
// over the model, the bottom edge down under the numbers, and a plain slice of the frame stretches between. A frame
// whose copies come out empty (a skin's picture in clips of its own) stays hidden and the window keeps its own sheet.
// Tells whether the window wears the card's frame
Equipment.WINDOW_FRAME_KEEP = 30;
Equipment.WINDOW_FRAME_SLICE = 4;
Equipment.WINDOW_FRAME_SIDES = 12;
Equipment.frameWindow = function()
{
   var window = Equipment.window;
   var card = Equipment.menu.itemCard;
   var back = card.background;
   var top = {x:0, y:0};
   var low = {x:0, y:window.high};
   var art;
   var pieces;
   if(back == undefined || Equipment.leavesCard())
   {
      return false;
   }
   Equipment.cardFrameAsSkyUI(back);
   back._visible = true;
   art = back.getBounds(card);
   window.localToGlobal(top);
   window.localToGlobal(low);
   card.globalToLocal(top);
   card.globalToLocal(low);
   pieces = Equipment.windowPieces(art, top.y, low.y, Equipment.WINDOW_FRAME_KEEP, Equipment.WINDOW_FRAME_SLICE);
   if(!(art.yMax - art.yMin > 0) || pieces.sliceHigh < 1 || pieces.fillHigh < 1 || !Equipment.frameCopies(card, back, art, pieces.cut, pieces, Equipment.WINDOW_FRAME_SIDES))
   {
      Equipment.cardFrameAsSkyUI(back);
      back._visible = false;
      return false;
   }
   window.hole = Equipment.frameHole(window, card, art, pieces);
   window.see = back._alpha;
   return true;
};

// The copies of the frame bring the card's own dark fill along its top and bottom edges and down its sides. The
// sheet fills only what lies between, in the window's units, so the whole window is as see-through as SkyUI's card
Equipment.frameHole = function(window, card, art, pieces)
{
   var from = {x:art.xMin + Equipment.WINDOW_FRAME_SIDES, y:pieces.cut - pieces.rise};
   var to = {x:art.xMax - Equipment.WINDOW_FRAME_SIDES, y:pieces.cut - pieces.rise + pieces.fillHigh};
   card.localToGlobal(from);
   card.localToGlobal(to);
   window.globalToLocal(from);
   window.globalToLocal(to);
   return {left:from.x, top:from.y, right:to.x, bottom:to.y};
};

// The pieces of the card's frame for the window, in the card's units: the top edge keeps its size and goes up to
// top, the bottom edge keeps its size and goes to bottom, a slice just under the top edge stretches between them.
// The frame may come out shorter than SkyUI drew it, the frame of a kind of card knows nothing of a text that grew
Equipment.windowPieces = function(art, top, bottom, keep, slice)
{
   var kept = Math.min(keep, (art.yMax - art.yMin) / 3);
   var cut = art.yMin + kept;
   return {cut:cut, rise:art.yMin - top, bottomFrom:art.yMax - kept, shift:bottom - art.yMax, sliceTop:cut, sliceHigh:Math.min(slice, art.yMax - art.yMin - 2 * kept), fillHigh:bottom - top - 2 * kept};
};

// A window or a menu of a slot opens beside it toward the doll, the doll is drawn under them. The names under
// the slots reach past them toward the doll, so it keeps clear of the widest name of the column
Equipment.besideSlot = function(slot, wide)
{
   var size = Equipment.SLOT_SIZE;
   var reach = size;
   var other;
   var i = 0;
   while(i < Equipment.SLOTS.length)
   {
      other = Equipment.SLOTS[i];
      if(other.column == slot.column && other.box.label != undefined && Equipment.slotShown(other))
      {
         reach = Math.max(reach, Math.min(Equipment.LABEL_WIDTH, other.box.label.textWidth + 4));
      }
      i++;
   }
   return slot.column == 0 ? slot.box._x + reach + Equipment.POPUP_GAP : slot.box._x + size - reach - Equipment.POPUP_GAP - wide;
};

// The game shows the model of the selected item where the top part of the window is. SkyUI's frame takes the top of
// that part with its edge, a long sword reached over it: the model's room starts under the edge and goes across the
// whole window, the plugin fits what is seen of the model into it both ways and lays a long one along it
Equipment.aimPreview = function()
{
   var window = Equipment.window;
   var high = window.preview != undefined ? window.preview : Equipment.PREVIEW_HEIGHT;
   var wide = Equipment.windowWide();
   var top = window.sheet.framed == true && window.hole != undefined ? Math.max(0, Math.min(window.hole.top, high / 2)) : 0;
   var screen = Stage.visibleRect.height;
   window.previewTop = top;
   var middle = Equipment.share(window._x + wide / 2, window._y + (top + high) / 2);
   if(Equipment.plugin() != undefined && !Equipment.inspecting)
   {
      Equipment.plugin().PlacePreview(middle.x, middle.y, (high - 2 * Equipment.WINDOW_PAD) / screen, (high - top - 2 * Equipment.WINDOW_PAD) / screen, (wide - 2 * Equipment.WINDOW_PAD) / screen);
   }
};

// A window or a menu can lie over slot models. Those are 3D drawn over the menu, so what the open one
// overlaps steps out while it's open. The doll is drawn under the menu and stays, a menu with its copy drawn over
// the 3D needs nothing to step out
Equipment.coverWindow = function()
{
   var window = Equipment.window;
   var context = Equipment.context;
   var over;
   var from;
   var to;
   if(Equipment.plugin() == undefined)
   {
      return undefined;
   }
   // the sheet of the look lies over the list, a row selected under it would bring its model over the sheet
   if(Equipment.look != undefined && Equipment.look._visible)
   {
      Equipment.coverLook();
      return undefined;
   }
   if(context._visible && !Equipment.contextOverlaid)
   {
      over = {x:context._x, y:context._y, wide:context.wide, high:context.high};
   }
   else if(Equipment.anchorSlot != undefined && window._visible)
   {
      over = {x:window._x, y:window._y, wide:Equipment.windowWide(), high:window.high};
   }
   if(over == undefined)
   {
      Equipment.plugin().Cover();
      return undefined;
   }
   from = Equipment.share(over.x, over.y);
   to = Equipment.share(over.x + over.wide, over.y + over.high);
   Equipment.plugin().Cover(from.x, from.y, to.x, to.y);
};

// The kind of the item stands under its name: its class and its kind as the chips and the column of kinds name them,
// Heavy Armor, Head or One-Handed, Sword. The name keeps its place on every kind of card, the parts under it move down
// a line. They are moved back before every new layout, a part the card placed anew meanwhile isn't touched. The
// card of a piece has the skill in small letters in its corner too, it steps out while the line says it
Equipment.layKind = function()
{
   var card = Equipment.menu.itemCard;
   var entry = Equipment.windowEntry();
   var skill = card.itemInfo != undefined ? card.itemInfo.skillText : undefined;
   var words = entry != undefined && Equipment.settings.quietTags ? Equipment.kindLine(entry, skill) : "";
   var corner = card.SkillTextInstance;
   var moved = Equipment.kindMoved;
   var name = card.ItemText;
   var kind = card.equipmentKind;
   var parts;
   var top;
   var box;
   var i = 0;
   while(moved != undefined && i < moved.length)
   {
      if(moved[i]._y == moved[i].kindTo)
      {
         moved[i]._y -= Equipment.KIND_LINE;
      }
      moved[i].kindTo = undefined;
      i++;
   }
   Equipment.kindMoved = [];
   if(kind == undefined)
   {
      kind = card.createTextField("equipmentKind", 900, 0, 0, 10, 10);
      Equipment.styleText(kind, Equipment.KIND_TEXT, "center", Equipment.inkOf("kind"));
   }
   kind._visible = false;
   if(corner != undefined && corner.equipmentHid == true)
   {
      corner._visible = true;
      corner.equipmentHid = false;
   }
   if(words == "" || name == undefined || !name._visible)
   {
      return undefined;
   }
   if(corner != undefined && corner._visible && entry.formType == Equipment.ARMOR)
   {
      corner._visible = false;
      corner.equipmentHid = true;
   }
   top = Equipment.nameBox(card);
   parts = Equipment.shownParts();
   i = 0;
   while(i < parts.length)
   {
      box = Equipment.partBox(parts[i], card);
      // by its middle: a skin's fields can be taller than their words and reach up past the bottom of the name
      if(parts[i] != name && parts[i] != kind && parts[i] != card.background && (box.yMin + box.yMax) / 2 >= top.yMax)
      {
         parts[i]._y += Equipment.KIND_LINE;
         parts[i].kindTo = parts[i]._y;
         Equipment.kindMoved.push(parts[i]);
      }
      i++;
   }
   kind.textColor = Equipment.inkOf("kind");
   kind.text = words.toUpperCase();
   kind._x = top.xMin;
   kind._width = top.xMax - top.xMin;
   kind._height = Equipment.KIND_LINE + 4;
   kind._y = top.yMax;
   kind._visible = true;
};

// The name as the card shows it, in the card's units: its field is taller than its letters, the bottom is where
// the letters end
Equipment.nameBox = function(card)
{
   var field = card.ItemText.ItemTextField;
   var box = Equipment.partBox(card.ItemText, card);
   var bottom;
   if(field == undefined)
   {
      return box;
   }
   bottom = {x:0, y:field._y + field.textHeight + 4};
   card.ItemText.localToGlobal(bottom);
   card.globalToLocal(bottom);
   return {xMin:box.xMin, xMax:box.xMax, yMin:box.yMin, yMax:Math.min(box.yMax, bottom.y)};
};

// the word of a chip in the language of the game
Equipment.chipWord = function(family, key)
{
   var rows = Equipment.CHIP_ROWS[family];
   var g = 0;
   var c;
   while(rows != undefined && g < rows.length)
   {
      c = 0;
      while(c < rows[g].length)
      {
         if(rows[g][c].key == key)
         {
            return Equipment.translated(rows[g][c]);
         }
         c++;
      }
      g++;
   }
   return "";
};

// The words under the name: the class of a piece and its kind, the hand of a weapon and its kind, the kind of
// a potion, or the kind SkyUI gives everything else. A light or heavy piece is named by the skill the game gives
// its card, Light Armor or Heavy Armor in the game's language, the chip's word alone said only Light
Equipment.kindLine = function(entry, skill)
{
   var type = entry.subTypeDisplay != undefined && entry.subTypeDisplay != "-" ? String(entry.subTypeDisplay) : "";
   var parts = [];
   var keys;
   var i = 0;
   if(entry.formType == Equipment.ARMOR)
   {
      keys = Equipment.isShield(entry) ? (entry.weightClass == 1 ? ["heavy"] : (entry.weightClass == 0 ? ["light"] : [])) : ["light", "heavy", "clothing", "jewelry"];
      while(i < keys.length)
      {
         if(Equipment.isShield(entry) || Equipment.chipHolds(keys[i], entry))
         {
            parts.push((keys[i] == "light" || keys[i] == "heavy") && skill != undefined && String(skill) != "" ? String(skill) : Equipment.chipWord("armor", keys[i]));
            break;
         }
         i++;
      }
   }
   else if(entry.formType == Equipment.WEAPON && !Equipment.ranged(entry) && entry.subType != Equipment.STAFF)
   {
      parts.push(Equipment.chipWord("weapons", Equipment.twoHanded(entry) ? "twoHanded" : "oneHanded"));
   }
   else if(entry.formType == Equipment.POTION && !Equipment.kindHolds("poison", entry) && !Equipment.kindHolds("meal", entry) && !Equipment.kindHolds("drink", entry))
   {
      parts.push(Equipment.translated({words:"$Potion", fallback:"Potion"}));
   }
   if(type != "" && (parts.length == 0 || parts[0] != type))
   {
      parts.push(type);
   }
   return parts.join(", ");
};

// The cards of Dear Diary Dark Mode and the menus made on it (Norden UI, Untarnished UI) fit their effects by hand,
// a step of the font smaller at a time until the text is short enough, with no floor. A line for every effect left
// them a text in tiny letters, so such a card gets the effects as the game gives them
Equipment.cardFitsByHand = function()
{
   return typeof Equipment.menu.itemCard.changeTextSize == "function";
};

// One line for every sentence of an enchantment's text when it has more than one, the card then shows each effect
// apart. A sentence ends at a stop followed by a space and a capital or a number, tags of the text left whole
Equipment.effectLines = function(text)
{
   var source = String(text);
   var out = "";
   var tag = false;
   var i = 0;
   var c;
   while(i < source.length)
   {
      c = source.charAt(i);
      tag = c == "<" ? true : (c == ">" ? false : tag);
      out += c;
      if(!tag && (c == "." || c == "!") && source.charAt(i + 1) == " " && Equipment.sentenceStarts(source, i + 2))
      {
         out += "<br>";
         i += 2;
         continue;
      }
      i++;
   }
   return out;
};

Equipment.sentenceStarts = function(source, at)
{
   var c;
   while(source.charAt(at) == "<" && source.indexOf(">", at) > 0)
   {
      at = source.indexOf(">", at) + 1;
   }
   c = source.charAt(at);
   return c != "" && (c >= "0" && c <= "9" || c.toUpperCase() == c && c.toLowerCase() != c);
};

// For the test stand: the kind line of the selected row, its column of kinds, and the text of effects the card shows
Equipment.kindReport = function(note)
{
   var card = Equipment.menu.itemCard;
   var entry = Equipment.menu.inventoryLists.itemList.selectedEntry;
   var effects = [];
   var parts = Equipment.shownParts();
   var i = 0;
   while(i < parts.length)
   {
      if(parts[i].text != undefined && parts[i]._name.indexOf("Label") >= 0 && parts[i].text != "")
      {
         effects.push(parts[i]._name + " " + Math.round(parts[i]._height) + " high: " + parts[i].text.split("\r").join(" / "));
      }
      i++;
   }
   var name = Equipment.nameBox(card);
   var field = card.ItemText.ItemTextField;
   skse.Log("PANELTEST kind " + note + ": " + (entry != undefined ? entry.text + " = \"" + Equipment.kindLine(entry) + "\", type " + entry.subTypeDisplay + ", shown " + card.equipmentKind._visible + " \"" + card.equipmentKind.text + "\" at " + Math.round(card.equipmentKind._y) : "nothing selected") + "; corner skill \"" + card.SkillTextInstance.text + "\" shown " + card.SkillTextInstance._visible + "; name to " + Math.round(name.yMax) + " (field y " + Math.round(field._y) + " high " + Math.round(field._height) + " text " + Math.round(field.textHeight) + "); effects " + effects.join(" | "));
};

// the card moreHUD Inventory Edition draws beside the SkyUI one, when that mod is there
Equipment.otherCard = function()
{
   var root = Equipment.menu._parent;
   for(var name in root)
   {
      if(root[name].AHZmoreHUDInventory_mc != undefined)
      {
         return root[name].AHZmoreHUDInventory_mc;
      }
   }
   return undefined;
};

// A check by numbers that the card shown now sits inside the window
// The first line is the window, then one line for every part the card shows, each as left, top, right
// and bottom in the panel's units, with OUT on a part that leaves the window
// The card of moreHUD has a line when that mod is there, with its edges when it's drawn
// The lines are returned, kept in Equipment.cardReported and written to the log of SKSE,
// Equipment.cardOutside is the number of lines with OUT
// for the test stand: how the window got its width and how large the card is in it
Equipment.windowSizeReport = function()
{
   var window = Equipment.window;
   var list = Equipment.listFullWide != undefined ? Equipment.listFullWide : Equipment.rowsEdge().wide;
   var card = Equipment.menu.itemCard._parent;
   skse.Log("PANELTEST window size " + Equipment.settings.windowSize + "%: list " + Math.round(list) + ", screen high " + Math.round(Stage.visibleRect.height) + ", window " + Math.round(Equipment.windowWide()) + " wide " + Math.round(window.high) + " high at " + Math.round(window._x) + "," + Math.round(window._y) + ", card " + Math.round(card._yscale) + "%; " + Equipment.edgeReport());
};

Equipment.cardReport = function()
{
   var menu = Equipment.menu;
   var window = Equipment.window;
   var card = menu.itemCard;
   var frame = {xMin:window._x, yMin:window._y, xMax:window._x + Equipment.windowWide(), yMax:window._y + window.high};
   var parts = Equipment.shownParts();
   var other = Equipment.otherCard();
   var lines = [];
   var i = 0;

   Equipment.cardOutside = 0;
   lines.push("window " + Equipment.boxLine(frame, frame) + " shown " + window._visible + " frame " + card._currentframe + " type " + card.itemInfo.type);
   lines.push("item " + card.itemInfo.name + " armor " + card.itemInfo.armor + ", numbers " + (window.statLines != undefined ? window.statLines.length : 0) + ", quiet lines \"" + String(window.bodySlots.text).split("\r").join(" / ") + "\"");
   lines.push("model part " + Math.round(window.preview) + " high, room from " + Math.round(window.previewTop) + ", hole " + (window.hole != undefined ? Math.round(window.hole.left) + "," + Math.round(window.hole.top) + " to " + Math.round(window.hole.right) + "," + Math.round(window.hole.bottom) : "none") + ", see " + window.see);
   lines.push("sheet shown " + window.sheet._visible + " framed " + window.sheet.framed + " at " + Math.round(window.sheet._x) + "," + Math.round(window.sheet._y) + " " + Equipment.boxLine(window.sheet.getBounds(Equipment.under), frame) + " depth " + window.sheet.getDepth() + " of under " + Equipment.under.getDepth() + ", lists " + menu.inventoryLists.getDepth() + ", card " + card._parent.getDepth() + ", words " + Equipment.windowLayer.getDepth() + "; card frame copies " + (card.equipmentFrameFill != undefined) + " own frame shown " + card.background._visible);
   while(i < parts.length)
   {
      lines.push(parts[i]._name + " " + Equipment.boxLine(Equipment.partBox(parts[i], Equipment.under), frame));
      i++;
   }
   if(other != undefined)
   {
      lines.push("moreHUD card kept " + Equipment.cardKept + " swapped " + other._itemCardOverride + " alpha " + other._alpha + (other._alpha > 0 ? " " + Equipment.boxLine(other.getBounds(Equipment.under), frame) : ""));
   }
   lines.push("outside " + Equipment.cardOutside);

   i = 0;
   while(i < lines.length)
   {
      skse.Log("EquipmentPanel card: " + lines[i]);
      i++;
   }
   Equipment.cardReported = lines.join("\n");
   return Equipment.cardReported;
};

Equipment.boxLine = function(box, frame)
{
   var slack = Equipment.FIT_SLACK;
   var fits = box.xMin >= frame.xMin - slack && box.yMin >= frame.yMin - slack && box.xMax <= frame.xMax + slack && box.yMax <= frame.yMax + slack;
   if(!fits)
   {
      Equipment.cardOutside++;
   }
   return Equipment.tenth(box.xMin) + " " + Equipment.tenth(box.yMin) + " " + Equipment.tenth(box.xMax) + " " + Equipment.tenth(box.yMax) + (fits ? "" : " OUT");
};

Equipment.tenth = function(value)
{
   return Math.round(value * 10) / 10;
};

// things that look at the mouse or wait for the game every frame
Equipment.everyFrame = function()
{
   Equipment.dragFrame();
   Equipment.lensWaitFrame();
   Equipment.windowRestFrame();
   Equipment.watchPieces();
   Equipment.sendEyes();
   Equipment.keepFrameHidden();
};

// The window is the frame of the card. A reskin can fill the card by a way of its own and put the card's frame
// back up with it, Vel'dun UI does for the first card of a menu, so the frame is kept down while the window is up
Equipment.keepFrameHidden = function()
{
   if(Equipment.window._visible && !Equipment.leavesCard())
   {
      Equipment.menu.itemCard.background._visible = false;
   }
};
