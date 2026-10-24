// Runs the debug edition of the panel outside the game, the way the debug build makes it: every function with its
// calls in (debug_calls.py), the debug parts in their places. The calls keep this, the arguments and the result, a
// missing or older plugin leaves the panel as it is, and the pass copes with the odd corners of the script
//   node scripts/interface/equipment/tests/debug.test.cjs

const fs = require("fs");
const path = require("path");
const vm = require("vm");
const calls = require("./calls.cjs");

const source = path.join(__dirname, "..", "source");
const debug = path.join(__dirname, "..", "debug");
const plain = ["10-core.as", "12-compare.as", "20-rows.as", "21-marks.as", "22-sorting.as", "24-subcategories.as", "26-draw.as", "27-queue.as", "28-junk.as", "28-junk-ask.as", "29-chip-menu.as", "30-panel.as", "31-strip.as", "33-context.as", "34-drag.as", "35-models.as", "36-window.as", "37-inspect.as", "38-stats.as", "38-hero.as", "38-look.as", "39-row-menu.as", "39-gamepad.as", "39-card-numbers.as", "39-companions.as"];
// in the order build.py puts them: the log right after the core, the entries before the start, the search after it
const files = [path.join(source, plain[0]), path.join(debug, "11-log.as"), ...plain.slice(1).map((part) => path.join(source, part)), path.join(debug, "39-entries.as"), path.join(source, "40-start.as"), path.join(debug, "41-finding.as")];
const beforeStart = files.length - 2;
const stamp = 'Equipment.DEBUG_BUILD = "1.4.0 debug test";\n';
const panel = calls.panel(files, files.map((file) => (path.basename(file) === "11-log.as" ? stamp : "") + fs.readFileSync(file, "utf8")));

// the plugin as the debug panel sees it, every call noted. Without a_functions no plugin, with a list only those
function recorder(a_functions) {
   const calls = [];
   const plugin = {};
   for (const name of a_functions || ["Names", "Enter", "Leave", "Log", "Said", "Count", "Started"]) {
      plugin[name] = (...args) => calls.push([name, ...args]);
   }
   return {calls, plugin};
}

function freshPanel(plugin, start) {
   const listeners = [];
   const gameCalls = [];
   const skseLines = [];
   const context = {
      _global: {},
      skse: {plugins: plugin ? {EquipmentPanel: plugin} : {}, Log: (text) => skseLines.push(text)},
      Stage: {visibleRect: {x: 0, y: 0, width: 1280, height: 720}, safeRect: {x: 0, y: 0}},
      Math, String, Number, Array,
      getTimer: () => Date.now(),
      setInterval: () => 1,
      clearInterval: () => undefined,
      MovieClip: function() {},
      Mouse: {addListener: (listener) => listeners.push(listener), getTopMostEntity: () => "_level0.Menu_mc.slot"},
      _root: {_xmouse: 10, _ymouse: 20},
      skyui: {util: {Translator: {translate: (key) => key}}, defines: {Input: {XButton: {name: "XButton"}}}},
      gfx: {ui: {NavigationCode: {UP: "up", DOWN: "down", LEFT: "left", RIGHT: "right", ENTER: "enter-gamepad_A", ESCAPE: "escape-gamepad_B", TAB: "tab", SHIFT_TAB: "shifttab"}},
         io: {GameDelegate: {call(name, args) { gameCalls.push([this, name, args]); return "sent " + name; }}}},
      Shared: {GlobalFunc: {IsKeyPressed: (details) => details.value == "keyDown"}},
   };
   context._global.EquipmentPanelMenu = "inventory";
   vm.createContext(context);
   // the calls find the plugin of the context on their own, as in the movie
   panel.load(context, beforeStart, false);
   if (start) {
      panel.loadRest(context, beforeStart);
   }
   return {Equipment: context._global.Equipment, context, listeners, gameCalls, skseLines};
}

let failed = 0;
let passed = 0;
function same(name, got, wanted) {
   if (JSON.stringify(got) === JSON.stringify(wanted)) {
      passed++;
   } else {
      failed++;
      console.log(`FAIL ${name}: got ${JSON.stringify(got)}, wanted ${JSON.stringify(wanted)}`);
   }
}

// every function of the panel sends its name, numbered from 0 with no gap
{
   const {calls: heard, plugin} = recorder();
   freshPanel(plugin, true);
   const names = [];
   for (const call of heard.filter((call) => call[0] === "Names")) {
      call[1].forEach((name, at) => (names[call[2] + at] = name));
   }
   same("every function has a name", names.length === panel.count && names.every((name) => typeof name === "string"), true);
   // the whole start ran with the plugin there, each end closed what began last
   const open = [];
   let broken = 0;
   let began = 0;
   for (const call of heard.filter((call) => call[0] === "Enter" || call[0] === "Leave")) {
      if (call[0] === "Enter") {
         open.push(call[1]);
         began++;
      } else if (open.pop() !== call[1]) {
         broken++;
      }
   }
   same("the start of the panel calls its functions, breaks no pair and leaves none open", [began > 0, broken, open.length], [true, 0, 0]);
   same("a name says the part, the line and what it's set to", names.includes("10-core.as:" + (fs.readFileSync(path.join(source, "10-core.as"), "utf8").split("\n").findIndex((line) => line.startsWith("Equipment.count = function")) + 1) + " Equipment.count"), true);
}

// a function of the panel hands this, its arguments and its result through, the plugin hears it begin and end
{
   const {calls: heard, plugin} = recorder();
   const {Equipment} = freshPanel(plugin);
   const before = heard.length;
   same("the result stays", Equipment.count("3.7"), 3);
   const told = heard.slice(before).filter((call) => call[0] === "Enter" || call[0] === "Leave");
   same("one begin and one end with the same number", told.length === 2 && told[0][0] === "Enter" && told[1][0] === "Leave" && told[0][1] === told[1][1], true);
   const holder = {tag: "holder", has: Equipment.has};
   same("this and the arguments stay", holder.has([1, "two"], "two"), true);
}

// with no plugin, or one without the debug functions, the panel runs as it would
for (const [name, plugin] of [["no plugin", undefined], ["an older plugin", recorder(["Log"]).plugin]]) {
   const {Equipment} = freshPanel(plugin);
   same(`${name}: a function still gives its result`, [Equipment.count(21.5), Equipment.count(undefined)], [21, 0]);
   same(`${name}: the panel's own lines go nowhere quietly`, [Equipment.debugLog("x"), Equipment.debugSay("k", "x"), Equipment.debugCount("x")], [undefined, undefined, undefined]);
}

// the panel says which build it is
{
   const {calls: heard, plugin} = recorder();
   freshPanel(plugin);
   same("the panel says which build it is", heard.find((call) => call[0] === "Started")[1], "1.4.0 debug test, inventory");
}

// once built the panel says so with the shape of the menu, and the keys the menu gets are told
{
   const {calls: heard, plugin} = recorder();
   const {Equipment} = freshPanel(plugin);
   const Menu = function() {};
   Menu.prototype.handleInput = function() { return "skyui"; };
   const menu = new Menu();
   menu.handleInput = function(details) { return details.value === "keyDown"; };
   menu.inventoryLists = {itemList: {entryList: [1, 2, 3]}};
   Equipment.menu = menu;
   // the state report reads the built panel, there is none here
   Equipment.panelReport = (note) => heard.push(["report", note]);
   Equipment.debugAfterJoin();
   same("the state follows the build", heard.some((call) => call[0] === "report" && call[1] === "after the build"), true);
   same("the panel's handleInput keeps its answer", [menu.handleInput({value: "keyDown", navEquivalent: "up"}), menu.handleInput({value: "keyUp"})], [true, false]);
   same("a key is told", heard.filter((call) => call[0] === "Said" && call[1] === "key").length, 2);
   menu.handleInput({value: "keyHold"});
   same("a held key is only counted", heard.filter((call) => call[0] === "Count" && call[1] === "key held").length, 1);
   const built = heard.find((call) => call[0] === "Log" && call[1].startsWith("the panel is built"));
   same("the build is told with the shape of the menu", built !== undefined && built[1].includes("3 rows") && built[1].includes("itemCard missing"), true);
}

// what the menu asks of the game goes through as it was asked, frequent calls only counted
{
   const {calls: heard, plugin} = recorder();
   const {Equipment, context, gameCalls} = freshPanel(plugin);
   const delegate = context.gfx.io.GameDelegate;
   same("the game call answers as before", delegate.call("ItemSelect", [1]), "sent ItemSelect");
   same("the game call keeps this and arguments", [gameCalls[0][0] === delegate, gameCalls[0][1], gameCalls[0][2]], [true, "ItemSelect", [1]]);
   delegate.call("UpdateItem3D", [true]);
   same("a call is told", heard.some((call) => call[0] === "Said" && call[2] === "game call ItemSelect(1)"), true);
   same("a frequent call is counted", heard.some((call) => call[0] === "Count" && call[1] === "game call UpdateItem3D"), true);
   Equipment.debugWatchGame();
   delegate.call("ItemSelect", []);
   same("the game call is wrapped once", gameCalls.length, 3);
}

// skse.Log still writes the log of SKSE, the test stand reads it there, and the plugin's log gets it too
{
   const {calls: heard, plugin} = recorder();
   const {context, skseLines} = freshPanel(plugin);
   context.skse.Log("PANELTEST line");
   same("skse.Log still writes", skseLines, ["PANELTEST line"]);
   same("the plugin hears it", heard.some((call) => call[0] === "Said" && call[1] === "skse.Log" && call[2] === "PANELTEST line"), true);
}

// a press is told with what lies under the pointer, a move only counted
{
   const {calls: heard, plugin} = recorder();
   const {Equipment, listeners} = freshPanel(plugin);
   same("the debug listener of the mouse is added", listeners.includes(Equipment.debugMouse), true);
   Equipment.debugMouse.onMouseDown(0);
   Equipment.debugMouse.onMouseMove();
   same("a press is told", heard.some((call) => call[0] === "Said" && call[2] === "mouse press 0 at 10,20 on _level0.Menu_mc.slot"), true);
   same("a move is counted", heard.some((call) => call[0] === "Count" && call[1] === "mouse move"), true);
}

// a menu that never comes is told once after its tries, the search answers as before
{
   const {calls: heard, plugin} = recorder();
   const {Equipment} = freshPanel(plugin, true);
   const answers = [];
   for (let i = 0; i < Equipment.DEBUG_MISSES + 5; i++) {
      answers.push(Equipment.findMenu());
   }
   same("the search answers as before", answers.every((answer) => answer === false), true);
   same("the wait is told once", heard.filter((call) => call[0] === "Log" && call[1].startsWith("the panel still waits")).length, 1);
}

// The odd corners of the script. A bit of script goes through the pass and runs with a stand-in for the plugin
// that keeps every begin and end, each end has to close what began last
function run(text) {
   const made = calls.instrumented([text], ["test.as"]);
   const checked = calls.pairs(true);
   const context = {_global: {}, skse: {plugins: {}}};
   vm.createContext(context);
   vm.runInContext(calls.prelude, context);
   context.EquipmentCalls.plugin = checked.plugin;
   vm.runInContext("var Equipment = {};\n" + made.parts[0], context);
   return {Equipment: context.Equipment, told: checked.told, open: checked.open, count: made.count, text: made.parts[0]};
}

// the numbers of a run of begins and ends, by the names of the functions
function trail(told) {
   return told.trail.map(([what, id]) => `${what === "Enter" ? "+" : "-"}${told.names[id].replace(/^test\.as:\d+ /, "")}`);
}

// a return that ends its function with no semicolon, on lines of its own and on one line
{
   const {Equipment, told, open} = run(`
Equipment.last = function(a)
{
   return a + 1
};
Equipment.tight = function(a) { return a * 2 }
Equipment.lines = function(a, b)
{
   if(a > b) return a
   return a +
      b
};
`);
   same("a return with no semicolon keeps its value", [Equipment.last(2), Equipment.tight(4), Equipment.lines(5, 2), Equipment.lines(1, 2)], [3, 8, 5, 3]);
   same("each of them begins and ends", [told.entered, told.left, told.broken.length, open.length], [4, 4, 0, 0]);
}

// returns inside nested functions and inside one-line ifs belong to their own function
{
   const {Equipment, told} = run(`
Equipment.outer = function(x)
{
   var inner = function(y)
   {
      if(y > 1) return "big";
      return "small";
   };
   if(x == 0) return inner(x);
   if(x < 0)
      return;
   else if(x == 1) { return "one" }
   return inner(x) + "!";
};
`);
   same("each return gives its own value", [Equipment.outer(0), Equipment.outer(-1), Equipment.outer(1), Equipment.outer(5)], ["small", undefined, "one", "big!"]);
   same("the inner function opens and closes inside the outer one", trail(told).slice(0, 4), ["+Equipment.outer", "+inner", "-inner", "-Equipment.outer"]);
   same("no pair broke", told.broken, []);
}

// "function" and "return" in strings and comments are no code
{
   const {Equipment, told, count} = run(`
// a function that would return nothing here: function() { return 1; }
Equipment.words = function()
{
   /* return early? function() {} */
   var text = "function() { return 'x'; }";
   var other = 'return "function" \\'here\\'';
   return text + "|" + other; // return function
};
`);
   same("strings and comments stay as they are", Equipment.words(), "function() { return 'x'; }|return \"function\" 'here'");
   same("only the real function counts", [count, told.entered, told.left], [1, 1, 1]);
}

// a function written right in the arguments of a call
{
   const {Equipment, told} = run(`
Equipment.twice = function(f, v) { return f(f(v)); };
Equipment.useTwice = function(v)
{
   return Equipment.twice(function(n) { return n * 3; }, v);
};
`);
   same("the function given keeps working", Equipment.useTwice(2), 18);
   same("it's named by the call it's given to and nests right", trail(told), ["+Equipment.useTwice", "+Equipment.twice", "+a function given to Equipment.twice", "-a function given to Equipment.twice",
      "+a function given to Equipment.twice", "-a function given to Equipment.twice", "-Equipment.twice", "-Equipment.useTwice"]);
}

// functions as members of an object written out in place
{
   const {Equipment, told} = run(`
Equipment.tools = {tag:"tools", double:function(n) { return n * 2; }, half:function(n)
{
   return n / 2;
}, who:function() { return this.tag; }, count:function() { return arguments.length; }, comma:function() { var a = 1; return a++, a; }};
Equipment.nothing = function() {};
`);
   const tools = Equipment.tools;
   same("members keep their results, this and their arguments", [tools.double(4), tools.half(4), tools.who(), tools.count(1, 2, 3), tools.comma(), Equipment.nothing()], [8, 2, "tools", 3, 2, undefined]);
   same("members are named by their keys", trail(told).filter((step) => step.startsWith("+")), ["+double", "+half", "+who", "+count", "+comma", "+Equipment.nothing"]);
   same("no pair broke", [told.broken.length, told.entered, told.left], [0, 6, 6]);
}

// a value on the line after its return reads two ways, the pass won't guess
{
   let refused = false;
   try {
      calls.instrumented(["Equipment.odd = function()\n{\n   return\n      5;\n};\n"], ["test.as"]);
   } catch (error) {
      refused = String(error).includes("next line");
   }
   same("a return with its value on the next line is refused", refused, true);
}

console.log(`${passed} passed, ${failed} failed`);
process.exit(failed > 0 ? 1 : 0);
