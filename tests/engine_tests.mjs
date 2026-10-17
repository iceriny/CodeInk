import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { Reader, Writer, REQUEST_MAGIC, RESPONSE_MAGIC, parseRequest, successResponse, errorResponse, normalizeForParser, mapTokens, colorNumber } from '../engine/protocol.mjs';
import { loadThemeFile } from '../engine/theme-loader.mjs';
const req = (texts=['中文😀\r\nint x=1;'], lang='cpp') => {const w=new Writer().u32(REQUEST_MAGIC).u32(1).text(lang).text('github-light').text('').u32(texts.length);for(const s of texts)w.text(s);return w.finish();};
test('UTF-16 request round trip preserves Chinese, emoji, tabs, line endings',()=>{const texts=['😀\r\n中\v\t', 'print("x")'];assert.deepEqual(parseRequest(req(texts)).texts,texts);});
test('reject truncated request',()=>assert.throws(()=>parseRequest(req().subarray(0,-1))));
test('reject wrong protocol magic or trailing bytes',()=>{const b=req();b[0]=0;assert.throws(()=>parseRequest(b));assert.throws(()=>parseRequest(Buffer.concat([req(),Buffer.from([1])])));});
test('reject invalid batch and per-object limits',()=>{assert.throws(()=>parseRequest(req([])));assert.throws(()=>parseRequest(req(Array(33).fill('a'))));assert.throws(()=>parseRequest(req(['x'.repeat(50001)])));});
test('reject aggregate limit',()=>assert.throws(()=>parseRequest(req(Array(9).fill('a'.repeat(50000))))));
test('CRLF and PowerPoint soft breaks map to exact original UTF-16 offsets',()=>{const r='中😀\r\nx\vy\r';const n=normalizeForParser(r);assert.equal(n.text,'中😀\nx\ny\n');assert.deepEqual(n.map,[0,1,2,3,5,6,7,8,9]);});
test('mapped token offsets do not drift after CRLF or emoji',()=>{const raw='中😀\r\nint x;';const n=normalizeForParser(raw);const a=mapTokens(raw,n,[[{content:'中😀',color:'#f00'}],[{content:'int',color:'#00f'},{content:' x;',color:'#000'}]],0,0xffffff);assert.deepEqual(a.tokens.map(t=>[t.start,t.length]),[[0,3],[5,3],[8,3]]);assert.equal(a.length,11);});
test('coalesces adjacent identical runs and treats unspecified style as zero',()=>{const x=mapTokens('ab',{text:'ab',map:[0,1,2]},[[{content:'a',color:'#fff',fontStyle:-1},{content:'b',color:'#ffffff'}]],0,0xffffff);assert.equal(x.tokens.length,1);assert.equal(x.tokens[0].length,2);assert.equal(x.tokens[0].style,0);});
test('validates token content and complete line boundaries',()=>{assert.throws(()=>mapTokens('abc',normalizeForParser('abc'),[[{content:'ab'}]],0,0));assert.throws(()=>mapTokens('abc',normalizeForParser('abc'),[[{content:'xyz'}]],0,0));assert.throws(()=>mapTokens('a\nb',normalizeForParser('a\nb'),[[{content:'a'}]],0,0));});
test('empty text, blank lines and trailing newline',()=>{for(const s of ['', '\n', 'x\n\n']){const n=normalizeForParser(s);const tokens=n.text.split('\n').map(x=>x?[{content:x}]:[]);assert.equal(mapTokens(s,n,tokens,0,0).length,s.length);}});
test('RGB formats and alpha blend',()=>{assert.equal(colorNumber('#abc'),0xaabbcc);assert.equal(colorNumber('#123456'),0x123456);assert.equal(colorNumber('#00000000',0,0xffffff),0xffffff);assert.equal(colorNumber('#00000080',0,0xffffff),0x7f7f7f);assert.equal(colorNumber('bogus',0x112233),0x112233);});
test('success response records ranges and theme colors',()=>{const b=successResponse([{length:3,tokens:[{start:0,length:3,color:0x123456,style:2}]}],{fg:1,bg:2,gutter:3});const r=new Reader(b);assert.equal(r.u32(),RESPONSE_MAGIC);assert.deepEqual(Array.from({length:11},()=>r.u32()),[0,1,2,3,1,3,1,0,3,0x123456,2]);r.end();});
test('error text is bounded',()=>{const r=new Reader(errorResponse('中'.repeat(2000)));assert.equal(r.u32(),RESPONSE_MAGIC);assert.equal(r.u32(),1);assert.equal(r.text().length,1000);r.end();});
// Strict JSON adapter for loader mechanics only. Real jsonc-parser comments and
// trailing commas are tested by bridge.mjs --self-test during Windows setup.
const parseJSON = (text,errors)=>{try{return JSON.parse(text);}catch{errors.push({error:'parse'});return undefined;}};
function withFiles(files, callback){const dir=mkdtempSync(join(tmpdir(),'codeink-unit-'));try{for(const [name,value]of Object.entries(files))writeFileSync(join(dir,name),typeof value==='string'?value:JSON.stringify(value));return callback(dir);}finally{rmSync(dir,{recursive:true,force:true});}}
const base={type:'light',colors:{'editor.background':'#fff'},tokenColors:[{scope:'keyword',settings:{foreground:'#00f'}}]};
test('theme inheritance merges colors and token rules without extension execution',()=>withFiles({'base.json':base,'child.json':{include:'./base.json',colors:{'editor.foreground':'#222'},tokenColors:[{scope:'string',settings:{foreground:'#080'}}]}},dir=>{const t=loadThemeFile(join(dir,'child.json'),parseJSON);assert.equal(t.tokenColors.length,2);assert.equal(t.colors['editor.background'],'#fff');assert.match(t.name,/^codeink-custom-/);assert.equal(t.include,undefined);}));
test('rejects include cycles and absolute references',()=>withFiles({'a.json':{...base,include:'b.json'},'b.json':{...base,include:'a.json'},'c.json':{...base,include:'/tmp/evil.json'}},dir=>{assert.throws(()=>loadThemeFile(join(dir,'a.json'),parseJSON),/循环/);assert.throws(()=>loadThemeFile(join(dir,'c.json'),parseJSON),/相对/);}));
test('rejects empty-token or invalid JSON theme and external plist',()=>withFiles({'a.json':{},'b.json':'{bad','c.json':{tokenColors:'external.tmTheme'}},dir=>{for(const f of ['a.json','b.json','c.json'])assert.throws(()=>loadThemeFile(join(dir,f),parseJSON));}));
test('accepts UTF-8 BOM and produces stable theme identity',()=>withFiles({'a.json':'\ufeff'+JSON.stringify(base)},dir=>{assert.equal(loadThemeFile(join(dir,'a.json'),parseJSON).name,loadThemeFile(join(dir,'a.json'),parseJSON).name);}));

test('Unicode LS/PS map to line breaks without UTF-16 offset drift',()=>{const raw='a\u2028中😀\u2029';const n=normalizeForParser(raw);assert.equal(n.text,'a\n中😀\n');assert.deepEqual(n.map,[0,1,2,3,4,5,6]);const result=mapTokens(raw,n,[[{content:'a',color:'#000'}],[{content:'中😀',color:'#f00'}],[]],0,0);assert.deepEqual(result.tokens.map(t=>[t.start,t.length]),[[0,1],[2,3]]);});

// Diagnostics must not create persistent files unless explicitly opted in.
import { diagnosticEvent } from '../engine/diagnostics.mjs';
import { existsSync, statSync, readFileSync } from 'node:fs';
test('diagnostics default off: neither new files nor existing files are written',()=>withFiles({},dir=>{
  assert.equal(diagnosticEvent(dir,new Error('PRIVATE SOURCE')),false);
  assert.equal(existsSync(join(dir,'engine-error.txt')),false);
  writeFileSync(join(dir,'engine-error.txt'),'old');
  assert.equal(diagnosticEvent(dir,new Error('PRIVATE SOURCE'),false),false);
  assert.equal(readFileSync(join(dir,'engine-error.txt'),'utf8'),'old');
}));
test('diagnostics enabled: records category, never source, error message or stack',()=>withFiles({},dir=>{
  const error=Object.assign(new Error('const privateCode = 123;'),{code:'ERR_MODULE_NOT_FOUND'});
  assert.equal(diagnosticEvent(dir,error,true),true);
  const contents=readFileSync(join(dir,'engine-error.txt'),'utf8');
  assert.match(contents,/ERR_MODULE_NOT_FOUND/);assert.doesNotMatch(contents,/privateCode|const|Error:|engine_tests/);
}));
test('diagnostics can be turned off after being enabled without deleting prior data',()=>withFiles({},dir=>{
  diagnosticEvent(dir,new Error('x'),true);const original=readFileSync(join(dir,'engine-error.txt'),'utf8');
  diagnosticEvent(dir,new Error('y'),false);assert.equal(readFileSync(join(dir,'engine-error.txt'),'utf8'),original);
}));
test('diagnostic I/O failure is nonfatal and untrusted error codes are not persisted',()=>withFiles({},dir=>{
  assert.equal(diagnosticEvent(join(dir,'missing'),new Error('x'),true),false);
  diagnosticEvent(dir,{code:'const secret = 10'},true,'source code here');
  const contents=readFileSync(join(dir,'engine-error.txt'),'utf8');
  assert.match(contents,/bridge ERROR/);assert.doesNotMatch(contents,/secret|source code/);
}));
