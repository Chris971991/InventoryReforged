Equipment.LENS_SIZE = 22;
Equipment.INSPECT_SIZE = 0.62;
Equipment.INSPECT_MIDDLE_Y = 0.47;
Equipment.INSPECT_SHADE = 88;
Equipment.INSPECT_TURN = 0.45;
Equipment.INSPECT_ZOOM = 1.15;
Equipment.INSPECT_TITLE = 26;
Equipment.INSPECT_HINT = Equipment.say("Drag to turn") + "     " + Equipment.say("Wheel to zoom") + "     " + Equipment.say("Right click, Esc or Tab to close");
Equipment.CLOSE_SIZE = 34;

// Inspecting shows one item big in the middle of a dark screen. A drag turns it any way, the wheel brings
// it closer, a right click, Esc, Tab, the zoom key or the cross close the view. A worn piece is shown
// as it sits on the body, anything else as its own model. The plugin draws it and hides the panel's models
Equipment.buildInspect = function()
{
   Equipment.inspecting = false;
   Equipment.buildLens();
   Equipment.buildViewer();

   var menu = Equipment.menu;
   menu.equipmentHandleInput = menu.handleInput;
   menu.handleInput = function(details, pathToFocus)
   {
      if(Equipment.inspecting)
      {
         if(Shared.GlobalFunc.IsKeyPressed(details) && (details.navEquivalent == gfx.ui.NavigationCode.TAB || details.navEquivalent == gfx.ui.NavigationCode.SHIFT_TAB || details.navEquivalent == gfx.ui.NavigationCode.ESCAPE))
         {
            Equipment.leaveInspect();
         }
         return true;
      }
      if(Equipment.chargeKey(details) || Equipment.chipKeys(details))
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
      if(Equipment.anchorSlot != undefined && Shared.GlobalFunc.IsKeyPressed(details) && details.navEquivalent == gfx.ui.NavigationCode.ESCAPE)
      {
         Equipment.closePopup();
         return true;
      }
      if(Equipment.pieceStrip != undefined && Shared.GlobalFunc.IsKeyPressed(details) && details.navEquivalent == gfx.ui.NavigationCode.ESCAPE)
      {
         Equipment.closePieces();
         return true;
      }
      return this.equipmentHandleInput(details, pathToFocus);
   };

   Equipment.wheel = {};
   Equipment.wheel.onMouseWheel = function(delta)
   {
      if(Equipment.inspecting && Equipment.plugin() != undefined)
      {
         Equipment.plugin().InspectZoom(delta > 0 ? Equipment.INSPECT_ZOOM : 1 / Equipment.INSPECT_ZOOM);
      }
   };
   Mouse.addListener(Equipment.wheel);
};

// anything with a model can be looked at, a quest item as much as a sword
Equipment.inspectable = function(entry)
{
   return entry != undefined && !Equipment.isSpell(entry) && Equipment.plugin() != undefined;
};

// a worn piece is looked at as it sits on the body when its slot shows it that way
Equipment.inspectEntry = function(entry, fromSlot)
{
   var slot = Equipment.slotOf(entry);
   var worn = entry.equipState > 0 && slot != undefined && Equipment.has(Equipment.WORN_LOOK, slot.name) && Equipment.settings.wornLook;
   if(!Equipment.inspectable(entry) || Equipment.inspecting)
   {
      return false;
   }
   Equipment.closePopup();
   Equipment.closeContext();
   if(Equipment.plugin().Inspect(entry.formId, worn, 0.5, Equipment.INSPECT_MIDDLE_Y, Equipment.INSPECT_SIZE) != true)
   {
      return false;
   }
   Equipment.inspecting = true;
   // SkyUI's lists scroll by the wheel wherever the pointer is over them, the dark view over them included,
   // and the wheel that brought the item closer scrolled the list behind with its sound
   Equipment.holdRows(true);
   Equipment.viewer.title.text = Equipment.gameName(entry.text);
   Equipment.viewer.title._x = (Equipment.screenRect().width - Equipment.viewer.title._width) / 2 + Equipment.screenRect().x;
   Equipment.viewer._visible = true;
   Equipment.showLens(false);
   gfx.io.GameDelegate.call("PlaySound", ["UIMenuFocus"]);
   return true;
};

Equipment.leaveInspect = function()
{
   if(!Equipment.inspecting)
   {
      return undefined;
   }
   Equipment.inspecting = false;
   Equipment.inspectTurning = false;
   Equipment.holdRows(false);
   Equipment.viewer._visible = false;
   Equipment.plugin().InspectEnd();
   Equipment.showWindow();
};

// The game's item zoom key comes here first. True when the panel took it
// On a gamepad the click of the stick is the panel's, the plugin tells its press and hold apart (39-gamepad.as).
// The game's own zoom faded the menu and left the doll and the slot models over the world
Equipment.zoomKey = function()
{
   if(Equipment.menu._platform != 0)
   {
      return Equipment.padReady();
   }
   if(Equipment.inspecting)
   {
      Equipment.leaveInspect();
      return true;
   }
   if(Equipment.browserInspectKey())
   {
      return true;
   }
   if(!Equipment.settings.inspectOnZoomKey || !Equipment.listsAnswer())
   {
      return false;
   }
   var entry = Equipment.anchorSlot != undefined ? Equipment.anchorSlot.entries[0] : Equipment.menu.inventoryLists.itemList.selectedEntry;
   return Equipment.inspectEntry(entry, Equipment.anchorSlot != undefined);
};

// the cross of the inspect view lights up under the pointer, called on every move of the mouse
Equipment.lightClose = function()
{
   var close = Equipment.viewer.close;
   var lit = Equipment.inspecting && close.hitTest(_root._xmouse, _root._ymouse, true);
   if(close != undefined && lit != close.lit)
   {
      close.lit = lit;
      Equipment.drawClose(close, lit);
   }
};

// the plugin tells when the game's own zoom of the selected item goes in or out, the item can be turned by mouse while it's in
Equipment.gameZoom = function(zooming)
{
   Equipment.menu.mouseRotationRect._xscale = Equipment.menu.mouseRotationRect._yscale = zooming ? 100 : 0;
};

Equipment.buildViewer = function()
{
   var screen = Equipment.screenRect();
   var safe = Equipment.safeInset();
   var viewer = Equipment.over.createEmptyMovieClip("equipmentViewer", Equipment.over.getNextHighestDepth());
   var hint;
   var close;
   viewer._visible = false;
   viewer.createTextField("touch", 0, screen.x, screen.y, screen.width, screen.height);
   viewer.touch.selectable = false;
   viewer.beginFill(0, Equipment.INSPECT_SHADE);
   viewer.moveTo(screen.x, screen.y);
   viewer.lineTo(screen.x + screen.width, screen.y);
   viewer.lineTo(screen.x + screen.width, screen.y + screen.height);
   viewer.lineTo(screen.x, screen.y + screen.height);
   viewer.lineTo(screen.x, screen.y);
   viewer.endFill();

   Equipment.writeLine(viewer, "title", 1, "", Equipment.INSPECT_TITLE, Equipment.WHITE);
   viewer.title._y = screen.y + safe.y + 24;
   hint = Equipment.writeLine(viewer, "hint", 2, Equipment.INSPECT_HINT, 15, Equipment.PARCHMENT);
   hint._x = screen.x + (screen.width - hint._width) / 2;
   hint._y = screen.y + screen.height - safe.y - 54;
   hint._alpha = 80;

   close = viewer.createEmptyMovieClip("close", 3);
   close.createTextField("touch", 0, 0, 0, Equipment.CLOSE_SIZE, Equipment.CLOSE_SIZE);
   close.touch.selectable = false;
   close._x = screen.x + screen.width - safe.x - Equipment.CLOSE_SIZE - 24;
   close._y = screen.y + safe.y + 20;
   Equipment.drawClose(close, false);
   // the viewer takes the mouse of the cross, its press is the viewer's and its light comes from lightClose

   // the viewer takes the presses of the cross inside it, so it looks whether a press landed on the cross
   viewer.onPress = function()
   {
      if(this.close.hitTest(_root._xmouse, _root._ymouse, true))
      {
         Equipment.leaveInspect();
         return undefined;
      }
      Equipment.inspectTurning = true;
      Equipment.inspectFrom = {x:_root._xmouse, y:_root._ymouse};
   };
   viewer.onRelease = viewer.onReleaseOutside = function()
   {
      Equipment.inspectTurning = false;
   };
   viewer.onPressAux = function()
   {
      Equipment.leaveInspect();
   };
   viewer.onMouseMove = function()
   {
      var from = Equipment.inspectFrom;
      if(!Equipment.inspectTurning || Equipment.plugin() == undefined)
      {
         return undefined;
      }
      Equipment.plugin().InspectTurn((_root._xmouse - from.x) * Equipment.squash * Equipment.INSPECT_TURN, (_root._ymouse - from.y) * Equipment.INSPECT_TURN);
      Equipment.inspectFrom = {x:_root._xmouse, y:_root._ymouse};
   };
   Equipment.viewer = viewer;
};

Equipment.drawClose = function(close, lit)
{
   var size = Equipment.CLOSE_SIZE;
   close.clear();
   close.lineStyle(1, lit ? Equipment.GOLD : Equipment.WHITE, lit ? 90 : 40, true);
   close.beginFill(0, 60);
   Equipment.drawRing(close, size / 2, size / 2, size / 2);
   close.endFill();
   close.lineStyle();
   Equipment.drawIcon(close, "cross", size * 0.28, size * 0.28, size * 0.44, lit ? Equipment.GOLD : Equipment.PARCHMENT, 100);
};

// The lens sits in the corner of the item window that is next to the right end of the selected row
// The rows under the window take the mouse, and a pointer that crosses one of them moves the window away
// So the lens touches the selected row, the pointer steps onto it straight from the row,
// or comes to it from the bar of the list on the right
Equipment.buildLens = function()
{
   var lens = Equipment.over.createEmptyMovieClip("equipmentLens", Equipment.over.getNextHighestDepth());
   lens.createTextField("touch", 0, 0, 0, Equipment.LENS_SIZE, Equipment.LENS_SIZE);
   lens.touch.selectable = false;
   lens.createEmptyMovieClip("look", 1);
   lens._visible = false;
   lens.onRollOver = function()
   {
      Equipment.drawLens(true);
   };
   lens.onRollOut = lens.onReleaseOutside = function()
   {
      Equipment.drawLens(false);
   };
   lens.onPress = function()
   {
      Equipment.usePress();
      Equipment.inspectEntry(Equipment.menu.inventoryLists.itemList.selectedEntry, false);
   };
   Equipment.lens = lens;
   Equipment.drawLens(false);
};

Equipment.drawLens = function(lit)
{
   var look = Equipment.lens.look;
   var size = Equipment.LENS_SIZE;
   look.clear();
   look.lineStyle(1, lit ? Equipment.accentInk() : Equipment.inkOf("edge"), lit ? 100 : 60, true);
   Equipment.buttonFill(look, 70);
   Equipment.roundBox(look, 0, 0, size, size, 3);
   look.endFill();
   look.lineStyle();
   Equipment.drawIcon(look, "lens", 3, 3, size - 6, lit ? Equipment.accentInk() : Equipment.inkOf("sign"), 100);
};

Equipment.showLens = function(shown)
{
   var window = Equipment.window;
   var lens = Equipment.lens;
   var across = window.across != undefined ? window.across : 1;
   lens._visible = shown && !Equipment.inspecting && Equipment.inspectable(Equipment.menu.inventoryLists.itemList.selectedEntry);
   // in the window's proportions, it sits in the window's corner
   lens._xscale = across * 100;
   if(window.clamped)
   {
      // a window over its own row leaves the end of the row free, the lens sits there on the row
      lens._x = window._x + Equipment.windowWide() + across;
      lens._y = window.rowMiddle - Equipment.LENS_SIZE / 2;
   }
   else
   {
      lens._x = window._x + Equipment.windowWide() - Equipment.LENS_SIZE * across;
      lens._y = window.above ? window.rowEdge - Equipment.LENS_SIZE - Equipment.number(window.edge) : window.rowEdge;
   }
};

// for the test stand: turns and zooms the inspected item as a drag and the wheel would
Equipment.inspectBy = function(told)
{
   var parts = String(told).split(",");
   if(Equipment.inspecting)
   {
      Equipment.plugin().InspectTurn(Number(parts[0]), Number(parts[1]));
      Equipment.plugin().InspectZoom(Number(parts[2]));
   }
};
