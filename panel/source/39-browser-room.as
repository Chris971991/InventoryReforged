// the gap between SkyUI's hints in the bottom bar and the first button after them
Equipment.BAR_GAP = 24;
// frames the panel keeps its room for a browser that was left open and hasn't come yet
Equipment.BROWSER_WAIT = 30;

// Know your Skyrim, the item browser, is a mod of its own. With the panel it stands in the panel's room: the doll, the
// slots and their models go while it's open, and the look button follows its eye button in the bar. Either mod works
// alone. Whichever comes second shakes hands, the panel calls _global.KnowYourSkyrim.meetPanel(Equipment) once it is
// laid out, the browser calls it itself when it finds browserRoomReady. Then the browser draws in browserRoom by
// browserFrame() and tells browserShown(open) and followBarButton(button), the panel tells roomChanged()
Equipment.buildBrowserRoom = function()
{
   var browser = skse.plugins.KnowYourSkyrim;
   var kept = browser != undefined && browser.BrowserKept != undefined ? browser.BrowserKept() : undefined;
   // a browser left open opens again, the doll and the slot models aren't made for nothing meanwhile
   Equipment.browsing = kept != undefined && kept.open == true;
   // first, so the windows and menus of the panel lie over the browser
   Equipment.browserRoom = Equipment.under.createEmptyMovieClip("equipmentBrowserRoom", Equipment.under.getNextHighestDepth());
   Equipment.followHints();
   // The frame of the panel is laid out after this, the browser is met on the next frame. SkyUI has moved the numbers
   // of the player by then, the look button finds its place too. A browser left open that never comes, its movie
   // missing, leaves the room to the panel after a while
   Equipment.browserRoom.waited = 0;
   Equipment.browserRoom.onEnterFrame = function()
   {
      var met;
      if(this.waited == 0)
      {
         Equipment.placeLookButton();
         Equipment.browserRoomReady = true;
         Equipment.meetBrowser();
      }
      this.waited++;
      met = _global.KnowYourSkyrim != undefined && _global.KnowYourSkyrim.panel == Equipment;
      if(met || !Equipment.browsing || this.waited > Equipment.BROWSER_WAIT)
      {
         delete this.onEnterFrame;
         if(!met && Equipment.browsing)
         {
            Equipment.browserShown(false);
         }
      }
   };
};

Equipment.meetBrowser = function()
{
   var browser = _global.KnowYourSkyrim;
   if(browser != undefined && browser.meetPanel != undefined)
   {
      browser.meetPanel(Equipment);
   }
};

// the room of the browser in the units of browserRoom: the frame of the panel down past the names under the slots
Equipment.browserFrame = function()
{
   var frame = Equipment.frame;
   return {left:frame.left, top:frame.top, right:frame.right, bottom:frame.bottom + Equipment.LABEL_HEIGHT};
};

// The browser opened: the windows of the panel close, the panel, the drawer, the doll and the slot models go and
// nothing of them is made while it's open. Shut, the panel comes back as it was, its models and the doll made again
Equipment.browserShown = function(open)
{
   var plugin = Equipment.plugin();
   Equipment.browsing = open == true;
   if(Equipment.browsing)
   {
      Equipment.closePopup();
      Equipment.closeContext();
      Equipment.closePieces();
      Equipment.panel._visible = false;
      Equipment.hero._visible = false;
      plugin.HidePlayer();
      plugin.ShowSlots([]);
      Equipment.wearing = undefined;
      return undefined;
   }
   Equipment.panel._visible = true;
   Equipment.hero._visible = Equipment.settings.heroStats && plugin != undefined;
   Equipment.wearing = undefined;
   Equipment.showModels();
   // the sheet wasn't counted while the browser stood over it, an equip from the list meanwhile changed it
   Equipment.refreshHero();
};

// The eye button of the browser stands in the bottom bar, the look button right after it
Equipment.followBarButton = function(button)
{
   Equipment.browseButton = button;
   Equipment.placeLookButton();
};

// how much room the look button takes after the eye
Equipment.lookRoom = function()
{
   return Equipment.lookButton != undefined && Equipment.lookButton._visible ? Equipment.LOOK_BAR_GAP + Equipment.LOOK_BUTTON : 0;
};

// The zoom key closes the browser's inspect view the way it closes the panel's own
Equipment.browserInspectKey = function()
{
   var browser = _global.KnowYourSkyrim;
   if(browser == undefined || browser.inspecting != true)
   {
      return false;
   }
   browser.leaveInspect();
   return true;
};

Equipment.browserRoomChanged = function()
{
   var browser = _global.KnowYourSkyrim;
   if(Equipment.browserRoomReady && browser != undefined && browser.roomChanged != undefined)
   {
      browser.roomChanged();
   }
};

// The first free spot in the bottom bar after SkyUI's hints for a button this high, in the units of the bar. A bar
// with no hints has none
Equipment.afterHints = function(high)
{
   var hints = Equipment.menu.bottomBar.buttonPanel;
   var end = 0;
   var tallest = 0;
   var one;
   var i = 0;
   if(hints == undefined || hints.buttons == undefined)
   {
      return undefined;
   }
   while(i < hints.buttons.length)
   {
      one = hints.buttons[i];
      if(one._visible && one.label.length > 0)
      {
         end = Math.max(end, one._x + one._width);
         tallest = Math.max(tallest, one._height);
      }
      i++;
   }
   return {x:Math.round(hints._x + end + Equipment.BAR_GAP), y:Math.round(hints._y + (tallest - high) / 2)};
};

// SkyUI lays its hints out anew for every row, the look button follows them
Equipment.followHints = function()
{
   var hints = Equipment.menu.bottomBar.buttonPanel;
   if(hints == undefined || hints.doUpdateButtons == undefined)
   {
      return undefined;
   }
   hints.lookUpdate = hints.doUpdateButtons;
   hints.doUpdateButtons = function()
   {
      this.lookUpdate();
      Equipment.placeLookButton();
   };
};
