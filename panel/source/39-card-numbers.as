Equipment.CARD_NUMBERS_MARGIN = 8;
Equipment.CHEST_SCROLL_LINES = 3;
Equipment.CHEST_BAR = 3;
// The numbers take their sizes from the card's own letters: names as its labels, numbers a little smaller than its
// big values, the quiet lines a little smaller than its words. A card without one of them gets these, in the panel's
// units
Equipment.CHEST_VALUE_SHARE = 0.7;
Equipment.CHEST_QUIET_SHARE = 0.9;
Equipment.CHEST_TEXT = 20;
Equipment.CHEST_LABEL = 17;
Equipment.CHEST_QUIET = 18;
Equipment.CHEST_LINE = 27;
Equipment.CHEST_GAP = 14;
// the most of the line the names may take, the numbers keep the rest
Equipment.CHEST_NAMES_SHARE = 0.55;
Equipment.CHEST_BELOW = 16;
// the frame's bottom edge that moves down under the numbers with its ornaments, and the plain slice that fills between
Equipment.CHEST_KEEP = 30;
Equipment.CHEST_SLICE = 4;
// how many frames after a layout the card's frame is watched for a skin still bringing it in
Equipment.CHEST_SETTLE = 12;
// the lowest depth a clip can take
Equipment.LOWEST_DEPTH = -16384;

// In a chest or a shop the numbers of the item window go into SkyUI's own card: right under its words, in its
// letters, and the card's frame grows down to hold them, the way SkyUI grows it for a longer card. The card keeps its
// place and its look, the game's model stays where SkyUI shows it. Down to the bottom bar or the screen's edge the
// frame grows, past that the numbers scroll by the wheel. The numbers live inside the card, so they move, fade and
// take a skin's colours with it. It follows the selected row and is measured a frame later, when the card has its
// new size
Equipment.startCardNumbers = function()
{
   var menu = Equipment.menu;
   var card = menu.itemCard;
   var box;
   if(card == undefined)
   {
      return undefined;
   }
   box = card.createEmptyMovieClip("equipmentCardNumbers", card.getNextHighestDepth());
   box.createEmptyMovieClip("numbers", 1);
   box.createEmptyMovieClip("mask", 2);
   box.createEmptyMovieClip("bar", 3);
   box.createEmptyMovieClip("settle", 4);
   box.numbers.createTextField("names", 1, 0, 0, 10, 10);
   box.numbers.createTextField("quiet", 2, 0, 0, 10, 10);
   box.names = box.numbers.names;
   box.quiet = box.numbers.quiet;
   Equipment.styleText(box.names, Equipment.CHEST_TEXT, "left", Equipment.inkOf("name"));
   Equipment.styleText(box.quiet, Equipment.CHEST_QUIET, "left", Equipment.inkOf("quiet"));
   box.names.multiline = box.quiet.multiline = true;
   box.names.wordWrap = false;
   box.names.html = true;
   box.quiet.wordWrap = true;
   box.quiet.autoSize = "left";
   box.numbers.setMask(box.mask);
   box.hitTestDisable = true;
   box._visible = false;
   box.scrolled = 0;
   Equipment.cardNumbers = box;
   Equipment.chestWheel = {};
   Equipment.chestWheel.onMouseWheel = function(delta)
   {
      Equipment.scrollCardNumbers(delta);
   };
   Mouse.addListener(Equipment.chestWheel);
   menu.inventoryLists.addEventListener("itemHighlightChange", Equipment, "cardNumbersLater");
   menu.inventoryLists.addEventListener("hideItemsList", Equipment, "cardNumbersLater");
   menu.itemCard.addEventListener("subMenuAction", Equipment, "cardAsks");
   // The game fills the card on its own too, after a sale or when a count changes. Another kind of card brings a new
   // frame of SkyUI's that knew nothing of the numbers, and the frame laid out for the last one stayed beside it
   menu.equipmentCardInfo = menu.UpdateItemCardInfo;
   menu.UpdateItemCardInfo = function(info)
   {
      this.equipmentCardInfo(info);
      Equipment.cardNumbersLater();
   };
};

// While the card asks how many its Yes and No sit lower than the item's own lines, the card stays as SkyUI made it
Equipment.cardAsks = function(event)
{
   Equipment.cardAsking = event.opening == true;
   Equipment.cardStamp = Equipment.number(Equipment.cardStamp) + 1;
   Equipment.holdRows(Equipment.cardAsking);
   if(Equipment.cardAsking)
   {
      Equipment.cardNumbers._visible = false;
      Equipment.showCardNumbers();
   }
   else
   {
      Equipment.cardNumbersLater();
   }
};

// SkyUI keeps the rows still while the card asks how many, the menus of some skins don't. A row the pointer
// crossed on its way to the slider took the card, and the question went to that item. The inspect view and the
// look sheet hold them too, a letting go of one keeps them held while another still lies over them
Equipment.holdRows = function(asked)
{
   var lists = Equipment.menu.inventoryLists;
   var held = asked || Equipment.inspecting == true || Equipment.cardAsking == true || Equipment.look != undefined && Equipment.look._visible == true ||
      Equipment.junkAsk != undefined && Equipment.junkAsk._visible == true || Equipment.contextHoldsRows == true;
   lists.itemList.disableSelection = lists.itemList.disableInput = held;
   lists.categoryList.disableSelection = lists.categoryList.disableInput = held;
};

Equipment.cardNumbersLater = function()
{
   var box = Equipment.cardNumbers;
   if(box == undefined)
   {
      return undefined;
   }
   box.onEnterFrame = function()
   {
      this.onEnterFrame = undefined;
      Equipment.showCardNumbers();
   };
};

// The card's frame as SkyUI drew it: the pieces of a longer frame go and SkyUI's own frame shows again
Equipment.cardFrameAsSkyUI = function(back)
{
   var card = Equipment.menu.itemCard;
   var names = ["equipmentFrameTop", "equipmentFrameFill", "equipmentFrameBottom", "equipmentFrameTopMask", "equipmentFrameFillMask", "equipmentFrameBottomMask"];
   var i = 0;
   if(back != undefined && Equipment.cardBackHidden)
   {
      back._visible = true;
   }
   Equipment.cardBackHidden = false;
   // the size SkyUI gave it lives on the frame itself: a frame SkyUI put up anew for another kind of card has the
   // same name, and a reference kept anywhere else would lead to the new one
   if(back != undefined && back.equipmentGrid != undefined)
   {
      back._height = back.equipmentGrid.high;
      back._y = back.equipmentGrid.y;
      back.equipmentGrid = undefined;
   }
   if(Equipment.skinArt != undefined)
   {
      Equipment.skinArt.scale9Grid = null;
      Equipment.skinArt._height = Equipment.skinArtHigh;
      Equipment.skinArt._y = Equipment.skinArtY;
      Equipment.skinArt = undefined;
   }
   while(i < names.length)
   {
      if(card[names[i]] != undefined)
      {
         // a clip under the card's own parts can't be removed from there
         card[names[i]].swapDepths(card.getNextHighestDepth());
         card[names[i]].removeMovieClip();
      }
      i++;
   }
};

// the lowest depth of the card's own parts, the top piece of a longer frame goes under all of them
Equipment.lowestDepth = function(card)
{
   var lowest;
   for(var name in card)
   {
      if(card[name]._parent == card && card[name].getDepth != undefined)
      {
         lowest = lowest == undefined ? card[name].getDepth() : Math.min(lowest, card[name].getDepth());
      }
   }
   return lowest;
};

// Where the pieces of a longer frame go, in the card's units. The frame is cut under the card's words: above the cut
// it stays as it is, its last keep units (the bottom edge with its ornaments) move down till the frame ends at
// bottom, and the space between is a thin slice of the frame just under the cut stretched to fill it. A skin's bands
// and corners keep their size that way, only a plain strip grows. Nothing moves when the frame already reaches bottom
Equipment.framePieces = function(art, cut, bottom, keep, slice)
{
   var kept = Math.min(keep, Math.max(0, art.yMax - cut));
   var bottomFrom = art.yMax - kept;
   var pieces = {bottomFrom:bottomFrom, shift:Math.max(0, bottom - art.yMax), rise:0};
   // the slice comes from under the cut, or from just above it when the frame keeps nothing between
   pieces.sliceHigh = Math.min(slice, bottomFrom - cut);
   pieces.sliceTop = cut;
   if(pieces.sliceHigh < 1)
   {
      pieces.sliceHigh = slice;
      pieces.sliceTop = cut - slice;
   }
   pieces.fillHigh = bottomFrom + pieces.shift - cut;
   return pieces;
};

// Lays the longer frame out from three copies of SkyUI's own frame, each under a mask, and hides the frame itself. A
// mask set on the frame of some skins doesn't take and shows as a black box, on a copy it does. A copy is born at
// its own scale, it's laid over the frame by its bounds first. A frame that rises keeps the top edge's size too and
// moves it up by pieces.rise, the slice then reaches up to it. Only the top piece fits under the card's words, a
// slice that runs past the words shows only its sides (that many units of each) so it never lies over them.
// Tells whether the copies took
Equipment.frameCopies = function(card, back, art, cut, pieces, sides)
{
   var top;
   var fill;
   var bottom;
   var stretch = pieces.fillHigh / pieces.sliceHigh;
   var under = Equipment.lowestDepth(card) - 1;
   var from = cut - pieces.rise;
   top = back.duplicateMovieClip("equipmentFrameTop", card.getNextHighestDepth());
   fill = back.duplicateMovieClip("equipmentFrameFill", card.getNextHighestDepth());
   bottom = back.duplicateMovieClip("equipmentFrameBottom", card.getNextHighestDepth());
   // A skin that keeps its picture in clips of its own inside the frame gets copies without them, empty or half
   // drawn, and it draws that frame by its own code. Such a frame stays as the skin made it
   // AS2 builds >= as "not <", which holds for a missing number, a card with no parts gives one here
   if(top == undefined || fill == undefined || bottom == undefined || !(under > Equipment.LOWEST_DEPTH - 1) || Equipment.partCount(fill) < Equipment.partCount(back))
   {
      Equipment.cardFrameAsSkyUI(back);
      return false;
   }
   // the top piece lies where the frame lay, under the card's words. The other two are drawn only under the words
   top.swapDepths(under);
   Equipment.layOver(top, back, art);
   Equipment.layOver(fill, back, art);
   Equipment.layOver(bottom, back, art);
   // a picture loaded into the frame or drawn there by code doesn't come with a copy, the copy has nothing to show
   if(!Equipment.laidOver(top, art) || !Equipment.laidOver(fill, art) || !Equipment.laidOver(bottom, art))
   {
      Equipment.cardFrameAsSkyUI(back);
      return false;
   }
   top._y -= pieces.rise;
   // the slice's top lands on the cut and the slice reaches the moved bottom edge
   fill._y = from - (pieces.sliceTop - fill._y) * stretch;
   fill._yscale *= stretch;
   bottom._y += pieces.shift;
   top.setMask(Equipment.frameMask(card, "equipmentFrameTopMask", art, art.yMin - pieces.rise - 1, from));
   fill.setMask(Equipment.frameMask(card, "equipmentFrameFillMask", art, from, from + pieces.fillHigh, sides));
   bottom.setMask(Equipment.frameMask(card, "equipmentFrameBottomMask", art, pieces.bottomFrom + pieces.shift, art.yMax + pieces.shift + 1));
   back._visible = false;
   Equipment.cardBackHidden = true;
   return true;
};

// SkyUI draws its card's frame with a nine-slice grid and makes it longer for a longer card by its height alone: the
// edges and corners keep their size and only the plain middle stretches. A frame with a grid of its own grows that way
// too, so its bottom edge goes down whole under the numbers even when the card's words reach into it. Cut copies left
// half of that edge where the words ended and the corners showed twice. A frame that didn't take the new height stays
// as it was, so does one that keeps nothing written on it, its size couldn't be given back
Equipment.growByGrid = function(card, back, pieces)
{
   var was = back.getBounds(card);
   if(back.scale9Grid == undefined || !(was.yMax > was.yMin))
   {
      return false;
   }
   back.equipmentGrid = {high:back._height, y:back._y};
   if(back.equipmentGrid == undefined)
   {
      return false;
   }
   back._height += pieces.shift;
   back._y += was.yMin - back.getBounds(card).yMin;
   if(!Equipment.sameEdge(back.getBounds(card).yMax, was.yMax + pieces.shift))
   {
      Equipment.cardFrameAsSkyUI(back);
      return false;
   }
   return true;
};

Equipment.growCardFrame = function(card, back, art, cut, pieces)
{
   if(!Equipment.frameCopies(card, back, art, cut, pieces))
   {
      return false;
   }
   Equipment.cardNumbers.swapDepths(card.getNextHighestDepth());
   // a skin whose code shows its frame again keeps its frame as it made it from then on
   card.equipmentFrameTopMask.onEnterFrame = function()
   {
      this.onEnterFrame = undefined;
      // a new row on its way is laid out anew anyway, SkyUI may show the frame for it
      if(back._visible && Equipment.cardBackHidden && Equipment.cardNumbers.onEnterFrame == undefined)
      {
         Equipment.frameOfSkin = true;
         Equipment.showCardNumbers();
      }
   };
   return true;
};

// The frame of a skin that draws it by code keeps its picture in a clip inside, the tallest one when there are a few.
// That clip grows by its own nine-slice grid: everything above the slice under the cut and everything below it keeps
// its size, only the slice stretches. A clip grows from its own origin, the picture is moved back so its top stays
Equipment.growSkinFrame = function(card, back, pieces)
{
   var art;
   var own;
   var top;
   var low;
   var was;
   for(var name in back)
   {
      if(typeof back[name] == "movieclip" && name.indexOf("equipment") != 0 && (art == undefined || back[name]._height > art._height))
      {
         art = back[name];
      }
   }
   if(art == undefined || flash.geom.Rectangle == undefined)
   {
      Equipment.skinGrowth = art == undefined ? "no picture clip in the frame" : "no Rectangle";
      return false;
   }
   own = art.getBounds(art);
   top = {x:0, y:pieces.sliceTop};
   low = {x:0, y:pieces.sliceTop + pieces.sliceHigh};
   card.localToGlobal(top);
   card.localToGlobal(low);
   art.globalToLocal(top);
   art.globalToLocal(low);
   if(!(low.y > top.y) || top.y <= own.yMin || low.y >= own.yMax)
   {
      Equipment.skinGrowth = "slice " + Math.round(top.y) + "-" + Math.round(low.y) + " outside the picture " + Math.round(own.yMin) + "-" + Math.round(own.yMax);
      return false;
   }
   Equipment.skinArt = art;
   Equipment.skinArtHigh = art._height;
   Equipment.skinArtY = art._y;
   was = art.getBounds(back).yMin;
   art.scale9Grid = new flash.geom.Rectangle(own.xMin + 1, top.y, own.xMax - own.xMin - 2, low.y - top.y);
   art._height += pieces.shift * art._height / (back.getBounds(card).yMax - back.getBounds(card).yMin);
   art._y += was - art.getBounds(back).yMin;
   Equipment.skinGrowth = "grown by " + Math.round(pieces.shift) + ", picture " + Math.round(Equipment.skinArtHigh) + " to " + Math.round(art._height) + ", grid " + (art.scale9Grid != undefined);
   return true;
};

Equipment.partCount = function(clip)
{
   var count = 0;
   for(var name in clip)
   {
      if(typeof clip[name] == "movieclip" || clip[name] instanceof TextField)
      {
         count++;
      }
   }
   return count;
};

// a copy of the frame put exactly where the frame is, by their bounds in the card
Equipment.layOver = function(copy, back, art)
{
   var card = copy._parent;
   var now;
   copy.gotoAndStop(back._currentframe);
   copy._xscale = back._xscale;
   copy._yscale = back._yscale;
   now = copy.getBounds(card);
   if(now.yMax - now.yMin > 0 && now.xMax - now.xMin > 0)
   {
      copy._yscale *= (art.yMax - art.yMin) / (now.yMax - now.yMin);
      copy._xscale *= (art.xMax - art.xMin) / (now.xMax - now.xMin);
   }
   now = copy.getBounds(card);
   copy._x += art.xMin - now.xMin;
   copy._y += art.yMin - now.yMin;
};

Equipment.laidOver = function(copy, art)
{
   var now = copy.getBounds(copy._parent);
   return Equipment.sameEdge(now.xMin, art.xMin) && Equipment.sameEdge(now.xMax, art.xMax) && Equipment.sameEdge(now.yMin, art.yMin) && Equipment.sameEdge(now.yMax, art.yMax);
};

// two edges a unit apart at most. Never for a missing number: in AS2 a <= b with NaN is true, a < b and == are false
Equipment.sameEdge = function(a, b)
{
   var apart = Math.abs(a - b);
   return apart < 1 || apart == 1;
};

// a mask over the frame from top to low, or over its two sides only, that many units of each
Equipment.frameMask = function(card, name, art, top, low, sides)
{
   var mask = card.createEmptyMovieClip(name, card.getNextHighestDepth());
   if(sides == undefined)
   {
      Equipment.maskBox(mask, art.xMin - 2, top, art.xMax + 2, low);
   }
   else
   {
      Equipment.maskBox(mask, art.xMin - 2, top, art.xMin + sides, low);
      Equipment.maskBox(mask, art.xMax - sides, top, art.xMax + 2, low);
   }
   return mask;
};

Equipment.maskBox = function(mask, left, top, right, low)
{
   mask.beginFill(0, 100);
   mask.moveTo(left, top);
   mask.lineTo(right, top);
   mask.lineTo(right, low);
   mask.lineTo(left, low);
   mask.lineTo(left, top);
   mask.endFill();
};

Equipment.showCardNumbers = function()
{
   var box = Equipment.cardNumbers;
   var menu = Equipment.menu;
   var card = menu.itemCard;
   var back = card != undefined ? card.background : undefined;
   var entry = menu.inventoryLists.itemList.selectedEntry;
   var lines = entry != undefined && Equipment.settings.stats ? Equipment.statsOf(entry) : [];
   var quiet = entry != undefined ? Equipment.quietLines(entry) : [];
   var names = [];
   var values = [];
   var changes = [];
   var frame;
   var inPanel;
   var unit;
   var pad;
   var words;
   var art;
   var cut;
   var keep;
   var pieces;
   var below;
   var limit;
   var shown;
   var numbersHigh;
   var letters;
   var column;
   var format;
   var measure;
   var i = 0;
   if(box == undefined || back == undefined)
   {
      return undefined;
   }
   Equipment.cardLayouts = Equipment.number(Equipment.cardLayouts) + 1;
   Equipment.keepCard();
   Equipment.cardFrameAsSkyUI(back);
   frame = Equipment.frameBox(card, card);
   inPanel = Equipment.frameBox(card, Equipment.under);
   // a frame with no width yet gives the card's unit no number, everything laid out by it would be lost
   if(entry == undefined || frame == undefined || inPanel == undefined || !(inPanel.xMax - inPanel.xMin > 0) || !(frame.xMax - frame.xMin > 0) || Equipment.cardAsking || Equipment.cardBox() == undefined || lines.length == 0 && quiet.length == 0)
   {
      box._visible = false;
      return undefined;
   }
   // the card's units for every length, a skin may scale its card
   unit = (frame.xMax - frame.xMin) / (inPanel.xMax - inPanel.xMin);
   pad = Equipment.STATS_PAD * unit;
   while(i < lines.length)
   {
      names.push(lines[i][0]);
      values.push(lines[i][1]);
      changes.push(lines[i][2]);
      i++;
   }
   // names like the card's labels, numbers like its values, the quiet lines like its words, all from the left edge
   // of its labels. A name and its number share a line of one field, so the rows keep one height
   letters = Equipment.wearCardLetters(box, frame, unit);
   box.names._x = letters.left;
   box.names._width = frame.xMax - pad - letters.left;
   box.names._y = 0;
   measure = function(words)
   {
      box.names.htmlText = Equipment.cardRows(letters, [words], undefined, undefined);
      return box.names.textWidth;
   };
   i = 0;
   while(i < names.length)
   {
      names[i] = Equipment.cutToFit(names[i], box.names._width * Equipment.CHEST_NAMES_SHARE - Equipment.CHEST_GAP * unit, measure);
      i++;
   }
   box.names.htmlText = Equipment.cardRows(letters, names, undefined, undefined);
   column = box.names.textWidth + Equipment.CHEST_GAP * unit;
   box.names.htmlText = Equipment.cardRows(letters, names, values, changes);
   format = new TextFormat();
   format.tabStops = [column];
   box.names.setTextFormat(format);
   box.names._height = box.names.textHeight + 6 * unit;
   box.names._visible = lines.length > 0;
   box.rowHigh = lines.length > 0 ? box.names.textHeight / lines.length : Equipment.CHEST_LINE * unit;
   box.quiet._x = letters.left;
   box.quiet._width = frame.xMax - pad - letters.left;
   box.quiet.text = quiet.join("\n");
   box.quiet._y = lines.length > 0 ? box.names.textHeight + pad / 2 : 0;
   box.quiet._visible = quiet.length > 0;
   numbersHigh = quiet.length > 0 ? box.quiet._y + box.quiet._height : box.names.textHeight;

   words = Equipment.cardWordsBottom(frame.yMax, card);
   // the frame as drawn, a skin's picture may reach past the box it counts as the frame
   art = back.getBounds(card);
   cut = words + pad / 4;
   box.numbersTop = words + pad / 2;
   keep = Math.min(Equipment.CHEST_KEEP * unit, Math.max(0, art.yMax - cut));
   // the last line ends this far above the frame's bottom, its ornaments sit in the corners beside it. A skin's
   // picture can have a clear margin round the frame, the line keeps its distance from what shows
   below = Equipment.CHEST_BELOW * unit + Math.max(0, art.yMax - frame.yMax);
   limit = Equipment.cardBottomLimit(card, unit);
   shown = Equipment.shownLines(box, numbersHigh, limit - below);
   pieces = Equipment.framePieces(art, cut, box.numbersTop + shown + below, keep, Equipment.CHEST_SLICE * unit);
   if(pieces.shift > 0 && !Equipment.growByGrid(card, back, pieces) && (Equipment.frameOfSkin || !Equipment.growCardFrame(card, back, art, cut, pieces)))
   {
      if(Equipment.growSkinFrame(card, back, pieces))
      {
         Equipment.frameOfSkin = true;
      }
      else
      {
         // a frame that can't grow holds what fits in it, the rest scrolls
         shown = Equipment.shownLines(box, numbersHigh, art.yMax - below);
      }
   }
   // A skin may still be bringing its card in, its frame changing size: then it's all laid out again. The frame is
   // measured as it came out, a grown picture that missed the bottom it was meant for would lay the card out every frame
   box.settle.frames = Equipment.CHEST_SETTLE;
   box.settle.low = back.getBounds(card).yMax;
   // a skin may put the picture into its frame a moment after the card changed
   box.settle.wantsPicture = Equipment.frameOfSkin && pieces.shift > 0 && Equipment.skinArt == undefined && Equipment.partCount(back) == 0;
   box.settle.onEnterFrame = function()
   {
      var back = Equipment.menu.itemCard.background;
      if(--this.frames <= 0 || back == undefined)
      {
         this.onEnterFrame = undefined;
      }
      else if((Math.abs(back.getBounds(back._parent).yMax - this.low) > 1 || this.wantsPicture && Equipment.partCount(back) > 0) && Equipment.cardNumbers.onEnterFrame == undefined && !Equipment.cardBackHidden)
      {
         this.onEnterFrame = undefined;
         Equipment.showCardNumbers();
      }
   };
   box.shown = shown;
   box.left = frame.xMin;
   box.right = frame.xMax;
   box.unit = unit;
   box.scrollMost = Math.max(0, Math.ceil((numbersHigh - shown) / box.rowHigh) * box.rowHigh);
   if(entry != box.entry)
   {
      box.entry = entry;
      box.scrolled = 0;
   }
   box.mask.clear();
   box.mask.beginFill(0, 100);
   box.mask.moveTo(frame.xMin, box.numbersTop);
   box.mask.lineTo(frame.xMax, box.numbersTop);
   box.mask.lineTo(frame.xMax, box.numbersTop + shown);
   box.mask.lineTo(frame.xMin, box.numbersTop + shown);
   box.mask.lineTo(frame.xMin, box.numbersTop);
   box.mask.endFill();
   Equipment.scrollCardNumbers(0);
   box._visible = true;
};

// How much of the numbers shows when they may end at lowest: whole lines, a line cut in half at the edge reads as a
// mistake, and one line at the least
Equipment.shownLines = function(box, high, lowest)
{
   if(box.numbersTop + high <= lowest)
   {
      return high;
   }
   return Math.min(high, Math.max(box.rowHigh, Math.floor((lowest - box.numbersTop) / box.rowHigh) * box.rowHigh));
};

// How low the card's frame may reach, in the card's units: a little above the bottom bar when the bar lies under the
// card, else above the screen's edge. Some skins keep the bar under the list only
Equipment.cardBottomLimit = function(card, unit)
{
   var bar = Equipment.menu.bottomBar;
   var lowest = {x:0, y:Stage.visibleRect.y + Stage.visibleRect.height - Stage.safeRect.y};
   var top;
   var across;
   var under;
   if(bar != undefined)
   {
      top = {x:0, y:bar._y};
      bar._parent.localToGlobal(top);
      across = bar.getBounds(_root);
      under = card.background.getBounds(_root);
      if(across.xMax > under.xMin && across.xMin < under.xMax && top.y < lowest.y)
      {
         lowest = top;
      }
      Equipment.limitWhy = "bar " + Math.round(across.xMin) + "-" + Math.round(across.xMax) + " top " + Math.round(top.y) + ", card " + Math.round(under.xMin) + "-" + Math.round(under.xMax) + ", screen bottom " + Math.round(Stage.visibleRect.y + Stage.visibleRect.height - Stage.safeRect.y) + ", took " + Math.round(lowest.y);
   }
   card.globalToLocal(lowest);
   return lowest.y - Equipment.CARD_NUMBERS_MARGIN * unit;
};

// Gives the numbers the card's letters and tells where the card's labels start, in the card's units. The labels of
// SkyUI's card (WEIGHT, DAMAGE) are short fields named ...Label, its numbers are bold, its words are the enchantment
// or the description. A skin keeps those names with letters of its own. What the card hasn't got stays as it was
Equipment.wearCardLetters = function(box, frame, unit)
{
   var card = Equipment.menu.itemCard;
   var letters;
   var label;
   var value;
   var words;
   var leftmost;
   var field;
   var format;
   var lower;
   var edge;
   for(var name in card)
   {
      field = card[name];
      if(!(field instanceof TextField) || field._parent != card || !field._visible)
      {
         continue;
      }
      format = field.text != "" ? field.getTextFormat(0, 1) : field.getNewTextFormat();
      lower = name.toLowerCase();
      if(lower.indexOf("enchant") >= 0 || lower.indexOf("description") >= 0 || lower.indexOf("effects") >= 0)
      {
         words = format;
      }
      else if((lower.indexOf("label") >= 0 || lower.indexOf("lable") >= 0) && field.text.length > 0 && field.text.length < 16)
      {
         label = format;
         // where the letters start, a label's field is often wider than its word and set right or centred
         edge = Equipment.partBox(field, card);
         edge = format.align == "right" ? edge.xMax - (edge.xMax - edge.xMin) * (field.textWidth + 4) / field._width : format.align == "center" ? (edge.xMin + edge.xMax) / 2 - (edge.xMax - edge.xMin) * (field.textWidth + 4) / field._width / 2 : edge.xMin;
         leftmost = leftmost == undefined ? edge : Math.min(leftmost, edge);
      }
      else if(format.bold && value == undefined)
      {
         value = format;
      }
   }
   letters = {left:leftmost != undefined ? leftmost : frame.xMin + 2 * Equipment.STATS_PAD * unit, label:label, value:value != undefined ? value : label};
   letters.labelSize = label != undefined ? label.size : Math.round(Equipment.CHEST_LABEL * unit);
   letters.valueSize = value != undefined ? Math.round(value.size * Equipment.CHEST_VALUE_SHARE) : Math.round(Equipment.CHEST_TEXT * unit);
   Equipment.letterLike(box.quiet, words != undefined ? words : label, words != undefined ? Math.round(words.size * Equipment.CHEST_QUIET_SHARE) : Math.round(Equipment.CHEST_QUIET * unit));
   return letters;
};

// The lines of numbers as html: a name in the card's label letters, a tab, the number in its value letters and its
// change against the worn one in brackets after it, in the smaller label size. Without numbers only the names, to
// measure their column
Equipment.cardRows = function(letters, names, values, changes)
{
   var rows = [];
   var change;
   var value;
   var i = 0;
   while(i < names.length)
   {
      change = changes != undefined ? changes[i] : undefined;
      value = "";
      if(values != undefined)
      {
         value = "\t" + Equipment.lettersTag(letters.value, letters.valueSize, Equipment.htmlSafe(String(values[i])), "value", change != undefined && change.text == undefined ? Equipment.inkOf(change.role) : undefined);
      }
      if(values != undefined && change != undefined && change.text != undefined)
      {
         value += " " + Equipment.lettersTag(letters.value, letters.labelSize, Equipment.htmlSafe(Equipment.bracketed(change)), "value", Equipment.inkOf(change.role));
      }
      rows.push(Equipment.lettersTag(letters.label, letters.labelSize, Equipment.htmlSafe(String(names[i]).toUpperCase()), "name") + value);
      i++;
   }
   return rows.join("<br>");
};

// A skin on paper writes its card in black whatever colour its fields keep (Dear Diary's say white), the panel's
// ink for the role stands in for the colour there. An ink given colours the words on any skin
Equipment.lettersTag = function(like, size, words, role, ink)
{
   var colour = ink != undefined ? ink : (like == undefined || Equipment.paper ? Equipment.inkOf(role) : like.color);
   if(like == undefined)
   {
      return "<font size='" + size + "'" + (ink != undefined ? " color='" + Equipment.hex(Number(ink)) + "'" : "") + ">" + words + "</font>";
   }
   return "<font face='" + like.font + "' size='" + size + "' color='" + Equipment.hex(Number(colour)) + "'>" + (like.bold ? "<b>" + words + "</b>" : words) + "</font>";
};

Equipment.htmlSafe = function(words)
{
   return words.split("&").join("&amp;").split("<").join("&lt;").split(">").join("&gt;");
};

Equipment.letterLike = function(field, like, size)
{
   var format;
   if(like == undefined)
   {
      return undefined;
   }
   format = new TextFormat(like.font, size, Equipment.paper ? Equipment.inkOf("quiet") : like.color, like.bold);
   format.align = "left";
   field.setNewTextFormat(format);
};

// the wheel over the card moves its numbers when they don't all fit, a few lines a notch
Equipment.scrollCardNumbers = function(delta)
{
   var box = Equipment.cardNumbers;
   var length;
   var bar;
   if(box == undefined || box.numbersTop == undefined)
   {
      return undefined;
   }
   if(delta != 0 && (!box._visible || box.scrollMost <= 0 || !Equipment.menu.itemCard.hitTest(_root._xmouse, _root._ymouse, false)))
   {
      return undefined;
   }
   box.scrolled = Math.max(0, Math.min(box.scrollMost, box.scrolled - delta * Equipment.CHEST_SCROLL_LINES * box.rowHigh));
   box.numbers._y = box.numbersTop - box.scrolled;
   box.bar.clear();
   if(box.scrollMost <= 0)
   {
      return undefined;
   }
   bar = Equipment.CHEST_BAR * box.unit;
   length = Math.max(box.rowHigh, box.shown * box.shown / (box.shown + box.scrollMost));
   box.bar.beginFill(Equipment.inkOf("edge"), 60);
   Equipment.roundBox(box.bar, box.right - bar - 2 * box.unit, box.numbersTop + (box.shown - length) * box.scrolled / box.scrollMost, bar, length, 1);
   box.bar.endFill();
};

// Where the card's words end, no lower than the parts that aren't words, in the viewer's units. A text field keeps
// its full height when its text is short or gone, and the numbers stood far under the last line of the card
Equipment.cardWordsBottom = function(otherwise, viewer)
{
   var card = Equipment.menu.itemCard;
   var shown = Equipment.shownParts();
   var bottom;
   var point;
   var i = 0;
   while(i < shown.length)
   {
      if(shown[i] != card.background)
      {
         if(shown[i].textHeight != undefined)
         {
            if(shown[i].text != "")
            {
               point = {x:shown[i]._x, y:shown[i]._y + shown[i].textHeight + 4};
               card.localToGlobal(point);
               viewer.globalToLocal(point);
               bottom = bottom == undefined ? point.y : Math.max(bottom, point.y);
            }
         }
         else
         {
            point = Equipment.partBox(shown[i], viewer);
            bottom = bottom == undefined ? point.yMax : Math.max(bottom, point.yMax);
         }
      }
      i++;
   }
   return bottom == undefined ? otherwise : bottom;
};

// the card's parts in sight in the panel's units, nothing while the card is faded out
Equipment.cardBox = function()
{
   var card = Equipment.menu.itemCard;
   var parts = Equipment.shownParts();
   var box;
   var one;
   var holder = card._parent;
   var i = 0;
   if(card == undefined || !card._visible || holder._visible == false || holder._alpha <= 0)
   {
      return undefined;
   }
   while(i < parts.length)
   {
      // the frame counts without the clear margin a skin's picture can have around it
      one = parts[i] == card.background ? Equipment.frameBox(card, Equipment.over) : Equipment.partBox(parts[i], Equipment.over);
      if(one == undefined)
      {
         i++;
         continue;
      }
      if(box == undefined)
      {
         box = {xMin:one.xMin, xMax:one.xMax, yMin:one.yMin, yMax:one.yMax};
      }
      else
      {
         box.xMin = Math.min(box.xMin, one.xMin);
         box.xMax = Math.max(box.xMax, one.xMax);
         box.yMin = Math.min(box.yMin, one.yMin);
         box.yMax = Math.max(box.yMax, one.yMax);
      }
      i++;
   }
   return box;
};

// for the test stand: the numbers in the card of a chest or a shop, where they are and what they say
Equipment.cardNumbersReport = function(note)
{
   var box = Equipment.cardNumbers;
   var card = Equipment.cardBox();
   var back = Equipment.menu.itemCard.background;
   skse.Log("PANELTEST card numbers " + note + ": " + (box._visible ? "numbers from " + Math.round(box.numbersTop) + " shown " + Math.round(box.shown) + " of " + Math.round(box.shown + box.scrollMost) + " scrolled " + Math.round(box.scrolled) + ", " + box.names.text.split("\r").join("/").split("\n").join("/") + " | " + box.quiet.text.split("\n").join("/") : "hidden") + ", frame " + Math.round(back._height) + (Equipment.menu.itemCard.equipmentFrameBottom != undefined ? " grown by " + Math.round(Equipment.menu.itemCard.equipmentFrameBottom._y - back._y) : back.equipmentGrid != undefined ? " grown by its grid from " + Math.round(back.equipmentGrid.high) : " as SkyUI's") + ", skin's frame " + Equipment.frameOfSkin + " " + Equipment.skinGrowth + ", limit " + Equipment.limitWhy + (card != undefined ? ", card " + Math.round(card.xMin) + "-" + Math.round(card.xMax) + " x " + Math.round(card.yMin) + "-" + Math.round(card.yMax) : ", no card") + ", laid out " + Equipment.number(Equipment.cardLayouts) + " times");
};

// for the test stand: SkyUI's frame and the pieces of a longer frame as they stand now
Equipment.cardFrameReport = function(note)
{
   var card = Equipment.menu.itemCard;
   var lines = [];
   var names = ["background", "equipmentFrameTop", "equipmentFrameFill", "equipmentFrameBottom"];
   var piece;
   var box;
   var i = 0;
   while(i < names.length)
   {
      piece = card[names[i]];
      if(piece == undefined)
      {
         lines.push(names[i] + " none");
      }
      else
      {
         box = piece.getBounds(card);
         lines.push(names[i] + " visible " + piece._visible + " alpha " + piece._alpha + " y " + Math.round(piece._y) + " yscale " + Math.round(piece._yscale) + " depth " + piece.getDepth() + " frame " + piece._currentframe + " of " + piece._totalframes + " bounds " + Math.round(box.yMin) + "-" + Math.round(box.yMax) + " own frame code " + (piece.onEnterFrame != undefined) + " grid " + (piece.scale9Grid != undefined ? Math.round(piece.scale9Grid.y) + "+" + Math.round(piece.scale9Grid.height) : "none") + " masked " + (piece._parent[names[i] + "Mask"] != undefined) + " parts" + Equipment.partsOf(piece));
      }
      i++;
   }
   skse.Log("PANELTEST card frame " + note + ": " + lines.join("; ") + "; hidden by us " + Equipment.cardBackHidden);
};

Equipment.partsOf = function(clip)
{
   var told = "";
   for(var name in clip)
   {
      if(typeof clip[name] == "movieclip" || clip[name] instanceof TextField)
      {
         told += " " + name + "(" + typeof clip[name] + (clip[name]._visible ? "" : " hidden") + ")";
      }
   }
   return told == "" ? " none" : told;
};

// for the test stand: every text field of the card with its font, size and colour, to match the numbers to the skin
Equipment.cardFieldsReport = function(note)
{
   var card = Equipment.menu.itemCard;
   var lines = [];
   var field;
   var format;
   for(var name in card)
   {
      field = card[name];
      if(field instanceof TextField && field._parent == card)
      {
         format = field.text != "" ? field.getTextFormat(0, 1) : field.getNewTextFormat();
         lines.push(name + (field._visible ? "" : "(hidden)") + " \"" + field.text.substr(0, 24) + "\" " + format.font + " " + format.size + " #" + Number(format.color).toString(16) + (format.bold ? " bold" : "") + " text #" + Number(field.textColor).toString(16) + " alpha " + field._alpha + " filters " + (field.filters != undefined ? field.filters.length : "none") + " blend " + field.blendMode);
      }
   }
   for(var inner in card)
   {
      if(card[inner] instanceof MovieClip && card[inner]._parent == card)
      {
         lines.push("clip " + inner + (card[inner]._visible ? "" : "(hidden)") + " colours " + Equipment.colourOf(card[inner]) + " blend " + card[inner].blendMode + Equipment.innerFields(card[inner]));
      }
   }
   skse.Log("PANELTEST card fields " + note + ": " + lines.join("; ") + "; card colours " + Equipment.colourOf(card) + ", holder " + Equipment.colourOf(card._parent) + ", card filters " + (card.filters != undefined ? card.filters.length : "none"));
};

Equipment.innerFields = function(clip)
{
   var told = "";
   for(var name in clip)
   {
      if(clip[name] instanceof TextField && clip[name]._parent == clip)
      {
         told += " [" + name + " \"" + clip[name].text.substr(0, 12) + "\" #" + Number(clip[name].textColor).toString(16) + "]";
      }
   }
   return told;
};

Equipment.colourOf = function(clip)
{
   var colour = clip.transform != undefined ? clip.transform.colorTransform : undefined;
   return colour == undefined ? "none" : colour.redMultiplier + "/" + colour.greenMultiplier + "/" + colour.blueMultiplier + "/" + colour.alphaMultiplier + " +" + colour.redOffset + "/" + colour.greenOffset + "/" + colour.blueOffset;
};
