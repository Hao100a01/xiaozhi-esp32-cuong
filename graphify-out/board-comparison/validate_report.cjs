const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');
const base = __dirname;
const page = fs.readFileSync(path.join(base, 'boards.html'), 'utf8');
const script = page.match(/<script>([\s\S]*?)<\/script>/)[1];
const elements = Object.fromEntries(['q','chip','feature','count','summary','rows'].map(id=>[id,{value:'',innerHTML:'',textContent:'',add(){}}]));
const sandbox = {...elements, document:{getElementById:id=>elements[id]},Option:function(text,value){return {text,value}}};
vm.createContext(sandbox);
vm.runInContext(script, sandbox);
assert.equal(elements.count.textContent, '152 / 152 board');
elements.chip.value = 'esp32c3'; sandbox.render();
assert.equal(elements.count.textContent, '10 / 152 board');
elements.chip.value = ''; elements.q.value = 'otto-robot'; sandbox.render();
assert.equal(elements.count.textContent, '1 / 152 board');
elements.q.value = ''; elements.feature.value = 'web'; sandbox.render();
assert.equal(elements.count.textContent, '2 / 152 board');
elements.feature.value = 'release'; sandbox.render();
assert.equal(elements.count.textContent, '150 / 152 board');
const data = JSON.parse(fs.readFileSync(path.join(base,'boards.json'),'utf8'));
assert.equal(new Set(data.boards.map(r=>r.board)).size,152);
const root = path.resolve(base,'../..');
let checked = 0;
for (const board of data.boards) {
  for(const entries of Object.values(board.evidence)) {
    for(const ref of entries) {
      const text = fs.readFileSync(path.join(root,ref.file),'utf8').split(/\r?\n/);
      assert(ref.line>=1 && ref.line<=text.length,`${ref.file}:${ref.line}`);
      if(ref.line!==1 || !ref.file.endsWith('config.json')) assert.equal(text[ref.line-1].trim().slice(0,260),ref.text);
      checked++;
    }
  }
}
console.log(`Report checks passed: 152 boards; filters/search; ${checked} evidence locations.`);
