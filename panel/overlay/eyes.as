// The movie of the eyes of the slots, drawn by the plugin right after an item menu and its 3D, so an eye lies over
// a model that fills its slot. The panel owns the eyes and tells where each one in sight lies and how it looks,
// this only draws them and takes no mouse. The drawing of icons comes from the panel's own parts
if(_global.Equipment == undefined)
{
   _global.Equipment = {};
}
_global.Eyes = {};
Eyes.STAGE_WIDE = 1280;
Eyes.STAGE_HIGH = 720;
Eyes.NUMBERS = 6;

// The open menu of a slot or a row, a copy of the panel's that lies over the 3D: its left and top in shares of the
// screen, the panel's unit in a share of the screen height, its width and height in the panel's units, the accent,
// text and white inks, then four values a line: lit (2), divider (1) or plain (0), its top, its words and its icon.
// Nothing told takes it away
Eyes.MENU_HEAD = 8;
Eyes.MENU_LINE = 4;
Eyes.menu = function(told, aspect)
{
   var menu;
   var line;
   var wide = told[3];
   var pad = Equipment.CONTEXT_PAD;
   var i = Eyes.MENU_HEAD;
   _root.menu.removeMovieClip();
   if(told.length < Eyes.MENU_HEAD)
   {
      return undefined;
   }
   Equipment.GOLD = told[5];
   Equipment.PARCHMENT = told[6];
   Equipment.WHITE = told[7];
   menu = _root.createEmptyMovieClip("menu", 2);
   Equipment.panelBox(menu, wide, told[4], 100, 40);
   while(i + Eyes.MENU_LINE - 1 < told.length)
   {
      if(told[i] == 1)
      {
         menu.lineStyle(1, Equipment.WHITE, 20, true);
         menu.moveTo(2 * pad, told[i + 1]);
         menu.lineTo(wide - 2 * pad, told[i + 1]);
         menu.lineStyle();
      }
      else
      {
         line = menu.createEmptyMovieClip("line" + i, i);
         line._x = pad;
         line._y = told[i + 1];
         Equipment.drawMenuLine(line.createEmptyMovieClip("look", 1), wide - 2 * pad, told[i] == 2, told[i + 3]);
         Equipment.writeMenuWords(line, told[i + 2], told[i] == 2);
      }
      i += Eyes.MENU_LINE;
   }
   menu._x = told[0] * Eyes.STAGE_WIDE;
   menu._y = told[1] * Eyes.STAGE_HIGH;
   menu._yscale = 100 * told[2] * Eyes.STAGE_HIGH;
   menu._xscale = menu._yscale * Eyes.STAGE_WIDE / Eyes.STAGE_HIGH / aspect;
};

// Six numbers an eye: left and top in shares of the screen, its size in a share of the screen height, shut, lit
// and its colour. The stage is stretched over the whole screen, so an eye is drawn narrower or wider to stay round
Eyes.draw = function(told, aspect)
{
   var holder;
   var eye;
   var size;
   var lit;
   var i = 0;
   _root.eyes.removeMovieClip();
   holder = _root.createEmptyMovieClip("eyes", 1);
   while(i + Eyes.NUMBERS - 1 < told.length)
   {
      size = told[i + 2] * Eyes.STAGE_HIGH;
      lit = told[i + 4] != 0;
      eye = holder.createEmptyMovieClip("eye" + i, i);
      eye.beginFill(0, lit ? 85 : 72);
      Equipment.drawRing(eye, size / 2, size / 2, size / 2);
      eye.endFill();
      Equipment.drawIcon(eye, told[i + 3] != 0 ? "eyeShut" : "eye", size / 9, size / 9, size * 7 / 9, told[i + 5], lit ? 100 : 80);
      eye._x = told[i] * Eyes.STAGE_WIDE;
      eye._y = told[i + 1] * Eyes.STAGE_HIGH;
      eye._xscale = 100 * Eyes.STAGE_WIDE / Eyes.STAGE_HIGH / aspect;
      i += Eyes.NUMBERS;
   }
};
