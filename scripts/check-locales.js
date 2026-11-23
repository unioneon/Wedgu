// Run with Node.js. Catalog coverage catches missing strings before release.
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.join(__dirname,'..');
const dir=path.join(root,'res','locales');const names=fs.readdirSync(dir).filter(n=>n.endsWith('.json'));assert.equal(names.length,13);
const en=JSON.parse(fs.readFileSync(path.join(dir,'en.json'),'utf8'));const keys=Object.keys(en).sort();
for(const n of names){const catalog=JSON.parse(fs.readFileSync(path.join(dir,n),'utf8'));assert.deepEqual(Object.keys(catalog).sort(),keys,n+' coverage differs');for(const [k,v]of Object.entries(catalog))assert(typeof v==='string'&&v.length,n+': empty '+k);}
const exempt=new Set(['Ташкент','\nПогода: ']);
for(const f of ['settings_ui.cpp','color_picker.cpp','main.cpp','environment.cpp','app_settings.cpp','location_service.cpp','lockscreen.cpp'])for(const m of fs.readFileSync(path.join(root,'src',f),'utf8').matchAll(/L"((?:[^"\\]|\\.)*)"/g)){const value=JSON.parse('"'+m[1]+'"');if(/[А-Яа-яЁё]/.test(value)&&!exempt.has(value))assert(value in en,`${f}: missing ${value}`);}
console.log(`13 catalogs, ${keys.length} messages each; UI/status coverage passed.`);
