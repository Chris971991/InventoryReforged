// Runs the comparison rules and the subcategory chips of the panel outside the game
//   node scripts/interface/equipment/tests/logic.test.cjs
// The parts are plain enough script to run in node with a few stand-ins for SkyUI and the menu

const fs = require("fs");
const path = require("path");
const vm = require("vm");

const source = path.join(__dirname, "..", "source");
const parts = ["10-core.as", "12-compare.as", "20-rows.as", "21-marks.as", "22-sorting.as", "24-subcategories.as", "26-draw.as", "27-queue.as", "28-junk.as", "28-junk-ask.as", "29-chip-menu.as", "30-panel.as", "31-strip.as", "33-context.as", "34-drag.as", "35-models.as", "36-window.as", "37-inspect.as", "38-stats.as", "38-hero.as", "38-look.as", "39-row-menu.as", "39-gamepad.as", "39-card-numbers.as", "39-companions.as", "39-magic.as"];
// PANEL_CALLS=1 runs the same tests on the parts the way the debug build puts them in the movie, with a call at the
// start and every way out of each function. They have to pass all the same, that's the proof the pass keeps what
// the panel does
const withCalls = process.env.PANEL_CALLS === "1" ? require("./calls.cjs").panel(parts.map((part) => path.join(source, part))) : undefined;

function freshPanel(sides, words, stage) {
   const context = {
      _global: {},
      skse: {plugins: {}},
      // what the game shows of the stage of a 1280x720 menu that keeps its proportions, unless a test gives another
      Stage: stage || {visibleRect: {x: 0, y: 0, width: 1280, height: 720}, safeRect: {x: 0, y: 0}},
      Math, String, Number, Array,
      getTimer: () => Date.now(),
      MovieClip: function() {},
      Mouse: {addListener: () => undefined},
      _root: {_xmouse: 0, _ymouse: 0},
      skyui: {util: {Translator: {translate: (key) => words && words[key] !== undefined ? words[key] : key}}, defines: {Input: {XButton: {name: "XButton"}}}},
      // what gfx.managers.InputDelegate of SkyUI names the moves and buttons
      gfx: {ui: {NavigationCode: {UP: "up", DOWN: "down", LEFT: "left", RIGHT: "right", ENTER: "enter-gamepad_A", ESCAPE: "escape-gamepad_B", TAB: "tab", SHIFT_TAB: "shifttab"}}},
      Shared: {GlobalFunc: {IsKeyPressed: (details, held) => details.value == "keyDown" || (held === undefined || held) && details.value == "keyHold"}},
   };
   context._global.EquipmentPanelMenu = "inventory";
   vm.createContext(context);
   if (withCalls) {
      withCalls.load(context);
   } else {
      for (const part of parts) {
         vm.runInContext(fs.readFileSync(path.join(source, part), "utf8"), context, {filename: part});
      }
   }
   const Equipment = context._global.Equipment;
   // the stand-ins a test changes on the way: the pointer, the clock, what lies under the pointer
   freshPanel.context = context;
   const categories = sides ? [{flag: 2048}, {flag: 4096}, {flag: 0}, {flag: 2}, {flag: 4}] : [{flag: 1}, {flag: 1023}, {flag: 2}, {flag: 4}];
   Equipment.menu = {inventoryLists: {
      categoryList: {entryList: categories, dividerIndex: sides ? 2 : -1},
      _typeFilter: {isMatch: (entry, flag) => entry.side === undefined || entry.side === flag},
   }};
   return Equipment;
}

let failed = 0;
let passed = 0;
function expect(name, got, wanted) {
   const same = wanted === undefined ? got === undefined : Math.abs(got - wanted) < 1e-6;
   if (same) {
      passed++;
   } else {
      failed++;
      console.log(`FAIL ${name}: got ${got}, wanted ${wanted}`);
   }
}

const W = {WEAPON: 41, ARMOR: 26, AMMO: 42, LIGHT: 31};
const sword = (damage, worn) => ({formType: W.WEAPON, subType: 1, infoDamage: damage, equipState: worn || 0});
const dagger = (damage, worn) => ({formType: W.WEAPON, subType: 2, infoDamage: damage, equipState: worn || 0});
const greatsword = (damage, worn) => ({formType: W.WEAPON, subType: 5, infoDamage: damage, equipState: worn || 0});
const bow = (damage, worn) => ({formType: W.WEAPON, subType: 8, infoDamage: damage, equipState: worn || 0});
const crossbow = (damage, worn) => ({formType: W.WEAPON, subType: 9, infoDamage: damage, equipState: worn || 0});
const staff = (worn) => ({formType: W.WEAPON, subType: 10, infoDamage: null, equipState: worn || 0});
const arrow = (damage, worn) => ({formType: W.AMMO, subType: 0, infoDamage: damage, equipState: worn || 0});
const bolt = (damage, worn) => ({formType: W.AMMO, subType: 1, infoDamage: damage, equipState: worn || 0});
const armor = (mask, rating, worn, extra) => Object.assign({formType: W.ARMOR, partMask: mask, infoArmor: rating, equipState: worn || 0}, extra || {});
const torch = (worn) => ({formType: W.LIGHT, equipState: worn || 0});

function difference(rows, item) {
   const Equipment = freshPanel(false);
   const all = rows.concat([item]);
   return Equipment.difference(item, Equipment.worn(all, undefined), undefined);
}

// ammo counts only while something that shoots it is in the hands
expect("arrow, empty hands", difference([], arrow(10)), undefined);
expect("arrow, sword in hand", difference([sword(8, 3)], arrow(10)), undefined);
expect("arrow, bow and no arrows worn", difference([bow(9, 3)], arrow(10)), 10);
expect("arrow, bow and iron arrows worn", difference([bow(9, 3), arrow(8, 1)], arrow(10)), 2);
expect("arrow, bow and bolts worn", difference([bow(9, 3), bolt(12, 1)], arrow(10)), 10);
expect("bolt, bow in hand", difference([bow(9, 3)], bolt(12)), undefined);
expect("bolt, crossbow and arrows worn", difference([crossbow(19, 3), arrow(8, 1)], bolt(12)), 12);
expect("bolt, crossbow and bolts worn", difference([crossbow(19, 3), bolt(10, 1)], bolt(12)), 2);
expect("arrow, crossbow in hand", difference([crossbow(19, 3)], arrow(10)), undefined);
expect("arrow, a bow in the left only is not how bows are held", difference([bow(9, 2)], arrow(10)), undefined);

// a bow against a bow, a melee weapon against a melee weapon
expect("bow, sword in hand", difference([sword(8, 3)], bow(12)), undefined);
expect("bow, bow in hand", difference([bow(9, 3)], bow(12)), 3);
expect("crossbow, bow in hand", difference([bow(9, 3)], crossbow(19)), 10);
expect("sword, bow in hand", difference([bow(9, 3)], sword(10)), undefined);
expect("sword, empty hands", difference([], sword(10)), undefined);
expect("sword, sword in right", difference([sword(8, 3)], sword(10)), 2);
expect("sword, dagger in left only", difference([dagger(4, 2)], sword(10)), 6);
expect("greatsword, sword in right", difference([sword(8, 3)], greatsword(18)), 10);
expect("sword, greatsword in both", difference([greatsword(18, 4)], sword(10)), -8);
expect("sword, staff in right and dagger in left", difference([staff(3), dagger(4, 2)], sword(10)), 6);
expect("sword, only a staff", difference([staff(3)], sword(10)), undefined);
expect("staff, sword in hand", difference([sword(8, 3)], staff()), undefined);
expect("torch", difference([sword(8, 3)], torch()), undefined);
expect("worn row", difference([], sword(10, 3)), undefined);

// armor against what it would replace by slot mask
expect("helmet over helmet", difference([armor(1, 15, 1)], armor(1, 20)), 5);
expect("helmet over nothing", difference([], armor(1, 20)), 20);
expect("cuirass over cuirass", difference([armor(4, 30, 1), armor(1, 15, 1)], armor(4, 25)), -5);
expect("circlet with no armor over a helmet that takes both head bits", difference([armor(1 | 4096, 15, 1)], armor(4096, 0)), -15);
expect("ring over nothing, no armor either side", difference([], armor(64, 0)), 0);
expect("shield over shield", difference([armor(512, 20, 1)], armor(512, 24)), 4);
expect("clothes over armor", difference([armor(4, 30, 1)], armor(4, 0)), -30);

// in a container only the player's side counts as worn
{
   const Equipment = freshPanel(true);
   const players = armor(1, 15, 1, {side: 2});
   const bodys = armor(1, 30, 1, {side: 2048});
   const shown = armor(1, 22, 0, {side: 2048});
   const rows = [players, bodys, shown];
   const flags = Equipment.ownFlags();
   const worn = Equipment.worn(rows, flags);
   expect("container, the body's helmet is not worn by the player", worn.armor.length, 1);
   expect("container, a helmet of the body is compared with the player's", Equipment.difference(bodys, worn, flags), 15);
   expect("container, the player's own worn row has no mark", Equipment.difference(players, worn, flags), undefined);
   expect("container, a helmet of the chest", Equipment.difference(shown, worn, flags), 7);
}

// chips
{
   const Equipment = freshPanel(false);
   const holds = (key, entry) => Equipment.chipHolds(key, entry) ? 1 : 0;
   expect("chip one handed, sword", holds("oneHanded", sword(8)), 1);
   expect("chip one handed, staff is not", holds("oneHanded", staff()), 0);
   expect("chip one handed, greatsword is not", holds("oneHanded", greatsword(18)), 0);
   expect("chip two handed, greatsword", holds("twoHanded", greatsword(18)), 1);
   expect("chip bows, crossbow", holds("ranged", crossbow(19)), 1);
   expect("chip staves, staff", holds("staff", staff()), 1);
   expect("chip ammo, bolt", holds("ammo", bolt(10)), 1);
   expect("chip light, light helmet", holds("light", armor(1, 10, 0, {weightClass: 0})), 1);
   expect("chip light, light shield is a shield", holds("light", armor(512, 10, 0, {weightClass: 0})), 0);
   expect("chip heavy, heavy cuirass", holds("heavy", armor(4, 30, 0, {weightClass: 1})), 1);
   expect("chip clothing", holds("clothing", armor(4, 0, 0, {weightClass: 3})), 1);
   expect("chip jewelry", holds("jewelry", armor(64, 0, 0, {weightClass: 4})), 1);
   expect("chip shields", holds("shield", armor(512, 20, 0, {weightClass: 1})), 1);
   expect("chip head, helmet", holds("head", armor(1, 10, 0)), 1);
   expect("chip head, circlet", holds("head", armor(4096, 0, 0)), 1);
   expect("chip feet, boots", holds("feet", armor(128, 5, 0)), 1);
   expect("chip hands, gauntlets", holds("hands", armor(8, 5, 0)), 1);
   expect("chip body, cuirass", holds("body", armor(4, 30, 0)), 1);
   expect("chip other, cloak", holds("other", armor(65536, 0, 0)), 1);
   expect("chip other, ring", holds("other", armor(64, 0, 0)), 1);
   expect("chip body, shield is not", holds("body", armor(512, 20, 0)), 0);

   // two rows add up
   Equipment.chosenChips = ["heavy", "head"];
   const rows = [armor(1, 20, 0, {weightClass: 1}), armor(1, 10, 0, {weightClass: 0}), armor(4, 30, 0, {weightClass: 1})];
   Equipment.chipFilter.applyFilter(rows);
   expect("heavy and head leave one heavy helmet", rows.length, 1);
   expect("the one left is the heavy helmet", rows[0].infoArmor, 20);
   Equipment.chosenChips = [undefined, "body"];
   const more = [armor(1, 20, 0, {weightClass: 1}), armor(4, 30, 0, {weightClass: 1}), armor(4, 0, 0, {weightClass: 3})];
   Equipment.chipFilter.applyFilter(more);
   expect("only the second row: every body piece", more.length, 2);

   // the strip takes a row's height from the list, the rows that fit are counted again (SkyUI counts them only when
   // the columns are laid out)
   const list = {_y: 100, listHeight: 600, entryHeight: 30, _maxListIndex: 20, requestUpdate() {}};
   Equipment.menu.inventoryLists.itemList = list;
   Equipment.menu.updateDynamicListHeight = () => { list.listHeight = 700 - list._y; };
   Equipment.roomHigh = 0;
   Equipment.openStrip(true);
   expect("the strip moves the list down", list._y, 100 + Equipment.STRIP_HIGH);
   expect("one row less fits under the strip", list._maxListIndex, 19);
   Equipment.openStrip(false);
   expect("the row comes back when the strip goes", list._maxListIndex, 20);
   Equipment.menu.updateDynamicListHeight = undefined;
   Equipment.openStrip(true);
   expect("a menu that can't fit its list is cut by hand", list.listHeight, 570);
   expect("and counts its rows too", list._maxListIndex, 19);
   delete Equipment.roomHigh;
   list._y = 100;
   list.listHeight = 600;
   list._maxListIndex = 20;
   Equipment.openStrip(false);
   expect("a menu that opens with no strip keeps its height", list.listHeight, 600);
   expect("and its rows", list._maxListIndex, 20);
   delete Equipment.menu.updateDynamicListHeight;
   delete Equipment.menu.inventoryLists.itemList;

   // slot fit
   const left = Equipment.slotNamed("left");
   expect("left takes a dagger", Equipment.fits(dagger(4), left) ? 1 : 0, 1);
   expect("left takes a shield", Equipment.fits(armor(512, 20), left) ? 1 : 0, 1);
   expect("left takes a torch", Equipment.fits(torch(), left) ? 1 : 0, 1);
   expect("left takes no greatsword", Equipment.fits(greatsword(18), left) ? 1 : 0, 0);
   expect("left takes no bow", Equipment.fits(bow(9), left) ? 1 : 0, 0);
   expect("left takes a staff", Equipment.fits(staff(), left) ? 1 : 0, 1);
   expect("left takes a pickaxe", Equipment.fits({formType: W.WEAPON, subType: 11}, left) ? 1 : 0, 1);
   expect("right takes a bow", Equipment.fits(bow(9), Equipment.slotNamed("right")) ? 1 : 0, 1);
   expect("head takes a circlet", Equipment.fits(armor(4096, 0), Equipment.slotNamed("head")) ? 1 : 0, 1);
   expect("ammo takes a bolt", Equipment.fits(bolt(10), Equipment.slotNamed("ammo")) ? 1 : 0, 1);
}

// weapon numbers against the weapon in the hands, the pairs of the row arrows: melee with melee, a bow with a bow
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const stats = {
      1: {kind: "weapon", speed: 1, speedMult: 1, leftSpeedMult: 1, reach: 1, stagger: 0.75, critDamage: 0},
      2: {kind: "weapon", speed: 1.3, speedMult: 1, leftSpeedMult: 1, reach: 0.7, stagger: 0.75, critDamage: 5},
      3: {kind: "weapon", speed: 1, speedMult: 1, leftSpeedMult: 1, reach: 1, stagger: 0, critDamage: 3},
   };
   Equipment.plugin = () => ({Stats: (formId) => stats[formId]});
   const lineOf = (lines, name) => {
      const line = lines.find((one) => one[0] === name);
      return line === undefined ? "missing" : line[1] + (line[2] ? " " + line[2].role + " " + Equipment.bracketed(line[2]) : "");
   };
   const held = Object.assign(sword(8, 1), {formId: 2});
   const longer = Object.assign(sword(10), {formId: 1});
   Equipment.wornNow = {armor: [], right: held};
   const lines = Equipment.statsOf(longer);
   said("a slower sword lands later", lineOf(lines, "Swing"), "0.7 s worse (+0.16)");
   said("the faster held sword hits more a second", lineOf(lines, "DPS"), "~8.3 worse (-0.3)");
   said("a longer blade reaches farther", lineOf(lines, "Reach"), "1.00 better (+0.3)");
   said("the same stagger is equal", lineOf(lines, "Stagger"), "0.75 equal (=)");
   said("crit damage the held one has and this one lacks", lineOf(lines, "Crit damage"), "0 worse (-5)");

   said("the sword in the hands is weighed against nothing", lineOf(Equipment.statsOf(held), "Reach"), "0.7");
   const longbow = Object.assign(bow(9), {formId: 3});
   said("a bow against a sword in the hands says no change", lineOf(Equipment.statsOf(longbow), "Draw"), "1.00 s");
   said("a change goes into brackets in its ink", Equipment.valueHtml("1.00", Equipment.change(0.3, 2, 1)), "<font color='" + Equipment.hex(Equipment.inkOf("value")) + "'>1.00</font> <font color='" + Equipment.hex(Equipment.inkOf("better")) + "'>(+0.3)</font>");

   // the worn arrows count for the held bow only, a crossbow's damage a second would lose to them
   Equipment.wornNow = {armor: [], right: Object.assign(bow(9, 1), {formId: 3}), ammo: Object.assign(arrow(10, 1), {text: "Iron Arrow"})};
   const heavyBow = Object.assign(crossbow(19), {formId: 3});
   said("a crossbow against a bow with arrows says no change of its damage a second", lineOf(Equipment.statsOf(heavyBow), "DPS"), "~7.6");
   said("but still of its draw", lineOf(Equipment.statsOf(heavyBow), "Draw"), "1.00 s equal (=)");
   said("another bow counts the same arrows", lineOf(Equipment.statsOf(Object.assign(bow(12), {formId: 3})), "DPS (with Iron Arrow)"), "~12.2 better (+1.7)");
}

// a body's worn things are things to take, weighed against what the player wears
{
   const Equipment = freshPanel(true);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const told = {armorScale: 0.12, armorBase: 0.03, armorCap: 80};
   Equipment.kind = "container";
   Equipment.subject = {player: true};
   const mine = armor(4, 27, 1, {weightClass: 1, side: 2});
   const onBody = armor(4, 40, 1, {weightClass: 1, side: 2048});
   Equipment.wornNow = {armor: [mine]};
   const lines = Equipment.armorStats(onBody, told);
   said("a body's cuirass says what it would stop", lines[0][0], "Would stop");
   said("in place of the player's one", lines[0][1], "1.6% of a hit");
   said("in the ink of better", lines[0][2].role, "better");
   said("the player's own cuirass still says what it stops", Equipment.armorStats(mine, told)[0][0], "Stops");
}

// a list that scrolls smoothly keeps its rows one clip deeper, the row under the pointer is still found
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const list = Equipment.menu.inventoryLists.itemList = {entryList: [{text: "Iron Sword"}, {text: "Steel Armor"}]};
   const holder = {_parent: list};
   const row = {_parent: holder, itemIndex: 1, _visible: true};
   freshPanel.context.Mouse.getTopMostEntity = () => ({_parent: row});
   said("a row inside the list's own holder", Equipment.rowUnderMouse().text, "Steel Armor");
   row._parent = list;
   said("a row right in the list", Equipment.rowUnderMouse().text, "Steel Armor");
   row._parent = {};
   said("a clip of another list is no row", Equipment.rowUnderMouse(), undefined);
}

// a line too long for its column is cut with dots, one that fits stays whole
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const letters = (words) => words.length * 10;
   said("a short line stays", Equipment.cutToFit("DPS", 100, letters), "DPS");
   said("a long one is cut to fit with its dots", Equipment.cutToFit("DPS (with Ancient Nord Arrow)", 100, letters), "DPS (wi...");
   said("an endless room cuts nothing", Equipment.cutToFit("DPS (with Ancient Nord Arrow)", Infinity, letters), "DPS (with Ancient Nord Arrow)");

   // the tabs' names share the doll's room, the short ones keep their width
   const pads = 2 * Equipment.COMPANION_PAD + Equipment.COMPANION_GAP;
   said("names that fit have no limit", Equipment.namesRoom([30, 60, 60], 500), Infinity);
   said("long names share what the short one leaves", Equipment.namesRoom([30, 200, 200], 300 - Equipment.COMPANION_GAP + 3 * pads), 135);
}

// armor numbers: rating times the scale plus a share for each worn piece of armor, up to the cap
{
   const Equipment = freshPanel(false);
   const told = {armorScale: 0.12, armorBase: 0.03, armorCap: 80};
   const heavy = (mask, rating, worn) => armor(mask, rating, worn, {weightClass: 1});
   const percentOf = (lines, name) => parseFloat(lines.find((line) => line[0] === name)[1]);
   const four = [heavy(4, 300, 1), heavy(1, 100, 1), heavy(8, 84, 1), heavy(128, 83, 1)];
   Equipment.wornNow = {armor: four};
   expect("567 with four pieces is the cap", percentOf(Equipment.armorStats(four[0], told), "Whole set"), 80);

   const set = [heavy(4, 27, 1), heavy(1, 16, 1), heavy(8, 8, 1), heavy(128, 11, 1)];
   Equipment.wornNow = {armor: set};
   const worn = Equipment.armorStats(set[0], told);
   expect("62 rating and four pieces stop 19.4%", percentOf(worn, "Whole set"), 19.4);
   expect("a worn cuirass adds its rating and its piece", percentOf(worn, "Stops"), 6.2);

   const ring = armor(64, 0, 0, {weightClass: 4});
   expect("a ring with no armor says nothing", Equipment.armorStats(ring, told).length, 0);

   const better = heavy(4, 40, 0);
   expect("a cuirass in place of the worn one adds only the rating it has more", percentOf(Equipment.armorStats(better, told), "Would stop"), 1.6);

   const shield = armor(512, 20, 0, {weightClass: 1});
   expect("a shield into an empty hand adds a piece too", percentOf(Equipment.armorStats(shield, told), "Would stop"), 20 * 0.12 + 3);

   const worse = heavy(4, 20, 0);
   expect("a worse cuirass says how much less it would stop", percentOf(Equipment.armorStats(worse, told), "Would stop"), -0.8);

   // a piece that isn't worn colours its change, a worn one says what it adds without a colour
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const roleOf = (lines, name) => {
      const line = lines.find((one) => one[0] === name);
      return line && line[2] ? line[2].role + (line[2].text !== undefined ? " " + line[2].text : "") : "none";
   };
   const lighter = armor(4, 27, 0, {weightClass: 1, infoWeight: 15});
   set[0].infoWeight = 30;
   const lighterLines = Equipment.armorStats(lighter, told);
   said("the same rating stops the same", roleOf(lighterLines, "Would stop"), "equal");
   said("half the weight doubles the armor per weight", roleOf(lighterLines, "Armor per weight"), "better +0.9");
   said("a better cuirass reads better", roleOf(Equipment.armorStats(heavy(4, 40, 0), told), "Would stop"), "better");
   said("a worse cuirass reads worse", roleOf(Equipment.armorStats(worse, told), "Effective health"), "worse");
   said("a worn cuirass has no colour", roleOf(Equipment.armorStats(set[0], told), "Stops"), "none");

   // a slot filter puts chosen chips aside, a mod's piece without keywords still finds its chip
   Equipment.chosenChips = ["light"];
   Equipment.activeSlot = Equipment.slotNamed("ring");
   const ringRows = [armor(64, 0, 0, {weightClass: 4}), armor(64, 0, 0)];
   Equipment.chipFilter.applyFilter(ringRows);
   expect("a slot filter keeps rows a chip would drop", ringRows.length, 2);
   Equipment.activeSlot = undefined;
   expect("a ring without keywords counts as jewelry", Equipment.chipHolds("jewelry", armor(64, 0, 0)) ? 1 : 0, 1);
   expect("a robe without keywords counts as clothing", Equipment.chipHolds("clothing", armor(4, 0, 0)) ? 1 : 0, 1);
   expect("a ring without keywords is no clothing", Equipment.chipHolds("clothing", armor(64, 0, 0)) ? 1 : 0, 0);
}

// subcategories of potions, food, books and things, by the kinds SkyUI tells
{
   const Equipment = freshPanel(false);
   const holds = (key, entry) => Equipment.chipHolds(key, entry) ? 1 : 0;
   const potion = (subType) => ({formType: 46, subType});
   const book = (subType) => ({formType: 27, subType});
   const thing = (subType) => ({formType: 32, subType});
   expect("a health potion is health", holds("health", potion(0)), 1);
   expect("a health potion is no other", holds("otherPotion", potion(0)), 0);
   expect("a regeneration of magicka is magicka", holds("magicka", potion(4)), 1);
   expect("a fire resistance is other", holds("otherPotion", potion(9)), 1);
   expect("a poison is a poison", holds("poison", potion(15)), 1);
   expect("a poison is no other", holds("otherPotion", potion(15)), 0);
   expect("mead is a drink", holds("drink", potion(13)), 1);
   expect("bread is food", holds("meal", potion(14)), 1);
   expect("a note is a note", holds("note", book(1)), 1);
   expect("a recipe counts as a note", holds("note", book(2)), 1);
   expect("a note is no book", holds("book", book(1)), 0);
   expect("a plain book is a book", holds("book", book(undefined)), 1);
   expect("a spell tome", holds("spellTome", book(0)), 1);
   expect("a treasure map", holds("map", book(3)), 1);
   expect("iron ore", holds("ore", thing(31)), 1);
   expect("an ingot", holds("ingot", thing(7)), 1);
   expect("a ruby", holds("gem", thing(0)), 1);
   expect("a pelt", holds("pelt", thing(5)), 1);
   expect("leather goes with pelts", holds("pelt", thing(3)), 1);
   expect("a soul gem", holds("soulGem", {formType: 52}), 1);
   expect("a soul gem is no other", holds("otherMisc", {formType: 52}), 0);
   expect("clutter is other", holds("otherMisc", thing(19)), 1);
   expect("a torch in misc is other", holds("otherMisc", torch()), 1);
   const oldMenu = (made, formId) => Object.assign(made, {formId});
   expect("iron ore an older menu calls an ingot", holds("ore", oldMenu(thing(7), 0x71CF3)), 1);
   expect("and it is no ingot then", holds("ingot", oldMenu(thing(7), 0x71CF3)), 0);
   expect("a real ingot stays one", holds("ingot", oldMenu(thing(7), 0x5ACE4)), 1);
   expect("a treasure map an older menu calls a note", holds("map", oldMenu(book(1), 996303)), 1);
   expect("and it is no note then", holds("note", oldMenu(book(1), 996303)), 0);
   expect("a torch is no ore", holds("ore", torch()), 0);
   const family = (flag) => Equipment.chipFamily(flag) || "none";
   expect("potions of a container", family(8192) === "potions" ? 1 : 0, 1);
   expect("books of a container", family(131072) === "books" ? 1 : 0, 1);
   expect("misc of the inventory", family(512) === "misc" ? 1 : 0, 1);
   expect("food of a container", family(32768) === "food" ? 1 : 0, 1);
   expect("all has none", family(1023) === "none" ? 1 : 0, 1);
   expect("all of a container has none", family(1047552) === "none" ? 1 : 0, 1);
}

// under a slot filter only the subcategories that split what fits the slot are offered
{
   const Equipment = freshPanel(false);
   const helmets = [armor(1, 20, 0, {weightClass: 1}), armor(1, 12, 0, {weightClass: 0}), armor(2, 0, 0, {weightClass: 3}), armor(4, 30, 0, {weightClass: 1})];
   Equipment.menu.inventoryLists.itemList = {entryList: helmets};
   Equipment.menu.inventoryLists.categoryList.selectedEntry = {flag: 4};
   Equipment.activeSlot = Equipment.slotNamed("head");
   const splits = (key) => Equipment.splitsSlot(key) ? 1 : 0;
   expect("heavy splits the helmets", splits("heavy"), 1);
   expect("clothing splits the helmets", splits("clothing"), 1);
   expect("head holds every helmet and splits nothing", splits("head"), 0);
   expect("jewelry holds no helmet", splits("jewelry"), 0);
   expect("the cuirass doesn't count for the head", splits("body"), 0);
}

// the menu of a row offers what the menu it's in can do with the row
{
   const menuOf = (kind, side, charge) => {
      const Equipment = freshPanel(false);
      Equipment.kind = kind;
      Equipment.settings = {};
      Equipment.viewer = kind == "inventory" ? {} : undefined;
      Equipment.inspectable = () => true;
      Equipment.menu.itemCard = {itemInfo: {charge}};
      Equipment.menu.isViewingContainer = () => side;
      Equipment.menu.isViewingVendorItems = () => side;
      return (entry) => Equipment.rowLines(entry).filter((line) => line.key !== "-").map((line) => line.words).join(" | ");
    };
   const said = (name, got, wanted) => {
      if (got === wanted) {
         passed++;
      } else {
         failed++;
         console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`);
      }
   };
   const counted = (entry, count) => Object.assign(entry, {count});
   const potion = (count) => ({formType: 46, count, equipState: 0});
   said("inventory, a sword", menuOf("inventory")(counted(sword(8), 1)), "Equip | Equip in left hand | Drop | Add to favorites | Inspect");
   said("inventory, a sword in the left hand", menuOf("inventory")(counted(sword(8, 2), 1)), "Take off | Drop | Add to favorites | Inspect");
   said("inventory, a greatsword has no left hand", menuOf("inventory")(counted(greatsword(18), 1)), "Equip | Drop | Add to favorites | Inspect");
   said("inventory, five potions", menuOf("inventory")(potion(5)), "Use | Drop one | Drop all (5) | Drop some... | Add to favorites | Inspect");
   said("inventory, an ingredient is eaten", menuOf("inventory")({formType: 30, count: 1, equipState: 0}), "Eat | Drop | Add to favorites | Inspect");
   said("inventory, a book is read", menuOf("inventory")({formType: 27, count: 1, equipState: 0}), "Read | Drop | Add to favorites | Inspect");
   said("inventory, a worn favorite helmet", menuOf("inventory")(Object.assign(armor(1, 16, 1), {count: 1, favorite: true})), "Take off | Drop | Remove from favorites | Inspect");
   said("inventory, a drained sword can be recharged", menuOf("inventory", false, 40)(counted(sword(8), 1)), "Equip | Equip in left hand | Recharge | Drop | Add to favorites | Inspect");
   said("container side, two arrows", menuOf("container", true)(counted(arrow(8), 2)), "Take one | Take all (2)");
   said("own side of a container, a greatsword", menuOf("container", false)(counted(greatsword(18), 1)), "Equip | Store | Add to favorites");
   said("a shop's three potions", menuOf("barter", true)(potion(3)), "Buy one | Buy all (3) | Buy some...");
   said("own side of a shop, a sword", menuOf("barter", false)(counted(sword(8), 1)), "Sell");
}

// a window or a menu of a slot opens beside it toward the doll, from either column
{
   const Equipment = freshPanel();
   const named = (column, x, words) => ({column, box: {_x: x, label: {textWidth: words}}});
   const left = named(0, 880, 30);
   const right = named(1, 1780, 40);
   Equipment.SLOTS = [left, named(0, 880, 120), right, named(1, 1780, 50)];
   const size = Equipment.SLOT_SIZE;
   expect("left slot, beside it on the right, past the widest name", Equipment.besideSlot(left, 300), 880 + 124 + Equipment.POPUP_GAP);
   expect("right slot, beside it on the left", Equipment.besideSlot(right, 300), 1780 - Equipment.POPUP_GAP - 300);
   Equipment.SLOTS.push(named(1, 1780, 150));
   expect("right slot, past a long name of its column", Equipment.besideSlot(right, 300), 1780 + size - 154 - Equipment.POPUP_GAP - 300);
   Equipment.SLOTS.push(named(1, 1780, 400));
   expect("a name is never counted wider than its field", Equipment.besideSlot(right, 300), 1780 + size - Equipment.LABEL_WIDTH - Equipment.POPUP_GAP - 300);
}

// the pieces of a slot keep one order whatever the list's sort, so the slot shows the same one
{
   const Equipment = freshPanel(false);
   const piece = (form, bits, extra) => Object.assign(armor(bits.reduce((mask, bit) => mask | (1 << (bit - 30)), 0), 0, 1, {formId: form, text: "f" + form}), extra || {});
   const order = (rows, name) => {
      Equipment.place(Equipment.worn(rows, undefined));
      return Equipment.slotNamed(name).entries.map((entry) => entry.formId).join(",");
   };
   const bag = piece(1, [47]);
   const cloak = piece(2, [46]);
   const lamp = piece(3, [60]);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   said("other, a cloak first, then by body slot", order([lamp, bag, cloak], "other"), "2,1,3");
   said("other, the same order from another sort", order([bag, cloak, lamp], "other"), "2,1,3");
   said("head, the hood before the circlet", order([piece(5, [42]), piece(4, [31, 41])], "head"), "4,5");
   const real = piece(10, [36], {myPreciousesVanillaRingSlotEquipped: true});
   const finger = (form) => piece(form, [36], {myPreciousesVanillaRingSlotEquipped: false, equipState: 2});
   said("rings, the game's own finger first", order([finger(7), real, finger(6)], "ring"), "10,6,7");
   expect("a ring on a virtual finger", Equipment.virtualRing(finger(6)) ? 1 : 0, 1);
   expect("a ring without My Precious...es", Equipment.virtualRing(piece(8, [36])) ? 1 : 0, 0);
   expect("an unworn ring My Precious...es marked", Equipment.virtualRing(piece(9, [36], {myPreciousesVanillaRingSlotEquipped: false, equipState: 0})) ? 1 : 0, 0);

   // MARA takes the slots off jewelry, the keywords still place it
   const maraRing = (form) => armor(0, 0, 1, {formId: form, text: "r" + form, keywords: {ArmorJewelry: 1, gearMaraRing: 1}});
   const maraAmulet = (form) => armor(0, 0, 1, {formId: form, text: "a" + form, keywords: {ArmorJewelry: 1, gearMaraAmulet: 1}});
   said("MARA rings go to the ring slot", order([maraRing(21), maraRing(20), lamp], "ring"), "20,21");
   said("MARA amulets go to the neck slot", order([maraAmulet(31), maraAmulet(30)], "amulet"), "30,31");
   said("a slotless necklace of a mod by its keyword", order([armor(0, 0, 1, {formId: 40, keywords: {ClothingNecklace: 1}})], "amulet"), "40");
   said("other keeps only the lamp", order([maraRing(21), lamp], "other"), "3");
   const enchantableCloak = piece(41, [40, 46], {keywords: {ClothingNecklace: 1}});
   said("a cloak with the necklace keyword stays in other", order([enchantableCloak, maraAmulet(30)], "other"), "41");
   said("and the neck holds only the amulet", order([enchantableCloak, maraAmulet(30)], "amulet"), "30");
   let hung = [];
   Equipment.plugin = () => ({HungPieces: () => hung});
   Equipment.hiddenMask = 0;
   Equipment.hiddenPieces = [];
   Equipment.settings = {hideIcons: true, pieceStrips: true};
   order([maraRing(21), maraRing(20)], "ring");
   expect("a ring slot of MARA rings never seen hanging has no eye", Equipment.canToggle(Equipment.slotNamed("ring")) ? 1 : 0, 0);
   expect("nor a MARA ring in a strip", Equipment.canToggle({parent: Equipment.slotNamed("ring"), entries: [maraRing(20)], name: "ring"}) ? 1 : 0, 0);
   hung = [20];
   expect("once its copy was seen the slot has an eye", Equipment.canToggle(Equipment.slotNamed("ring")) ? 1 : 0, 1);
   expect("and so has that ring in the strip", Equipment.canToggle({parent: Equipment.slotNamed("ring"), entries: [maraRing(20)], name: "ring"}) ? 1 : 0, 1);
   expect("the other ring still has none", Equipment.canToggle({parent: Equipment.slotNamed("ring"), entries: [maraRing(21)], name: "ring"}) ? 1 : 0, 0);
   hung = [];
   order([real, finger(6)], "ring");
   expect("the game's own ring keeps the eye of the slot", Equipment.canToggle(Equipment.slotNamed("ring")) ? 1 : 0, 1);
   expect("an empty small slot has no eye", Equipment.canToggle({parent: Equipment.slotNamed("ring"), entries: [], plus: true, name: "ring"}) ? 1 : 0, 0);
   Equipment.hiddenMask = 1 << Equipment.slotIndex(Equipment.slotNamed("ring"));
   expect("a virtual ring in a hidden slot shows no shut eye, it stays on the finger", Equipment.canToggle({parent: Equipment.slotNamed("ring"), entries: [finger(6)], name: "ring"}) ? 1 : 0, 0);
   expect("the hidden slot keeps its eye to be shown again", Equipment.canToggle(Equipment.slotNamed("ring")) ? 1 : 0, 1);
   Equipment.hiddenMask = 0;
   Equipment.plugin = () => undefined;

   Equipment.settings = {pieceStrips: true};
   order([lamp, bag, cloak], "other");
   const other = Equipment.slotNamed("other");
   const mini = {parent: other, entries: [bag]};
   expect("three pieces pull out a strip", Equipment.hasPieces(other) ? 1 : 0, 1);
   expect("a small slot has no strip of its own", Equipment.hasPieces(mini) ? 1 : 0, 0);
   expect("a small slot takes what its slot takes", Equipment.fits(piece(11, [48]), mini) ? 1 : 0, 1);
   expect("a small slot of Other takes no helmet", Equipment.fits(piece(12, [30]), mini) ? 1 : 0, 0);
   order([cloak], "other");
   expect("one piece pulls out nothing", Equipment.hasPieces(other) ? 1 : 0, 0);
   Equipment.settings = {pieceStrips: false};
   order([lamp, bag, cloak], "other");
   expect("strips switched off", Equipment.hasPieces(other) ? 1 : 0, 0);
}

// a strip is one row that ends before the other column, past that it scrolls between two arrows
{
   const Equipment = freshPanel(false);
   const fit = (room, count) => { const told = Equipment.piecesFit(room, count); return told.shown + (told.scrolls ? " scrolls" : ""); };
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   said("four rings in a wide room", fit(950, 4), "4");
   said("as many as fit, no arrows", fit(950, 15), "15");
   said("one more needs the arrows, they take a piece's room", fit(950, 16), "14 scrolls");
   said("thirty rings on a wide screen", fit(950, 30), "14 scrolls");
   said("a narrow room still shows one", fit(150, 30), "1 scrolls");
}

// The tab of a slot with several pieces takes the press of a real mouse (the published 1.1.0 left it under
// hitTestDisable beside the slot, so every press went through to the doll). The game's picking stands in here: a press
// lands on a visible text field whose clips are all visible and none of them under hitTestDisable
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const fakeClip = () => {
      const clip = {_x: 0, _y: 0, _visible: true, parts: []};
      clip.createEmptyMovieClip = (name) => (clip[name] = fakeClip(), clip.parts.push(clip[name]), clip[name]);
      clip.createTextField = (name, depth, x, y, wide, high) => (clip[name] = {_x: x, _y: y, _width: wide, _height: high}, clip.parts.push(clip[name]), clip[name]);
      ["clear", "lineStyle", "beginFill", "endFill", "moveTo", "lineTo", "curveTo"].forEach((name) => clip[name] = () => undefined);
      return clip;
   };
   const takesMouse = (clip, x, y) => clip._visible && !clip.hitTestDisable && clip.parts.some((part) => part.parts !== undefined
      ? takesMouse(part, x - part._x, y - part._y)
      : x >= part._x && x <= part._x + part._width && y >= part._y && y <= part._y + part._height);
   // the pointer at a place of the slot, every part of it reads the pointer in its own space
   const pointAt = (box, x, y) => [box, box.tab, box.eye].forEach((clip) => { clip._xmouse = x - (clip === box ? 0 : clip._x); clip._ymouse = y - (clip === box ? 0 : clip._y); });
   const pressed = [];
   Object.assign(Equipment, {panel: fakeClip(), settings: {pieceStrips: true}, pieceStrip: {}});
   Equipment.showSlot = Equipment.styleText = Equipment.roundBox = () => undefined;
   Equipment.openPieces = (slot) => pressed.push("strip " + slot.name);
   Equipment.toggleHidden = (slot) => pressed.push("eye " + slot.name);
   Equipment.click = (slot) => pressed.push("slot " + slot.name);
   const size = Equipment.SLOT_SIZE;
   const middle = (size - Equipment.TAB_HIGH) / 2 + Equipment.TAB_HIGH / 2;
   const rings = [{formId: 1}, {formId: 2}, {formId: 3}];
   [["ring", size + Equipment.TAB_GAP + Equipment.TAB_WIDE / 2, size + 1], ["other", -Equipment.TAB_GAP - Equipment.TAB_WIDE / 2, -1]].forEach(([name, onTab, inGap]) => {
      const slot = Equipment.slotNamed(name);
      Equipment.buildSlot(slot, 1);
      slot.entries = rings;
      Equipment.drawTab(slot);
      const box = slot.box;
      said(name + " has a tab", box.tab._visible, true);
      said("a press on the " + name + " tab comes to the slot", takesMouse(box, onTab, middle), true);
      said("so does a press in the gap between the " + name + " slot and its tab", takesMouse(box, inGap, middle), true);
      pointAt(box, onTab, middle);
      box.onPress();
      pointAt(box, inGap, middle);
      box.onPress();
      pointAt(box, size / 2, size / 2);
      box.onPress();
      box.eye._visible = true;
      pointAt(box, box.eye._x + 2, box.eye._y + 2);
      box.onPress();
   });
   said("tab, gap, middle and eye of both columns", pressed.join(", "), "strip ring, strip ring, slot ring, eye ring, strip other, strip other, slot other, eye other");
   const ring = Equipment.slotNamed("ring");
   ring.entries = [rings[0]];
   Equipment.drawTab(ring);
   pressed.length = 0;
   pointAt(ring.box, size + Equipment.TAB_GAP + Equipment.TAB_WIDE / 2, middle);
   ring.box.onPress();
   said("one piece left, the hidden tab's place is no tab", pressed.join(", "), "slot ring");
}

// With one strip out a press on the tab of another slot pulls out that slot's strip, and the click that closes a
// strip on a press outside it leaves the new one out, whether its listener hears the press before or after the tab
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const ring = Equipment.slotNamed("ring");
   const other = Equipment.slotNamed("other");
   let closed = 0;
   Object.assign(Equipment, {context: {_visible: false}, anchorSlot: {}, contextSlot: {}});
   Equipment.pressOnPieces = () => false;
   Equipment.closePieces = () => { closed++; Equipment.pieceStrip = undefined; };
   Equipment.buildPieces();
   const click = (opens) => { Equipment.piecesMouse.onMouseDown(); opens(); Equipment.piecesMouse.onMouseUp(); };
   Equipment.pieceStrip = {slot: ring};
   click(() => { Equipment.pieceStrip = {slot: other}; });
   said("the other slot's strip stays out", closed + " " + Equipment.pieceStrip.slot.name, "0 other");
   Equipment.pieceStrip = {slot: ring};
   click(() => { Equipment.pieceStrip = {slot: ring}; });
   said("the same slot's strip made again from the list still goes in on a click outside", closed + " " + Equipment.pieceStrip, "1 undefined");
   Equipment.pieceStrip = {slot: ring};
   click(() => undefined);
   said("a plain click outside puts it back", closed + " " + Equipment.pieceStrip, "2 undefined");
}

// a window of the strip opens over the strip when there's room there, under it otherwise
{
   const Equipment = freshPanel(false);
   const slot = {column: 1, row: 4, box: {_x: 1780, _y: 900}};
   const mini = {parent: slot, column: 1, box: {_x: 1600, _y: 907}};
   const window = {wide: 300, high: 400};
   Equipment.frame = {left: 900, right: 1844, top: 40, bottom: 990};
   Equipment.pieceStrip = {slot, left: 1500, top: 900, wide: 280, high: 64};
   expect("over the strip", Equipment.placeByPieces(mini, window, 1000) ? window._y : -1, 900 - Equipment.POPUP_GAP - 400);
   expect("its right edge on the small slot's", window._x, 1600 + Equipment.MINI_SIZE - 300);
   Equipment.pieceStrip = {slot, left: 1500, top: 120, wide: 280, high: 64};
   Equipment.placeByPieces(mini, window, 1000);
   expect("under a strip high on the screen", window._y, 120 + 64 + Equipment.POPUP_GAP);
   expect("another slot's window is left alone", Equipment.placeByPieces({column: 0, box: {}}, window, 1000) ? 1 : 0, 0);
}

// the character sheet: pools with what comes back a second, armor by the game's rate and cap, resistances capped
{
   const Equipment = freshPanel(false);
   const pool = (now, most, regen, combat) => ({now, most, regen, combat});
   const told = {health: pool(80, 100, 0.7, 0.7), magicka: pool(100, 100, 3, 0), stamina: pool(50, 100, 5, 1),
      armorScale: 0.12, armorBase: 0.03, armorCap: 80, resistCap: 85, magic: 10, fire: 120, frost: 0, shock: 0, poison: 0, disease: 0,
      speedMult: 1, leftSpeedMult: 0.8, crit: 0, unarmed: 4, move: 100, level: 12, toNextLevel: 0.45,
      worn: 46.45, wornForSpeed: 12, noise: 0.5, shout: 0.8, absorb: 0, reflect: 10, waterBreathing: 1,
      skills: [{name: "One-handed", points: 0, modifier: 20, power: 10}, {name: "Heavy armor", points: 15, modifier: 0, power: 0},
         {name: "Destruction", points: 0, modifier: 25, power: 0}, {name: "Restoration", points: 0, modifier: 0, power: 0}]};
   const worn = {armor: [armor(4, 30, 1, {weightClass: 1}), armor(1, 15, 1, {weightClass: 1}), armor(64, 0, 1, {weightClass: 4})]};
   const sheet = Equipment.heroSheet(told, {armor: 100, damage: 12.6, encumbrance: 152.3, maxEncumbrance: 320}, worn);
   const named = (list, name) => list.find((one) => one.name === name) || {};
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   said("health and its pace in a fight", named(sheet.pools, "Health").note, "+0.7/s, in a fight +0.5");
   said("magicka that stops in a fight", named(sheet.pools, "Magicka").note, "+3.0/s, none in a fight");
   said("stamina keeps its pace in a fight", named(sheet.pools, "Stamina").note, "+5.0/s");
   expect("armor stops by rating and two pieces", sheet.armor.stops, 0.18);
   expect("armor's cap", sheet.armor.cap, 0.8);
   said("fire past the cap", named(sheet.resists, "Fire").value + " " + named(sheet.resists, "Fire").capped, "85% true");
   said("a zero resistance is faint", String(named(sheet.resists, "Frost").zero) + " " + String(named(sheet.resists, "Magic").zero), "true false");
   said("fists: no swing, the left hand slowed, no crit", sheet.offense.map((one) => one.name + " " + one.value).join(", "), "Damage 12, Left attack speed -20%, Unarmed damage 4");
   told.hands = [{entry: {formType: 41, subType: 1}, speed: 1, left: false}, {entry: {formType: 41, subType: 2}, speed: 1.3, left: true}];
   told.speedMult = 1.1;
   told.leftSpeedMult = 1.1;
   told.crit = 5;
   const armed = Equipment.heroSheet(told, {armor: 100, damage: 12.6, encumbrance: 152.3, maxEncumbrance: 320}, worn);
   said("a sword and a dagger", armed.offense.map((one) => one.name + " " + one.value).join(", "), "Damage 12, Swing 0.64 s, Damage a second ~11.6, Left hand swing 0.49 s, Attack speed +10%, Critical chance 5%");
   told.hands = [{entry: {formType: 41, subType: 8}, speed: 1, left: false}];
   told.speedMult = 1;
   told.leftSpeedMult = 1;
   told.crit = 0;
   said("a bow is drawn", Equipment.heroSheet(told, {damage: 20}, worn).offense.map((one) => one.name).join(","), "Damage,Draw,Damage a second");
   expect("level", sheet.level, 12);
   expect("the way to the next", sheet.toNext, 0.45);
   said("movement", sheet.movement.map((one) => one.name + " " + one.value).join(", "), "Carry weight 153 / 320, Gear weight 46.5, slows you down 12, Noise of steps 50%");
   const near = Object.assign({}, told, {worn: 92.8, wornForSpeed: 92.75});
   said("a weight that reads the same is not told twice", Equipment.heroSheet(near, {encumbrance: 1, maxEncumbrance: 2}, worn).movement.map((one) => one.name).join(","), "Carry weight,Gear weight,Noise of steps");
   said("other only what's there", sheet.other.map((one) => one.name + " " + one.value).join(", "), "Shout cooldown -20%, Reflect damage 10%, Water breathing yes");
   told.voice = {name: "Unrelenting Force", words: [{seconds: 20, learned: true}, {seconds: 30, learned: true}, {seconds: 45, learned: false}]};
   told.voiceWait = 12;
   said("a shout in seconds of its learned words", Equipment.voiceLines(told).map((one) => one.name + " " + one.value + (one.minor ? " (dim)" : "")).join(", "), "Unrelenting Force 16 / 24 s, Voice rests 12 s more (dim), Shout cooldown -20% (dim)");
   told.move = 110;
   said("speed only when changed", Equipment.heroSheet(told, {}, worn).movement[0].name + " " + Equipment.heroSheet(told, {}, worn).movement[0].value, "Speed +10%");
   delete told.voice;
   told.voiceWait = 0;
   told.move = 100;
   said("skills only when raised", sheet.skills.map((one) => one.name + " " + one.value).join(", "), "One-handed +30% damage, Heavy armor +15 skill, Destruction cost -25%");
   said("no numbers, no sheet", String(Equipment.heroSheet(undefined, undefined, worn)), "undefined");
}

// spells in hand by what they cost, bounties, statuses with their time left
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const told = {magicka: {now: 100}, spells: [{name: "Firebolt", cost: 41, stream: false}, {name: "Flames", cost: 14, stream: true}]};
   said("a cast and a stream", Equipment.spellLines(told).map((one) => one.name + " " + one.aside).join(", "), "Firebolt 41 magicka, Flames 14 magicka a second");
   told.spells = [{name: "Flames", cost: 14, stream: true}, {name: "Flames", cost: 14, stream: true}];
   expect("the same spell in both hands is one line", Equipment.spellLines(told).length, 1);
   said("a bounty", Equipment.bountyLines({bounties: [{name: "Whiterun", gold: 40}]})[0].value, "40 gold");
   const statuses = Equipment.statusLines({statuses: [{name: "Rockjoint", text: "Your melee hits do 25% less damage.", left: -1, type: 1}, {name: "Blessing of Talos", text: "Shouts recharge 20% faster.", left: 28800, type: 4}]});
   said("a disease in red without a time", statuses[0].aside + " " + (statuses[0].color === Equipment.HERO_HEAVY), " true");
   said("a blessing with its time", statuses[1].aside, "8 h");
   const fight = {health: {now: 1, most: 1, regen: 0, combat: 0}, magicka: {now: 1, most: 1, regen: 0, combat: 0}, stamina: {now: 100, most: 100, regen: 0, combat: 0},
      armorScale: 0.12, armorBase: 0.03, armorCap: 80, resistCap: 85, speedMult: 1, leftSpeedMult: 1, crit: 0, unarmed: 4, move: 100,
      hands: [{entry: {formType: 41, subType: 8}, speed: 1, left: false}], bowDamage: 21.6, arrow: "Iron Arrow", sneakMult: 2, runSpeed: 300, walkSpeed: 100};
   const armed = Equipment.heroSheet(fight, {damage: 12}, {armor: []});
   said("a bow with its arrow and a sneak shot", armed.offense.map((one) => one.name + " " + one.value).join(", "), "Damage with Iron Arrow 21, Draw 1.00 s, Damage a second ~12.0, Sneak attack x2, 43");
   said("run and walk in metres", armed.movement.slice(0, 2).map((one) => one.name + " " + one.value).join(", "), "Run 4.3 m/s, Walk 1.4 m/s");
   delete fight.bowDamage;
   fight.hands = [{entry: {formType: 41, subType: 1}, speed: 1, left: false}];
   fight.sneakMult = 3;
   fight.powerStamina = 26.5;
   fight.powerDamage = 2;
   said("a power attack", Equipment.heroSheet(fight, {damage: 10}, {armor: []}).offense.slice(3).map((one) => one.name + " " + one.value).join(", "), "Sneak attack x3, 30, Power attack x2, 27 stamina, in a row from full 3");
   said("no fists with a spell in the right hand", Equipment.offenseLines({speedMult: 1, leftSpeedMult: 1, crit: 0, unarmed: 4, spells: [{name: "Flames", left: false}]}, {}).length + "", "0");
   said("minutes and seconds", Equipment.timeLeft(90) + ", " + Equipment.timeLeft(40.2) + ", " + Equipment.timeLeft(3720), "1 min, 41 s, 1 h 2 min");
}

// the character drawer: as far out as asked within the room and the sheet
{
   const Equipment = freshPanel(false);
   const least = Equipment.heroLeast();
   Equipment.hero = {room: 500, full: 419};
   expect("never pulled shows all of the sheet", Equipment.heroHigh(0), 419);
   expect("no further out than the sheet", Equipment.heroHigh(800), 419);
   expect("a few lines at the least", Equipment.heroHigh(10), least);
   expect("as it was left", Equipment.heroHigh(210), 210);
   Equipment.hero = {room: 300, full: 419};
   expect("a low room holds it", Equipment.heroHigh(0), 300);
   Equipment.hero = {room: 40, full: 419};
   expect("a room under the least wins", Equipment.heroHigh(0), 40);
   expect("gold blends toward white", Equipment.blendColor(0x000000, 0xFFFFFF, 0.5), 0x808080);
}

// a chip brings the list up to date through the call the game makes after an equip, a menu without it the old way
{
   const Equipment = freshPanel(false);
   const lists = Equipment.menu.inventoryLists;
   const calls = [];
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   Equipment.chosenChips = [];
   Equipment.settings = {subcategories: true};
   lists.categoryList.selectedEntry = {flag: 4};
   lists.categoryLabel = {_x: 0, textField: {_x: 0}};
   lists.itemList = {InvalidateData: () => calls.push("list")};
   lists.InvalidateListData = () => calls.push("lists");
   Equipment.chooseChip("0,clothing");
   said("a chosen chip refreshes like an equip does", calls.join(","), "lists");
   said("and the chip is chosen", Equipment.chosenChips[0], "clothing");
   delete lists.InvalidateListData;
   Equipment.chooseChip("0,");
   said("a menu without that call refreshes the list itself", calls.join(","), "lists,list");
}

// the body slots of a piece from its mask, bit n is slot 30 + n, up to slot 61 in the top bit
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const line = (entry) => Equipment.bodySlotsLine(entry);
   said("a helmet takes slot 30", line(armor(1, 15)), "Slot 30");
   said("a cuirass and a cloak bit", line(armor(4 | 65536, 30)), "Slots 32, 46");
   said("a shield is slot 39", line(armor(512, 20)), "Slot 39");
   said("the top bit is slot 61", line(armor(2 ** 31, 0)), "Slot 61");
   said("every slot of a mask", Equipment.bodySlots(0xFFFFFFFF).length + " " + Equipment.bodySlots(0xFFFFFFFF).join(",").slice(0, 8), "32 30,31,32");
   said("a piece MARA took the slots off has none", line(armor(0, 0)), "No body slot");
   said("a mask SkyUI never told says nothing", line({formType: W.ARMOR}), "");
   said("a weapon takes no body slot", line(sword(8)), "No body slot");
   said("nor do arrows", line({formType: W.AMMO}), "No body slot");
   said("nor a torch", line({formType: W.LIGHT}), "No body slot");
   said("a potion says nothing", line({formType: 46}), "");

   // in a window of a row the line shares the strip of the lens, in a window of a slot it adds its own height
   // the lens is a part this test doesn't load
   Equipment.LENS_SIZE = 22;
   Equipment.window = {statLines: [], bodySlots: {_visible: true, _height: 17}};
   expect("a row window over its row keeps the lens's height", Equipment.underCard(true), Equipment.LENS_SIZE);
   expect("one under its row only the line's", Equipment.underCard(false), 17);
   Equipment.window.bodySlots._visible = false;
   expect("and with no line no strip at all", Equipment.underCard(false), 0);
   Equipment.window.bodySlots._visible = true;
   Equipment.anchorSlot = Equipment.slotNamed("body");
   expect("a slot window grows by the line", Equipment.underCard(), 17 + Equipment.ACTION_HIGH + 2 * Equipment.ACTION_GAP + Equipment.ACTION_EDGE);
   Equipment.window.bodySlots._visible = false;
   expect("switched off it adds nothing", Equipment.underCard(), Equipment.ACTION_HIGH + 2 * Equipment.ACTION_GAP + Equipment.ACTION_EDGE);
}

// the colours of the menu replace the gold and the parchment every part draws with
{
   const Equipment = freshPanel(false);
   Equipment.plugin = () => ({Settings: () => ({accent: 0x3A7BD5, text: 0xDDDDDD})});
   Equipment.readSettings();
   expect("the accent of the menu", Equipment.GOLD, 0x3A7BD5);
   expect("the text colour of the menu", Equipment.PARCHMENT, 0xDDDDDD);
   Equipment.plugin = () => undefined;
   Equipment.readSettings();
   expect("with no plugin the gold stays", Equipment.GOLD, 14069864);
   expect("and the parchment", Equipment.PARCHMENT, 15130824);
}

// the words of the panel come from the translation file of the game's language, English where it has none
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const english = freshPanel(false);
   said("no file keeps the English", english.say("Find in list"), "Find in list");
   said("a value in place of #", english.say("# gold", 40), "40 gold");
   said("values by number", english.say("#1 h #2 min", 7, 59), "7 h 59 min");
   const german = freshPanel(false, {
      "$IR_Find in list": "In der Liste finden",
      "$IR_#1 h #2 min": "#2 Min. von #1 Std.",
      "$IR_Head": "Kopf",
      "$IR_Bows": "Bögen",
   });
   said("a translated line", german.say("Find in list"), "In der Liste finden");
   said("a language puts the values in its own order", german.say("#1 h #2 min", 7, 59), "59 Min. von 7 Std.");
   said("the slots are named in it", german.SLOTS[5].title, "Kopf");
   said("a chip SkyUI has no word for takes the mod's word", german.translated({words: "$Bows", fallback: "Bows"}), "Bögen");
   const crowded = freshPanel(false, {"$Bows": "Bows", "$IR_Bows": "Bögen", "$Heavy": "Schwer", "$IR_Heavy": "Heavy"});
   said("the mod's own translation wins over another mod's English key", crowded.translated({words: "$Bows", fallback: "Bows"}), "Bögen");
   said("with the mod's words in English SkyUI's word stays", crowded.translated({words: "$Heavy", fallback: "Heavy"}), "Schwer");
   said("the item window line", german.bodySlotsLine({formType: 41}), "No body slot");
   const fromPlugin = freshPanel(false, {"$IR_Health": "HEALTH"});
   fromPlugin.plugin = () => ({Words: () => ({"$IR_Health": "Health"})});
   said("the plugin's words win over the game's pool, which keeps one letter case", fromPlugin.say("Health"), "Health");
}

// the kind under the name, the column of kinds, the effects of an enchantment line by line, the ink of paper skins
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel();
   const armor = (more) => Object.assign({formType: 26, partMask: 0}, more);
   said("a heavy helmet", Equipment.kindLine(armor({weightClass: 1, partMask: 1, subTypeDisplay: "Head"})), "Heavy, Head");
   said("clothing on the body", Equipment.kindLine(armor({weightClass: 3, partMask: 4, subTypeDisplay: "Body"})), "Clothing, Body");
   // the card's skill names a light or heavy piece whole, the corner of the card said it a second time
   said("a heavy helmet by its card's skill", Equipment.kindLine(armor({weightClass: 1, partMask: 1, subTypeDisplay: "Head"}), "Heavy Armor"), "Heavy Armor, Head");
   said("a light shield by its card's skill", Equipment.kindLine(armor({weightClass: 0, partMask: 512, subTypeDisplay: "Shield"}), "Light Armor"), "Light Armor, Shield");
   said("clothing has no skill, its chip names it", Equipment.kindLine(armor({weightClass: 3, partMask: 4, subTypeDisplay: "Body"}), ""), "Clothing, Body");
   said("a heavy shield", Equipment.kindLine(armor({weightClass: 1, partMask: 512, subTypeDisplay: "Shield"})), "Heavy, Shield");
   said("a one handed sword", Equipment.kindLine({formType: 41, subType: 1, subTypeDisplay: "Sword"}), "One-Handed, Sword");
   said("a greatsword", Equipment.kindLine({formType: 41, subType: 5, subTypeDisplay: "Greatsword"}), "Two-Handed, Greatsword");
   said("a bow is a bow", Equipment.kindLine({formType: 41, subType: 8, subTypeDisplay: "Bow"}), "Bow");
   said("a health potion", Equipment.kindLine({formType: 46, subType: 0, subTypeDisplay: "Health"}), "Potion, Health");
   said("a poison", Equipment.kindLine({formType: 46, subType: 15, subTypeDisplay: "Poison"}), "Poison");
   said("a gem", Equipment.kindLine({formType: 32, subType: 0, subTypeDisplay: "Gem"}), "Gem");
   said("no kind at all", Equipment.kindLine({formType: 32}), "");

   const ring = armor({weightClass: 4, keywords: {gearMaraRing: true}});
   Equipment.fillKind(ring);
   said("a ring MARA left with no slot", ring.subTypeDisplay, "Ring");
   said("and its line", Equipment.kindLine(ring), "Jewelry, Ring");
   const amulet = armor({weightClass: 4, keywords: {ClothingNecklace: true}, subTypeDisplay: "-"});
   Equipment.fillKind(amulet);
   said("an amulet with no slot", amulet.subTypeDisplay, "Amulet");
   const cloak = armor({weightClass: 3, partMask: 65536});
   Equipment.fillKind(cloak);
   said("a cloak of slot 46", cloak.subTypeDisplay, "Cloak");
   const helmet = armor({weightClass: 1, partMask: 1, subTypeDisplay: "Head"});
   Equipment.fillKind(helmet);
   said("SkyUI's own kind stays", helmet.subTypeDisplay, "Head");

   const iconOf = function(piece) {
      Equipment.fillIcon(piece);
      return piece.iconLabel;
   };
   said("a ring MARA left with no slot gets the ring icon", iconOf(armor({weightClass: 4, keywords: {gearMaraRing: true}, iconLabel: "default_armor"})), "armor_ring");
   said("an amulet with no slot gets the necklace icon", iconOf(armor({weightClass: 4, keywords: {ClothingNecklace: true}, iconLabel: "default_armor"})), "armor_amulet");
   said("a ring with a clothing keyword on its own slot", iconOf(armor({weightClass: 3, partMask: 0x40, iconLabel: "default_armor"})), "armor_ring");
   said("a cloak of slot 46 SkyUI left a shield", iconOf(armor({weightClass: 3, partMask: 65536, iconLabel: "default_armor"})), "clothing_cloak");
   said("a cloak with the necklace keyword stays a cloak", iconOf(armor({partMask: 65536, keywords: {ClothingNecklace: true}, iconLabel: "default_armor"})), "clothing_cloak");
   said("a piece on a slot of nobody keeps the shield", iconOf(armor({partMask: 0x200000, iconLabel: "default_armor"})), "default_armor");
   said("an icon SkyUI picked stays", iconOf(armor({partMask: 0x40, keywords: {gearMaraRing: true}, iconLabel: "lightarmor_hands"})), "lightarmor_hands");
   said("a weapon is not touched", iconOf({formType: 41, iconLabel: "default_armor"}), "default_armor");

   said("one effect stays one line", Equipment.effectLines("Your Magicka regenerates 25% faster."), "Your Magicka regenerates 25% faster.");
   said("two effects, two lines", Equipment.effectLines("Increases Destruction by 15%. Magicka regenerates 50% faster."),
      "Increases Destruction by 15%.<br>Magicka regenerates 50% faster.");
   said("a number starts a line", Equipment.effectLines("Fire damage. 25 points of frost."), "Fire damage.<br>25 points of frost.");
   said("a decimal point is no end", Equipment.effectLines("Weighs 1.5 less. Carry 20 more."), "Weighs 1.5 less.<br>Carry 20 more.");
   said("a tag between", Equipment.effectLines("Burns. <font color='#FF0000'>Freezes</font>."), "Burns.<br><font color='#FF0000'>Freezes</font>.");
   said("a small letter goes on", Equipment.effectLines("Lasts 60 s. and more"), "Lasts 60 s. and more");
   said("Cyrillic capitals", Equipment.effectLines("Шкода вогнем. Шкода морозом."), "Шкода вогнем.<br>Шкода морозом.");
   const menuBefore = Equipment.menu;
   Equipment.menu = {itemCard: {}};
   said("SkyUI's card sizes its effects itself", Equipment.cardFitsByHand(), false);
   Equipment.menu = {itemCard: {changeTextSize: function() {}}};
   said("a card of Dear Diary Dark Mode fits them by hand", Equipment.cardFitsByHand(), true);
   Equipment.menu = menuBefore;

   Equipment.readSkin({Appearance: {colors: {text: {enabled: 0xFFFFFF}}}});
   said("SkyUI's white list is no paper", Equipment.paper, false);
   said("the ink of junk on a dark list", Equipment.inkOf("junk"), 0x8A8579);
   Equipment.readSkin({Appearance: {colors: {text: {enabled: 0x141414}}}});
   said("Dear Diary's black list is paper", Equipment.paper, true);
   said("the ink of junk on paper", Equipment.inkOf("junk"), 0x5E574E);
   said("the window is a sheet of paper", Equipment.inkOf("sheet"), 0xD9CEB6);
   Equipment.readSkin({});
   said("a config with no colours is no paper", Equipment.paper, false);
}

// quiet lines, junk, weight warnings, the queue of many moves
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(true);
   Equipment.settings = {quietTags: true, modOrigin: true, questLine: true, slotNumbers: false, junk: true, weightWarnings: true, newMarks: true};
   const mine = (more) => Object.assign({side: 2048, formType: 32, count: 1, infoWeight: 1}, more);
   const theirs = (more) => Object.assign({side: 4096, formType: 32, count: 1, infoWeight: 1}, more);
   Equipment.ownFlags = () => [2048];
   said("a quest keeps it", Equipment.questLine({equipmentQuest: 1, equipmentQuestName: "The Golden Claw"}), "Still needed for The Golden Claw");
   said("a quest follows it", Equipment.questLine({equipmentQuest: 2, equipmentQuestName: "Delivery"}), "Still needed, Delivery follows it");
   said("an objective names it", Equipment.questLine({equipmentQuest: 3, equipmentQuestName: "Few and Far Between"}), "Still needed, Few and Far Between asks for it");
   said("the quest is done", Equipment.questLine({equipmentQuest: 4, equipmentQuestName: "Bleak Falls"}), "No longer needed, Bleak Falls is done");
   said("a quest with no name", Equipment.questLine({equipmentQuest: 1}), "Still needed for a quest");
   said("a running quest points at it", Equipment.questLine({equipmentQuest: 5, equipmentQuestName: "Lost Legacy"}), "Still needed for Lost Legacy");
   said("a quest of a place with no name", Equipment.questLine({equipmentQuest: 5, equipmentQuestName: "Shroud Hearth Barrow", equipmentQuestPlace: true}), "Still needed for a quest at Shroud Hearth Barrow");
   said("a quest not begun", Equipment.questLine({equipmentQuest: 6, equipmentQuestName: "Lost Legacy"}), "Needed later, Lost Legacy hasn't begun");
   said("a place's quest not begun", Equipment.questLine({equipmentQuest: 6, equipmentQuestName: "Shroud Hearth Barrow", equipmentQuestPlace: true}), "Needed later for a quest at Shroud Hearth Barrow");
   said("a quest that is over", Equipment.questLine({equipmentQuest: 7, equipmentQuestName: "Shroud Hearth Barrow", equipmentQuestPlace: true}), "No longer needed, the quest at Shroud Hearth Barrow is over");
   said("an unnamed quest that is over", Equipment.questLine({equipmentQuest: 7}), "No longer needed, its quest is over");
   said("how many you have, in a chest", Equipment.quietLines(theirs({equipmentCarried: 3})).join("|"), "You have 3");
   said("nothing of it on your own row", Equipment.quietLines(mine({equipmentCarried: 3})).join("|"), "");
   said("silver and the plugin", Equipment.quietLines(mine({keywords: {WeapMaterialSilver: true}, equipmentOrigin: "Silverware.esp"})).join("|"), "Silver|From Silverware.esp");
   Equipment.settings.modOrigin = false;
   said("the plugin's line switched off alone", Equipment.quietLines(mine({keywords: {WeapMaterialSilver: true}, equipmentOrigin: "Silverware.esp"})).join("|"), "Silver");
   Equipment.settings.modOrigin = true;
   const wanted = mine({equipmentQuest: 1, equipmentQuestName: "The Golden Claw", equipmentOrigin: "Claws.esp"});
   Equipment.settings.quietTags = false;
   said("the quest's line stays with the quiet tags off", Equipment.quietLines(wanted).join("|"), "Still needed for The Golden Claw");
   Equipment.settings.quietTags = true;
   Equipment.settings.questLine = false;
   said("the quest's line off, the quiet tags stay", Equipment.quietLines(wanted).join("|"), "From Claws.esp");
   Equipment.settings.questLine = true;
   expect("a plain keyword is no silver", Equipment.silver({keywords: {SilverSword: true}}) ? 1 : 0, 0);
   expect("worn stays", Equipment.locked(mine({equipState: 1})) ? 1 : 0, 1);
   expect("a favorite stays", Equipment.locked(mine({favorite: true})) ? 1 : 0, 1);
   expect("what a quest keeps stays", Equipment.locked(mine({equipmentQuest: 1})) ? 1 : 0, 1);
   expect("what a done quest left may go", Equipment.locked(mine({equipmentQuest: 4})) ? 1 : 0, 0);
   expect("what a running quest points at stays", Equipment.locked(mine({equipmentQuest: 5})) ? 1 : 0, 1);
   expect("what a quest not begun wants stays", Equipment.locked(mine({equipmentQuest: 6})) ? 1 : 0, 1);
   expect("what an ended quest wanted may go", Equipment.locked(mine({equipmentQuest: 7})) ? 1 : 0, 0);
   const rows = [mine({formId: 1, text: "Bone", equipmentJunk: true}), mine({formId: 2, text: "Cup", equipmentJunk: true, favorite: true}), theirs({formId: 3, text: "Pot", equipmentJunk: true}), mine({formId: 4, text: "Gem"})];
   Equipment.menu.inventoryLists.itemList = {entryList: rows, requestUpdate: () => {}};
   said("junk that may go", Equipment.junkRows().map((row) => row.text).join(","), "Bone");
   let carry = {carried: 290, most: 300};
   Equipment.plugin = () => ({Carry: () => carry});
   Equipment.kind = "container";
   const heavy = [theirs({infoWeight: 20}), theirs({infoWeight: 4, count: 5}), theirs({infoWeight: 2}), mine({infoWeight: 50})];
   Equipment.tintWeights(heavy, [2048]);
   expect("one is too heavy", heavy[0].equipmentWeightColor, Equipment.TOO_HEAVY);
   expect("the whole stack is too heavy", heavy[1].equipmentWeightColor, Equipment.HEAVY_STACK);
   expect("it fits", heavy[2].equipmentWeightColor, undefined);
   expect("your own rows say nothing", heavy[3].equipmentWeightColor, undefined);
   carry = {carried: 90000, most: 2000000};
   Equipment.tintWeights(heavy, [2048]);
   expect("no real limit, no warning", heavy[0].equipmentWeightColor, undefined);
   carry = {carried: 1990, most: 2000};
   said("taking all says its weight and what goes over", Equipment.takeAllWords("Gems", [theirs({infoWeight: 4, count: 5})]), "Take all Gems (5), weight 20, 10 over your limit");
   said("what the chest holds already", Equipment.heldThere([mine({formId: 3, text: "Pot"}), mine({formId: 4, text: "Gem"})]).map((row) => row.text).join(","), "Pot");
   Equipment.menu.itemCard = {addEventListener: () => {}};
   Equipment.menu.createEmptyMovieClip = () => ({});
   Equipment.menu.getNextHighestDepth = () => 1;
   Equipment.closeContext = () => {};
   Equipment.pickRow = () => true;
   const moves = [];
   Equipment.menu.onQuantityMenuSelect = (event) => moves.push(event.amount);
   let result;
   Equipment.menu.inventoryLists.itemList = {entryList: [mine({formId: 1, text: "Bone", count: 3}), mine({formId: 5, text: "Rag", count: 2})]};
   Equipment.startQueue([mine({formId: 1, text: "Bone"}), mine({formId: 5, text: "Rag"})], (moved, kept, stopped) => result = [moved, kept, stopped]);
   Equipment.queueTick();
   Equipment.queueTick();
   expect("the first whole stack moves", moves[0], 3);
   Equipment.menu.inventoryLists.itemList.entryList = [mine({formId: 5, text: "Rag", count: 2})];
   Equipment.queueListed();
   Equipment.queueTick();
   Equipment.queueTick();
   expect("then the next one", moves[1], 2);
   Equipment.queueListed();
   Equipment.queueTick();
   said("a row the game kept is counted", String(result), "1,1,false");
   moves.length = 0;
   const stolenDaggers = mine({formId: 9, text: "Dagger", count: 3, equipmentStolen: 3});
   Equipment.menu.inventoryLists.itemList = {entryList: [mine({formId: 9, text: "Dagger", count: 2}), stolenDaggers]};
   Equipment.startQueue([stolenDaggers], (moved, kept, stopped) => result = [moved, kept, stopped]);
   Equipment.queueTick();
   Equipment.queueTick();
   expect("the stolen row of an item moves, not its own row beside it", moves[0], 3);
   Equipment.menu.inventoryLists.itemList.entryList = [mine({formId: 9, text: "Dagger", count: 2})];
   Equipment.queueListed();
   Equipment.queueTick();
   said("it went and the own row stays", String(result), "1,0,false");
   const marks = freshPanel(false);
   marks.settings = {newMarks: true};
   expect("new until the pointer passed", marks.isNew({equipmentNew: true, formId: 7}) ? 1 : 0, 1);
   marks.newSeen.f7 = true;
   expect("passed once it stays seen in the menu", marks.isNew({equipmentNew: true, formId: 7}) ? 1 : 0, 0);
}

// the queue moves the rows the filters hide and never a worn row of the same name as one it moves
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(true);
   Equipment.settings = {junk: true};
   Equipment.ownFlags = () => [2048];
   const mine = (more) => Object.assign({side: 2048, formType: 32, count: 1}, more);
   // SkyUI's list selects only a row its filters let through, the category in view shows armor. A worn helmet and
   // its spare are two rows of one name, the spare is junk
   const list = {
      _selectedIndex: -1,
      disableSelection: false,
      getListEnumIndex(index) { return this.entryList[index] !== undefined && this.entryList[index].formType == 26 ? index : undefined; },
      get selectedIndex() { return this._selectedIndex; },
      set selectedIndex(index) { if (!this.disableSelection && (index == -1 || this.getListEnumIndex(index) !== undefined)) this._selectedIndex = index; },
      get selectedEntry() { return this.entryList[this._selectedIndex]; },
   };
   const listed = (entries) => {
      list.entryList = entries;
      entries.forEach((entry, index) => entry.itemIndex = index);
      list._selectedIndex = -1;
   };
   const worn = mine({formId: 9, formType: 26, text: "Iron Helmet", equipState: 1, equipmentJunk: true});
   const spare = mine({formId: 9, formType: 26, text: "Iron Helmet", equipmentJunk: true});
   const bone = mine({formId: 4, text: "Bone", count: 2, equipmentJunk: true});
   const moves = [];
   let result;
   Equipment.menu.inventoryLists.itemList = list;
   Equipment.menu.inventoryLists.dispatchEvent = () => {};
   Equipment.menu.itemCard = {addEventListener: () => {}};
   Equipment.menu.createEmptyMovieClip = () => ({});
   Equipment.menu.getNextHighestDepth = () => 1;
   Equipment.closeContext = () => {};
   Equipment.menu.onQuantityMenuSelect = (event) => moves.push((list.selectedEntry === worn ? "worn " : "") + list.selectedEntry.text + " x" + event.amount);
   listed([worn, spare, bone]);
   said("junk of both kinds may go", Equipment.junkRows().map((row) => row.text).join(","), "Iron Helmet,Bone");
   Equipment.startQueue(Equipment.junkRows(), (moved, kept, stopped) => result = [moved, kept, stopped]);
   Equipment.queueTick();
   Equipment.queueTick();
   said("the spare goes and the worn helmet of the same name stays", moves[0], "Iron Helmet x1");
   listed([worn, bone]);
   Equipment.queueListed();
   Equipment.queueTick();
   Equipment.queueTick();
   said("a row the category hides goes too", moves[1], "Bone x2");
   listed([worn]);
   Equipment.queueListed();
   Equipment.queueTick();
   said("both are counted as moved", String(result), "2,0,false");
   listed([worn, spare]);
   list.disableSelection = true;
   Equipment.startQueue([spare], (moved, kept, stopped) => result = [moved, kept, stopped]);
   Equipment.queueTick();
   Equipment.queueTick();
   Equipment.queueTick();
   said("a row the list won't select stays and is counted", String(result), "0,1,false");
}

// the two ways to sort go into the columns a UI mod brings under its own names
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(false);
   Equipment.plugin = () => ({});
   Equipment.settings = {lastAdded: true, stackWeight: true};
   const weight = (order) => ({sortAttributes: ["infoWeight", "text"], label: {text: "$WGT", arrowDown: order}});
   const columns = {
      gearNameColumn: {type: 3, states: 2, state1: {sortAttributes: ["text"]}, state2: {sortAttributes: ["text"]}},
      bookNameColumn: {type: 3, states: 2, state1: {sortAttributes: ["text"]}, state2: {sortAttributes: ["text"]}},
      itemWtColumn: {type: 2, states: 2, state1: weight(true), state2: weight(false)},
      itemTWtColumn: {type: 2, states: 2, state1: {sortAttributes: ["infoTotalWeight", "text"]}, state2: {}},
   };
   Equipment.addSorting({ListLayout: {columns}});
   said("last added in the names of gear", columns.gearNameColumn.state2.label.text, "LAST ADDED");
   expect("and of books", columns.bookNameColumn.states, 3);
   said("the sum in the weight of one item", columns.itemWtColumn.state2.label.text, "SUMW");
   expect("both ways of it", columns.itemWtColumn.states, 4);
   said("the weight of one item stays first", columns.itemWtColumn.state1.label.text, "$WGT");
   expect("a total weight column of its own is left alone", columns.itemTWtColumn.states, 2);

   // Kome's Inventory Tweaks brings an iconColumn with nothing in it, ActionScript 2 looped forever over its states
   Equipment.sortingAdded = false;
   const bare = {iconColumn: {}, nameColumn: {type: 3}};
   Equipment.addSorting({ListLayout: {columns: bare}});
   said("a column with no states stays as it came", JSON.stringify(bare.iconColumn), "{}");
   said("a name column with no states gets none", bare.nameColumn.states, undefined);
   said("a loop bound of nothing counts nothing", Equipment.count(undefined), 0);
   said("nor of a word", Equipment.count("some"), 0);
   said("a count written as text still counts", Equipment.count("2"), 2);
}

// a gamepad walks the panel: two columns of five slots, a strip between a slot and the doll, the subcategories above
// the top row, the drawer under the bottom one, the buttons of a slot window left and right
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const where = (at) => at.kind + (at.column !== undefined ? ` ${at.column},${at.row}` : "") + (at.cell !== undefined ? ` ${at.cell}` : "") + (at.index !== undefined ? ` ${at.index}` : "") + (at.scroll !== undefined ? ` scroll ${at.scroll}` : "");
   const slot = (column, row) => ({kind: "slot", column, row});
   const plain = {rows: 5, chips: 0, chipStart: 0, hero: false, heroOpen: false, last: slot(1, 0)};
   const world = (extra) => Object.assign({}, plain, extra || {});
   const go = (at, way, extra) => where(Equipment.padNext(at, way, world(extra)));

   said("down a column", go(slot(0, 1), "down"), "slot 0,2");
   said("up a column", go(slot(1, 3), "up"), "slot 1,2");
   said("the left column goes right across the doll", go(slot(0, 2), "right"), "slot 1,2");
   said("the right column goes left across the doll", go(slot(1, 2), "left"), "slot 0,2");
   said("nothing past the outer edge", go(slot(0, 2), "left"), "slot 0,2");
   said("nothing past the right edge", go(slot(1, 2), "right"), "slot 1,2");
   said("the top row without subcategories stays", go(slot(0, 0), "up"), "slot 0,0");
   said("the top row goes up to the subcategories, the chosen one first", go(slot(1, 0), "up", {chips: 6, chipStart: 2}), "chip 2");
   said("the bottom row without the drawer stays", go(slot(1, 4), "down"), "slot 1,4");
   said("the bottom row goes down to the drawer", go(slot(1, 4), "down", {hero: true}), "hero");

   const ring = {strip: {column: 0, row: 4, cells: 4}};
   said("toward the doll into the strip of the slot", go(slot(0, 4), "right", ring), "mini 0");
   said("another slot passes its strip", go(slot(0, 3), "right", ring), "slot 1,3");
   said("along the strip", go({kind: "mini", cell: 1}, "right", ring), "mini 2");
   said("the strip ends", go({kind: "mini", cell: 3}, "right", ring), "mini 3");
   said("back along the strip", go({kind: "mini", cell: 2}, "left", ring), "mini 1");
   said("out of the strip back to its slot", go({kind: "mini", cell: 0}, "left", ring), "slot 0,4");
   said("up out of the strip", go({kind: "mini", cell: 2}, "up", ring), "slot 0,3");
   const other = {strip: {column: 1, row: 4, cells: 3}};
   said("a strip of the right column goes left", go(slot(1, 4), "left", other), "mini 0");
   said("and its pieces go on to the left", go({kind: "mini", cell: 0}, "left", other), "mini 1");
   said("right goes back toward its slot", go({kind: "mini", cell: 1}, "right", other), "mini 0");

   said("along the subcategories", go({kind: "chip", index: 2}, "right", {chips: 4}), "chip 3");
   said("the subcategories end", go({kind: "chip", index: 3}, "right", {chips: 4}), "chip 3");
   said("and start", go({kind: "chip", index: 0}, "left", {chips: 4}), "chip 0");
   said("down from the subcategories to the top of the last column", go({kind: "chip", index: 1}, "down", {chips: 4, last: slot(0, 3)}), "slot 0,0");
   said("nothing above the subcategories", go({kind: "chip", index: 1}, "up", {chips: 4}), "chip 1");

   said("up from a closed drawer to the last column", go({kind: "hero"}, "up", {hero: true, last: slot(0, 2)}), "slot 0,4");
   said("left from the drawer", go({kind: "hero"}, "left", {hero: true}), "slot 0,4");
   said("right from the drawer", go({kind: "hero"}, "right", {hero: true}), "slot 1,4");
   said("an open drawer scrolls up", go({kind: "hero"}, "up", {hero: true, heroOpen: true}), "hero scroll -1");
   said("and down", go({kind: "hero"}, "down", {hero: true, heroOpen: true}), "hero scroll 1");

   const window = {actions: 3, anchor: slot(1, 1)};
   said("along the buttons of a window", go({kind: "action", index: 0}, "right", window), "action 1");
   said("the buttons end", go({kind: "action", index: 2}, "right", window), "action 2");
   said("down leaves the window as from its slot", go({kind: "action", index: 1}, "down", window), "slot 1,2");
   said("a window of a piece leaves as from the piece", go({kind: "action", index: 0}, "up", Object.assign({actions: 2, anchor: {kind: "mini", cell: 2}}, ring)), "slot 0,3");
}

// the buttons of a gamepad on the panel: A presses, B steps back, X works the eye, a held button acts once, and a B
// that left the panel or closed the inspect view doesn't go on to close the menu while it's held
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const done = [];
   const key = (value, nav, code) => ({value, navEquivalent: nav, skseKeycode: code});
   Equipment.panel = {};
   Equipment.menu._platform = 1;
   Equipment.padFocus = {kind: "slot", column: 0, row: 0};
   Equipment.padLast = Equipment.padFocus;
   Equipment.padMove = (way) => done.push(way);
   Equipment.padPress = () => done.push("press");
   Equipment.padBack = () => done.push("back");
   Equipment.padHide = () => done.push("hide");
   Equipment.padSettle = () => undefined;
   Equipment.listsAnswer = () => true;
   said("a move on the panel", Equipment.padInput(key("keyDown", "down")), true);
   said("a held move repeats", Equipment.padInput(key("keyHold", "down")), true);
   said("A presses", Equipment.padInput(key("keyDown", "enter-gamepad_A", 276)), true);
   said("its hold is kept from the list", Equipment.padInput(key("keyHold", "enter-gamepad_A", 276)), true);
   said("and its let go", Equipment.padInput(key("keyUp", "enter-gamepad_A", 276)), true);
   said("X by its code", Equipment.padInput(key("keyDown", undefined, 278)), true);
   said("a shoulder goes on to SkyUI's sort", Equipment.padInput(key("keyDown", undefined, 275)), false);
   said("what the panel did", done.join(","), "down,down,press,hide");
   done.length = 0;
   Equipment.padBack = () => { done.push("back"); Equipment.padFocus = undefined; };
   said("B as the game sends it", Equipment.padInput(key("keyDown", "tab", 277)), true);
   said("B held after it left the panel stays off the list", Equipment.padInput(key("keyHold", "tab", 277)), true);
   said("its let go too", Equipment.padInput(key("keyUp", "tab", 277)), true);
   said("the next B on the list is SkyUI's", Equipment.padInput(key("keyDown", "tab", 277)), false);
   Equipment.padFocus = {kind: "slot", column: 0, row: 0};
   Equipment.listsAnswer = () => false;
   said("a dialog of SkyUI over the lists takes the cross", Equipment.padInput(key("keyDown", "down")), false);
   Equipment.listsAnswer = () => true;
   Equipment.padFocus = undefined;
   said("B stepped back once", done.join(","), "back");
   said("a move on the list is SkyUI's", Equipment.padInput(key("keyDown", "down")), false);

   const zoom = [];
   Equipment.plugin = () => ({InspectZoom: (factor) => zoom.push(factor > 1 ? "in" : "out"), InspectTurn: (yaw) => zoom.push("turn " + yaw)});
   Equipment.INSPECT_ZOOM = 1.15;
   Equipment.inspecting = true;
   said("up in the inspect view", Equipment.padInput(key("keyDown", "up")), true);
   said("down in the inspect view", Equipment.padInput(key("keyDown", "down")), true);
   said("left in the inspect view", Equipment.padInput(key("keyDown", "left")), true);
   said("the view zooms and turns", zoom.join(","), "in,out,turn -15");
   said("B in the inspect view goes to the view, which closes", Equipment.padInput(key("keyDown", "escape-gamepad_B", 277)), false);
   Equipment.inspecting = false;
   said("and held after it stays off the list", Equipment.padInput(key("keyHold", "escape-gamepad_B", 277)), true);

   Equipment.menu._platform = 0;
   Equipment.padFocus = {kind: "slot", column: 0, row: 0};
   said("the keyboard never comes here", Equipment.padInput(key("keyDown", "down")), false);
   said("which button is which", ["enter-gamepad_A", "tab", "escape-gamepad_B", "shifttab"].map((nav) => Equipment.padButton(nav, undefined)).join(",") + "," + Equipment.padButton(undefined, 278), "press,back,back,back,hide");
}

// the way back from the panel, the inspect view closing under the panel, and the bottom bar that holds six at most
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const calls = [];
   const buttons = [];
   const list = {selectedIndex: -1, getListEnumIndex: (index) => index < 10 ? index : undefined};
   const clip = () => ({clear: () => undefined});
   const menu = Object.assign(Equipment.menu, {
      _platform: 1,
      createEmptyMovieClip: clip,
      getNextHighestDepth: () => 1,
      handleInput: () => true,
      updateBottomBar: () => calls.push("skyui bar"),
      SetPlatform: () => undefined,
      navPanel: {clearButtons: () => { buttons.length = 0; }, addButton: (data) => buttons.push(data.text), updateButtons: () => undefined},
      _acceptControls: {}, _cancelControls: {},
   });
   menu.inventoryLists.itemList = list;
   menu.inventoryLists.zoomButtonHolder = {};
   Object.assign(Equipment, {panel: {}, viewer: {}, window: {_visible: false}, hero: {_visible: true, high: 0}, actions: {}, chips: {}, settings: {popup: true, pieceStrips: true}});
   Equipment.under = Equipment.over = {createEmptyMovieClip: clip, getNextHighestDepth: () => 1};
   Equipment.plugin = () => ({PadTurns: (what) => calls.push("turns " + what)});
   Equipment.leaveInspect = () => { Equipment.inspecting = false; };
   Equipment.closePopup = () => undefined;
   Equipment.closePieces = () => undefined;
   Equipment.closeContext = () => undefined;
   // the inspect part and the slots built on screen aren't loaded here, a helmet in the head slot stands in
   const helmet = {name: "head", entries: [{formId: 1}], box: {}};
   Equipment.inspectable = () => true;
   Equipment.padCell = () => helmet;
   Equipment.hasPieces = () => false;
   Equipment.canToggle = () => true;
   Equipment.looksHidden = () => false;
   Equipment.buildPad();
   Equipment.padMark = () => calls.push("mark");
   Equipment.padUnmark = () => undefined;

   Equipment.padFocus = {kind: "slot", column: 1, row: 0};
   Equipment.padKept = 4;
   list.selectedIndex = 7;
   Equipment.leavePad(false);
   said("a row the panel found stays selected on the list", list.selectedIndex, 7);
   Equipment.padFocus = {kind: "slot", column: 1, row: 0};
   Equipment.padKept = 4;
   list.selectedIndex = -1;
   Equipment.leavePad(false);
   said("with none found the row it came from comes back", list.selectedIndex, 4);
   said("and the stick turns nothing on the list", calls.filter((one) => one.startsWith("turns")).pop(), undefined);

   calls.length = 0;
   Equipment.padFocus = {kind: "slot", column: 1, row: 0};
   Equipment.inspecting = true;
   Equipment.leaveInspect();
   said("the inspect view closed by B marks the focus again", calls.includes("mark"), true);
   said("and the stick turns the doll again", calls.filter((one) => one.startsWith("turns")).pop(), "turns 1");

   // the most a state of the panel asks of the bar: a slot with a piece that hides and can be inspected
   Equipment.padHints();
   said("a slot with a piece", buttons.join(" | "), "Details | Hide | Back | List | Inspect (hold)");
   Equipment.padFocus = {kind: "hero"};
   Equipment.hero.high = 200;
   Equipment.padCell = () => undefined;
   Equipment.padHints();
   said("the open drawer", buttons.join(" | "), "Close | Scroll | Back | List");
   Equipment.padFocus = undefined;
   buttons.length = 0;
   ["Equip", "Drop", "Favorite", "Charge"].forEach((text) => buttons.push(text));
   list.selectedEntry = {formId: 2};
   Equipment.padListHint();
   said("the list with a row that charges, SkyUI's four and two more", buttons.length, 6);
}

// the quest sign of a row: gold while needed, dimmer gold for a quest not begun, grey once over, and it stands
// before the junk sign so the two never overlap
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   Equipment.plugin = () => undefined;
   Equipment.readSettings();
   const look = (kind) => Equipment.questLook(kind).color + "/" + Equipment.questLook(kind).alpha;
   said("held by a running quest", look(1), Equipment.GOLD + "/100");
   said("followed by a quest", look(2), Equipment.GOLD + "/100");
   said("named by an objective", look(3), Equipment.GOLD + "/100");
   said("pointed at by a running quest", look(5), Equipment.GOLD + "/100");
   said("wanted by a quest not begun", look(6), Equipment.GOLD + "/" + Equipment.QUEST_LATER);
   said("its quest done", look(4), Equipment.inkOf("quiet") + "/" + Equipment.QUEST_DONE);
   said("its quest over", look(7), Equipment.inkOf("quiet") + "/" + Equipment.QUEST_DONE);
   said("no quest, no sign", Equipment.questShown({equipmentQuest: 0}), false);
   said("a done quest still has its sign", Equipment.questShown({equipmentQuest: 4}), true);
   const row = {bestIcon: {_width: 10}};
   const field = {_x: 10, _width: 100};
   const both = {equipmentQuest: 1, equipmentJunk: true};
   const icons = Equipment.iconsEnd(row, field, both, {});
   const junkAt = Equipment.questEnd(row, field, both, {});
   expect("the junk sign starts after the quest sign", junkAt, icons + Equipment.QUEST_SIGN + Equipment.ICON_GAP);
   expect("the arrow after both", Equipment.afterIcons(row, field, both, {}), junkAt + Equipment.JUNK_SIGN + Equipment.ICON_GAP);
   expect("junk alone stands where the icons end", Equipment.questEnd(row, field, {equipmentJunk: true}, {}), icons);
   Equipment.settings.questMarks = false;
   said("switched off", Equipment.questShown({equipmentQuest: 1}), false);
   expect("and takes no room", Equipment.afterIcons(row, field, both, {}), icons + Equipment.JUNK_SIGN + Equipment.ICON_GAP);
}

// the stolen line of the item window counts the copies that belong to someone else
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   said("one stolen item", Equipment.stolenLine({count: 1, equipmentStolen: 1}), "Stolen");
   said("some of a stack", Equipment.stolenLine({count: 5, equipmentStolen: 3}), "Stolen: 3 of 5");
   said("a whole stack, a row of stolen copies of its own", Equipment.stolenLine({count: 4, equipmentStolen: 4}), "Stolen");
   said("nothing stolen", Equipment.stolenLine({count: 2, equipmentStolen: 0}), "");
   said("the game's own mark when the plugin counted none", Equipment.stolenLine({count: 2, isStolen: true}), "Stolen");
   said("never more than the stack", Equipment.stolenLine({count: 2, equipmentStolen: 5}), "Stolen");
   const own = {formId: 9, text: "Iron Dagger", count: 2};
   const stolen = {formId: 9, text: "Iron Dagger", count: 3, equipmentStolen: 3};
   expect("the stolen row of an item is told from its own row", Equipment.isRow(stolen, Equipment.rowKey(own)) ? 1 : 0, 0);
   expect("and the own row from the stolen one", Equipment.isRow(own, Equipment.rowKey(stolen)) ? 1 : 0, 0);
   expect("a stolen row found again by the game's mark alone", Equipment.isRow({formId: 9, text: "Iron Dagger", isStolen: true}, Equipment.rowKey(stolen)) ? 1 : 0, 1);
   const inChest = {equipmentStolen: 3, isStolen: false};
   const ownInChest = {equipmentStolen: 0};
   Equipment.fillStolen(inChest);
   Equipment.fillStolen(ownInChest);
   expect("a chest's row of stolen copies gets the stolen mark the game left off", inChest.isStolen ? 1 : 0, 1);
   expect("the own row beside it stays without", ownInChest.isStolen ? 1 : 0, 0);
   Equipment.window = {statLines: [], bodySlots: {_visible: false}, stolen: {_visible: true, _height: 18}};
   Equipment.anchorSlot = Equipment.slotNamed("body");
   expect("a slot window grows by the stolen line", Equipment.underCard(), 18 + Equipment.STOLEN_GAP + Equipment.ACTION_HIGH + 2 * Equipment.ACTION_GAP + Equipment.ACTION_EDGE);
   Equipment.window.stolen._visible = false;
   expect("and not without it", Equipment.underCard(), Equipment.ACTION_HIGH + 2 * Equipment.ACTION_GAP + Equipment.ACTION_EDGE);
}

// a click on a filled slot never touches the category, the chips, the search or the filter of a slot. A row the list
// shows is selected, one it hides is selected past the filters and opens the window of the slot
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const done = [];
   const listed = [2, 3];
   const list = {
      _selectedIndex: -1,
      entryList: [],
      listState: {},
      getListEnumIndex: (index) => listed.includes(index) ? listed.indexOf(index) : undefined,
      getClipByIndex: () => ({setEntry: () => done.push("redrawn")}),
      get selectedIndex() { return this._selectedIndex; },
      set selectedIndex(index) {
         if (index == -1 || listed.includes(index)) {
            this._selectedIndex = index;
            done.push("select " + index);
         }
      },
      get selectedEntry() { return this.entryList[this._selectedIndex]; },
   };
   const lists = Equipment.menu.inventoryLists;
   lists.itemList = list;
   lists.dispatchEvent = (event) => done.push("highlight " + event.index);
   lists.categoryList.onItemPress = () => done.push("category changed");
   Equipment.drawFrame = () => undefined;
   Equipment.openPopup = (slot) => { Equipment.anchorSlot = slot; done.push("window of " + slot.name); };
   Equipment.closePopup = () => done.push("closed");
   Equipment.settings = {popup: false};
   const helmet = {itemIndex: 3, text: "Helmet", clipIndex: 1};
   const boots = {itemIndex: 7, text: "Boots", clipIndex: 4};
   list.entryList[3] = helmet;
   list.entryList[7] = boots;
   const head = Object.assign(Equipment.slotNamed("head"), {entries: [helmet]});
   const feet = Object.assign(Equipment.slotNamed("feet"), {entries: [boots]});
   Equipment.showWorn(head, false);
   said("a row in the list is selected with the window under it", done.join(","), "closed,select 3");
   said("and no slot window", Equipment.anchorSlot, undefined);
   done.length = 0;
   Equipment.showWorn(feet, false);
   said("a row the list hides is selected past it, the row before drawn plain, and the slot opens its window", done.join(","), "redrawn,highlight 7,window of feet");
   said("the list's own index names it", list._selectedIndex, 7);
   said("its old row clip is forgotten", boots.clipIndex, undefined);
   list._selectedIndex = -1;
   Equipment.keepApart();
   said("a list made anew lets go of it, the window takes it back", list._selectedIndex, 7);
   said("nothing in the list was changed", done.join(",").indexOf("category"), -1);
   said("the list shows the helmet", Equipment.listShows(helmet), true);
   said("not the boots", Equipment.listShows(boots), false);
}

// a gamepad passes the slots the player took off the panel
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const where = (at) => at.kind + (at.column !== undefined ? ` ${at.column},${at.row}` : "");
   const slot = (column, row) => ({kind: "slot", column, row});
   const world = (extra) => Object.assign({rows: 5, chips: 0, chipStart: 0, hero: false, heroOpen: false, last: slot(1, 0)}, extra);
   const go = (at, way, extra) => where(Equipment.padNext(at, way, world(extra)));
   const column = ["0,0", "0,1", "0,2", "0,3", "0,4"];
   said("down past a slot off the panel", go(slot(0, 1), "down", {gone: ["0,2"]}), "slot 0,3");
   said("up past it", go(slot(0, 3), "up", {gone: ["0,2"]}), "slot 0,1");
   said("across to the nearest slot on the panel", go(slot(1, 2), "left", {gone: ["0,2"]}), "slot 0,1");
   said("from the subcategories to the first slot on", go({kind: "chip", index: 1}, "down", {chips: 4, last: slot(0, 3), gone: ["0,0"]}), "slot 0,1");
   said("the drawer from the last slot on", go(slot(1, 3), "down", {hero: true, gone: ["1,4"]}), "hero");
   said("a column all off, across stays", go(slot(1, 2), "left", {gone: column}), "slot 1,2");
   said("the drawer to a column all off goes to the other", go({kind: "hero"}, "left", {hero: true, gone: column}), "slot 1,4");
   Equipment.settings = {popup: false};
   Equipment.padCell = () => ({name: "head", entries: [{itemIndex: 1}]});
   Equipment.hasPieces = () => false;
   Equipment.listShows = () => false;
   said("A on a slot whose row the list hides opens its window", Equipment.padPressWords(slot(1, 0)), "Details");
   Equipment.listShows = () => true;
   said("one the list shows finds it", Equipment.padPressWords(slot(1, 0)), "Find in list");
}

// the sheet of the panel's look: its words, its steps within their range, what it tells the plugin, the slots off
// the panel and the width of the list
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const told = [];
   Equipment.plugin = () => undefined;
   Equipment.readSettings();
   said("full size", Equipment.lookWords("dollScale", 100), "100%");
   said("in the middle", Equipment.lookWords("dollRight", 0), "Middle");
   said("to the left", Equipment.lookWords("dollRight", -15), "15% left");
   said("up", Equipment.lookWords("dollUp", 10), "10% up");
   said("down", Equipment.lookWords("dollUp", -5), "5% down");
   said("the narrowest list", Equipment.lookWords("listWidth", 2), "Narrower");
   expect("a size step", Equipment.lookStepped("dollScale", 1), 105);
   Equipment.settings.dollScale = 140;
   expect("never past the largest", Equipment.lookStepped("dollScale", 1), 140);
   Equipment.settings.dollRight = -40;
   expect("never past the left end", Equipment.lookStepped("dollRight", -1), -40);
   expect("the left arrow narrows the list", Equipment.lookStepped("listWidth", -1), 1);
   expect("a full list can't widen", Equipment.lookStepped("listWidth", 1), 0);

   Equipment.plugin = () => ({SetSetting: (id, value) => told.push(id + "=" + value)});
   Equipment.changeSetting("doll", "bShow:Doll", false);
   said("a switch goes to the plugin as a number under the menu's id", told[0], "bShow:Doll=0");
   said("and holds in the panel", Equipment.settings.doll, false);

   Equipment.settings.slotsOff = 1 << 2;
   said("the ammo slot is off", Equipment.slotShown(Equipment.slotNamed("ammo")), false);
   said("the ring slot is on", Equipment.slotShown(Equipment.slotNamed("ring")), true);
   said("a small slot goes with its slot", Equipment.slotShown({parent: Equipment.slotNamed("ammo")}), false);
   expect("the slots on the panel", Equipment.cells().length, 9);
   Equipment.closePopup = () => undefined;
   Equipment.closePieces = () => undefined;
   Equipment.layout = () => undefined;
   Equipment.showModels = () => undefined;
   Equipment.flipSlot(Equipment.slotNamed("ammo"));
   said("a slot put back is told by its own switch", told[1], "bShowAmmo:Slots=1");
   expect("and all are on again", Equipment.cells().length, 10);

   const lists = Equipment.menu.inventoryLists;
   const widths = [];
   lists.itemList = {layout: {entryWidth: 530, updateLayout: () => widths.push("layout")}, getBounds: () => ({xMin: 10, xMax: 576})};
   lists.applyDynamicWidth = (width) => widths.push(width);
   lists.categoryList.UpdateList = () => undefined;
   lists.getContentBounds = () => [0, 0, 555, 600];
   lists._x = 0;
   Equipment.settings.listWidth = 2;
   said("the narrowest list moves SkyUI's parts by the difference", Equipment.narrowList() + " " + widths.join(","), "true 403,layout");
   expect("its entries are that wide", lists.itemList.layout.entryWidth, 403);
   expect("the window keeps the full width in mind", Equipment.listFullWide, 555);
   said("the same width again changes nothing", Equipment.narrowList(), false);
   Equipment.settings.listWidth = 0;
   Equipment.narrowList();
   expect("back to the full width", lists.itemList.layout.entryWidth, 530);
   expect("by the difference from where it was", widths[2], 657);
}

// Skins built on SkyUI 5 with paper looks, Dear Diary the one at hand: weapon kinds, the first strip, the list's
// picture, the card's paper, the bottom bar laid out around its hints, inks on the panel's own dark boxes
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(false);
   const weapon = (subType, anim, worn) => ({formType: W.WEAPON, subType: subType, weaponType: anim, infoDamage: 10, equipState: worn || 0});

   // SkyUI 5 menus number keyword kinds from 13 (spear 13, pike 15, halberd 16, rapier 17, gun 23), SkyUI 6 has its
   // fishing rod at 13. The game's own kind of swing decides the hands
   const halberd = weapon(16, 6);
   const pike = weapon(15, 5);
   const rapier = weapon(17, 1);
   const katana = weapon(21, 11);
   const gun = weapon(23, 9);
   const fishingRod = weapon(13, 1);
   said("a halberd is two handed", Equipment.twoHanded(halberd), true);
   said("and not one handed", Equipment.oneHanded(halberd), false);
   said("its chip is Two-Handed", Equipment.chipHolds("twoHanded", halberd), true);
   said("a pike's kind under its name", Equipment.kindLine(pike), "Two-Handed");
   said("a pike swings like a greatsword", Equipment.swingOf(pike), Equipment.SWING.twoHanded);
   said("a rapier fits the left hand", Equipment.fits(rapier, Equipment.slotNamed("left")), true);
   said("a rapier's chip is One-Handed", Equipment.chipHolds("oneHanded", rapier), true);
   said("a katana of the second ten of kinds is one handed", Equipment.oneHanded(katana), true);
   said("a gun on a crossbow's swing is ranged", Equipment.ranged(gun), true);
   said("and shoots bolts", Equipment.shoots(gun, bolt(10)), true);
   said("but no arrows", Equipment.shoots(gun, arrow(10)), false);
   said("SkyUI 6's fishing rod stays one handed", Equipment.oneHanded(fishingRod), true);
   said("a row with no kind of the game goes by SkyUI's numbers", Equipment.twoHanded(greatsword(18)), true);
   said("a bow with no kind of the game", Equipment.ranged(bow(9)), true);
   Equipment.place(Equipment.worn([weapon(16, 6, 4)], undefined));
   said("a halberd in both hands takes the left one", Equipment.slotNamed("left").taken, true);

   // The list starts with no strip. A menu with no call to fit its list keeps its own height until a strip opens
   const fresh = freshPanel(false);
   const lists = fresh.menu.inventoryLists;
   const list = {_y: 100, listHeight: 480, entryHeight: 24, _maxListIndex: 20, requestUpdate() {}};
   lists.showItemsList = () => undefined;
   lists.addEventListener = () => undefined;
   lists.panelContainer = {createEmptyMovieClip: () => ({}), getNextHighestDepth: () => 1};
   lists.itemList = list;
   fresh.settings = {subcategories: false};
   fresh.showChips = () => undefined;
   fresh.startRows({});
   fresh.openStrip(false);
   expect("no strip at the start leaves a SkyUI 5 list as it is", list.listHeight, 480);
   expect("and in its place", list._y, 100);
   fresh.openStrip(true);
   expect("a strip cuts it", list.listHeight, 450);
   expect("and moves it down", list._y, 130);
   fresh.openStrip(false);
   expect("and gives the height back", list.listHeight, 480);

   // Dear Diary's page and binder reach 80 past the rows, the window keeps to the rows
   lists.getContentBounds = () => [-21, 0, 641, 720];
   lists._x = 16;
   lists.itemList = {getBounds: () => ({xMin: 35, xMax: 577})};
   expect("the slots keep clear of the whole picture", fresh.listEdge().right, 661);
   expect("what lies over the list ends at the rows", fresh.rowsEdge().right, 581);
   expect("and is as wide as the page less what hangs past them", fresh.rowsEdge().wide, 561);
   lists.getContentBounds = () => [0, 0, 557, 720];
   lists._x = 0;
   lists.itemList = {getBounds: () => ({xMin: 35, xMax: 578})};
   expect("SkyUI's own frame", fresh.rowsEdge().right, fresh.listEdge().right);
   lists.itemList = {getBounds: () => ({xMin: 35, xMax: 575})};
   lists.getContentBounds = () => [0, 0, 430, 720];
   expect("a list SkyUI narrowed keeps its frame", fresh.rowsEdge().right, 455);

   // SkyUI's hints stand under the bar's corner, Dear Diary's over it in the strip of its binder
   fresh.menu.bottomBar = {_y: 645, _yscale: 100, buttonPanel: {_y: 15}};
   expect("SkyUI's bar starts at its corner", fresh.barTop(), 645);
   fresh.menu.bottomBar.buttonPanel._y = -35;
   expect("Dear Diary's at its hints", fresh.barTop(), 610);
   delete fresh.menu.bottomBar;

   // the card's paper of Dear Diary has a clear margin of 20, SkyUI's frame starts at the card's corner
   const card = (box, scale) => ({_xscale: scale, _parent: {_width: 900, _xscale: 100}, background: {_width: box.xMax - box.xMin, getBounds: () => box}});
   fresh.menu.itemCard = card({xMin: -19.75, yMin: -20, xMax: 448.25, yMax: 205}, 100);
   expect("the paper of a picture counts without its margin", fresh.cardWidth(), 428.5);
   expect("and from the card's corner", fresh.frameBox(fresh.menu.itemCard).xMin, 0);
   expect("its bottom too", fresh.frameBox(fresh.menu.itemCard).yMax, 185.25);
   fresh.menu.itemCard = card({xMin: 0, yMin: 0, xMax: 427.85, yMax: 221.65}, 85);
   expect("SkyUI's frame at Vel'dun's 85%", fresh.cardWidth(), 427.85 * 0.85);
   fresh.menu.itemCard = {_xscale: 100, _parent: {_width: 500, _xscale: 50}, background: {_width: 0}};
   expect("a frame still loading leaves the holder", fresh.cardWidth(), 1000);

   // the look sheet is a dark box of its own on any skin, the list's width only where SkyUI can narrow it
   fresh.readSkin({Appearance: {colors: {text: {enabled: 0x141414}}}});
   said("quiet words over paper", fresh.inkOf("quiet"), 0x66584A);
   said("quiet words on the look sheet over paper", fresh.inkOf("quiet", true), 0x9C978B);
   said("a sign on paper", fresh.inkOf("sign"), 0x2A2420);
   fresh.readSkin({});
   said("a sign on a dark skin", fresh.inkOf("sign"), fresh.PARCHMENT);
   lists.itemList = {layout: {entryWidth: 530}};
   lists.applyDynamicWidth = undefined;
   said("a SkyUI 5 list keeps its width", fresh.listNarrows(), false);
   lists.applyDynamicWidth = () => undefined;
   said("SkyUI 6 narrows it", fresh.listNarrows(), true);

   // The kind line goes under the name and what stands under it moves down a line. Vel'dun's fields of numbers are
   // taller than their figures and reach up past the bottom of the name's letters
   {
      const same = {localToGlobal: () => undefined, globalToLocal: () => undefined};
      const kindCard = (numbersTop) => {
         const made = Object.assign({_visible: true, _alpha: 100, parts: []}, same);
         const part = (name, top, bottom) => (made[name] = {_name: name, _parent: made, _visible: true, _alpha: 100, _x: 0, _y: top, _width: 300, _height: bottom - top});
         made.ItemText = {_name: "ItemText", _parent: made, _visible: true, _alpha: 100, getBounds: () => ({xMin: 40, xMax: 260, yMin: 30, yMax: 70}), ItemTextField: {_y: 0, textHeight: 26}, localToGlobal: (point) => (point.y += 30)};
         part("ItemType", 10, 24);
         part("ArmorValue", numbersTop, numbersTop + 34);
         part("ArmorLabel", 96, 110);
         made.createTextField = (name) => (made[name] = {_name: name, _parent: made, _visible: false, _alpha: 100, _x: 0, _y: 0, _width: 0, _height: 0, setNewTextFormat: () => undefined, setTextFormat: () => undefined});
         return made;
      };
      fresh.settings = {quietTags: true};
      fresh.styleText = () => undefined;
      fresh.windowEntry =() => ({formType: 26, weightClass: 1, partMask: 4, subTypeDisplay: "Body"});
      fresh.menu.itemCard = kindCard(52);
      fresh.kindMoved = undefined;
      fresh.layKind();
      said("Vel'dun's numbers that reach up into the name move down", fresh.menu.itemCard.ArmorValue._y, 52 + fresh.KIND_LINE);
      said("and their labels", fresh.menu.itemCard.ArmorLabel._y, 96 + fresh.KIND_LINE);
      said("the type over the name stays", fresh.menu.itemCard.ItemType._y, 10);
      said("the kind line stands under the letters", fresh.menu.itemCard.equipmentKind._y, 60);
      fresh.layKind();
      said("laid out again it doesn't move twice", fresh.menu.itemCard.ArmorValue._y, 52 + fresh.KIND_LINE);
      fresh.menu.itemCard = kindCard(62);
      fresh.kindMoved = undefined;
      fresh.layKind();
      said("SkyUI's numbers under the name move down too", fresh.menu.itemCard.ArmorValue._y, 62 + fresh.KIND_LINE);
   }
}

// Menus made on SkyUI 5 have a stage 2560x720 the game stretches over the screen, the panel squeezes its own drawing
// back. The window over a row keeps the list's proportions, beside a slot it's square
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const squashOf = (stage, screen) => {
      const Equipment = freshPanel(false, undefined, {visibleRect: stage, safeRect: {x: 20, y: 10}});
      Equipment.plugin = () => (screen ? {ScreenSize: () => screen} : undefined);
      Equipment.readSquash();
      return Equipment;
   };
   expect("Dear Diary on 16:9 shows a unit across half as wide", squashOf({x: 0, y: 0, width: 2560, height: 720}, {width: 1920, height: 1080}).squash, 0.5);
   expect("and on 16:10 less than half", squashOf({x: 0, y: 0, width: 2560, height: 720}, {width: 1920, height: 1200}).squash, 0.45);
   expect("SkyUI 6 on 16:10 keeps its proportions", squashOf({x: 0, y: -40, width: 1280, height: 800}, {width: 1920, height: 1200}).squash, 1);
   expect("a pixel of rounding is no stretch", squashOf({x: 0, y: 0, width: 1280, height: 720}, {width: 1366, height: 768}).squash, 1);
   expect("without the plugin nothing changes", squashOf({x: 0, y: 0, width: 2560, height: 720}, undefined).squash, 1);

   const Equipment = squashOf({x: 0, y: 0, width: 2560, height: 720}, {width: 1920, height: 1080});
   expect("the screen in the panel's units is as wide as SkyUI 6's", Equipment.screenRect().width, 1280);
   expect("the margin of the screen across too", Equipment.safeInset().x, 10);
   expect("a point of the stage into the panel's units", Equipment.toPanel({x: 600, y: 300}).x, 300);
   expect("and back", Equipment.toStage({x: 300, y: 300}).x, 600);
   said("a share is the same in both", Equipment.share(640, 360).x, Equipment.screenShare(1280, 360).x);

   const lists = Equipment.menu.inventoryLists;
   lists.getContentBounds = () => [-21, 0, 641, 720];
   lists._x = 16;
   lists.itemList = {getBounds: () => ({xMin: 35, xMax: 577})};
   expect("Dear Diary's page ends in the panel's units", Equipment.listEdge().right, 330.5);
   expect("its rows", Equipment.rowsEdge().right, 290.5);
   expect("as wide as its page less what hangs past them", Equipment.rowsEdge().wide, 280.5);

   const card = {_yscale: 63};
   Equipment.menu.itemCard = {_parent: card};
   Equipment.window = {wide: 269};
   Equipment.placeWindowLeft();
   expect("over a row the window stands where it stood on the stretched stage", Equipment.window.left, 146.5);
   Equipment.windowUnits(Equipment.squash);
   expect("in the list's proportions", Equipment.window._xscale, 50);
   expect("its card too", card._xscale, 63);
   expect("as wide in the panel's units as half its own", Equipment.windowWide(), 134.5);
   Equipment.windowUnits(1);
   expect("beside a slot it's square", Equipment.window._xscale, 100);
   expect("the card in it too", card._xscale, 126);
   expect("and as wide as its own units", Equipment.windowWide(), 269);
}

// The game equips the selected row on a press whenever the pointer is over it. A press a line of a menu, the lens or
// the dark sheet took must not equip the row under it, in either order of the game's call and the press
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(false);
   const box = freshPanel.context;
   let now = 10000;
   box.getTimer = () => now;
   Equipment.menu._platform = 0;
   Equipment.over = {_parent: undefined};
   Equipment.under = {_parent: undefined};
   const list = {};
   const row = {_parent: list, itemIndex: 0, _visible: true};
   const line = {_parent: {_parent: Equipment.over}};
   const sheet = {_parent: {_parent: Equipment.under}};
   box.Mouse.getTopMostEntity = () => row;
   said("a press on a row is the row's", Equipment.pressUsed(), false);
   box.Mouse.getTopMostEntity = () => line;
   said("the game's equip before the line had the press", Equipment.pressUsed(), true);
   box.Mouse.getTopMostEntity = () => sheet;
   said("the dark sheet under the panel is the panel's too", Equipment.pressUsed(), true);
   Equipment.menu._platform = 1;
   said("a pad equips whatever the pointer rests on", Equipment.pressUsed(), false);
   Equipment.menu._platform = 0;
   box.Mouse.getTopMostEntity = () => row;
   Equipment.usePress();
   now += 50;
   said("the game's equip after the line had the press and closed the menu", Equipment.pressUsed(), true);
   now += Equipment.PRESS_USED;
   said("a later press on a row equips again", Equipment.pressUsed(), false);
   box.Mouse.getTopMostEntity = () => undefined;
   said("nothing under the pointer", Equipment.pressUsed(), false);
}

// The lens sits under the right end of the selected row. A row the pointer crosses on its way there waits, the
// window stays with the row it was on, and the row is selected once the pointer turns away or stops on it
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(false);
   const box = freshPanel.context;
   let now = 10000;
   box.getTimer = () => now;
   const ray = (x, y, dx, dy) => Equipment.stepsToBox(x, y, dx, dy, {xMin: 100, yMin: 40, xMax: 120, yMax: 60});
   said("a step that ends short of the box", ray(0, 50, 50, 0), 2.4);
   said("a step inside the box", ray(0, 50, 110, 0), 120 / 110);
   said("a step past the box", ray(0, 50, 200, 0) < 1, true);
   said("a step away from it", ray(0, 50, -10, 0), -1);
   said("a step down beside it", ray(0, 0, 0, 10), -1);

   const list = {entryList: [{text: "a"}, {text: "b"}, {text: "c"}], selectedIndex: 0};
   list.selectedEntry = list.entryList[0];
   list.onItemRollOver = function(index) { this.selectedIndex = index; this.selectedEntry = this.entryList[index]; };
   Equipment.menu.inventoryLists.itemList = list;
   Equipment.menu._platform = 0;
   // rows 30 high from 0 down, the lens 22 square under the right end of row a
   const rows = list.entryList.map((entry, i) => ({_parent: list, itemIndex: i, _visible: true}));
   const lens = {_visible: true, getBounds: () => ({xMin: 400, yMin: 30, xMax: 422, yMax: 52})};
   lens.hitTest = (x, y) => x >= 400 && x <= 422 && y >= 30 && y <= 52;
   Equipment.lens = lens;
   box.Mouse.getTopMostEntity = () => lens.hitTest(box._root._xmouse, box._root._ymouse) ? lens : rows[Math.floor(box._root._ymouse / 30)];
   // a move of the pointer as the menu sees it: the mouse listener, then a row the pointer came onto rolls over
   const move = (x, y) => {
      box._root._xmouse = x;
      box._root._ymouse = y;
      const under = box.Mouse.getTopMostEntity();
      Equipment.followLensAim();
      if (under !== lens && under !== move.last) {
         move.last = under;
         list.onItemRollOver(under.itemIndex);
      }
   };
   const backOnRowA = () => {
      list.equipmentRollOver(0);
      Equipment.lensWait = undefined;
      Equipment.lensFrom = undefined;
      move.last = rows[0];
   };
   Equipment.aimForLens();
   move(100, 15);
   move(110, 15);
   move(250, 32);
   said("row b waits while the pointer heads for the lens", list.selectedIndex, 0);
   now += Equipment.LENS_AIM_MS - 50;
   move(330, 32);
   now += Equipment.LENS_AIM_MS - 50;
   Equipment.lensWaitFrame();
   said("a slow hand still on the way", list.selectedIndex, 0);
   move(405, 35);
   said("on the lens the window stays with row a", list.selectedIndex, 0);
   said("and row b gave up its turn", Equipment.lensWait, undefined);

   backOnRowA();
   move(370, 15);
   move(380, 15);
   move(390, 35);
   said("a step down toward the lens waits", list.selectedIndex, 0);
   move(200, 45);
   said("turning away from the lens selects the row under the pointer", list.selectedIndex, 1);

   backOnRowA();
   move(150, 15);
   move(160, 15);
   move(330, 33);
   said("waits again", list.selectedIndex, 0);
   now += Equipment.LENS_AIM_MS;
   Equipment.lensWaitFrame();
   said("a pointer that stops on the row it crossed selects it in a moment", list.selectedIndex, 1);

   backOnRowA();
   move(150, 15);
   move(160, 15);
   move(330, 33);
   Equipment.endLensWait();
   said("a press on the waiting row selects it first", list.selectedIndex, 1);

   backOnRowA();
   move(100, 15);
   move(100, 75);
   said("straight down far from the lens never waits", list.selectedIndex, 2);

   backOnRowA();
   lens._visible = false;
   move(100, 15);
   move(110, 15);
   move(250, 32);
   said("no lens, no wait", list.selectedIndex, 1);
}

// All parts share one Equipment object, a function a later part names again replaces the earlier one. The strip
// of pieces once took the name of the strip of subcategories and the menu hung. Only the stubs that a later
// part fills in on purpose may share a name
{
   const stubs = ["listReady", "started", "categoryShown"];
   const seen = {};
   for (const part of fs.readdirSync(source).filter((name) => name.endsWith(".as"))) {
      for (const match of fs.readFileSync(path.join(source, part), "utf8").matchAll(/^Equipment\.(\w+) = function/gm)) {
         (seen[match[1]] = seen[match[1]] || []).push(part);
      }
   }
   for (const [name, parts] of Object.entries(seen)) {
      if (parts.length > 1 && !stubs.includes(name)) {
         failed++;
         console.log(`FAIL Equipment.${name} is defined in ${parts.join(" and ")}`);
      } else {
         passed++;
      }
   }
}

// a request for an update while one waits starts no second timer, SkyUI's own would leave the first behind
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   let timers = 0;
   const list = {
      requestUpdate() { this._bRequestUpdate = true; if (this._invalidateRequestID) { return; } this._updateRequestID = ++timers; },
      commitUpdate() { delete this._updateRequestID; this._bRequestUpdate = false; },
   };
   Equipment.guardUpdates(list);
   list.requestUpdate();
   list.requestUpdate();
   list.requestUpdate();
   said("three requests before the update, one timer", timers, 1);
   list.commitUpdate();
   list.requestUpdate();
   said("a request after the update starts a new one", timers, 2);
   list.commitUpdate();
   list._invalidateRequestID = 7;
   list.requestUpdate();
   said("a waiting invalidate takes the update along", timers, 2);
   said("and the update is still wanted", list._bRequestUpdate, true);
   Equipment.guardUpdates(list);
   delete list._invalidateRequestID;
   list.requestUpdate();
   list.requestUpdate();
   said("guarded twice, still one timer a request", timers, 3);
}

// the card's frame grows by its plain strip only: the top stays, the bottom edge moves down under the numbers
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const art = { yMin: 0, yMax: 208 };
   let pieces = Equipment.framePieces(art, 170, 290, 30, 4);
   said("the last 30 units are the bottom edge", pieces.bottomFrom, 178);
   said("the frame ends where it's asked to", pieces.shift, 82);
   said("the slice is taken right under the cut", pieces.sliceTop, 170);
   said("the slice is thin", pieces.sliceHigh, 4);
   said("the fill reaches the moved edge", pieces.fillHigh, 90);
   pieces = Equipment.framePieces(art, 100, 150, 30, 4);
   said("numbers that fit move nothing", pieces.shift, 0);
   pieces = Equipment.framePieces(art, 190, 260, 30, 4);
   said("a frame with no room under the cut keeps what it has as its edge", pieces.bottomFrom, 190);
   said("and takes its slice from above the cut", pieces.sliceTop, 186);
   said("a chest's frame never rises", pieces.rise, 0);

   // the item window's frame: the top edge goes up over the model, the bottom one down under the numbers
   const sky = { yMin: 0, yMax: 153 };
   pieces = Equipment.windowPieces(sky, -100, 300, 30, 4);
   said("the top edge rises to the window's top", pieces.rise, 100);
   said("the window's frame is cut under its top edge", pieces.cut, 30);
   said("the bottom edge goes to the window's bottom", pieces.shift, 147);
   said("the fill runs between the two edges", pieces.cut - pieces.rise + pieces.fillHigh, pieces.bottomFrom + pieces.shift);
   pieces = Equipment.windowPieces(sky, 20, 120, 30, 4);
   said("a window shorter than the frame pulls the bottom edge up", pieces.shift, -33);
   said("and still fills between", pieces.cut - pieces.rise + pieces.fillHigh, pieces.bottomFrom + pieces.shift);
   pieces = Equipment.windowPieces({ yMin: 0, yMax: 60 }, 0, 200, 30, 4);
   said("a short frame keeps a third as each edge", pieces.cut, 20);
   const box = { numbersTop: 100, rowHigh: 30 };
   said("all lines show when they fit", Equipment.shownLines(box, 200, 300), 200);
   said("whole lines show when they don't", Equipment.shownLines(box, 200, 195), 90);
   said("one line at the least", Equipment.shownLines(box, 200, 110), 30);
}

// while the card asks how many the rows stay still and the box under the card keeps off its Yes and No
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const itemList = {};
   const categoryList = {};
   let later = 0;
   Equipment.menu = { inventoryLists: { itemList, categoryList } };
   Equipment.cardNumbers = { _visible: true };
   Equipment.cardNumbersLater = () => later++;
   Equipment.cardAsks({ opening: true, menu: "quantity" });
   said("the rows hold while the card asks", itemList.disableSelection && itemList.disableInput && categoryList.disableSelection && categoryList.disableInput, true);
   said("the box under the card goes", Equipment.cardNumbers._visible, false);
   Equipment.cardAsks({ opening: false, canceled: true, menu: "quantity" });
   said("the rows answer again", itemList.disableSelection || itemList.disableInput || categoryList.disableSelection || categoryList.disableInput, false);
   said("the box is measured again", later, 1);
}

// the look sheet lies over a still list, a letting go of the inspect view keeps it still while the sheet is up
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const itemList = {};
   const categoryList = {};
   Equipment.menu = { inventoryLists: { itemList, categoryList } };
   Equipment.look = { _visible: true };
   Equipment.holdRows(true);
   said("the rows hold under the sheet", itemList.disableInput && categoryList.disableSelection, true);
   Equipment.inspecting = false;
   Equipment.holdRows(false);
   said("an inspect view that ends keeps them held under the sheet", itemList.disableSelection && itemList.disableInput, true);
   Equipment.look._visible = false;
   Equipment.holdRows(false);
   said("with the sheet gone they answer", itemList.disableSelection || itemList.disableInput || categoryList.disableInput, false);
}

// the item window takes half the list, never less than four tenths of the screen's height, the size the player
// chose on top, and never more than eight tenths of the list
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   said("a wide list gives its share", Equipment.windowWideFor(800, 720, 100, 1), 384);
   said("a narrow list keeps a share of the screen", Equipment.windowWideFor(500, 720, 100, 1), 288);
   said("a bigger window by the player's size", Equipment.windowWideFor(800, 720, 130, 1), 499);
   said("never over the names of the rows", Equipment.windowWideFor(500, 720, 160, 1), 400);
   said("in the skin's units of a stretched menu", Equipment.windowWideFor(800, 720, 100, 1.5), 256);
}

// the item window comes up over a row at once, once the row has stayed selected a moment, or never
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(false);
   const box = freshPanel.context;
   let now = 10000;
   let shown = 0;
   box.getTimer = () => now;
   Equipment.menu = { inventoryLists: { itemList: { selectedIndex: 3 } } };
   Equipment.showWindow = () => shown++;
   Equipment.settings = {windowShow: 0};
   Equipment.wantWindow(true);
   said("on hover it comes at once", Equipment.windowWanted, true);
   Equipment.settings.windowShow = 1;
   Equipment.wantWindow(true);
   said("after a pause it waits", Equipment.windowWanted, false);
   now += 200;
   Equipment.windowRestFrame();
   said("not yet a moment later", Equipment.windowWanted, false);
   now += 250;
   Equipment.windowRestFrame();
   said("it comes once the row stayed", Equipment.windowWanted, true);
   said("and is shown then", shown, 1);
   Equipment.wantWindow(true);
   Equipment.menu.inventoryLists.itemList.selectedIndex = -1;
   now += 500;
   Equipment.windowRestFrame();
   said("a row let go before the pause brings nothing", Equipment.windowWanted, false);
   Equipment.settings.windowShow = 2;
   Equipment.menu.inventoryLists.itemList.selectedIndex = 3;
   Equipment.wantWindow(true);
   now += 1000;
   Equipment.windowRestFrame();
   said("never means never", Equipment.windowWanted, false);
}

// all junk at once asks first with the whole list, yes moves it, no leaves it
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(false);
   const box = freshPanel.context;
   box.gfx.io = {GameDelegate: {call: () => {}}};
   box.gfx.ui = {NavigationCode: {ENTER: "enter", ESCAPE: "escape", TAB: "tab", UP: "up", DOWN: "down"}};
   box.Shared = {GlobalFunc: {IsKeyPressed: () => true}};
   box.skse.Log = () => {};
   const rows = [{text: "Bone", count: 3, infoValue: 2, infoWeight: 0.5, equipmentJunk: true}, {text: "Rag", count: 1, value: 1, infoWeight: 0.1, equipmentJunk: true}];
   let queued;
   Equipment.junkVerb = () => "sell";
   Equipment.junkRows = () => rows;
   Equipment.listsAnswer = () => true;
   Equipment.holdRows = () => {};
   Equipment.closeContext = () => {};
   Equipment.closePopup = () => {};
   Equipment.drawJunkAsk = () => {};
   Equipment.startQueue = (moving) => (queued = moving);
   Equipment.junkAsk = {_visible: false};
   said("a shop pays for every copy", Equipment.junkWorth("sell", rows[0]), 6);
   said("a row with no shop price takes its value", Equipment.junkWorth("sell", rows[1]), 1);
   said("a chest weighs every copy", Equipment.junkWorthWords("store", Equipment.junkWorth("store", rows[0])), "1.5");
   Equipment.moveJunk();
   said("nothing moves before the answer", queued, undefined);
   said("the window is up", Equipment.junkAsk._visible, true);
   Equipment.answerJunk(false);
   said("no leaves the junk where it is", queued, undefined);
   said("and shuts the window", Equipment.junkAsk._visible, false);
   Equipment.moveJunk();
   said("a key the window has no use for is taken all the same", Equipment.junkAskKey({navEquivalent: "left"}), true);
   Equipment.junkAskKey({navEquivalent: "enter"});
   said("Enter moves the junk", queued === rows, true);
   said("a key with the window gone goes on to the list", Equipment.junkAskKey({navEquivalent: "enter"}), false);
   queued = undefined;
   Equipment.moveJunk();
   Equipment.junkAskKey({navEquivalent: "tab"});
   said("Tab calls it off and keeps the menu", queued, undefined);
   Equipment.askJunk("sell", []);
   said("no junk, no window", Equipment.junkAsk._visible, false);
}

// a window that waits or never comes holds the model of a selected row back until it's up
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(false);
   const box = freshPanel.context;
   const calls = [];
   box.gfx.io = {GameDelegate: {call: (name, args) => calls.push(name + " " + args.join(","))}};
   Equipment.menu = { inventoryLists: { itemList: { selectedIndex: 3 } } };
   Equipment.window = {_visible: false};
   Equipment.settings = {windowShow: 1};
   Equipment.holdModels();
   const delegate = box.gfx.io.GameDelegate;
   delegate.call("UpdateItem3D", [true]);
   said("a row selected with no window up asks for no model", calls.join(";"), "");
   delegate.call("PlaySound", ["UIMenuFocus"]);
   said("other calls go on", calls.join(";"), "PlaySound UIMenuFocus");
   Equipment.window._visible = true;
   Equipment.askModel();
   said("the window that comes up asks for it", calls.join(";"), "PlaySound UIMenuFocus;UpdateItem3D true");
   Equipment.askModel();
   said("once", calls.length, 2);
   delegate.call("UpdateItem3D", [true]);
   said("the same row again while its window is up goes on", calls.length, 3);
   Equipment.menu.inventoryLists.itemList.selectedIndex = 4;
   delegate.call("UpdateItem3D", [true]);
   said("the next row waits though the last window is still up", calls.length, 3);
   delegate.call("UpdateItem3D", [false]);
   said("letting a model go goes on", calls[3], "UpdateItem3D false");
   Equipment.anchorSlot = {};
   delegate.call("UpdateItem3D", [true]);
   said("a slot's window asks as before", calls.length, 5);
   Equipment.anchorSlot = undefined;
   Equipment.settings.windowShow = 0;
   Equipment.window._visible = false;
   Equipment.menu.inventoryLists.itemList.selectedIndex = 6;
   delegate.call("UpdateItem3D", [true]);
   said("a window at once asks as before", calls.length, 6);
}

// In a follower's trade the panel stands for the follower: their side of the list is what they wear, their outfit
// fills the slots the list leaves empty, what goes on or off goes through the plugin, a row of the player's side is
// given through the menu's own move first
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(true);
   const box = freshPanel.context;
   const worn = [];
   const moves = [];
   box.skse.plugins.EquipmentPanel = {
      Wear: (index, form, left, on) => (worn.push([index, form, left, on].join(",")), true),
      Outfit: () => [armor(4, 31, 1, {outfit: true, text: "Steel Armor"}), {formType: W.WEAPON, weaponType: 1, infoDamage: 9, equipState: 3, outfit: true}],
   };
   box.gfx.io = {GameDelegate: {call: (name, args) => moves.push(name + " " + args.join(","))}};
   Equipment.kind = "container";
   Equipment.subject = {player: false, name: "Lydia"};
   Equipment.menu.inventoryLists.itemList = {_selectedIndex: 5, entryList: []};
   Equipment.menu.inventoryLists.categoryList.entryList[0].flag = 2048;
   said("the follower's side is before the divider", Equipment.wearerFlags().join(","), "2048,4096");
   const theirs = armor(1, 16, 1, {side: 2048, itemIndex: 0, formId: 7, text: "Iron Helmet"});
   const mine = armor(1, 23, 0, {side: 2, itemIndex: 1, formId: 9, text: "Ebony Helmet"});
   const playerWorn = armor(4, 40, 1, {side: 2, itemIndex: 2, formId: 11, text: "Glass Armor"});
   said("a row of their side is theirs", Equipment.wearersRow(theirs), true);
   said("a row of the player's side is not", Equipment.wearersRow(mine), false);
   const slots = Equipment.addOutfit(Equipment.worn([theirs, mine, playerWorn], Equipment.wearerFlags()));
   said("their worn helmet and the outfit, the player's own armor stays out", slots.armor.map((piece) => piece.infoArmor).join(","), "16,31");
   said("an outfit weapon holds the empty right hand", slots.right.infoDamage, 9);
   const outfit = box.skse.plugins.EquipmentPanel.Outfit;
   box.skse.plugins.EquipmentPanel.Outfit = () => [armor(4, 33, 1, {outfit: true, formId: 80210, text: "Steel Armor"})];
   const listed = armor(4, 33, 1, {side: 2048, itemIndex: 3, formId: 80210, text: "Steel Armor"});
   said("an outfit a mod lists among the rows counts once", Equipment.addOutfit(Equipment.worn([theirs, listed], Equipment.wearerFlags())).armor.map((piece) => piece.text).join(","), "Iron Helmet,Steel Armor");
   box.skse.plugins.EquipmentPanel.Outfit = outfit;
   said("an outfit piece can be taken off", Equipment.canTakeOff({entries: [slots.armor[1]]}), true);
   const offPieces = [];
   box.skse.plugins.EquipmentPanel.TakeOffPiece = (formId) => (offPieces.push(formId), true);
   const piece = armor(4, 31, 1, {outfit: true, formId: 80211, text: "Steel Armor"});
   said("an outfit piece comes off through the plugin by its form", Equipment.dressOther(piece, false, false) && offPieces.join(";"), "80211");
   said("and never goes on that way", Equipment.dressOther(piece, false, true), false);
   said("their own piece can", Equipment.canTakeOff({entries: [theirs]}), true);
   const keptSettings = Equipment.settings;
   Equipment.settings = Object.assign({}, keptSettings, {hideIcons: true});
   said("a follower's slot has an eye", Equipment.canHide(Equipment.slotNamed("head")), true);
   said("but not their hands", Equipment.canHide(Equipment.slotNamed("right")), false);
   box.skse.plugins.EquipmentPanel.ReadOnly = () => true;
   box.skse.plugins.EquipmentPanel.HungPieces = () => [];
   said("nor a follower a mod keeps as they are", Equipment.canHide(Equipment.slotNamed("head")), false);
   said("whose hidden slot isn't shown from here either", Equipment.canToggle(Equipment.slotNamed("head")), false);
   delete box.skse.plugins.EquipmentPanel.ReadOnly;
   Equipment.settings = keptSettings;
   said("their own row goes through the plugin", Equipment.dressOther(theirs, false, false), true);
   said("off, by its row and item", worn.join(";"), "0,7,false,false");
   said("a row of the player's side is given first", Equipment.dressOther(mine, false, true), true);
   said("by the menu's move from the player's side", moves.join(";"), "ItemTransfer 1,false");
   said("the selection is left as it was", Equipment.menu.inventoryLists.itemList._selectedIndex, 5);
   said("nothing taken off the player's side", Equipment.dressOther(mine, false, false), false);
   let later;
   box.setInterval = (run) => (later = run, 7);
   box.clearInterval = () => undefined;
   Equipment.wearGivenNow([mine, armor(1, 23, 0, {side: 2048, itemIndex: 3, formId: 9, text: "Ebony Helmet", count: 0})]);
   said("a gift the game refused leaves what they had", Equipment.wearNext, undefined);
   Equipment.wearGivenNow([mine, armor(1, 23, 0, {side: 2048, itemIndex: 3, formId: 9, text: "Ebony Helmet", count: 1})]);
   said("nothing goes on while the game fills the rows", worn.join(";"), "0,7,false,false");
   later();
   said("the given row goes on a moment after it turned up on their side", worn.join(";"), "0,7,false,false;3,9,false,true");
   said("and is waited for no more", Equipment.wearGiven, undefined);
   const lines = [];
   Equipment.wearLines(sword(9, 0), lines);
   said("their one handed weapon goes on in either hand", lines.map((line) => line.key).join(","), "wear,wearLeft,-");
   const off = [];
   Equipment.wearLines(theirs, off);
   said("their worn piece comes off", off[0].key + " " + off[0].words, "wear Take off");
   Equipment.menu.bFadedIn = true;
   Equipment.menu.inventoryLists.itemList.disableInput = false;
   Equipment.context = {_visible: false};
   said("the rows may be filled anew while nothing holds one", Equipment.refillReady(), true);
   Equipment.context._visible = true;
   said("not under the menu of a row", Equipment.refillReady(), false);
   Equipment.context._visible = false;
   Equipment.cardAsking = true;
   said("not while the slider asks how many", Equipment.refillReady(), false);
   Equipment.cardAsking = false;
   Equipment.heldEquip = {index: 1};
   said("not while a press waits to become a drag", Equipment.refillReady(), false);
   Equipment.heldEquip = undefined;
   box.skse.plugins.EquipmentPanel.ReadOnly = () => true;
   const before = worn.length;
   said("a follower a mod keeps look only takes nothing off", Equipment.dressOther(theirs, false, false), false);
   said("and gets nothing", Equipment.dressOther(mine, false, true), false);
   said("the plugin heard nothing", worn.length, before);
   said("their slot offers no take off", Equipment.canTakeOff({entries: [theirs]}), false);
   const looking = [];
   Equipment.wearLines(theirs, looking);
   said("their rows offer nothing to wear", looking.length, 0);
   Equipment.settings = {hideIcons: true};
   box.skse.plugins.EquipmentPanel.HungPieces = () => [];
   Equipment.hiddenMask = 0;
   Equipment.hiddenPieces = [];
   const why = Equipment.contextLines({name: "head", icon: "helmet", entries: [theirs]}).map((line) => line.key).join(",");
   said("the slot's menu says a mod keeps them as they are", why.indexOf("readOnly") >= 0 && why.indexOf("takeOff") < 0, true);
   box.skse.plugins.EquipmentPanel.ReadOnly = () => false;
   said("dressed again once the mod lets go", Equipment.canTakeOff({entries: [theirs]}), true);
   const spare = armor(1, 18, 0, {side: 2048, itemIndex: 4, formId: 13, text: "Steel Helmet"});
   Equipment.busy = false;
   Equipment.activeSlot = Equipment.slotNamed("head");
   Equipment.menu._platform = 0;
   said("a mouse's click on a row moves it as the game does", Equipment.padWears(spare), false);
   Equipment.menu._platform = 1;
   said("a pad's A on a row of the filtered list puts it in the slot", Equipment.padWears(spare), true);
   said("through the plugin", worn[worn.length - 1], "4,13,false,true");
   Equipment.busy = false;
   said("a row that doesn't fit the slot moves as the game does", Equipment.padWears(sword(9, 0)), false);
   Equipment.activeSlot = undefined;
   Equipment.menu._platform = 0;
   Equipment.subject = {player: true};
   said("a plain chest stands for the player as before", Equipment.wearerFlags().join(","), "2,4");
   said("and has no follower's rows", Equipment.wearersRow(theirs), false);
}

// Numbers and names from outside the mod: a word where a number goes, NaN, a "#" inside a name. ActionScript 2 reads
// "a <= NaN" as true where node reads it false, so these check what comes out rather than a loop that never ends
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(true);
   const box = freshPanel.context;

   said("a number of a word is 0", Equipment.number("lots"), 0);
   said("a number of NaN is 0", Equipment.number(NaN), 0);
   said("a number written as text counts", Equipment.number("12.5"), 12.5);
   said("a loop bound is capped", Equipment.count(1e9), 32);
   said("an endless one too", Equipment.count(Infinity), 32);
   said("a small one stays", Equipment.count(3), 3);

   said("a name with a sign in a numbered place", Equipment.say("#1 of #2", "Sword #2", "Bow"), "Sword #2 of Bow");
   said("a name with a sign in a place in turn", Equipment.say("# and #", "a#", "b"), "a# and b");
   said("places by number in their own order", Equipment.say("#2 then #1", "x", "y"), "y then x");
   said("words with no values keep their sign", Equipment.say("Slot #"), "Slot #");
   said("a sign with no value left stays", Equipment.say("# and #", "a"), "a and #");
   // the item window's From line: the sign ends the words, its piece is empty (AS2 read NaN there and kept the sign)
   said("a sign at the very end takes its value", Equipment.say("From #", "Fish.esm"), "From Fish.esm");

   Equipment.readSkin({Appearance: {colors: {text: {enabled: "dark"}}}});
   said("a skin's text colour of a word is no paper", Equipment.paper, false);
   Equipment.readSkin({Appearance: {colors: {text: {enabled: "0x141414"}}}});
   said("a colour written as text still counts", Equipment.paper, true);
   Equipment.readSkin({});

   const categories = Equipment.menu.inventoryLists.categoryList;
   categories.dividerIndex = "2";
   said("a divider written as text", Equipment.ownFlags().join(","), "2,4");
   categories.dividerIndex = "none";
   said("a divider of a word splits nothing", Equipment.ownFlags(), undefined);
   Equipment.kind = "container";
   Equipment.subject = {player: false, name: "Lydia"};
   categories.dividerIndex = 1e9;
   said("a divider past the list stops with the list", Equipment.wearerFlags().length, categories.entryList.length);
   categories.dividerIndex = NaN;
   said("a divider of NaN splits nothing", Equipment.wearerFlags(), undefined);
   categories.dividerIndex = 2;
   Equipment.kind = "inventory";
   Equipment.subject = {player: true};

   // a slot's category is looked for on one side, the chest's or follower's side has the player's flags moved up ten
   // bits: the Other slot of a follower's trade sent the list over to the player
   categories.activeSegment = 0;
   said("armor of the follower's side in view", Equipment.categoryWith(4), 1);
   said("their own flag on their side", Equipment.categoryWith(4096), 1);
   categories.activeSegment = 1;
   said("the player's armor on the player's side", Equipment.categoryWith(4), 4);
   said("their flag isn't looked for on the player's side", Equipment.categoryWith(4096), -1);
   categories.activeSegment = 0;
   said("an entry's own side wins over the side in view", Equipment.categoryWith(4, {side: 4}), 4);
   said("and theirs stays theirs", Equipment.categoryWith(4, {side: 4096}), 1);
   categories.activeSegment = undefined;

   // the list of a recharge on the card closes on Tab, Esc and a right press like the panel's menus
   const card = {HandleListMenuInput: () => true};
   card.handleInput = (details) => {
      card.closedBy = details.navEquivalent;
      card.InputHandler = undefined;
   };
   Equipment.menu.itemCard = card;
   said("no list, Tab is the menu's", Equipment.chargeKey({value: "keyDown", navEquivalent: "tab"}), false);
   card.InputHandler = card.HandleListMenuInput;
   said("an arrow walks the list", Equipment.chargeKey({value: "keyDown", navEquivalent: "down"}), false);
   said("Tab closes the list", Equipment.chargeKey({value: "keyDown", navEquivalent: "tab"}), true);
   said("through the card's own cancel", card.closedBy, "tab");
   card.InputHandler = card.HandleListMenuInput;
   said("Esc too", Equipment.chargeKey({value: "keyDown", navEquivalent: "escape-gamepad_B"}), true);
   card.InputHandler = card.HandleListMenuInput;
   said("a key let go does nothing", Equipment.chargeKey({value: "keyUp", navEquivalent: "tab"}), false);
   said("a right press closes it", Equipment.closeChargeList(), true);
   said("and finds nothing more to close", Equipment.closeChargeList(), false);
   Equipment.menu.itemCard = undefined;

   said("no time left for NaN", Equipment.timeLeft(NaN), "");
   said("nor for forever", Equipment.timeLeft(Infinity), "");
   said("nor for a word", Equipment.timeLeft("soon"), "");
   said("a time written as text", Equipment.timeLeft("90"), "1 min");

   const fills = [];
   box.flash = {geom: {Matrix: function() { this.createGradientBox = (wide) => fills.push(wide); }}};
   const page = {lineStyle() {}, beginFill() {}, endFill() {}, beginGradientFill() {}};
   const roundBox = Equipment.roundBox;
   Equipment.roundBox = () => undefined;
   Equipment.heroBar(page, 0, 0, 100, 8, 0 / 0, 0xFF0000);
   said("a pool of 0 of 0 fills nothing", fills.length, 0);
   Equipment.heroBar(page, 0, 0, 100, 8, 0.5, 0xFF0000);
   said("half a pool fills half the bar", fills.join(","), "49");
   Equipment.heroBar(page, 0, 0, 100, 8, Infinity, 0xFF0000);
   said("more than full fills it all", fills.join(","), "49,98");
   Equipment.roundBox = roundBox;

   const field = {_x: 10, _width: 100, _height: 20};
   said("icons step by the row's best icon", Equipment.iconsEnd({bestIcon: {_width: 16}}, field, {favorite: true}, {}), 10 + 100 + Equipment.ICON_GAP + 16 * Equipment.ICON_STEP);
   said("a row with no best icon steps by the name's height", Equipment.iconsEnd({bestIcon: {}}, field, {favorite: true, isEnchanted: true}, {}), 10 + 100 + Equipment.ICON_GAP + 2 * 20);

   // the voice of the magic menu has the ammo's place in the inventory's switches
   const keptSettings = Equipment.settings;
   Equipment.INVENTORY_SLOTS = Equipment.SLOTS;
   Equipment.settings = {slotsOff: 4};
   said("ammo off leaves the voice", Equipment.magicSlotShown(Equipment.MAGIC_SLOTS[2]), true);
   Equipment.settings = {slotsOff: 1};
   said("the right hand off takes the right hand", Equipment.magicSlotShown(Equipment.MAGIC_SLOTS[0]), false);
   said("and leaves the left", Equipment.magicSlotShown(Equipment.MAGIC_SLOTS[1]), true);
   Equipment.settings = keptSettings;
}

// a damage of a word compares as none
expect("sword, the held sword's damage is a word", difference([sword("lots", 3)], sword(10)), 10);
expect("arrow, the worn arrow's damage is a word", difference([bow(9, 3), arrow("lots", 1)], arrow(10)), 10);

// the eyes' own movie draws an eye for every six numbers, a stray number or a list with no length draws none
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const made = [];
   const holder = {createEmptyMovieClip: (name) => (made.push(name), {beginFill() {}, endFill() {}})};
   const context = {Math, _root: {eyes: {removeMovieClip() {}}, createEmptyMovieClip: () => holder}};
   // the movie finds _global's members by their bare names
   context._global = context;
   context.Equipment = {drawRing() {}, drawIcon() {}};
   vm.createContext(context);
   vm.runInContext(fs.readFileSync(path.join(__dirname, "..", "overlay", "eyes.as"), "utf8"), context, {filename: "eyes.as"});
   const eyes = (told) => (made.length = 0, context.Eyes.draw(told, 16 / 9), made.length);
   said("six numbers make an eye", eyes([0.1, 0.2, 0.05, 0, 1, 255]), 1);
   said("a stray seventh makes no second", eyes([0.1, 0.2, 0.05, 0, 1, 255, 0.4]), 1);
   said("twelve make two", eyes(new Array(12).fill(0.1)), 2);
   said("five make none", eyes([0.1, 0.2, 0.05, 0, 1]), 0);
   said("a list with no length makes none", eyes({}), 0);
}

// a row the keys chose stays while the pointer rests off the rows and nudges, a row the mouse chose goes when it leaves
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const list = {selectedIndex: 3, isMouseDrivenNav: false};
   Equipment.menu.inventoryLists.itemList = list;
   Equipment.menu._platform = 0;
   Equipment.context = {_visible: false};
   Equipment.lens = {_visible: false};
   Equipment.listsAnswer = () => true;
   Equipment.rowUnderMouse = () => undefined;
   Equipment.leaveRows();
   said("a row the keys chose stays", list.selectedIndex, 3);
   list.isMouseDrivenNav = true;
   Equipment.rowChosen({index: 3, keyboardOrMouse: 1});
   Equipment.leaveRows();
   said("so does the first one the keys chose, SkyUI leaves the list as the mouse's then", list.selectedIndex, 3);
   Equipment.rowChosen({index: 3, keyboardOrMouse: 0});
   Equipment.leaveRows();
   said("a row the mouse chose goes off the rows", list.selectedIndex, -1);
   list.selectedIndex = 2;
   Equipment.rowUnderMouse = () => ({text: "Iron Sword"});
   Equipment.leaveRows();
   said("and stays while the pointer is on a row", list.selectedIndex, 2);
}

// SkyUI's frame has a nine-slice grid of its own and grows by its height, the size it had comes back from the frame
// itself, a new frame SkyUI put up for another kind of card keeps its own
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const frame = (high, grid) => {
      const back = {_y: 0, _height: high, scale9Grid: grid ? {y: 27, height: 166} : undefined};
      back.getBounds = () => ({xMin: 0, xMax: 428, yMin: back._y, yMax: back._y + back._height});
      return back;
   };
   const card = {};
   Equipment.menu.itemCard = card;
   let back = frame(208, true);
   said("a frame with its own grid grows", Equipment.growByGrid(card, back, {shift: 40}), true);
   said("down by the shift", back.getBounds().yMax, 248);
   said("its top stays", back._y, 0);
   Equipment.cardFrameAsSkyUI(back);
   said("and gets SkyUI's height back", back._height, 208);
   Equipment.growByGrid(card, back, {shift: 40});
   const next = frame(153, true);
   Equipment.cardFrameAsSkyUI(next);
   said("a new frame for another kind keeps its own height", next._height, 153);
   said("a frame without a grid is left to the copies", Equipment.growByGrid(card, frame(208, false), {shift: 40}), false);
   const stiff = frame(208, true);
   stiff.getBounds = () => ({xMin: 0, xMax: 428, yMin: 0, yMax: 208});
   said("one that doesn't take the height stays as it was", Equipment.growByGrid(card, stiff, {shift: 40}), false);
   said("with its height back", stiff._height, 208);
}

// numbers a skin or another mod hands over may be missing: in AS2 a <= b and a >= b are true then, < and > are false
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const card = {background: {_width: 428, getBounds: () => ({xMin: -214, xMax: 214, yMin: -100, yMax: 100})}};
   const box = Equipment.frameBox(card, card);
   said("a frame drawn round its middle keeps its width", box.xMax - box.xMin, 428);
   card.background.getBounds = () => ({xMin: -20, xMax: 448, yMin: -20, yMax: 228});
   said("a frame with a clear margin round it still loses the margin", Equipment.frameBox(card, card).xMax - Equipment.frameBox(card, card).xMin, 428);
   said("edges a unit apart are the same", Equipment.sameEdge(10, 11), true);
   said("two units apart they aren't", Equipment.sameEdge(10, 12), false);
   said("a missing number is never the same edge", Equipment.sameEdge(NaN, 10), false);
   const wanted = [];
   Equipment.menu.inventoryLists.itemList = {selectedEntry: undefined};
   Equipment.wantWindow = (row) => wanted.push(row);
   Equipment.showWindow = () => undefined;
   Equipment.onHighlight({type: "hideItemsList"});
   Equipment.onHighlight({type: "itemHighlightChange", index: 3});
   said("an event of the lists with no index wants no window", wanted.join(","), "false,true");
   const holder = {_yscale: 70, _xscale: 70};
   Equipment.window = {};
   Equipment.menu.itemCard = {_parent: holder};
   Equipment.settings = {windowSize: 100};
   Equipment.squash = 1;
   Equipment.rowsEdge = () => ({wide: 800, right: 800});
   Equipment.cardWidth = () => 0;
   Equipment.windowUnits = () => undefined;
   Equipment.placeWindowLeft = () => undefined;
   Equipment.sizeWindow();
   said("a card with no width keeps its scale", holder._yscale, 70);
}

// a worn thing without its model shows the icon its row has in the list, from a mod's set or the list's own
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const loads = [];
   const fakeClip = () => {
      const clip = {_visible: true, clear: () => undefined};
      clip.createEmptyMovieClip = (name) => (clip[name] = fakeClip(), clip[name]._parent = clip, clip[name]);
      clip.gotoAndStop = (label) => clip.label = label;
      clip.getBounds = () => ({xMin: 0, yMin: 0, xMax: 40, yMax: 40});
      return clip;
   };
   freshPanel.context.MovieClipLoader = function() {
      let listener;
      this.addListener = (one) => listener = one;
      this.loadClip = (source, target) => { loads.push(source); target.loaded = true; listener.onLoadInit(target); };
   };
   freshPanel.context.flash = {geom: {ColorTransform: function() {}, Transform: function() {}}};
   const signs = [];
   Equipment.drawIcon = (clip, icon) => signs.push(icon);
   Equipment.menu.inventoryLists.itemList = {listState: {iconSource: "skyui/icons_item_psychosteve.swf"}};
   Equipment.settings = {slotModels: false};
   Equipment.plugin = () => ({});
   const box = fakeClip();
   box.icon = fakeClip();
   const slot = {name: "body", icon: "body", box, entries: [armor(4, 27, 1, {iconSource: "I4IconAddon/icons.swf", iconLabel: "clothing_backpack", iconColor: 0xe07020})]};
   Equipment.showIcon(slot);
   said("the row's own set is loaded", loads.join(","), "I4IconAddon/icons.swf");
   said("at the row's label", box.icon.row.art.label, "clothing_backpack");
   said("and no sign is drawn", signs.length, 0);
   slot.entries = [armor(4, 27, 1, {iconLabel: "armor_body"})];
   Equipment.showIcon(slot);
   said("a row with no set of its own takes the list's", loads[loads.length - 1], "skyui/icons_item_psychosteve.swf");
   said("at its label", box.icon.row.art.label, "armor_body");
   Equipment.showIcon(slot);
   said("the same set isn't loaded again", loads.length, 2);
   slot.entries = [];
   Equipment.showIcon(slot);
   said("an empty slot hides the icon", box.icon.row._visible, false);
   said("and shows its sign", signs.join(","), "body");
   slot.entries = [armor(4, 27, 1, {iconLabel: "armor_body"})];
   Equipment.settings.slotModels = true;
   Equipment.showIcon(slot);
   said("a slot with its model shows no icon", box.icon.row._visible, false);
}

// A teammate's drained weapon gets recharged in their trade: a line of the row menu, the card's list of the soul gems
// of both, the rows of the weapon and of the gem the player picks go to the plugin
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(true);
   const box = freshPanel.context;
   const calls = [];
   const charges = {21: 40, 22: 100, 23: -1};
   const shown = [];
   box.skse.plugins.EquipmentPanel = {
      ChargeOf: (index, form) => charges[form],
      SoulGems: (index, form) => form == 21 ? {type: 14, currentCharge: 40, listItems: [{text: "Grand Soul Gem", owner: "", count: 1, chargeAdded: 300, itemIndex: 4, formId: 80},
         {text: "Petty Soul Gem", owner: "Lydia", count: 2, chargeAdded: 25, itemIndex: 7, formId: 90}]} : undefined,
      Recharge: (weapon, weaponForm, gem, gemForm) => (calls.push(`Recharge ${weapon},${weaponForm},${gem},${gemForm}`), true),
   };
   box.gfx.io = {GameDelegate: {call: (name, args) => calls.push(name + " " + args.join(","))}};
   Equipment.kind = "container";
   Equipment.settings = {};
   Equipment.subject = {player: true};
   Equipment.closeContext = () => undefined;
   const menu = Equipment.menu;
   menu.bNPCMode = true;
   menu.bFadedIn = true;
   menu.isViewingContainer = () => true;
   menu.itemCard = {itemInfo: {}, ItemList: {entryList: [], addEventListener: () => undefined}};
   menu.UpdateItemCardInfo = (info) => {
      shown.push(info.type + ":" + info.listItems.length);
      menu.itemCard.ItemList.entryList = info.listItems;
   };
   menu.inventoryLists.itemList = {entryList: [], disableInput: false, disableSelection: false};
   const lines = (entry) => Equipment.rowLines(entry).filter((line) => line.key !== "-").map((line) => line.words).join(" | ");
   const blade = (form, extra) => Object.assign(sword(9, 0), {itemIndex: form - 20, formId: form, count: 1, text: "Sword " + form}, extra || {});
   said("a drained sword of the one traded with", lines(blade(21)), "Recharge | Take");
   said("a full one has nothing to recharge", lines(blade(22)), "Take");
   said("one without an enchantment neither", lines(blade(23)), "Take");
   menu.isViewingContainer = () => false;
   said("the player's own side recharges nothing in a trade", lines(blade(21)).indexOf("Recharge"), -1);
   menu.isViewingContainer = () => true;
   menu.bNPCMode = false;
   said("a chest is no teammate", lines(blade(21)), "Take");
   menu.bNPCMode = true;
   Equipment.subject = {player: false, name: "Lydia"};
   said("with the panel up, among what goes on them", lines(blade(21, {side: 2048})), "Equip | Equip in left hand | Recharge | Take");
   Equipment.subject = {player: true};

   const drained = blade(21);
   menu.inventoryLists.itemList.selectedEntry = drained;
   menu.inventoryLists.itemList.entryList = [drained];
   Equipment.pickRow = () => true;
   Equipment.rowAct("charge", drained);
   said("the line puts the list of gems on the card", shown.join(";"), "14:2");
   said("the teammate's gem says whose it is", menu.itemCard.ItemList.entryList.map((line) => line.text).join(" / "), "Grand Soul Gem / Lydia's Petty Soul Gem");
   said("the list holds the row it charges", Equipment.charging.weapon.formId, 21);
   Equipment.chargeAsks({type: "subMenuAction", opening: true, menu: "list"});
   said("the game holds the item menu's keys while the list is up", calls.join(";"), "QuantitySliderOpen true");
   Equipment.lineChosen({index: 1});
   said("the weapon's row and the gem's row picked go to the plugin", calls[calls.length - 1], "Recharge 1,21,7,90");
   Equipment.chargeAsks({type: "subMenuAction", opening: false, menu: "list"});
   said("the list gone, the card asks for the item again", calls.slice(-2).join(";"), "QuantitySliderOpen false;RequestItemCardInfo ");
   said("and the charge is over", Equipment.charging, undefined);
   Equipment.lineChosen({index: 0});
   said("a pick without a list up does nothing", calls.filter((call) => call.indexOf("Recharge") == 0).length, 1);
   shown.length = 0;
   said("a row the plugin can't charge shows no list", Equipment.chargeTeammate(blade(23)) || shown.length, 0);
   Equipment.chargeAsks({type: "subMenuAction", opening: true, menu: "quantity"});
   said("the slider of a move is none of it", calls.filter((call) => call == "QuantitySliderOpen true").length, 1);
}

// A soul gem's row charges a weapon of the one whose inventory is open: a line of its menu, the card's list of the
// drained weapons, the meter of the one the list points at. The inventory keeps the game's own recharge on its card
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(false);
   const box = freshPanel.context;
   const calls = [];
   const shown = [];
   const meter = [];
   const weapons = {type: 14, name: "Choose Soul Gem to Use", listItems: [{text: "Sword of Frost", count: 1, charge: 40, chargeAdded: 25, itemIndex: 3, formId: 21}, {text: "Staff", count: 1, charge: 10, chargeAdded: 8, itemIndex: 5, formId: 33}], currentCharge: 40};
   box.skse.plugins.EquipmentPanel = {
      Weapons: (index, form) => form == 90 ? weapons : undefined,
      Recharge: (weapon, weaponForm, gem, gemForm) => (calls.push(`Recharge ${weapon},${weaponForm},${gem},${gemForm}`), true),
   };
   box.gfx.io = {GameDelegate: {call: (name, args) => calls.push(name + " " + args.join(","))}};
   Equipment.kind = "inventory";
   Equipment.settings = {};
   Equipment.subject = {player: true};
   const menu = Equipment.menu;
   const list = {entryList: [], selectedEntry: undefined, addEventListener: () => undefined};
   const listeners = [];
   menu.itemCard = {itemInfo: {}, ItemList: list, ItemCardMeters: {14: {SetPercent: (to) => meter.push("at " + to), SetDeltaPercent: (to) => meter.push("to " + to)}},
      addEventListener: (event, scope, name) => listeners.push(event + ":" + name)};
   menu.UpdateItemCardInfo = (info) => {
      shown.push(info.name + ":" + info.listItems.length);
      list.entryList = info.listItems;
      list.selectedEntry = info.listItems[0];
   };
   menu.inventoryLists.itemList = {entryList: [], disableInput: false, disableSelection: false};
   const lines = (entry) => Equipment.rowLines(entry).filter((line) => line.key !== "-").map((line) => line.words).join(" | ");
   const gem = (form) => ({formType: 52, itemIndex: form - 80, formId: form, count: 2, text: "Petty Soul Gem", equipState: 0});
   said("a gem that can charge a weapon of the player's", lines(gem(90)), "Recharge a weapon | Drop one | Drop all (2) | Add to favorites");
   said("one with no soul or nothing drained to charge has no such line", lines(gem(91)), "Drop one | Drop all (2) | Add to favorites");
   Equipment.kind = "barter";
   menu.isViewingVendorItems = () => false;
   said("a shop charges nothing", lines(gem(90)), "Sell one | Sell all (2)");
   Equipment.kind = "container";
   menu.bNPCMode = false;
   menu.isViewingContainer = () => true;
   said("nor a chest", lines(gem(90)), "Take one | Take all (2)");
   menu.bNPCMode = true;
   said("a gem of either side of a trade charges the teammate's weapons", lines(gem(90)), "Recharge a weapon | Take one | Take all (2)");
   Equipment.kind = "inventory";

   Equipment.pickRow = () => true;
   Equipment.rowAct("chargeWith", gem(90));
   said("the card asks for the weapon", shown.join(";"), "Choose a weapon to charge:2");
   said("its meter shows the first weapon's charge and what the gem adds", meter.join(", "), "at 40, to 65");
   list.selectedEntry = weapons.listItems[1];
   Equipment.chargeMeter();
   said("and follows the list to the next", meter.slice(-2).join(", "), "at 10, to 18");
   Equipment.chargeAsks({type: "subMenuAction", opening: true, menu: "list"});
   said("the inventory holds its keys itself", calls.length, 0);
   Equipment.lineChosen({index: 1});
   said("the weapon picked and the gem's row go to the plugin", calls[calls.length - 1], "Recharge 5,33,10,90");
   Equipment.chargeAsks({type: "subMenuAction", opening: false, menu: "list"});
   said("the inventory asks for its card again itself", calls.length, 1);
   said("the charge is over", Equipment.charging, undefined);

   const vanilla = [];
   menu.onItemCardListPress = (event) => vanilla.push(event.index);
   Equipment.startCharging();
   said("the card's list goes to the panel", listeners.join(","), "itemPress:lineChosen,subMenuAction:chargeAsks");
   menu.onItemCardListPress({index: 2});
   said("the game's own list of gems for the player's weapon goes on to the game", vanilla.join(","), "2");
   Equipment.charging = {gem: {itemIndex: 10, formId: 90}};
   menu.onItemCardListPress({index: 0});
   said("a list of the panel's never reaches the game's recharge", vanilla.join(","), "2");
   Equipment.charging = undefined;
}


// a chest's side in view takes the chest's junk, the player's side stores the player's
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(true);
   Equipment.kind = "container";
   let chestInView = true;
   Equipment.menu.bNPCMode = false;
   Equipment.menu.isViewingContainer = () => chestInView;
   const mine = {formId: 1, text: "Pelt", equipmentJunk: true, side: 2, count: 3, equipState: 0};
   const theirs = {formId: 2, text: "Plate", equipmentJunk: true, side: 2048, count: 2, equipState: 0};
   const kept = {formId: 3, text: "Cup", side: 2048, count: 1, equipState: 0};
   Equipment.menu.inventoryLists.itemList = {entryList: [mine, theirs, kept]};
   said("the chest's side takes", Equipment.junkVerb(), "take");
   said("the chest's junk", Equipment.junkRows().map((row) => row.text).join(","), "Plate");
   said("its button says so", Equipment.junkWords("take", 2), "Take all junk (2)");
   chestInView = false;
   said("the player's side stores", Equipment.junkVerb(), "store");
   said("the player's junk", Equipment.junkRows().map((row) => row.text).join(","), "Pelt");
   Equipment.menu.bNPCMode = true;
   said("a follower's trade has no junk button", Equipment.junkVerb(), undefined);
}

// the subcategories chosen in a family come back from the plugin's memory, only keys its rows still have
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(false);
   const kept = {};
   Equipment.plugin = () => ({Remember: (key, words) => { kept[key] = words; }, Recall: (key) => kept[key] !== undefined ? kept[key] : ""});
   const family = Object.keys(Equipment.CHIP_ROWS)[0];
   const rows = Equipment.CHIP_ROWS[family];
   Equipment.chipsFamily = family;
   Equipment.activeSlot = undefined;
   Equipment.chosenChips = [];
   Equipment.chosenChips[rows.length - 1] = rows[rows.length - 1][0].key;
   Equipment.keepChips();
   const back = Equipment.keptChips(family);
   said("a chosen chip comes back in its row", back[rows.length - 1], rows[rows.length - 1][0].key);
   said("an empty row stays empty", back[0] === undefined || rows.length === 1, true);
   kept["chips"] = family + "|no such chip";
   said("a key the rows lost is dropped", Equipment.keptChips(family)[0], undefined);
   said("a family with nothing kept starts empty", Equipment.keptChips("nothing here").length, 0);
   Equipment.activeSlot = {name: "head"};
   kept["chips"] = "";
   Equipment.chosenChips = [rows[0][0].key];
   Equipment.keepChips();
   said("the chips of a slot filter aren't kept", kept["chips"], "");
   Equipment.activeSlot = undefined;
   kept["chips"] = "armor|clothing";
   said("another family gets nothing of what was kept", Equipment.keptChips("potions").length, 0);
   said("its own family gets it back", Equipment.keptChips("armor")[0], "clothing");

   // a pick that leaves no row with the other groups lets them go
   const lists = Equipment.menu.inventoryLists;
   lists.categoryList.selectedEntry = {flag: 4};
   lists._typeFilter = {isMatch: () => true};
   lists.itemList = {entryList: [armor(4, 0, 0, {weightClass: 3}), armor(0x1000, 10, 0, {weightClass: 0})]};
   Equipment.chipsFamily = "armor";
   Equipment.chosenChips = ["clothing", "other"];
   Equipment.leaveRowsOpen(1);
   said("Other alone stays after Clothing left nothing", Equipment.chosenChips.join(","), ",other");
   Equipment.chosenChips = ["clothing", "body"];
   Equipment.leaveRowsOpen(1);
   said("a pick that leaves rows keeps the other groups", Equipment.chosenChips.join(","), "clothing,body");

   // the strip takes two lines when the junk button has its own
   const strip = {_y: 100, listHeight: 600, entryHeight: 30, _maxListIndex: 20, requestUpdate() {}};
   lists.itemList = strip;
   Equipment.menu.updateDynamicListHeight = undefined;
   Equipment.roomHigh = 0;
   Equipment.openStrip(true, 2);
   said("two lines move the list down twice", strip._y, 100 + 2 * Equipment.STRIP_HIGH);
   Equipment.openStrip(true, 1);
   said("one line gives one back", strip._y, 100 + Equipment.STRIP_HIGH);
   said("and the height with it", strip.listHeight, 600 - Equipment.STRIP_HIGH);
}

// a spell in a hand has no row in the inventory, the plugin names it and its slot shows SkyUI's sign of its school
{
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const Equipment = freshPanel(false);
   let hands = [];
   Equipment.plugin = () => ({Hands: () => hands});
   Equipment.settings = {};
   const healing = {formId: 0x12FCC, name: "Healing", item: false, spell: true, school: 22, resistance: -1, cost: 13.4};
   const flames = {formId: 0x12FCD, name: "Flames", item: false, spell: true, school: 20, resistance: 41, cost: 9};
   const empty = {formId: 0, name: "", item: false, spell: false};
   hands = [healing, flames, empty];
   Equipment.place({armor: []});
   const right = Equipment.slotNamed("right").entries[0];
   const left = Equipment.slotNamed("left").entries[0];
   said("a restoration spell takes its school's sign", right.iconLabel, "default_restoration");
   said("with no colour", right.iconColor, undefined);
   said("a fire spell takes the fire sign", left.iconLabel, "magic_fire");
   said("in SkyUI's fire red", left.iconColor, 0xC73636);
   said("the number of a spell is its cost", Equipment.amount(right), "13");
   said("a spell can't be taken off", Equipment.canTakeOff(Equipment.slotNamed("left")), false);
   said("nor inspected", Equipment.inspectable(left), false);
   Equipment.noIconColors = true;
   Equipment.place({armor: []});
   said("SkyUI's plain icons leave the colour out", Equipment.slotNamed("left").entries[0].iconColor, undefined);
   Equipment.noIconColors = false;
   hands = [{formId: 5, name: "Spark", spell: true, school: 20, resistance: -1}, empty, empty];
   Equipment.place({armor: []});
   said("a destruction spell with no element takes the school's sign", Equipment.slotNamed("right").entries[0].iconLabel, "default_destruction");
   hands = [{formId: 6, name: "Odd", spell: true, school: -1, resistance: -1}, empty, empty];
   Equipment.place({armor: []});
   said("a spell of no school takes SkyUI's sign for one", Equipment.slotNamed("right").entries[0].iconLabel, "default_power");
   hands = [healing, flames, empty];
   const held = greatsword(20, 3);
   Equipment.place({armor: [], right: held, left: held});
   said("a weapon in the hand keeps its slot", Equipment.slotNamed("right").entries[0], held);
   said("a two-handed one takes the left hand", Equipment.slotNamed("left").taken, true);
   said("and leaves no room for a spell there", Equipment.slotNamed("left").entries.length, 1);
   Equipment.place({armor: [], left: torch(2)});
   said("a torch keeps the left hand", Equipment.slotNamed("left").entries[0].formType, W.LIGHT);
   said("while the right hand shows its spell", Equipment.slotNamed("right").entries[0].text, "Healing");
   hands = undefined;
   Equipment.plugin = () => ({});
   Equipment.place({armor: []});
   said("a plugin with no hands leaves them empty", Equipment.slotNamed("left").entries.length, 0);
}

// a gamepad reaches the companions' tabs: up from the top row, the subcategories past them, A opens the one under
// the focus and says nothing on the one already shown
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const where = (at) => at.kind + (at.column !== undefined ? ` ${at.column},${at.row}` : "") + (at.index !== undefined ? ` ${at.index}` : "");
   const slot = (column, row) => ({kind: "slot", column, row});
   const world = (extra) => Object.assign({rows: 5, chips: 0, chipStart: 0, people: 0, personStart: 0, hero: false, heroOpen: false, last: slot(1, 0)}, extra);
   const go = (at, way, extra) => where(Equipment.padNext(at, way, world(extra)));
   const person = (index) => ({kind: "person", index});
   said("up from the top row to the tab of the one shown", go(slot(0, 0), "up", {people: 3, personStart: 1}), "person 1");
   said("before the subcategories", go(slot(1, 0), "up", {people: 2, chips: 4}), "person 0");
   said("along the tabs", go(person(0), "right", {people: 3}), "person 1");
   said("the tabs end", go(person(2), "right", {people: 3}), "person 2");
   said("up from the tabs to the subcategories", go(person(1), "up", {people: 3, chips: 4, chipStart: 2}), "chip 2");
   said("nothing above the tabs without them", go(person(1), "up", {people: 3}), "person 1");
   said("down from the tabs to the top of the last column", go(person(1), "down", {people: 3, last: slot(0, 3)}), "slot 0,0");
   said("down from the subcategories to the tabs", go({kind: "chip", index: 1}, "down", {people: 2, chips: 4, personStart: 1}), "person 1");

   const switched = [];
   const you = {_visible: true, shown: true, person: {formId: 0, player: true}};
   const lydia = {_visible: true, shown: false, person: {formId: 0xA2C94, player: false}};
   const far = {_visible: false, shown: false, person: {formId: 7, player: false}};
   Equipment.companions = {_visible: true, tab0: you, tab1: lydia, tab2: far};
   Equipment.switchCompanion = (one) => switched.push(one.formId);
   Equipment.listsAnswer = () => true;
   Equipment.padMark = () => undefined;
   Equipment.padHints = () => undefined;
   said("the tabs with room on the row", Equipment.padPeople().length, 2);
   said("A on the one shown says nothing", Equipment.padPressWords(person(0)), undefined);
   said("A on a companion opens their trade", Equipment.padPressWords(person(1)), "Open");
   Equipment.padFocus = person(1);
   Equipment.padPress();
   said("and opens it", switched.join(","), String(0xA2C94));
   Equipment.companions._visible = false;
   Equipment.padLast = slot(1, 2);
   Equipment.padFocus = person(1);
   Equipment.slotShown = () => true;
   Equipment.padSettle();
   said("tabs gone, the focus goes back to the last slot", where(Equipment.padFocus), "slot 1,2");
}

// a gamepad in the magic menu: three slots walked where they stand, A narrows the list, X takes a weapon off the
// hand, the hold inspects nothing there
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const where = (at) => at.kind + (at.column !== undefined ? ` ${at.column},${at.row}` : "");
   const slot = (column, row) => ({kind: "slot", column, row});
   Equipment.INVENTORY_SLOTS = Equipment.SLOTS;
   Equipment.SLOTS = Equipment.MAGIC_SLOTS;
   Equipment.slotShown = Equipment.magicSlotShown;
   Equipment.settings = {slotsOff: 0};
   Equipment.hero = {};
   Equipment.padLast = slot(0, 0);
   const go = (at, way) => where(Equipment.padNext(at, way, Equipment.padWorld()));
   said("down from the right hand to the left", go(slot(0, 0), "down"), "slot 0,1");
   said("nothing under the left hand", go(slot(0, 1), "down"), "slot 0,1");
   said("across from the right hand to the voice", go(slot(0, 0), "right"), "slot 1,0");
   said("and from the left hand", go(slot(0, 1), "right"), "slot 1,0");
   said("nothing under the voice", go(slot(1, 0), "down"), "slot 1,0");
   said("back across to the right hand", go(slot(1, 0), "left"), "slot 0,0");
   Equipment.settings = {slotsOff: 1};
   said("the right hand off the panel, up from the left stays", go(slot(0, 1), "up"), "slot 0,1");
   Equipment.settings = {slotsOff: 0};

   const taken = [];
   const right = Equipment.slotNamed("right");
   Equipment.buildPad = () => undefined;
   Equipment.readHandsSoon = () => taken.push("read");
   Equipment.plugin = () => ({TakeOffHand: (place) => { taken.push("off " + place); return true; }});
   Equipment.startMagicPad();
   said("the pad comes to the right hand first", where(Equipment.padLast), "slot 0,0");
   said("A narrows the list", Equipment.padPressWords(slot(0, 0)), "Show what fits");
   Equipment.activeSlot = right;
   said("and lets it go again", Equipment.padPressWords(slot(0, 0)), "Show everything");
   Equipment.activeSlot = undefined;
   Equipment.padFocus = slot(0, 0);
   right.entries = [{formId: 0x12EB7, text: "Iron Sword", item: true}];
   right.taken = false;
   said("X takes a sword off", Equipment.padHideWords(), "Take off");
   Equipment.padHide();
   said("by the plugin, the hands read again", taken.join(","), "off 0,read");
   right.entries = [{formId: 0x12FCD, text: "Flames", formType: 22}];
   said("a spell has no X", Equipment.padHideWords(), undefined);
   Equipment.padFocus = slot(1, 0);
   Equipment.slotNamed("voice").entries = [];
   said("nor an empty voice", Equipment.padHideWords(), undefined);

   const inspected = [];
   Equipment.inspectEntry = (entry) => inspected.push(entry.text);
   Equipment.padFocus = slot(0, 0);
   right.entries = [{formId: 0x12EB7, text: "Iron Sword", item: true}];
   Equipment.padInspect();
   said("the hold inspects nothing without the inspect view", inspected.length, 0);
   said("nor offers it", Equipment.padInspectable(right.entries[0]), false);
   const stick = [];
   Equipment.panel = {};
   Equipment.menu._platform = 1;
   Equipment.inspecting = false;
   Equipment.listsAnswer = () => true;
   Equipment.enterPad = () => stick.push("panel");
   Equipment.padInspect = () => stick.push("inspect");
   Equipment.padMark = () => undefined;
   Equipment.padTurns = () => undefined;
   Equipment.padFocus = undefined;
   Equipment.padStick(true);
   said("a slow click of the stick there still goes to the panel", stick.join(","), "panel");
   Equipment.viewer = {};
   said("the inventory's view does", Equipment.padInspectable(right.entries[0]), true);
}

// a gamepad and the menu of a row: a hold of the stick on a row of the list opens it, on the panel the hold inspects,
// with the row menu switched off it inspects on the list too. Up and down walk the lines, A presses the lit one, B
// and the stick close it, and the game's own calls for the row wait while it's up
{
   const Equipment = freshPanel(false);
   const said = (name, got, wanted) => got === wanted ? passed++ : (failed++, console.log(`FAIL ${name}: got "${got}", wanted "${wanted}"`));
   const done = [];
   const buttons = [];
   const key = (value, nav, code) => ({value, navEquivalent: nav, skseKeycode: code});
   const line = (name, y) => Object.assign(new freshPanel.context.MovieClip(), {what: {key: name, words: name}, _y: y, onPress: () => done.push("press " + name)});
   const context = {_visible: false};
   const open = () => Object.assign(context, {_visible: true, line3: line("favorite", 65), line0: line("equip", 4), line1: line("drop", 30)});
   freshPanel.context.gfx.io = {GameDelegate: {call: () => undefined}};
   Object.assign(Equipment.menu, {
      _platform: 1,
      _acceptControls: {}, _cancelControls: {},
      navPanel: {clearButtons: () => { buttons.length = 0; }, addButton: (data) => buttons.push(data.text), updateButtons: () => undefined},
   });
   Equipment.menu.inventoryLists.itemList = {selectedEntry: {formId: 7, text: "Iron Sword"}, selectedIndex: 3};
   Object.assign(Equipment, {panel: {}, viewer: {}, context, settings: {rowMenu: true}});
   Equipment.listsAnswer = () => true;
   Equipment.padMark = () => undefined;
   Equipment.padTurns = () => undefined;
   Equipment.openRowMenu = (entry) => { done.push("menu of " + entry.text); open(); return true; };
   Equipment.padPlaceMenu = () => undefined;
   Equipment.drawContextLine = (one, lit) => lit && done.push("lit " + one.what.key);
   Equipment.closeContext = () => { context._visible = false; done.push("closed"); };
   Equipment.padInspect = () => done.push("inspect");

   Equipment.padStick(true);
   said("a hold on a row opens its menu, the first line lit", done.join(","), "menu of Iron Sword,lit equip");
   said("the bar tells A and B", buttons.join(" | "), "Choose | Back");
   said("the lines go top to bottom", Equipment.padLines().map((one) => one.what.key).join(","), "equip,drop,favorite");
   done.length = 0;
   said("down walks the menu", Equipment.padInput(key("keyDown", "down")), true);
   Equipment.padInput(key("keyHold", "down"));
   Equipment.padInput(key("keyDown", "down"));
   Equipment.padInput(key("keyDown", "up"));
   said("down and up along the lines, the last one stays", done.join(","), "lit drop,lit favorite,lit favorite,lit drop");
   said("left keeps the category", Equipment.padInput(key("keyDown", "left")), true);
   said("the game's drop of the row waits", Equipment.padHolds(), true);
   done.length = 0;
   Equipment.padInput(key("keyDown", "enter-gamepad_A", 276));
   said("A presses the lit line", done.join(","), "press drop");
   Equipment.padInput(key("keyUp", "enter-gamepad_A", 276));
   done.length = 0;
   Equipment.padInput(key("keyDown", "tab", 277));
   said("B closes the menu", done.join(","), "closed");
   said("held it stays off the list", Equipment.padInput(key("keyHold", "tab", 277)), true);
   Equipment.padInput(key("keyUp", "tab", 277));
   said("nothing waits once it's closed", Equipment.padHolds(), false);
   open();
   done.length = 0;
   Equipment.padStick(false);
   said("the stick closes it as well", done.join(","), "closed");

   done.length = 0;
   Equipment.padFocus = {kind: "slot", column: 1, row: 0};
   Equipment.padStick(true);
   said("a hold on the panel inspects", done.join(","), "inspect");
   Equipment.padFocus = undefined;
   Equipment.settings.rowMenu = false;
   done.length = 0;
   Equipment.padStick(true);
   said("with the row menu off a hold on a row inspects", done.join(","), "inspect");

   Equipment.inspectable = () => true;
   Equipment.settings.rowMenu = true;
   buttons.length = 0;
   Equipment.padListHint();
   said("the list tells the hold for the menu", buttons.join(" | "), "Panel | Menu (hold)");
   Equipment.settings.rowMenu = false;
   buttons.length = 0;
   Equipment.padListHint();
   said("or for inspecting with the menu off", buttons.join(" | "), "Panel | Inspect (hold)");
}

if (withCalls) {
   failed += withCalls.report();
}
console.log(`${passed} passed, ${failed} failed`);
process.exit(failed ? 1 : 0);
