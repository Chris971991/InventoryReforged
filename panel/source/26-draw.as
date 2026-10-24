Equipment.WHITE = 16777215;
Equipment.GOLD = 14069864;
Equipment.PARCHMENT = 15130824;
Equipment.MOVE = 0;
Equipment.LINE = 1;
Equipment.CURVE = 2;
Equipment.ICON_BOX = 24;

// Draws an icon of Equipment.ICONS into a clip in a square of a size at x, y
// A part with width 0 is a fill, holes come from the even-odd rule, any other part is a line that wide
Equipment.drawIcon = function(clip, name, x, y, size, color, alpha)
{
   var parts = Equipment.ICONS[name];
   var unit = size / Equipment.ICON_BOX;
   var p = 0;
   var part;
   var i;
   if(parts == undefined)
   {
      return undefined;
   }
   while(p < parts.length)
   {
      part = parts[p];
      p++;
      if(part[0] == 0)
      {
         clip.lineStyle();
         clip.beginFill(color, alpha);
      }
      else
      {
         clip.lineStyle(part[0] * unit, color, alpha, true, "normal", "round", "round");
      }
      i = 1;
      while(i < part.length)
      {
         if(part[i] == Equipment.CURVE)
         {
            clip.curveTo(x + part[i + 1] * unit, y + part[i + 2] * unit, x + part[i + 3] * unit, y + part[i + 4] * unit);
            i += 5;
         }
         else
         {
            if(part[i] == Equipment.MOVE)
            {
               clip.moveTo(x + part[i + 1] * unit, y + part[i + 2] * unit);
            }
            else
            {
               clip.lineTo(x + part[i + 1] * unit, y + part[i + 2] * unit);
            }
            i += 3;
         }
      }
      if(part[0] == 0)
      {
         clip.endFill();
      }
   }
   clip.lineStyle();
};

Equipment.drawBox = function(clip, wide, high, shade)
{
   clip.beginFill(0, shade);
   clip.moveTo(0, 0);
   clip.lineTo(wide, 0);
   clip.lineTo(wide, high);
   clip.lineTo(0, high);
   clip.lineTo(0, 0);
   clip.endFill();
};

// a box with round corners from x, y, with a border when a line style is set before
Equipment.roundBox = function(clip, x, y, wide, high, round)
{
   var r = Math.min(round, wide / 2, high / 2);
   clip.moveTo(x + r, y);
   clip.lineTo(x + wide - r, y);
   clip.curveTo(x + wide, y, x + wide, y + r);
   clip.lineTo(x + wide, y + high - r);
   clip.curveTo(x + wide, y + high, x + wide - r, y + high);
   clip.lineTo(x + r, y + high);
   clip.curveTo(x, y + high, x, y + high - r);
   clip.lineTo(x, y + r);
   clip.curveTo(x, y, x + r, y);
};

// a panel of the mod: dark, round cornered, a thin light edge, a shade of gradient down
Equipment.panelBox = function(clip, wide, high, shade, edge)
{
   var turn = new flash.geom.Matrix();
   turn.createGradientBox(wide, high, Math.PI / 2, 0, 0);
   clip.lineStyle(1, Equipment.WHITE, edge, true);
   clip.beginGradientFill("linear", [1710618, 0], [shade, shade + 8 > 100 ? 100 : shade + 8], [0, 255], turn);
   Equipment.roundBox(clip, 0, 0, wide, high, 4);
   clip.endFill();
   clip.lineStyle();
};

// A line of the menu of a slot or a row, the panel's menu and its copy over the 3D draw it alike
Equipment.CONTEXT_LINE = 26;
Equipment.CONTEXT_PAD = 4;
Equipment.CONTEXT_ICON = 14;
Equipment.drawMenuLine = function(look, wide, lit, icon)
{
   var size = Equipment.CONTEXT_ICON;
   look.clear();
   look.beginFill(lit ? Equipment.GOLD : 0, lit ? 20 : 0);
   Equipment.roundBox(look, 0, 0, wide, Equipment.CONTEXT_LINE, 3);
   look.endFill();
   if(lit)
   {
      look.beginFill(Equipment.GOLD, 100);
      Equipment.roundBox(look, 0, 4, 2, Equipment.CONTEXT_LINE - 8, 1);
      look.endFill();
   }
   Equipment.drawIcon(look, icon, Equipment.CONTEXT_PAD + 4, (Equipment.CONTEXT_LINE - size) / 2, size, lit ? Equipment.GOLD : Equipment.PARCHMENT, 100);
};

// the words of a menu line beside its icon, in the middle of the line
Equipment.writeMenuWords = function(line, words, lit)
{
   var field = Equipment.writeLine(line, "words", 2, words, 14, lit ? Equipment.GOLD : Equipment.WHITE);
   field._x = 2 * Equipment.CONTEXT_PAD + Equipment.CONTEXT_ICON + 8;
   field._y = (Equipment.CONTEXT_LINE - field._height) / 2;
   return field;
};

// a circle out of eight curves
Equipment.drawRing = function(clip, x, y, radius)
{
   var step = Math.PI / 4;
   var reach = radius / Math.cos(step / 2);
   var turn;
   var i = 1;
   clip.moveTo(x + radius, y);
   while(i <= 8)
   {
      turn = i * step;
      clip.curveTo(x + reach * Math.cos(turn - step / 2), y + reach * Math.sin(turn - step / 2), x + radius * Math.cos(turn), y + radius * Math.sin(turn));
      i++;
   }
};

// The panel puts its words in translated, a field of its own never looks a text up. A field that was handed a key
// it had no words for ("$TNG_TCT", an item some mod named so) showed squares for every text after
Equipment.styleText = function(field, size, align, color)
{
   var format = new TextFormat("$EverywhereMediumFont", size, color == undefined ? Equipment.WHITE : color);
   format.align = align;
   field.selectable = false;
   field.noTranslate = true;
   field.setNewTextFormat(format);
};

// one line of text that's as wide as its words
Equipment.writeLine = function(parent, name, depth, words, size, color)
{
   parent.createTextField(name, depth, 0, 0, 10, 10);
   var line = parent[name];
   Equipment.styleText(line, size, "left", color);
   line.autoSize = "left";
   line.text = words;
   return line;
};
