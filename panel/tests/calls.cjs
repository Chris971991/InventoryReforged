// The parts of the panel the way the debug build puts them into the movie, with the calls of debug_calls.py in
// every function. The tests run them in node like the plain parts
//   PANEL_CALLS=1 node scripts/interface/equipment/tests/logic.test.cjs   runs the logic tests on them
// The python of tools/venv runs the pass, PYTHON names another one

const {spawnSync} = require("child_process");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const equipment = path.join(__dirname, "..");
const venv = path.join(equipment, "..", "..", "..", "tools", "venv", "Scripts", "python.exe");
const prelude = fs.readFileSync(path.join(equipment, "debug", "09-calls.as"), "utf8");

function python() {
   return process.env.PYTHON || (fs.existsSync(venv) ? venv : "python");
}

// the texts with the calls put in and how many functions they have
function instrumented(texts, titles) {
   const run = spawnSync(python(), ["-I", path.join(equipment, "debug_calls.py")], {input: JSON.stringify({parts: texts, titles}), maxBuffer: 256 * 1024 * 1024});
   if (run.status !== 0) {
      throw new Error(`debug_calls.py failed: ${run.error || run.stderr.toString()}`);
   }
   return JSON.parse(run.stdout.toString("utf8"));
}

// Stands in for the plugin's Enter and Leave. Every leave has to close the function opened last, so a way out the
// pass missed or a call it put in twice shows up as a broken pair
function pairs(keepTrail) {
   const open = [];
   const told = {entered: 0, left: 0, broken: [], names: [], trail: []};
   return {
      told,
      open,
      plugin: {
         Names: (names, first) => { for (let at = 0; at < names.length; at++) told.names[first + at] = names[at]; },
         Enter: (id) => {
            told.entered++;
            open.push(id);
            if (keepTrail) told.trail.push(["Enter", id]);
         },
         Leave: (id) => {
            told.left++;
            if (keepTrail) told.trail.push(["Leave", id]);
            if (open[open.length - 1] === id) {
               open.pop();
            } else {
               told.broken.push(`leave ${id} with ${JSON.stringify(open.slice(-3))} open`);
            }
         },
      },
   };
}

// The panel's parts with the calls in, made once. load runs them into a context the way the movie runs them, the
// calls' own part first, with a stand-in that checks the pairs. a_texts may give a part's text in place of its file
function panel(files, a_texts) {
   const texts = a_texts || files.map((file) => fs.readFileSync(file, "utf8"));
   const made = instrumented(texts, files.map((file) => path.basename(file)));
   const loaded = [];
   return {
      count: made.count,
      // a_until: how many parts to run, all of them when left out. a_plugin false leaves the stand-in out, the
      // calls then find the plugin of the context the way the movie does
      load(context, a_until, a_plugin) {
         const checked = pairs();
         vm.runInContext(prelude, context, {filename: "09-calls.as"});
         if (a_plugin !== false) {
            context.EquipmentCalls.plugin = checked.plugin;
         }
         made.parts.slice(0, a_until === undefined ? made.parts.length : a_until).forEach((text, at) => vm.runInContext(text, context, {filename: path.basename(files[at])}));
         loaded.push(checked);
         return checked;
      },
      // the rest of the parts after a load that stopped early
      loadRest(context, a_from) {
         made.parts.slice(a_from).forEach((text, at) => vm.runInContext(text, context, {filename: path.basename(files[a_from + at])}));
      },
      // a line on the pairs of every panel loaded, and how many of them broke
      report() {
         const entered = loaded.reduce((sum, one) => sum + one.told.entered, 0);
         const left = loaded.reduce((sum, one) => sum + one.told.left, 0);
         const broken = loaded.flatMap((one) => one.told.broken);
         const open = loaded.filter((one) => one.open.length > 0).length;
         console.log(`with the calls in: ${made.count} functions, ${loaded.length} panels, ${entered} calls began and ${left} ended, ${broken.length} broken pairs, ${open} panels left a call open`);
         for (const line of broken.slice(0, 10)) {
            console.log(`FAIL broken pair: ${line}`);
         }
         return broken.length + open;
      },
   };
}

module.exports = {instrumented, panel, pairs, prelude};
