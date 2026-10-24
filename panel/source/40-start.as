// The plugin loads this file into a clip of the menu while the menu sets itself up, so the menu
// may not be there yet on the first frame
Equipment.findMenu = function()
{
   var menu = _root.Menu_mc;
   if(menu == undefined || menu.inventoryLists == undefined || menu.inventoryLists.itemList == undefined)
   {
      return false;
   }
   Equipment.attach(menu);
   return true;
};

if(!Equipment.findMenu())
{
   this.onEnterFrame = function()
   {
      if(Equipment.findMenu())
      {
         delete this.onEnterFrame;
      }
   };
}
