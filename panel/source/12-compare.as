// How much the raw number of an item beats what's worn in its place, nothing when the two don't compare:
// arrows with no bow in the hands, a bow against a sword, a staff, a torch
Equipment.difference = function(entry, worn, flags)
{
   if(Equipment.wornByPlayer(entry, flags))
   {
      return undefined;
   }
   if(entry.formType == Equipment.ARMOR)
   {
      return Equipment.armorDifference(entry, worn);
   }
   if(entry.formType == Equipment.WEAPON)
   {
      return Equipment.weaponDifference(entry, worn);
   }
   if(entry.formType == Equipment.AMMO)
   {
      return Equipment.ammoDifference(entry, worn);
   }
   return undefined;
};

Equipment.armorDifference = function(entry, worn)
{
   var replaced = 0;
   var i = 0;
   while(i < worn.armor.length)
   {
      if(worn.armor[i].partMask & entry.partMask)
      {
         replaced += Equipment.number(worn.armor[i].infoArmor);
      }
      i++;
   }
   // clothes over an empty place change nothing and say so with the sign of an equal piece
   return Equipment.number(entry.infoArmor) - replaced;
};

Equipment.ranged = function(entry)
{
   return entry != undefined && (Equipment.isBow(entry) || Equipment.isCrossbow(entry));
};

Equipment.melee = function(entry)
{
   return entry != undefined && entry.formType == Equipment.WEAPON && !Equipment.ranged(entry) && entry.subType != Equipment.STAFF;
};

// the weapon in the hands a new one would be weighed against: a melee one for melee, a bow or crossbow for those
Equipment.heldLike = function(entry, worn)
{
   if(Equipment.ranged(entry))
   {
      return Equipment.ranged(worn.right) ? worn.right : undefined;
   }
   if(Equipment.melee(worn.right))
   {
      return worn.right;
   }
   return Equipment.melee(worn.left) ? worn.left : undefined;
};

Equipment.weaponDifference = function(entry, worn)
{
   if(entry.subType == Equipment.STAFF || entry.infoDamage == undefined || entry.infoDamage == null)
   {
      return undefined;
   }
   var held = Equipment.heldLike(entry, worn);
   if(held == undefined)
   {
      return undefined;
   }
   return Equipment.number(entry.infoDamage) - Equipment.number(held.infoDamage);
};

// the launcher an ammo needs: a bow shoots arrows, a crossbow bolts
Equipment.shoots = function(launcher, ammo)
{
   if(launcher == undefined || launcher.formType != Equipment.WEAPON)
   {
      return false;
   }
   return ammo.subType == Equipment.BOLT ? Equipment.isCrossbow(launcher) : Equipment.isBow(launcher);
};

// Ammo is weighed only while a weapon that shoots it is in the hands, against the worn ammo of that kind
Equipment.ammoDifference = function(entry, worn)
{
   if(entry.infoDamage == undefined || entry.infoDamage == null || !Equipment.shoots(worn.right, entry))
   {
      return undefined;
   }
   var current = worn.ammo != undefined && worn.ammo.subType == entry.subType ? Equipment.number(worn.ammo.infoDamage) : 0;
   return Equipment.number(entry.infoDamage) - current;
};
