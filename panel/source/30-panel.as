Equipment.SLOT_SIZE = 64;
Equipment.ICON_SIZE = 36;
Equipment.ROWS = 5;
Equipment.MARGIN = 14;
Equipment.LABEL_HEIGHT = 18;
Equipment.LABEL_WIDTH = 170;
Equipment.LABEL_LENGTH = 24;
Equipment.PLAYER_GAP = 10;
Equipment.LIST_FRAME = 25;
Equipment.RIGHT_BUTTON = 1;
Equipment.LEFT_HAND_SLOT = 1;
Equipment.DOUBLE_CLICK = 400;
Equipment.SAME_PRESS = 150;
Equipment.PUSH_AFTER = 400;
Equipment.PRESS_USED = 400;
Equipment.BUSY_FOR = 1500;
Equipment.EYE_SIZE = 18;
Equipment.MINI_EYE_SIZE = 15;
Equipment.EYE_INSET = 3;
Equipment.CORNER = 9;
Equipment.EMPTY_LABEL = 9474192;

// worn items sorted into the slots that show them
Equipment.place = function(worn)
{
   var left = Equipment.slotNamed("left");
   var i = 0;
   while(i < Equipment.SLOTS.length)
   {
      Equipment.SLOTS[i].entries = [];
      i++;
   }
   if(worn.right != undefined)
   {
      Equipment.slotNamed("right").entries.push(worn.right);
   }
   if(worn.left != undefined)
   {
      left.entries.push(worn.left);
   }
   if(worn.ammo != undefined)
   {
      Equipment.slotNamed("ammo").entries.push(worn.ammo);
   }
   i = 0;
   while(i < worn.armor.length)
   {
      Equipment.armorSlot(worn.armor[i]).entries.push(worn.armor[i]);
      i++;
   }
   i = 0;
   while(i < Equipment.SLOTS.length)
   {
      Equipment.sortPieces(Equipment.SLOTS[i].entries);
      i++;
   }
   // a weapon that takes both hands is shown in the right one, the left one only tells it's taken
   left.taken = worn.left != undefined && worn.left == worn.right && !Equipment.oneHanded(worn.left);
   Equipment.placeSpells();
};

// A spell in a hand has no row in the item lists, the plugin names what each hand holds. The slot shows the sign SkyUI
// gives the spell's row in the magic menu: its school, fire, frost or shock for destruction. Clicked it acts as an
// empty hand, the game can't take a spell off, only put something else into that hand
Equipment.SCHOOL_ICONS = {s18:"default_alteration", s19:"default_conjuration", s20:"default_destruction", s21:"default_illusion", s22:"default_restoration"};
Equipment.RESIST_ICONS = {r41:{label:"magic_fire", color:0xC73636}, r42:{label:"magic_shock", color:0xEAAB00}, r43:{label:"magic_frost", color:0x1FFBFF}};

Equipment.placeSpells = function()
{
   var plugin = Equipment.plugin();
   var hands = plugin != undefined && plugin.Hands != undefined ? plugin.Hands() : undefined;
   var names = ["right", "left"];
   var slot;
   var held;
   var i = 0;
   while(hands != undefined && i < names.length)
   {
      slot = Equipment.slotNamed(names[i]);
      held = hands[i];
      if(slot != undefined && slot.entries.length == 0 && !slot.taken && held != undefined && held.spell == true)
      {
         slot.entries.push(Equipment.spellEntry(held));
      }
      i++;
   }
};

Equipment.spellEntry = function(held)
{
   var school = Equipment.SCHOOL_ICONS["s" + Equipment.number(held.school)];
   var resist = Equipment.number(held.school) == 20 ? Equipment.RESIST_ICONS["r" + Equipment.number(held.resistance)] : undefined;
   var entry = {formId:Number(held.formId), text:String(held.name), formType:Equipment.SPELL, spell:true, infoSpellCost:Equipment.number(held.cost)};
   entry.iconLabel = resist != undefined ? resist.label : (school != undefined ? school : "default_power");
   if(resist != undefined && !Equipment.noIconColors)
   {
      entry.iconColor = resist.color;
   }
   return entry;
};

Equipment.isSpell = function(entry)
{
   return entry != undefined && entry.spell == true;
};

// a slot holds a few pieces, they're put in order one by one
Equipment.sortPieces = function(pieces)
{
   var i = 1;
   var at;
   var piece;
   while(i < pieces.length)
   {
      piece = pieces[i];
      at = i;
      while(at > 0 && Equipment.pieceOrder(piece, pieces[at - 1]) < 0)
      {
         pieces[at] = pieces[at - 1];
         at--;
      }
      pieces[at] = piece;
      i++;
   }
};

// The pieces of a slot go in a fixed order, so the one the slot shows doesn't change with the sort of the list:
// a cloak first, then by the lowest body slot a piece takes (a helmet before a circlet, gloves before bracers),
// rings of My Precious...es after the ring of the game's own finger
Equipment.pieceOrder = function(a, b)
{
   var rank = Equipment.pieceRank(a) - Equipment.pieceRank(b);
   return rank != 0 ? rank : Equipment.number(a.formId) - Equipment.number(b.formId);
};

Equipment.pieceRank = function(entry)
{
   var mask = Equipment.number(entry.partMask);
   var bit = 0;
   if(entry.formType != Equipment.ARMOR)
   {
      return 0;
   }
   if(mask & Equipment.CLOAK)
   {
      return -1;
   }
   if(Equipment.virtualRing(entry))
   {
      return 32;
   }
   while(bit < 32 && (mask & (1 << bit)) == 0)
   {
      bit++;
   }
   return bit;
};

// Menus made on SkyUI 5 (Vel'dun UI, Dear Diary) have a stage 2560 wide and let the game stretch it over the screen.
// On 16:9 a unit across then shows half as wide as a unit down, and all the panel drew came out squeezed. So the
// panel draws in two layers of the menu, one under the item card and one over it, both stretched back across by that
// much: in the panel's units a square is square on screen. squash is how wide a unit across shows against one down,
// 1 on SkyUI 6 and in any menu that keeps its proportions. The skin's own clips stay as the skin drew them
Equipment.squash = 1;
Equipment.SQUASH_SLACK = 0.02;

Equipment.readSquash = function()
{
   var plugin = Equipment.plugin();
   var told = plugin != undefined && plugin.ScreenSize != undefined ? plugin.ScreenSize() : undefined;
   var stage = Stage.visibleRect;
   var squash = 1;
   if(told != undefined && told.width > 0 && told.height > 0 && stage.width > 0 && stage.height > 0)
   {
      squash = told.width / stage.width / (told.height / stage.height);
   }
   // a pixel of rounding is no stretched menu
   Equipment.squash = Math.abs(squash - 1) < Equipment.SQUASH_SLACK ? 1 : squash;
};

Equipment.buildLayers = function()
{
   var menu = Equipment.menu;
   Equipment.under = menu.createEmptyMovieClip("equipmentUnder", menu.getNextHighestDepth());
   Equipment.over = menu.createEmptyMovieClip("equipmentOver", menu.getNextHighestDepth());
   Equipment.under._xscale = Equipment.over._xscale = 100 / Equipment.squash;
};

// the part of the stage on screen in the panel's units
Equipment.screenRect = function()
{
   var stage = Stage.visibleRect;
   return {x:stage.x * Equipment.squash, y:stage.y, width:stage.width * Equipment.squash, height:stage.height};
};

// the margins the game keeps clear at the edges of the screen, in the panel's units
Equipment.safeInset = function()
{
   return {x:Stage.safeRect.x * Equipment.squash, y:Stage.safeRect.y};
};

// a point of the stage (the mouse, what localToGlobal gave) into the panel's units, and back
Equipment.toPanel = function(point)
{
   if(Equipment.under != undefined)
   {
      Equipment.under.globalToLocal(point);
   }
   else
   {
      point.x *= Equipment.squash;
   }
   return point;
};

Equipment.toStage = function(point)
{
   if(Equipment.under != undefined)
   {
      Equipment.under.localToGlobal(point);
   }
   else
   {
      point.x /= Equipment.squash;
   }
   return point;
};

Equipment.build = function()
{
   Equipment.panel = Equipment.under.createEmptyMovieClip("equipmentPanel", Equipment.under.getNextHighestDepth());
   Equipment.buildStage();
   // the plugin draws the eyes over the 3D of the menu until it says it can't
   Equipment.eyesOver = Equipment.plugin() != undefined;
   Equipment.eyesTold = "";
   var i = 0;
   while(i < Equipment.SLOTS.length)
   {
      Equipment.buildSlot(Equipment.SLOTS[i], i + 1);
      i++;
   }
   Equipment.readHidden();
};

Equipment.buildSlot = function(slot, depth)
{
   var size = Equipment.SLOT_SIZE;
   var box = Equipment.panel.createEmptyMovieClip("slot" + depth, depth);
   var onRight = slot.column == 1;
   slot.entries = [];
   Equipment.buildCell(box, slot, size);
   // The tab of a strip of the pieces sits beside the slot on the inner side. It takes the mouse as a part of the
   // slot, so its press comes to the slot's onPress. With hitTestDisable on it every press fell through to the doll
   box.createEmptyMovieClip("tab", 3);
   // the label is wider than the slot and reaches over the doll, the mouse goes through it to the doll
   box.createEmptyMovieClip("words", 4);
   box.words.hitTestDisable = true;
   box.words.createTextField("label", 0, onRight ? size - Equipment.LABEL_WIDTH : 0, size + 1, Equipment.LABEL_WIDTH, Equipment.LABEL_HEIGHT);
   box.label = box.words.label;
   Equipment.styleText(box.label, 13, onRight ? "right" : "left");
   box.label.html = true;
   Equipment.showSlot(slot);
};

// what a slot and a small slot of a strip share: the touch field, the frame, the sign, the eye and the mouse
Equipment.buildCell = function(box, slot, size)
{
   box.slot = slot;
   slot.box = box;
   // A drawn shape takes no mouse in this game, a press went through the middle of a slot where the model
   // stands. An empty text field takes the mouse over its whole box, so one lies under the slot
   box.createTextField("touch", 0, 0, 0, size, size);
   box.touch.selectable = false;
   box.createEmptyMovieClip("frame", 1);
   box.createEmptyMovieClip("icon", 2);
   Equipment.buildEye(box, size);
   box.onRollOver = function()
   {
      Equipment.hover(this.slot, true);
   };
   box.onRollOut = box.onReleaseOutside = function()
   {
      Equipment.hover(this.slot, false);
   };
   // A clip with press handlers takes the presses of the clips inside it, so the eye never got its own.
   // The slot looks whether the press landed on the eye or on the tab
   box.onPress = function()
   {
      if(Equipment.pointerOnPart(this.eye))
      {
         Equipment.toggleHidden(this.slot);
         return undefined;
      }
      // the tab beside a slot with several pieces pulls their strip out
      if(Equipment.pointerOnPart(this.tab))
      {
         Equipment.openPieces(this.slot);
         return undefined;
      }
      Equipment.click(this.slot);
   };
   // in case some build of the game hands a right press to the clip, the menu of the slot opens from it too
   box.onPressAux = function()
   {
      Equipment.rightPress();
   };
};

// Whether the pointer is on a part of a slot that takes the mouse by its touch field (the eye, the tab). Told in the
// part's own space: the eye drawn by the plugin's movie leaves only the empty field in it, and no scale of the menu
// on some screen moves the answer
Equipment.pointerOnPart = function(part)
{
   var touch;
   if(part == undefined || !part._visible || part.touch == undefined)
   {
      return false;
   }
   touch = part.touch;
   return part._xmouse >= touch._x && part._xmouse <= touch._x + touch._width && part._ymouse >= touch._y && part._ymouse <= touch._y + touch._height;
};

Equipment.cellSize = function(slot)
{
   return slot.parent != undefined ? Equipment.MINI_SIZE : Equipment.SLOT_SIZE;
};

// The plugin tells of every right press while the inventory is open, a press over a slot opens its menu
// In item menus the game also equips the selected row to the left hand on a right press,
// a press the panel used marks the time so that equip gets dropped. With Ctrl, Shift or Alt held (keyHeld) a press
// on a row is SkyUI's or another mod's, the row's menu stays shut and the game's equip goes on
Equipment.rightPress = function(keyHeld)
{
   var slot = Equipment.slotAt(_root._xmouse, _root._ymouse);
   var now = getTimer();
   if(now - Equipment.rightPressedAt < Equipment.SAME_PRESS)
   {
      return undefined;
   }
   Equipment.rightPressedAt = now;
   if(Equipment.closeChargeList())
   {
      // the list of a recharge took the press
   }
   else if(Equipment.inspecting)
   {
      Equipment.leaveInspect();
   }
   else if(slot != undefined)
   {
      Equipment.openContext(slot);
   }
   else if(Equipment.openChipMenu(Equipment.chipAt(_root._xmouse, _root._ymouse)))
   {
      // a subcategory of a chest took the press
   }
   else if(Equipment.settings.rowMenu && keyHeld != true && Equipment.openRowMenu(Equipment.rowUnderMouse()))
   {
      // the menu of the row took the press, the game's equip to the left hand waits out
   }
   else if(Equipment.anchorSlot != undefined)
   {
      Equipment.closePopup();
   }
   else if(Equipment.context._visible)
   {
      Equipment.closeContext();
   }
   else
   {
      return undefined;
   }
   Equipment.usedPressAt = now;
};

// The game equips the selected row on any press while the pointer is over that row, whatever lies on top of it.
// A press a line of a menu, the lens or the dark sheet took marks the time, so the equip that comes with it gets
// dropped. The game may send its equip a frame before the press reaches the menu, the line is still under the
// pointer then
Equipment.usePress = function()
{
   Equipment.usedPressAt = getTimer();
};

Equipment.pressUsed = function()
{
   // a pad equips by its buttons, the pointer may rest anywhere then
   return getTimer() - Equipment.usedPressAt < Equipment.PRESS_USED || Equipment.menu._platform == 0 && Equipment.ownUnderMouse();
};

// whether the pointer is on something of the panel's own over the list: a menu, the lens, the look sheet
Equipment.ownUnderMouse = function()
{
   var top = Mouse.getTopMostEntity();
   while(top != undefined)
   {
      if(top == Equipment.over || top == Equipment.under || top == Equipment.windowLayer)
      {
         return true;
      }
      top = top._parent;
   }
   return false;
};

// the slot whose box holds a point of the stage, labels left out. The small slots of an open strip come first
Equipment.slotAt = function(x, y)
{
   var point = {x:x, y:y};
   var cells;
   var size;
   var i = 0;
   var box;
   if(Equipment.panel == undefined || !Equipment.panel._visible)
   {
      return undefined;
   }
   Equipment.panel.globalToLocal(point);
   cells = Equipment.cells();
   cells.reverse();
   while(i < cells.length)
   {
      box = cells[i].box;
      size = Equipment.cellSize(cells[i]);
      if(point.x >= box._x && point.x <= box._x + size && point.y >= box._y && point.y <= box._y + size)
      {
         return cells[i];
      }
      i++;
   }
   return undefined;
};

// every slot the player kept on the panel and the small slots of the strip that's out
Equipment.cells = function()
{
   var cells = [];
   var i = 0;
   while(i < Equipment.SLOTS.length)
   {
      if(Equipment.slotShown(Equipment.SLOTS[i]))
      {
         cells.push(Equipment.SLOTS[i]);
      }
      i++;
   }
   if(Equipment.pieceStrip != undefined)
   {
      cells = cells.concat(Equipment.pieceStrip.visible);
   }
   return cells;
};

// The little eye in the corner of a slot shows whether its piece is drawn on the character, a press switches it.
// The models of the slots are drawn by the game's 3D after the whole menu, so an eye of the menu went behind a ring
// that fills its slot. The plugin draws the eyes in a movie of their own over that 3D (Equipment.sendEyes), here an
// eye keeps its place, its mouse and its light, and draws itself only when that movie isn't there
Equipment.buildEye = function(box, cell)
{
   var size = box.slot.parent != undefined ? Equipment.MINI_EYE_SIZE : Equipment.EYE_SIZE;
   var eye = box.createEmptyMovieClip("eye", 5);
   eye.size = size;
   eye.createTextField("touch", 0, 0, 0, size, size);
   eye.touch.selectable = false;
   eye.createEmptyMovieClip("sign", 1);
   eye._x = cell - size - Equipment.EYE_INSET;
   eye._y = Equipment.EYE_INSET;
   // the slot around it takes the mouse, its press and its light are worked out by the slot and lightEye
};

// the eye under the pointer turns gold, called on every move of the mouse while the panel is up
Equipment.lightEye = function()
{
   var cells = Equipment.cells();
   var i = 0;
   var slot;
   var eye;
   var lit;
   while(i < cells.length)
   {
      slot = cells[i];
      eye = slot.box.eye;
      lit = slot.hovered && Equipment.pointerOnPart(eye);
      if(lit != eye.lit)
      {
         eye.lit = lit;
         Equipment.drawEye(slot);
      }
      i++;
   }
};

Equipment.layout = function()
{
   var menu = Equipment.menu;
   var screen = Equipment.screenRect();
   var safe = Equipment.safeInset();
   var size = Equipment.SLOT_SIZE;
   var right = screen.x + screen.width - safe.x - Equipment.MARGIN - size;
   var left = Equipment.listEdge().right + Equipment.MARGIN / 2;
   var top = screen.y + safe.y + Equipment.MARGIN;
   var bottom = menu.bottomBar._y - Equipment.MARGIN - Equipment.LABEL_HEIGHT;
   var step = (bottom - top - size) / (Equipment.ROWS - 1);
   var i = 0;
   var slot;
   while(i < Equipment.SLOTS.length)
   {
      slot = Equipment.SLOTS[i];
      slot.box._x = slot.column == 0 ? left : right;
      slot.box._y = top + slot.row * step;
      // a slot the player took off the panel leaves its place empty, the others keep theirs
      slot.box._visible = Equipment.slotShown(slot);
      i++;
   }
   Equipment.frame = {left:left, right:right + size, top:top, bottom:bottom};
   Equipment.placeStage();
};

// whether the player kept a slot on the panel, a small slot of a strip goes with its slot
Equipment.slotShown = function(slot)
{
   var index = slot != undefined ? Equipment.slotIndex(Equipment.ownerOf(slot)) : -1;
   var off = Equipment.settings != undefined ? Equipment.number(Equipment.settings.slotsOff) : 0;
   return index < 0 || (off & (1 << index)) == 0;
};

// Where the item list ends on screen and how wide it is, in the panel's units
// The frame of the list reaches past its content, SkyUI counts the same distance for its item card
Equipment.listEdge = function()
{
   var lists = Equipment.menu.inventoryLists;
   var bounds = lists.getContentBounds();
   var squash = Equipment.squash;
   return {right:(lists._x + bounds[0] + bounds[2] + Equipment.LIST_FRAME) * squash, wide:bounds[2] * squash};
};

// The top of what the bottom bar shows. SkyUI draws its hints and numbers under the bar's own corner, a bar can draw
// them over it instead, Dear Diary's stand 35 higher in the strip of its binder. What lies over the list stays above
// them, the slots stand where the bar shows nothing and keep to the corner
Equipment.barTop = function()
{
   var bar = Equipment.menu.bottomBar;
   var hints = bar.buttonPanel;
   return bar._y + (hints != undefined ? Math.min(0, hints._y * bar._yscale / 100) : 0);
};

// The part of the list its rows take, measured by the item list's own clip with its rows, column names and scroll bar.
// A skin can draw a picture behind the list that reaches far past them, the page and binder of Dear Diary end 80
// further. What lies over the list, the item window and the menu of a row, keeps to the rows, the slots keep clear of
// the whole picture. SkyUI's own frame ends ROWS_FRAME after the clip, and a list SkyUI narrowed keeps its frame
Equipment.ROWS_FRAME = 4;
Equipment.rowsEdge = function()
{
   var list = Equipment.listEdge();
   var rows = Equipment.menu.inventoryLists.itemList.getBounds(Equipment.menu);
   var squash = Equipment.squash;
   var past = rows.xMax > rows.xMin ? Math.max(0, list.right - (rows.xMax + Equipment.ROWS_FRAME) * squash) : 0;
   return {right:list.right - past, wide:list.wide - past};
};

Equipment.listReady = function(list, worn)
{
   if(Equipment.panel == undefined)
   {
      return undefined;
   }
   if(!Equipment.filtering && list.listEnumeration != undefined)
   {
      list.listEnumeration.addFilter(Equipment.slotFilter);
      Equipment.filtering = true;
   }
   Equipment.busy = false;
   Equipment.wearGivenNow(list.entryList);
   Equipment.place(worn);
   var i = 0;
   while(i < Equipment.SLOTS.length)
   {
      Equipment.showSlot(Equipment.SLOTS[i]);
      i++;
   }
   Equipment.refreshPieces();
   Equipment.showModels();
   Equipment.slotChanged();
   Equipment.refreshHero();
};

Equipment.showSlot = function(slot)
{
   var entry = slot.entries[0];
   var filled = entry != undefined && !slot.taken;
   var name = Equipment.shortName(filled ? entry.text : (slot.taken ? "" : slot.title));
   if(filled && slot.entries.length > 1)
   {
      name += " +" + (slot.entries.length - 1);
   }
   // the number goes beside the name: the model of the slot is drawn over anything inside the slot
   slot.box.label.htmlText = Equipment.labelHtml(name, filled ? Equipment.amount(entry) : "", filled, slot.column == 1);
   Equipment.drawFrame(slot);
   Equipment.showIcon(slot);
   Equipment.drawEye(slot);
   Equipment.drawTab(slot);
};

Equipment.shortName = function(name)
{
   var shown = Equipment.gameName(name);
   return shown.length > Equipment.LABEL_LENGTH ? shown.substr(0, Equipment.LABEL_LENGTH - 3) + "..." : shown;
};

// the label of a slot names the piece of its strip under the pointer, the piece it shows otherwise
Equipment.tellPiece = function(mini, over)
{
   var slot = mini.parent;
   var entry = mini.entries[0];
   if(!over || slot.box.label == undefined)
   {
      Equipment.showSlot(slot);
      return undefined;
   }
   slot.box.label.htmlText = Equipment.labelHtml(Equipment.shortName(entry != undefined ? entry.text : slot.title), entry != undefined ? Equipment.amount(entry) : "", entry != undefined, slot.column == 1);
};

Equipment.hex = function(color)
{
   var text = color.toString(16);
   while(text.length < 6)
   {
      text = "0" + text;
   }
   return "#" + text;
};

// a name, then the number of the slot in gold
Equipment.labelHtml = function(name, amount, filled, onRight)
{
   var words = name.split("&").join("&amp;").split("<").join("&lt;");
   // html text forgets the font of the field, the game's own font has to be named in every run of text
   var face = "<font face='$EverywhereMediumFont' size='13' color='";
   var html = face + Equipment.hex(filled ? Equipment.WHITE : Equipment.EMPTY_LABEL) + "'>" + words + "</font>";
   if(amount != "")
   {
      html += face + Equipment.hex(Equipment.GOLD) + "'>  " + amount + "</font>";
   }
   return "<p align='" + (onRight ? "right" : "left") + "'>" + html + "</p>";
};

// the number of a slot: how many arrows, how much armor or damage
Equipment.amount = function(entry)
{
   if(entry.formType == Equipment.AMMO)
   {
      return String(entry.count);
   }
   if(Equipment.isSpell(entry))
   {
      return Equipment.spellCost(entry);
   }
   var amount = Math.round(Equipment.number(entry.formType == Equipment.ARMOR ? entry.infoArmor : entry.infoDamage));
   return amount > 0 ? String(amount) : "";
};

// A dark tile with a soft light edge and small corner marks. Gold while the slot filters the list,
// holds an open window, has its strip out, or takes what's dragged over it
Equipment.drawFrame = function(slot)
{
   var size = Equipment.cellSize(slot);
   var frame = slot.box.frame;
   var filled = slot.entries.length > 0 && !slot.taken;
   var filters = Equipment.activeSlot == slot || slot.plus && Equipment.activeSlot == slot.parent;
   var gold = filters || Equipment.anchorSlot == slot || (slot.takes && slot.hovered) || Equipment.pieceStrip.slot == slot;
   var edge = gold ? 100 : (slot.hovered ? 80 : (filled ? 45 : 22));
   var corner = Math.round(Equipment.CORNER * size / Equipment.SLOT_SIZE);
   var turn = new flash.geom.Matrix();
   turn.createGradientBox(size, size, Math.PI / 2, 0, 0);
   frame.clear();
   frame.lineStyle(gold ? 2 : 1, gold ? Equipment.GOLD : Equipment.WHITE, edge, true);
   frame.beginGradientFill("linear", [2894892, 0], [filled ? 62 : 45, filled ? 72 : 55], [0, 255], turn);
   Equipment.roundBox(frame, 0, 0, size, size, 5);
   frame.endFill();
   frame.lineStyle(1, slot.takes ? Equipment.GOLD : Equipment.PARCHMENT, slot.takes ? 90 : (filled ? 55 : 30), true);
   frame.moveTo(3, 3 + corner);
   frame.lineTo(3, 3);
   frame.lineTo(3 + corner, 3);
   frame.moveTo(size - 3 - corner, 3);
   frame.lineTo(size - 3, 3);
   frame.lineTo(size - 3, 3 + corner);
   frame.moveTo(size - 3, size - 3 - corner);
   frame.lineTo(size - 3, size - 3);
   frame.lineTo(size - 3 - corner, size - 3);
   frame.moveTo(3 + corner, size - 3);
   frame.lineTo(3, size - 3);
   frame.lineTo(3, size - 3 - corner);
   frame.lineStyle();
};

// The flat sign of a slot stands for an empty slot and for a hand taken by a weapon of the other hand
// A worn item shows the icon its row has in the list while 3D models are off or when its model couldn't be made
Equipment.showIcon = function(slot)
{
   var box = slot.box;
   var entry = slot.entries[0];
   var cell = Equipment.cellSize(slot);
   var modelled = entry != undefined && !slot.taken && Equipment.plugin() != undefined && Equipment.settings.slotModels;
   if(modelled && Equipment.modelled != undefined && Equipment.wearing != undefined)
   {
      modelled = Equipment.has(Equipment.modelled, Equipment.modelIndex(slot));
   }
   var size = Equipment.ICON_SIZE * cell / Equipment.SLOT_SIZE;
   var inset = (cell - size) / 2;
   box.icon.clear();
   Equipment.hideRowIcon(box);
   if(modelled || entry != undefined && !slot.taken && !slot.plus && Equipment.showRowIcon(box, entry, cell))
   {
      return undefined;
   }
   Equipment.drawIcon(box.icon, slot.plus ? "plus" : slot.icon, inset, inset, size, Equipment.PARCHMENT, entry != undefined && !slot.taken ? 85 : (slot.plus ? 45 : 20));
};

// The icon a row has in the list: from the set a mod gave that row (the iconSource, iconLabel and iconColor that I4 and
// mods like it put on rows) or from the list's own set, tinted the way the list tints it. It lives in a clip inside
// the slot's sign and is loaded again only when the row's set changes. Tells whether there's a set to show it from
Equipment.ROW_ICON = 0.62;
Equipment.showRowIcon = function(box, entry, cell)
{
   var source = entry.iconSource != undefined && entry.iconSource != "" ? entry.iconSource : Equipment.menu.inventoryLists.itemList.listState.iconSource;
   var holder = box.icon.row;
   var loader;
   if(source == undefined || source == "" || typeof MovieClipLoader != "function")
   {
      return false;
   }
   if(holder == undefined)
   {
      holder = box.icon.createEmptyMovieClip("row", 1);
      holder.hitTestDisable = true;
   }
   holder.entry = entry;
   holder.cell = cell;
   holder._visible = true;
   if(holder.source != source)
   {
      holder.source = source;
      holder.createEmptyMovieClip("art", 0);
      loader = new MovieClipLoader();
      loader.addListener({onLoadInit:function(art)
      {
         art.loaded = true;
         // a set that came in after the slot took another one is let be
         if(art._parent.art == art)
         {
            Equipment.fitRowIcon(art._parent);
         }
      }});
      loader.loadClip(source, holder.art);
   }
   else if(holder.art.loaded)
   {
      Equipment.fitRowIcon(holder);
   }
   return true;
};

Equipment.hideRowIcon = function(box)
{
   if(box.icon.row != undefined)
   {
      box.icon.row._visible = false;
   }
};

Equipment.fitRowIcon = function(holder)
{
   var art = holder.art;
   var entry = holder.entry;
   var room = holder.cell * Equipment.ROW_ICON;
   var bounds;
   var scale;
   if(art == undefined || entry == undefined)
   {
      return undefined;
   }
   art._xscale = art._yscale = 100;
   art._x = art._y = 0;
   art.gotoAndStop(entry.iconLabel != undefined ? entry.iconLabel : "default_misc");
   Equipment.tintRowIcon(art, entry.iconColor);
   bounds = art.getBounds(art);
   scale = room / Math.max(1, Math.max(bounds.xMax - bounds.xMin, bounds.yMax - bounds.yMin));
   art._xscale = art._yscale = scale * 100;
   art._x = (holder.cell - (bounds.xMax + bounds.xMin) * scale) / 2;
   art._y = (holder.cell - (bounds.yMax + bounds.yMin) * scale) / 2;
};

// as SkyUI tints the icon of a row: every clip inside it takes the colour, white when the row has none
Equipment.tintRowIcon = function(art, color)
{
   var part;
   var tint;
   var shape;
   for(var name in art)
   {
      part = art[name];
      if(part instanceof MovieClip)
      {
         tint = new flash.geom.ColorTransform();
         tint.rgb = color == undefined ? 16777215 : Number(color);
         shape = new flash.geom.Transform(part);
         shape.colorTransform = tint;
      }
   }
};

// for the test stand: what every slot shows, a model, the icon of its row (with the set, label and colour) or its sign
Equipment.slotIconsReport = function(note)
{
   var lines = [];
   var cells = Equipment.cells();
   var row;
   var i = 0;
   while(i < cells.length)
   {
      row = cells[i].box.icon.row;
      lines.push(cells[i].name + (cells[i].entries.length > 0 ? " " + cells[i].entries[0].text : "") + ": " + (row != undefined && row._visible ? "row icon " + row.source + " " + row.entry.iconLabel + " #" + Number(row.entry.iconColor).toString(16) + (row.art.loaded ? " loaded " + row.art._currentframe + " of " + row.art._totalframes + ", " + Math.round(row.art._width) + "x" + Math.round(row.art._height) : " loading") : (Equipment.has(Equipment.modelled, Equipment.modelIndex(cells[i])) ? "model" : "sign")));
      i++;
   }
   skse.Log("PANELTEST slot icons " + note + ": " + lines.join("; "));
};

// the number the plugin knows the model of a slot by
Equipment.modelIndex = function(slot)
{
   return slot.parent != undefined ? slot.modelIndex : Equipment.slotIndex(slot);
};

// what the one shown hides, the player's own or the follower's of the trade
Equipment.readHidden = function()
{
   var asked = Equipment.plugin() != undefined;
   var pieces = asked ? Equipment.plugin().HiddenPieces() : undefined;
   Equipment.hiddenMask = asked ? Number(Equipment.plugin().HiddenSlots()) : 0;
   Equipment.hiddenPieces = pieces != undefined ? pieces : [];
};

Equipment.slotHidden = function(slot)
{
   return (Equipment.hiddenMask & (1 << Equipment.slotIndex(Equipment.ownerOf(slot)))) != 0;
};

// a piece hidden alone from a strip
Equipment.pieceHidden = function(entry)
{
   return entry != undefined && Equipment.has(Equipment.hiddenPieces, entry.formId);
};

// whether the eye of a slot or of a small slot is shut: the slot is hidden or the piece it shows is
Equipment.looksHidden = function(slot)
{
   return Equipment.slotHidden(slot) || Equipment.pieceHidden(slot.entries[0]);
};

// weapons stay drawn: combat mods hit with their model
// hands stay as they are: combat mods hit and block with weapons and shields, and the game reports no shield
// while a shield isn't on the body. A follower a mod keeps as they are hides nothing
Equipment.canHide = function(slot)
{
   return Equipment.plugin() != undefined && Equipment.settings.hideIcons && slot.name != "right" && slot.name != "left" && !Equipment.lookOnly();
};

// A hidden slot can always be shown again, the hide buttons switched off or not. A small slot hides its own
// piece. Jewelry another mod draws has an eye once the plugin saw its copy hanging on the body, a slot that holds
// nothing else has none before that
Equipment.canToggle = function(slot)
{
   var hung = Equipment.hungPieces();
   if(Equipment.lookOnly() || slot.parent != undefined && slot.entries.length == 0)
   {
      return false;
   }
   // a piece the plugin can't hide shows no shut eye in a hidden slot either, it stays on the body
   if(slot.parent != undefined && !Equipment.hideable(slot.entries[0], hung))
   {
      return false;
   }
   if(Equipment.plugin() != undefined && Equipment.looksHidden(slot))
   {
      return true;
   }
   return Equipment.canHide(slot) && !Equipment.noneHideable(slot, hung);
};

// MARA and the like draw their jewelry as copies of its model the plugin can find and hide
Equipment.hungPieces = function()
{
   var hung = Equipment.plugin() != undefined ? Equipment.plugin().HungPieces() : undefined;
   return hung != undefined ? hung : [];
};

Equipment.hideable = function(entry, hung)
{
   return !Equipment.drawnElsewhere(entry) || Equipment.has(hung, entry.formId);
};

Equipment.noneHideable = function(slot, hung)
{
   var i = 0;
   while(i < slot.entries.length)
   {
      if(Equipment.hideable(slot.entries[i], hung))
      {
         return false;
      }
      i++;
   }
   return slot.entries.length > 0;
};

Equipment.drawEye = function(slot)
{
   var eye = slot.box.eye;
   var hidden = Equipment.looksHidden(slot);
   var size = eye.size;
   eye.sign.clear();
   eye._visible = Equipment.canToggle(slot) && (hidden || slot.hovered || eye.lit);
   if(!eye._visible || Equipment.eyesOver)
   {
      return undefined;
   }
   eye.sign.beginFill(0, eye.lit ? 85 : 72);
   Equipment.drawRing(eye.sign, size / 2, size / 2, size / 2);
   eye.sign.endFill();
   Equipment.drawIcon(eye.sign, hidden ? "eyeShut" : "eye", 2, 2, size - 4, hidden || eye.lit ? Equipment.GOLD : Equipment.PARCHMENT, eye.lit ? 100 : 80);
};

// Every frame: the eyes in sight go to the plugin, which draws them over the 3D of the menu, when they changed. An eye
// a window, a menu or the drawer lies over is left out, the movie of the eyes is drawn over everything of the menu.
// With no such movie the eyes draw themselves again
Equipment.sendEyes = function()
{
   var numbers = [];
   var cells = Equipment.cells();
   var screen = Stage.visibleRect;
   var shown = Equipment.panel != undefined && Equipment.panel._visible && !Equipment.inspecting && Equipment.listsAnswer();
   var i = 0;
   var eye;
   var corner;
   var foot;
   var top;
   var told;
   while(shown && i < cells.length)
   {
      eye = cells[i].box.eye;
      if(eye._visible)
      {
         corner = {x:0, y:0};
         foot = {x:0, y:eye.size};
         eye.localToGlobal(corner);
         eye.localToGlobal(foot);
         top = Mouse.getTopMostEntity(corner.x + eye.size / 2, corner.y + eye.size / 2, true);
         if(top != undefined && String(top).indexOf(String(cells[i].box)) == 0)
         {
            numbers.push((corner.x - screen.x) / screen.width, (corner.y - screen.y) / screen.height, (foot.y - corner.y) / screen.height,
               Equipment.looksHidden(cells[i]) ? 1 : 0, eye.lit ? 1 : 0, Equipment.looksHidden(cells[i]) || eye.lit ? Equipment.GOLD : Equipment.PARCHMENT);
         }
      }
      i++;
   }
   told = numbers.join(",");
   if(told == Equipment.eyesTold || !Equipment.eyesOver)
   {
      return undefined;
   }
   Equipment.eyesTold = told;
   if(Equipment.plugin().Eyes(numbers) != true)
   {
      Equipment.eyesOver = false;
      i = 0;
      while(i < cells.length)
      {
         Equipment.drawEye(cells[i]);
         i++;
      }
   }
};

Equipment.toggleHidden = function(slot)
{
   var owner = Equipment.ownerOf(slot);
   var cells;
   var i = 0;
   if(!Equipment.canToggle(slot) || !Equipment.listsAnswer())
   {
      return undefined;
   }
   if(slot.parent != undefined)
   {
      Equipment.togglePiece(slot);
   }
   else
   {
      Equipment.toggleSlot(slot);
   }
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
   cells = Equipment.cells();
   while(i < cells.length)
   {
      if(Equipment.ownerOf(cells[i]) == owner)
      {
         Equipment.drawEye(cells[i]);
      }
      i++;
   }
   // a hidden piece keeps its look on the body in the slot, the plugin dresses a copy of the body in it
   Equipment.showModels();
   // the game builds the body again a few frames later, the doll follows it
   Equipment.followPlayerAgain();
   Equipment.slotChanged();
};

// The eye of a slot speaks for the piece the slot shows. Open, it hides the whole slot. Shut, it shows the slot
// and every piece in it again
Equipment.toggleSlot = function(slot)
{
   var index = Equipment.slotIndex(slot);
   var i = 0;
   if(!Equipment.looksHidden(slot))
   {
      Equipment.hiddenMask = Number(Equipment.plugin().SetHidden(index, true));
      return undefined;
   }
   Equipment.hiddenMask = Number(Equipment.plugin().SetHidden(index, false));
   while(i < slot.entries.length)
   {
      Equipment.setPiece(slot.entries[i], false);
      i++;
   }
};

// A piece of a strip hides alone. With its whole slot hidden it's the one that shows: the slot shows again and
// the other pieces in it hide one by one
Equipment.togglePiece = function(mini)
{
   var slot = mini.parent;
   var entry = mini.entries[0];
   var i = 0;
   if(!Equipment.slotHidden(slot))
   {
      Equipment.setPiece(entry, !Equipment.pieceHidden(entry));
      return undefined;
   }
   Equipment.hiddenMask = Number(Equipment.plugin().SetHidden(Equipment.slotIndex(slot), false));
   while(i < slot.entries.length)
   {
      Equipment.setPiece(slot.entries[i], slot.entries[i] != entry);
      i++;
   }
};

Equipment.setPiece = function(entry, hidden)
{
   if(Equipment.pieceHidden(entry) != hidden && (!hidden || Equipment.hideable(entry, Equipment.hungPieces())))
   {
      Equipment.hiddenPieces = Equipment.plugin().SetHiddenPiece(entry.formId, hidden);
   }
};

Equipment.hover = function(slot, over)
{
   slot.hovered = over;
   Equipment.drawFrame(slot);
   Equipment.drawEye(slot);
   if(slot.parent != undefined)
   {
      Equipment.tellPiece(slot, over);
   }
   else
   {
      Equipment.drawTab(slot);
   }
   // the rollover comes after the move of the mouse, a pointer that lands right on the eye lights it here
   Equipment.lightEye();
};

// The lists switch themselves off while the card asks something, a search is typed or an item is inspected
// The slots do nothing for that time
Equipment.listsAnswer = function()
{
   var lists = Equipment.menu.inventoryLists;
   return Equipment.menu.bFadedIn && !lists.itemList.disableInput && !lists.itemList.disableSelection && !lists.categoryList.disableInput && !Equipment.inspecting;
};

// A press acts as a single click right away. A second press on the same slot within the double click
// time takes the item off, and on an empty slot it does nothing, so a quick double press can't turn
// the filter on and off again
Equipment.click = function(slot)
{
   var now = getTimer();
   var second = Equipment.clicked == slot && now - Equipment.clickedAt < Equipment.DOUBLE_CLICK;
   Equipment.clicked = second ? undefined : slot;
   Equipment.clickedAt = now;
   Equipment.closeContext();
   // the empty small slot at the end of a strip acts as an empty slot of its own slot
   if(slot.plus)
   {
      Equipment.closePopup();
      if(!second && Equipment.listsAnswer())
      {
         Equipment.filterSlot(slot.parent);
      }
      return undefined;
   }
   if(!second)
   {
      Equipment.press(slot);
   }
   else if(slot.entries.length > 0 && !slot.taken && Equipment.settings.doubleClickTakesOff)
   {
      Equipment.closePopup();
      Equipment.takeOff(slot);
   }
};

// a single click shows the item of a slot that holds one and looks for things to put into an empty slot
Equipment.press = function(slot)
{
   if(slot == undefined || !Equipment.listsAnswer())
   {
      return undefined;
   }
   if(slot.entries.length > 0 && !Equipment.isSpell(slot.entries[0]))
   {
      Equipment.showWorn(slot.taken ? Equipment.slotNamed("right") : slot, Equipment.settings.popup);
   }
   else
   {
      Equipment.closePopup();
      Equipment.filterSlot(slot);
   }
};

// the list narrows to what can go into the slot, a second click on the slot lets everything back
Equipment.filterSlot = function(slot)
{
   var lists = Equipment.menu.inventoryLists;
   var previous = Equipment.activeSlot;
   var category;
   Equipment.activeSlot = previous == slot ? undefined : slot;
   Equipment.slotChips = [];
   if(Equipment.activeSlot != undefined)
   {
      category = Equipment.categoryWith(slot.flag);
      if(category < 0)
      {
         category = Equipment.categoryWith(Equipment.FLAG_ALL);
      }
      if(category >= 0)
      {
         Equipment.keepSlot = true;
         lists.categoryList.onItemPress(category, 0);
         Equipment.keepSlot = false;
      }
   }
   Equipment.showChips();
   lists.itemList.selectedIndex = -1;
   lists.itemList.scrollPosition = 0;
   Equipment.refilter();
   Equipment.drawFrame(slot);
   if(previous != undefined && previous != slot)
   {
      Equipment.drawFrame(previous);
   }
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
};

// A click on a slot that holds an item leaves the category, the chips, the search and the filter of a slot as they
// are. The worn item's row gets selected when the list shows it and the selected row brings up its window. A row
// the list hides is selected past it, its window opens beside the slot whatever the setting of the item window
Equipment.showWorn = function(slot, withPopup)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var entry = slot.entries[0];
   var listed = Equipment.listShows(entry);
   var popup = withPopup || !listed;
   var previous = Equipment.anchorSlot;
   if(entry.outfit == true)
   {
      Equipment.showPiece(slot);
      return undefined;
   }
   if(!popup)
   {
      Equipment.closePopup();
   }
   Equipment.anchorSlot = popup ? slot : undefined;
   if(previous != undefined && previous != slot)
   {
      Equipment.drawFrame(previous);
   }
   // the row was chosen by the slot, the list must keep it while the mouse rests somewhere else
   list.isMouseDrivenNav = false;
   if(listed)
   {
      Equipment.shownApart = undefined;
      if(list.selectedIndex == entry.itemIndex)
      {
         Equipment.windowWanted = true;
      }
      list.selectedIndex = entry.itemIndex;
   }
   else
   {
      Equipment.selectApart(entry);
   }
   if(popup)
   {
      Equipment.openPopup(slot);
   }
};

// A piece of a follower's outfit has no row in their trade. The list lets go of its row first, the game would put
// that row's card and model back, then the plugin fills the card with the piece. Its model goes up last, once the
// window with the card stands where it shows: the game sizes a model as it puts it up. With no plugin to ask the
// slot's menu says what the piece is, as before
Equipment.showPiece = function(slot)
{
   var menu = Equipment.menu;
   var list = menu.inventoryLists.itemList;
   var plugin = Equipment.plugin();
   var info;
   Equipment.closePopup();
   if(list.selectedIndex >= 0)
   {
      list.selectedIndex = -1;
   }
   info = plugin != undefined ? plugin.OutfitCard(slot.entries[0].formId) : undefined;
   if(info == undefined)
   {
      Equipment.openContext(slot);
      return undefined;
   }
   Equipment.shownPiece = slot.entries[0];
   if(!menu._bItemCardFadedIn)
   {
      menu._bItemCardFadedIn = true;
      menu.itemCard.FadeInCard();
   }
   Equipment.openPopup(slot);
   menu.UpdateItemCardInfo(info);
   // SkyUI's container menu offers Take for the selected row with every new card, and there's none
   if(menu.updateBottomBar != undefined)
   {
      menu.updateBottomBar(false);
   }
   plugin.OutfitModel(slot.entries[0].formId);
};

// whether the list shows the row of an item now, its category, chips, search and the filter of a slot let it through
Equipment.listShows = function(entry)
{
   return entry != undefined && Equipment.menu.inventoryLists.itemList.getListEnumIndex(entry.itemIndex) != undefined;
};

// SkyUI selects only a row its filters let through. The game fills the card and puts up the model of the row the
// list's selectedIndex names, filters or not: SkyUI's own ItemcardDataExtender sets the index of every row in turn and
// asks the game for its card. So a row the list hides is selected past the filters, and the menu hears of it the way
// it hears of any newly selected row
Equipment.selectApart = function(entry)
{
   var lists = Equipment.menu.inventoryLists;
   var list = lists.itemList;
   var before = list.selectedEntry;
   if(list.disableSelection)
   {
      return undefined;
   }
   // a row the list hides has no row clip, an old number of one would draw it over another row
   entry.clipIndex = undefined;
   list._selectedIndex = entry.itemIndex;
   Equipment.shownApart = entry;
   if(before != undefined && before != entry && before.clipIndex != undefined)
   {
      list.getClipByIndex(before.clipIndex).setEntry(before, list.listState);
   }
   lists.dispatchEvent({type:"itemHighlightChange", index:entry.itemIndex});
};

// SkyUI lets go of a selected row its filters hide each time it makes the list anew. While the window of a slot shows
// such a row it's selected again, called as the list is drawn
Equipment.keepApart = function()
{
   var list = Equipment.menu.inventoryLists.itemList;
   var entry = Equipment.anchorSlot != undefined ? Equipment.anchorSlot.entries[0] : undefined;
   if(Equipment.shownApart == undefined || entry == undefined || list._selectedIndex == entry.itemIndex || Equipment.listShows(entry))
   {
      return undefined;
   }
   entry.clipIndex = undefined;
   list._selectedIndex = entry.itemIndex;
   Equipment.shownApart = entry;
};

// The card and the model of a row the list hides go with its window. When the list let go of the row itself
// meanwhile, the menu is told nothing is selected the way SkyUI tells it
Equipment.dropApart = function()
{
   var lists = Equipment.menu.inventoryLists;
   if(Equipment.menu.bFadedIn)
   {
      lists.dispatchEvent({type:"itemHighlightChange", index:-1});
   }
};

// "Find in list" of the slot menu: the worn item's row is selected in the list and its window comes up under it.
// A slot filter and chips give way always, the category and the search give way when they hide the item
Equipment.findWorn = function(slot)
{
   var lists = Equipment.menu.inventoryLists;
   var list = lists.itemList;
   var entry = slot.entries[0];
   var stale;
   Equipment.closePopup();
   stale = Equipment.dropFilter();
   if(!lists._typeFilter.isMatch(entry, lists.categoryList.selectedEntry.flag))
   {
      lists.categoryList.onItemPress(Equipment.categoryOf(entry, slot), 0);
      stale = true;
   }
   if(stale)
   {
      list.InvalidateData();
   }
   if(list.getListEnumIndex(entry.itemIndex) == undefined && Equipment.chosenChips.length > 0)
   {
      Equipment.chosenChips = [];
      Equipment.showChips();
      Equipment.keepChips();
      list.InvalidateData();
   }
   if(list.getListEnumIndex(entry.itemIndex) == undefined)
   {
      Equipment.dropSearch();
      list.InvalidateData();
   }
   list.isMouseDrivenNav = false;
   if(list.selectedIndex == entry.itemIndex)
   {
      Equipment.windowWanted = true;
   }
   list.selectedIndex = entry.itemIndex;
};

// the category of the slot when it lists the item, the one with everything otherwise
Equipment.categoryOf = function(entry, slot)
{
   var lists = Equipment.menu.inventoryLists;
   var own = Equipment.categoryWith(slot.flag, entry);
   if(own >= 0 && lists._typeFilter.isMatch(entry, lists.categoryList.entryList[own].flag))
   {
      return own;
   }
   return Equipment.categoryWith(Equipment.FLAG_ALL, entry);
};

// tells whether a slot filter was on
Equipment.dropFilter = function()
{
   var previous = Equipment.activeSlot;
   if(previous == undefined)
   {
      return false;
   }
   Equipment.activeSlot = undefined;
   Equipment.slotChips = [];
   Equipment.drawFrame(previous);
   Equipment.showChips();
   return true;
};

// the search box goes back to its label, which lets every name through again
Equipment.dropSearch = function()
{
   var lists = Equipment.menu.inventoryLists;
   lists.searchWidget.textField.SetText(skyui.components.SearchWidget.S_FILTER);
   lists._nameFilter.filterText = "";
};

// The game acts on the row the list calls selected, so the worn item is named for the length of the call
// A second press before the list has caught up would put the item back on, so it waits for the list,
// though never longer than a moment: a piece the game refuses to take off leaves the list as it was
Equipment.takeOff = function(slot)
{
   var entry = slot.entries[0];
   var list = Equipment.menu.inventoryLists.itemList;
   var busy = Equipment.busy && getTimer() - Equipment.busyAt < Equipment.BUSY_FOR;
   if(entry == undefined || busy || !Equipment.listsAnswer())
   {
      return undefined;
   }
   var selected = list._selectedIndex;
   var inLeftHand = entry.formType == Equipment.WEAPON && slot.name == "left" && !slot.taken || Equipment.virtualRing(entry);
   if(!Equipment.canTakeOff(slot))
   {
      return undefined;
   }
   Equipment.busy = true;
   Equipment.busyAt = getTimer();
   if(Equipment.dressesOther())
   {
      Equipment.busy = Equipment.dressOther(entry, entry.formType == Equipment.WEAPON && slot.name == "left" && !slot.taken, false);
      return undefined;
   }
   list._selectedIndex = entry.itemIndex;
   gfx.io.GameDelegate.call("ItemSelect", inLeftHand ? [Equipment.LEFT_HAND_SLOT] : []);
   list._selectedIndex = selected;
};

// A follower's own row goes on or off through the plugin, the game's select dresses the player only. A row of the
// player's side is given through the menu's own move first, the game checks what it checks for any gift, and goes on
// once the list has it on the follower's side
Equipment.dressOther = function(entry, left, on)
{
   var plugin = Equipment.plugin();
   var list;
   var selected;
   if(plugin == undefined || plugin.Wear == undefined || entry == undefined || Equipment.lookOnly())
   {
      return false;
   }
   // a piece of their outfit has no row, it only comes off
   if(entry.outfit == true)
   {
      return !on && plugin.TakeOffPiece != undefined && plugin.TakeOffPiece(entry.formId) == true;
   }
   if(Equipment.wearersRow(entry))
   {
      return plugin.Wear(entry.itemIndex, entry.formId, left, on) == true;
   }
   if(!on)
   {
      return false;
   }
   list = Equipment.menu.inventoryLists.itemList;
   // a gift the game refuses leaves the follower's own copies as they were, only more of it on their side goes on
   Equipment.wearGiven = {formId:entry.formId, text:entry.text, left:left, at:getTimer(), had:Equipment.wearersCount(list.entryList, entry.formId, entry.text)};
   Equipment.menu._equipHand = undefined;
   if(plugin.Gift != undefined)
   {
      plugin.Gift();
   }
   // the game moves the row the list calls selected, from the player's side whichever side the list shows
   selected = list._selectedIndex;
   list._selectedIndex = entry.itemIndex;
   gfx.io.GameDelegate.call("ItemTransfer", [1, false]);
   list._selectedIndex = selected;
   return true;
};

// how many of an item the follower's side holds, by its form and name
Equipment.wearersCount = function(entries, formId, text)
{
   var count = 0;
   var i = 0;
   while(i < entries.length)
   {
      if(entries[i].formId == formId && entries[i].text == text && Equipment.wearersRow(entries[i]))
      {
         count += Equipment.number(entries[i].count);
      }
      i++;
   }
   return count;
};

// An inventory a script opened over the trade took the panel's models and doll along, they're made again
Equipment.sceneAgain = function()
{
   Equipment.wearing = undefined;
   Equipment.showModels();
};

Equipment.wearGivenLater = function()
{
   var next = Equipment.wearNext;
   clearInterval(Equipment.wearTimer);
   Equipment.wearNext = undefined;
   if(next != undefined && Equipment.plugin() != undefined)
   {
      Equipment.busy = Equipment.plugin().Wear(next.index, next.formId, next.left, true) == true;
      Equipment.busyAt = getTimer();
   }
};

// The plugin fills a follower's rows anew after their gear changed, only while nothing holds a row: the slider asking
// how many, a drag, a press held back for a drag, the menu of a row. The game's moves act on the selected row by its
// place, a fill under them would hand them another row
Equipment.refillReady = function()
{
   return Equipment.listsAnswer() && !Equipment.dragging && Equipment.heldEquip == undefined && !Equipment.context._visible && !Equipment.cardAsking;
};

// The given row turns up on the follower's side as the list is made anew, it goes on then. A follower may have put it
// on by themselves already, it's told as the player's choice all the same
Equipment.wearGivenNow = function(entries)
{
   var given = Equipment.wearGiven;
   var i = 0;
   if(given == undefined)
   {
      return undefined;
   }
   if(getTimer() - given.at > Equipment.BUSY_FOR)
   {
      // a report of a gift that vanished without going on has this to go by
      skse.Log("EquipmentPanel gift " + given.text + " " + given.formId + " not put on: their side held " + Equipment.wearersCount(entries, given.formId, given.text) + " of it, " + given.had + " before, " + (getTimer() - given.at) + " ms after the move");
      Equipment.wearGiven = undefined;
      return undefined;
   }
   if(Equipment.wearersCount(entries, given.formId, given.text) <= given.had)
   {
      return undefined;
   }
   while(i < entries.length)
   {
      if(entries[i].formId == given.formId && entries[i].text == given.text && Equipment.wearersRow(entries[i]))
      {
         Equipment.wearGiven = undefined;
         // this runs while the game fills the rows, inside its own move or the plugin's task, an equip there would run
         // under them. It goes on a moment later
         Equipment.busy = true;
         Equipment.busyAt = getTimer();
         Equipment.wearNext = {index:entries[i].itemIndex, formId:entries[i].formId, left:given.left};
         clearInterval(Equipment.wearTimer);
         Equipment.wearTimer = setInterval(Equipment.wearGivenLater, 1);
         return undefined;
      }
      i++;
   }
};

// for the test stand: a gift on its way to the follower's body, how many of it their side holds now
Equipment.wearReport = function(note)
{
   var given = Equipment.wearGiven;
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   skse.Log("PANELTEST wear " + note + ": " + (given == undefined ? "no gift waits" : "waits " + given.text + " " + given.formId + ", had " + given.had + ", holds now " + Equipment.wearersCount(entries, given.formId, given.text) + ", " + (getTimer() - given.at) + " ms") + ", next " + (Equipment.wearNext != undefined ? Equipment.wearNext.index : "-") + ", busy " + Equipment.busy);
};

// My Precious...es takes a ring off the left hand's own finger when it's put on with the left hand again. On any
// other of its fingers a select of the menu only moves the ring, it comes off in the finger menu of that mod
Equipment.canTakeOff = function(slot)
{
   var entry = slot.entries[0];
   return entry != undefined && !Equipment.isSpell(entry) && !Equipment.lookOnly() && !(Equipment.virtualRing(entry) && entry.equipState != Equipment.LEFT_HAND);
};

// a mod may ask that its follower be looked at only, a quest's disguise, a set of gear being switched. Asked each
// time, the mod may ask while the trade is up
Equipment.lookOnly = function()
{
   var plugin = Equipment.plugin();
   return Equipment.dressesOther() && plugin.ReadOnly != undefined && plugin.ReadOnly() == true;
};

// The plugin names worn pieces a new one pushed out of their slot while they were hidden. They go off the way
// a click takes a piece off, so the menu keeps its own list right. The select does nothing in the same moment
// the new piece went on, so it waits a little. One not in the list now goes off when the menu closes
Equipment.pushOff = function(forms)
{
   Equipment.pushing = forms;
   clearInterval(Equipment.pushTimer);
   Equipment.pushTimer = setInterval(Equipment.pushOffNow, Equipment.PUSH_AFTER);
};

Equipment.pushOffNow = function()
{
   var forms = Equipment.pushing;
   var list = Equipment.menu.inventoryLists.itemList;
   var entries = list.entryList;
   var selected = list._selectedIndex;
   var f = 0;
   var i;
   clearInterval(Equipment.pushTimer);
   Equipment.pushing = undefined;
   while(f < forms.length)
   {
      i = 0;
      while(i < entries.length && !(entries[i].formId == forms[f] && entries[i].equipState > 0))
      {
         i++;
      }
      if(i < entries.length)
      {
         list._selectedIndex = entries[i].itemIndex;
         gfx.io.GameDelegate.call("ItemSelect", []);
      }
      f++;
   }
   list._selectedIndex = selected;
};

// whether a slot already holds this item where a drop would put it, the game's select would take it off then
Equipment.alreadyThere = function(entry, slot)
{
   if(entry.formType != Equipment.WEAPON)
   {
      return entry.equipState > 0;
   }
   if(slot.name == "left")
   {
      return entry.equipState == Equipment.LEFT_HAND || entry.equipState == Equipment.BOTH_HANDS;
   }
   return entry.equipState > 0 && entry.equipState != Equipment.LEFT_HAND;
};

// puts an item of the list into a slot, the left hand gets a weapon by its own call
Equipment.putOn = function(entry, slot)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var selected = list._selectedIndex;
   var busy = Equipment.busy && getTimer() - Equipment.busyAt < Equipment.BUSY_FOR;
   // a row the player wears is given to a follower all the same, only one already on the one dressed is there
   var there = (!Equipment.dressesOther() || Equipment.wearersRow(entry)) && Equipment.alreadyThere(entry, slot);
   if(entry == undefined || busy || !Equipment.fits(entry, slot) || there)
   {
      return false;
   }
   Equipment.busy = true;
   Equipment.busyAt = getTimer();
   if(Equipment.dressesOther())
   {
      Equipment.busy = Equipment.dressOther(entry, slot.name == "left" && entry.formType == Equipment.WEAPON, true);
      return Equipment.busy;
   }
   if(Equipment.swapTwin(entry))
   {
      return true;
   }
   list._selectedIndex = entry.itemIndex;
   gfx.io.GameDelegate.call("ItemSelect", slot.name == "left" && entry.formType == Equipment.WEAPON ? [Equipment.LEFT_HAND_SLOT] : []);
   list._selectedIndex = selected;
   return true;
};

// the worn copy of the same armor as this row, when its slot is hidden
Equipment.wornTwin = function(entry)
{
   var slot = Equipment.slotOf(entry);
   var entries = Equipment.menu.inventoryLists.itemList.entryList;
   var i = 0;
   if(Equipment.kind != "inventory" || entry.formType == Equipment.WEAPON || entry.equipState > 0 || slot == undefined || !Equipment.slotHidden(slot) && !Equipment.pieceHidden(entry))
   {
      return undefined;
   }
   while(i < entries.length)
   {
      if(entries[i] != entry && entries[i].formId == entry.formId && entries[i].equipState > 0)
      {
         return entries[i];
      }
      i++;
   }
   return undefined;
};

// The game won't put on a second copy of an armor it already finds worn in a hidden slot, say a tempered twin
// of the hidden helmet. It does nothing then. So the worn twin is taken off first and the row goes on a moment
// later, the select does nothing in the same moment. True when the row went this way
Equipment.swapTwin = function(entry)
{
   var twin = Equipment.wornTwin(entry);
   var list = Equipment.menu.inventoryLists.itemList;
   var selected = list._selectedIndex;
   if(twin == undefined)
   {
      return false;
   }
   Equipment.twinWanted = Equipment.rowKey(entry);
   list._selectedIndex = twin.itemIndex;
   gfx.io.GameDelegate.call("ItemSelect", []);
   list._selectedIndex = selected;
   clearInterval(Equipment.twinTimer);
   Equipment.twinTimer = setInterval(Equipment.twinOn, Equipment.PUSH_AFTER);
   return true;
};

Equipment.twinOn = function()
{
   var wanted = Equipment.twinWanted;
   var list = Equipment.menu.inventoryLists.itemList;
   var entries = list.entryList;
   var selected = list._selectedIndex;
   var i = 0;
   clearInterval(Equipment.twinTimer);
   Equipment.twinWanted = undefined;
   while(i < entries.length && !(Equipment.isRow(entries[i], wanted) && entries[i].equipState == 0))
   {
      i++;
   }
   if(i < entries.length)
   {
      list._selectedIndex = entries[i].itemIndex;
      gfx.io.GameDelegate.call("ItemSelect", []);
      list._selectedIndex = selected;
   }
};

Equipment.categoryShown = function()
{
   if(!Equipment.keepSlot)
   {
      Equipment.dropFilter();
   }
};

Equipment.slotFilter = {};
Equipment.slotFilter.applyFilter = function(entries)
{
   var slot = Equipment.activeSlot;
   if(slot == undefined)
   {
      return undefined;
   }
   var i = 0;
   while(i < entries.length)
   {
      if(Equipment.fits(entries[i], slot))
      {
         i++;
      }
      else
      {
         entries.splice(i, 1);
      }
   }
};

// The doll stands on a stage between the two columns. A drag across it turns the doll sideways
Equipment.buildStage = function()
{
   var stage = Equipment.panel.createEmptyMovieClip("stage", 0);
   stage.createTextField("touch", 0, 0, 0, 10, 10);
   stage.touch.selectable = false;
   // with a slot window open a press on the doll closes the window, as a press anywhere outside it does
   stage.onPress = function()
   {
      Equipment.closeContext();
      if(Equipment.anchorSlot != undefined)
      {
         Equipment.closePopup();
         return undefined;
      }
      // the scroll bar of the character drawer lies over the doll and takes this press itself
      if(Equipment.onHeroBar(_root._xmouse, _root._ymouse))
      {
         return undefined;
      }
      Equipment.turning = true;
      Equipment.turnedFrom = _root._xmouse;
   };
   stage.onRelease = stage.onReleaseOutside = function()
   {
      Equipment.turning = false;
   };
   stage.onPressAux = function()
   {
   };
   stage.onMouseMove = function()
   {
      if(Equipment.turning && Equipment.plugin() != undefined)
      {
         Equipment.plugin().TurnPlayer((_root._xmouse - Equipment.turnedFrom) * Equipment.squash * Equipment.settings.turnSpeed);
         Equipment.turnedFrom = _root._xmouse;
      }
   };
   Equipment.stage = stage;
};

Equipment.placeStage = function()
{
   var frame = Equipment.frame;
   var stage = Equipment.stage;
   stage._x = frame.left + Equipment.SLOT_SIZE + Equipment.PLAYER_GAP;
   stage._y = Equipment.dollTop();
   stage.touch._width = frame.right - frame.left - 2 * (Equipment.SLOT_SIZE + Equipment.PLAYER_GAP);
   stage.touch._height = frame.bottom + Equipment.LABEL_HEIGHT - stage._y;
};

// for anything that has to know when what the slots show has changed
Equipment.slotChanged = function()
{
};

Equipment.started = function(config)
{
   // the list takes the width the player gave it before anything is laid out by its edge
   Equipment.narrowList();
   // first, so the windows and menus of the panel lie over the item browser
   Equipment.buildBrowserRoom();
   Equipment.buildBackdrop();
   Equipment.readCompanions();
   Equipment.build();
   Equipment.layout();
   Equipment.buildWindow();
   Equipment.buildPopup();
   // the strip hears a press before the menus that a press closes
   Equipment.buildPieces();
   Equipment.buildContext();
   Equipment.buildDrag();
   Equipment.buildInspect();
   Equipment.buildHero();
   Equipment.buildCompanions();
   Equipment.buildLook();
   Equipment.showChips();
   // the gamepad wraps what the parts above set up, so it comes last
   Equipment.buildPad();
};

// For the test stand: where a place of a slot lies on screen and what the mouse would land on there
// The place is told as "name,right,down", the two numbers in shares of the slot from its middle
Equipment.aimAt = function(told)
{
   var parts = String(told).split(",");
   var slot = Equipment.slotNamed(parts[0]);
   var size = Equipment.SLOT_SIZE;
   var x = slot.box._x + size * (0.5 + Number(parts[1]));
   var y = slot.box._y + size * (0.5 + Number(parts[2]));
   var share = Equipment.share(x, y);
   var point = Equipment.toStage({x:x, y:y});
   Equipment.aimedX = share.x;
   Equipment.aimedY = share.y;
   Equipment.aimedTop = String(Mouse.getTopMostEntity(point.x, point.y, true));
};

// For the test stand: words typed into the search of the list, as the search box filters by them
Equipment.searchList = function(words)
{
   var lists = Equipment.menu.inventoryLists;
   lists.searchWidget.textField.SetText(String(words));
   lists._nameFilter.filterText = String(words);
   lists.itemList.InvalidateData();
};

// For the test stand: the slot is pressed the way a click presses it, so a second call soon after is a double click
Equipment.clickSlot = function(name)
{
   Equipment.click(Equipment.slotNamed(name));
};

// For the test stand: the eye of a slot pressed the way a click on it presses it
Equipment.clickEye = function(name)
{
   Equipment.toggleHidden(Equipment.slotNamed(name));
};

// For the test stand: which slots and small slots have an eye, which are hidden, and the body slot line of the window
Equipment.eyeReport = function(note)
{
   var cells = Equipment.cells();
   var lines = [];
   var i = 0;
   var cell;
   while(i < cells.length)
   {
      cell = cells[i];
      if(cell.entries.length > 0)
      {
         lines.push((cell.parent != undefined ? cell.name + "." + cell.place : cell.name) + "=" + cell.entries[0].text + (Equipment.canToggle(cell) ? " eye" : " no eye") + (Equipment.looksHidden(cell) ? " hidden" : ""));
      }
      i++;
   }
   skse.Log("PANELTEST eyes " + note + ": " + lines.join(", ") + "; hung " + Equipment.hungPieces().join("/") + "; window " + (Equipment.window._visible ? Equipment.menu.inventoryLists.itemList.selectedEntry.text + " [" + Equipment.window.bodySlots.text + "]" : "-"));
};

// For the test stand: the worn pieces and the copies of them other mods hang on the body, into the plugin's log
Equipment.dumpHanging = function()
{
   Equipment.plugin().DumpHanging();
};

// for the test stand: every shape of the body the panel shows, drawn or not
Equipment.dumpBody = function(note)
{
   Equipment.plugin().DumpBody(String(note));
};

// For the test stand: the item with this form id is pressed in the list the way a click on its row presses it
Equipment.pressItem = function(formId)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var entries = list.entryList;
   var selected = list._selectedIndex;
   var i = 0;
   while(i < entries.length && entries[i].formId != Number(formId))
   {
      i++;
   }
   Equipment.pressed = i < entries.length;
   if(Equipment.pressed)
   {
      list._selectedIndex = entries[i].itemIndex;
      gfx.io.GameDelegate.call("ItemSelect", []);
      list._selectedIndex = selected;
   }
};

// For the test stand: the same press as for the left hand. My Precious...es puts a ring equipped this way on a
// finger of the left hand the game has no slot for
Equipment.pressItemLeft = function(formId)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var entries = list.entryList;
   var selected = list._selectedIndex;
   var i = 0;
   while(i < entries.length && entries[i].formId != Number(formId))
   {
      i++;
   }
   Equipment.pressed = i < entries.length;
   if(Equipment.pressed)
   {
      list._selectedIndex = entries[i].itemIndex;
      gfx.io.GameDelegate.call("ItemSelect", [Equipment.LEFT_HAND_SLOT]);
      list._selectedIndex = selected;
   }
};

// For the test stand: every piece a slot holds, outfit pieces marked
Equipment.slotReport = function(name)
{
   // a script of the game hands a word over in the letter case it met first, Left for left
   var slot = Equipment.slotNamed(String(name).toLowerCase());
   var pieces = [];
   var i = 0;
   while(i < slot.entries.length)
   {
      pieces.push(slot.entries[i].text + " worn " + slot.entries[i].equipState + (slot.entries[i].outfit == true ? " outfit" : ""));
      i++;
   }
   skse.Log("PANELTEST slot " + name + ": " + pieces.join(", "));
};

// For the test stand: the state of the panel in one line of the log
Equipment.panelReport = function(note)
{
   var lines = [];
   var i = 0;
   var slot;
   var list = Equipment.menu.inventoryLists.itemList;
   while(i < Equipment.SLOTS.length)
   {
      slot = Equipment.SLOTS[i];
      lines.push(slot.name + "=" + (slot.entries.length > 0 ? slot.entries[0].text : "-") + (slot.entries.length > 1 ? "(+" + (slot.entries.length - 1) + ")" : "") + (slot.taken ? "(taken)" : "") + (Equipment.slotHidden(slot) ? "(hidden)" : ""));
      i++;
   }
   skse.Log("EquipmentPanel state " + note + ": " + lines.join(", ") + "; strip " + Equipment.piecesReport() + ", hidden pieces " + Equipment.hiddenPieces.join("/") + "; filter " + (Equipment.activeSlot == undefined ? "-" : Equipment.activeSlot.name) + ", chips " + Equipment.chosenChips.join("/") + ", rows " + list.listEnumeration.size() + ", selected " + list.selectedEntry.text + ", popup " + (Equipment.anchorSlot == undefined ? "-" : Equipment.anchorSlot.name) + ", window " + Equipment.window._visible + ", context " + Equipment.context._visible + ", inspect " + Equipment.inspecting + ", hero " + Equipment.heroReport() + ", hidden " + Equipment.hiddenMask + ", category " + Equipment.menu.inventoryLists.categoryList.selectedEntry.text + ", " + Equipment.edgeReport());
};

// For the test stand: where the list ends by its background and where its rows end, in the panel's units
Equipment.edgeReport = function()
{
   var rows = Equipment.menu.inventoryLists.itemList.getBounds(Equipment.menu);
   var window = Equipment.window;
   return "list edge " + Math.round(Equipment.listEdge().right) + ", rows to " + Math.round(rows.xMax * Equipment.squash) + ", over rows to " + Math.round(Equipment.rowsEdge().right) + (window != undefined ? ", window " + Math.round(window.left) + ".." + Math.round(window.left + window.wide * Equipment.squash) : "") + ", squash " + Math.round(Equipment.squash * 100) / 100 + ", paper " + Equipment.paper;
};

// For the test stand: the middle of the stage the doll stands on, moved by shares of its width, as aimedX, aimedY
Equipment.aimStage = function(right)
{
   var stage = Equipment.stage;
   var x = stage._x + stage.touch._width * (0.5 + Number(right));
   var y = stage._y + stage.touch._height * 0.5;
   var point = {x:x, y:y};
   Equipment.panel.localToGlobal(point);
   Equipment.aimedTop = String(Mouse.getTopMostEntity(point.x, point.y, true));
   var share = Equipment.screenShare(point.x, point.y);
   Equipment.aimedX = share.x;
   Equipment.aimedY = share.y;
};

// For the test stand: the middle of the row of an item if the list shows it now, as aimedX, aimedY
Equipment.aimRow = function(formId)
{
   Equipment.aimEntry(function(entry)
   {
      return entry.formId == Number(formId);
   });
};

// the row of the stolen copies of an item or the row of the rest
Equipment.aimStolenRow = function(formId)
{
   Equipment.aimEntry(function(entry)
   {
      return entry.formId == Number(formId) && Equipment.stolenRow(entry);
   });
};

Equipment.aimOwnRow = function(formId)
{
   Equipment.aimEntry(function(entry)
   {
      return entry.formId == Number(formId) && !Equipment.stolenRow(entry);
   });
};

// the same by the text of the row, for twins that share a form id
Equipment.aimRowNamed = function(text)
{
   Equipment.aimEntry(function(entry)
   {
      return entry.text == String(text);
   });
};

// the row at the top of the list as it's drawn now
Equipment.aimTopRow = function()
{
   var clip = Equipment.menu.inventoryLists.itemList.getClipByIndex(0);
   Equipment.aimEntry(function(entry)
   {
      return clip != undefined && entry.itemIndex == clip.itemIndex;
   });
};

// For the test stand: where the open menu of a row lies against its row and the window's model, and whether its copy
// is drawn over the 3D
Equipment.contextPlaceReport = function(note)
{
   var context = Equipment.context;
   var box = Equipment.window._visible ? Equipment.modelBox() : undefined;
   var row = Equipment.contextEntry != undefined && Equipment.contextEntry.clipIndex != undefined ? Equipment.menu.inventoryLists.itemList.getClipByIndex(Equipment.contextEntry.clipIndex) : undefined;
   var at = {x:0, y:0};
   if(row != undefined)
   {
      row.localToGlobal(at);
      Equipment.toPanel(at);
   }
   skse.Log("PANELTEST menu place " + note + ": shown " + context._visible + " at " + Math.round(context._x) + "," + Math.round(context._y) + " " + Math.round(context.wide) + "x" + Math.round(context.high) + ", row top " + (row != undefined ? Math.round(at.y) : "none") + ", window model " + (box != undefined ? Math.round(box.top) + " to " + Math.round(box.bottom) : "none") + ", copy over the 3D " + Equipment.contextOverlaid + ", model hidden " + Equipment.contextHidesModel);
};

// For the test stand: the open menu moved onto the middle of the window's model, its copy over the 3D goes along
Equipment.contextOntoModel = function()
{
   var box = Equipment.modelBox();
   var context = Equipment.context;
   if(!context._visible || !Equipment.window._visible)
   {
      return undefined;
   }
   context._x = (box.left + box.right - context.wide) / 2;
   context._y = (box.top + box.bottom - context.high) / 2;
   if(Equipment.contextOverlaid)
   {
      Equipment.overlayContext();
   }
};

Equipment.aimEntry = function(matches)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var entries = list.entryList;
   var i = 0;
   var row;
   var point;
   Equipment.aimedX = Equipment.aimedY = -1;
   while(i < entries.length && !matches(entries[i]))
   {
      i++;
   }
   row = i < entries.length && entries[i].clipIndex != undefined ? list.getClipByIndex(entries[i].clipIndex) : undefined;
   if(row == undefined)
   {
      return undefined;
   }
   point = {x:row._x + row._width * 0.3, y:row._y + list.layout.entryHeight / 2};
   row._parent.localToGlobal(point);
   var share = Equipment.screenShare(point.x, point.y);
   Equipment.aimedX = share.x;
   Equipment.aimedY = share.y;
};

// For the test stand: the middle of a clip of the menu named by its path from the menu, as aimedX, aimedY. The panel's
// own clips are found in its layers by the same path
Equipment.aimClip = function(path)
{
   var parts = String(path).split(".");
   var clip = Equipment.menu[parts[0]];
   var i = 1;
   var bounds;
   var share;
   Equipment.aimedX = Equipment.aimedY = -1;
   if(clip == undefined)
   {
      clip = Equipment.over[parts[0]] != undefined ? Equipment.over[parts[0]] : (Equipment.windowLayer[parts[0]] != undefined ? Equipment.windowLayer[parts[0]] : Equipment.under[parts[0]]);
   }
   while(clip != undefined && i < parts.length)
   {
      clip = clip[parts[i]];
      i++;
   }
   if(clip == undefined)
   {
      return undefined;
   }
   bounds = clip.getBounds(_root);
   share = Equipment.screenShare((bounds.xMin + bounds.xMax) / 2, (bounds.yMin + bounds.yMax) / 2);
   Equipment.aimedX = share.x;
   Equipment.aimedY = share.y;
};

// For the test stand: the row of the item with this form id is selected the way the pointer over it selects it,
// the stand moves no pointer Scaleform sees
Equipment.selectRow = function(formId)
{
   var list = Equipment.menu.inventoryLists.itemList;
   var entries = list.entryList;
   var i = 0;
   while(i < entries.length && entries[i].formId != Number(formId))
   {
      i++;
   }
   if(i < entries.length)
   {
      list.selectedIndex = i;
   }
};
