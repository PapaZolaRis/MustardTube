#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
'use strict';
/* MustardTube radio discovery; no external npm packages. No audio downloaded.
 * Data: radio-browser.info (public directory) and country M3U fallback (CC0).
 * All network requests are bounded; data is filtered before reaching the C UI.
 */
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const crypto = require('node:crypto');
const net = require('node:net');
const {URL} = require('node:url');
const dataDir = process.env.MUSTARDTUBE_DATA_DIR || path.join(__dirname,'..','data');
const API_HOSTS = ['https://de1.api.radio-browser.info','https://de2.api.radio-browser.info','https://all.api.radio-browser.info'];
const PLAYLIST_ORIGIN='https://raw.githubusercontent.com/AlonDrilich/radio-playlists/main/countries/';
const BATCH=20;
const MAX_COUNTRIES=250;

const LOGO_CACHE=path.join(dataDir,'radio-logo-cache');
const LOGO_MAX_BYTES=48*1024, LOGO_MAX_PIXEL=128, LOGO_CACHE_LIMIT=3*1024*1024;
function validLogoUrl(value){
 if(!isPublicUrl(value))return false;
 try{const u=new URL(value);return u.protocol==='https:' && !net.isIP(u.hostname) && (!u.port || u.port==='443') && value.length<=550;}catch{return false;}
}
function cleaned(s,limit=110){return String(s??'').replace(/[\t\r\n\x00-\x1f\x7f\u202a-\u202e\u2066-\u2069]/g,' ').replace(/\|/g,' ').replace(/\s+/g,' ').trim().slice(0,limit);}
function validCountry(s){return /^[A-Z]{2}$/.test(s);}
function isPublicUrl(value){
  try {
    let u=new URL(value);
    if(!['http:','https:'].includes(u.protocol)||u.username||u.password||u.hash||!u.hostname||value.length>750)return false;
    let h=u.hostname.toLowerCase().replace(/\.$/,'').replace(/^\[(.*)\]$/,'$1');
    if(net.isIP(h)===6)return false;  // fail closed; IPv6 may be ULA/loopback/link-local
    if(net.isIP(h)===4){
      let p=h.split('.').map(Number);
      if(p[0]===0||p[0]===10||p[0]===127||p[0]>=224||p[0]===169&&p[1]===254||p[0]===172&&p[1]>=16&&p[1]<=31||p[0]===192&&p[1]===168||p[0]===100&&p[1]>=64&&p[1]<=127||p[0]===192&&p[1]===0||p[0]===198&&p[1]>=18&&p[1]<=19)return false;
    }else if(!h.includes('.')||h.endsWith('.local')||h.endsWith('.internal')||h.endsWith('.localhost')||h.endsWith('.test')||h.endsWith('.home')||h.endsWith('.lan'))return false;
    return true;
  }catch{return false;}
}
async function limitedFetch(url,maxBytes,timeoutMs=6500){
 const ac=new AbortController();const timer=setTimeout(()=>ac.abort(),timeoutMs);
 try {
   const r=await fetch(url,{signal:ac.signal,headers:{'User-Agent':'MustardTube-radio/0.9.11 (community beta)','Accept':'application/json,text/plain;q=0.7'},redirect:'follow'});
   if(!r.ok)throw Error('HTTP '+r.status);
   // Hosts in redirection chains are not under application control; only data is read and parsed.
   const len=Number(r.headers.get('content-length')||0);if(len>maxBytes)throw Error('Too large');
   let chunks=[],total=0;
   for await(const chunk of r.body){total+=chunk.length;if(total>maxBytes){ac.abort();throw Error('Too large');}chunks.push(chunk);}
   return Buffer.concat(chunks).toString('utf8');
 }finally{clearTimeout(timer);}
}
async function api(pathname,maxBytes=250000){let last;for(let h of API_HOSTS){try{return await limitedFetch(h+pathname,maxBytes);}catch(e){last=e;}}throw last||Error('No radio directory');}
function atomic(name,data){fs.mkdirSync(dataDir,{recursive:true,mode:0o700});const f=path.join(dataDir,name);const tmp=f+'.tmp.'+process.pid;fs.writeFileSync(tmp,data,{encoding:'utf8',mode:0o600});fs.renameSync(tmp,f);}
function countryName(code){try{return new Intl.DisplayNames(['en'],{type:'region'}).of(code)||code;}catch{return code;}}
async function countries(){
 let response=JSON.parse(await api('/json/countrycodes?order=name&reverse=false&hidebroken=true&limit=250',160000));
 if(!Array.isArray(response))throw Error('Invalid country list');
 let seen=new Set();const rows=[];
 for(let c of response){let code=String(c.name||'').toUpperCase();if(!validCountry(code)||seen.has(code))continue;seen.add(code);
 let count=Number(c.stationcount)||0;if(count<1)continue;rows.push([code,cleaned(countryName(code),64),Math.max(0,Math.floor(count))].join('|||'));if(rows.length>=MAX_COUNTRIES)break;}
 if(!rows.length)throw Error('No countries');atomic('radio-countries.tsv',rows.join('\n')+'\n');return rows.length;
}
function stationRows(items,country){
 const rows=[],seen=new Set();
 for(let s of items){if(!s||typeof s!=='object')continue;
   let u=String(s.url_resolved||s.url||'').trim(),codec=cleaned(s.codec||'Audio',16).toUpperCase(),name=cleaned(s.name,100);
   if(!name||!isPublicUrl(u)||!(/^(MP3|AAC|AAC\+|OGG|OPUS|MPEG|AUDIO|UNKNOWN|HLS)$/.test(codec)))continue;
   if(seen.has(u))continue;seen.add(u);
   let id=String(s.stationuuid||'').toLowerCase();if(!/^[0-9a-f-]{16,40}$/.test(id))id=crypto.createHash('sha256').update(u).digest('hex').slice(0,32);
   let favicon=String(s.favicon||'').trim();
   if(!validLogoUrl(favicon))favicon='-';
   rows.push([id,name,codec,u,country,favicon].join('|||'));
 }
 return rows;
}
function compareStationRow(a,b){
 const aa=String(a.split('|||')[1]||'').toLowerCase();
 const bb=String(b.split('|||')[1]||'').toLowerCase();
 return aa<bb?-1:aa>bb?1:a.localeCompare(b);
}
function parseM3U(m3u,country){const entries=[];let name='';for(let line of m3u.split(/\r?\n/)){
 line=line.trim();if(line.startsWith('#EXTINF:')){const comma=line.lastIndexOf(',');name=comma>=0?line.slice(comma+1):'';}
 else if(line && !line.startsWith('#')){entries.push({name:name||'Internet radio',url:line,codec:'AUDIO'});name='';}
 }return stationRows(entries,country);}
async function stations(country,offset){
 if(!validCountry(country)||!Number.isSafeInteger(offset)||offset<0||offset%BATCH!==0)throw Error('Invalid country or offset');
 let rows,source='Radio Browser',hasMore=false;
 try{
  let raw=await api(`/json/stations/bycountrycodeexact/${country}?hidebroken=true&order=name&reverse=false&offset=${offset}&limit=${BATCH}`,90000);
  let result=JSON.parse(raw);if(!Array.isArray(result))throw Error('Invalid radio stations');
  rows=stationRows(result,country);
  hasMore=result.length===BATCH;
  if(!rows.length && offset===0)throw Error('No stations');
 }catch(e){
  let m3u=await limitedFetch(PLAYLIST_ORIGIN+country.toLowerCase()+'.m3u',8*1024*1024,12000);
  const playlist=parseM3U(m3u,country).sort(compareStationRow);
  rows=playlist.slice(offset,offset+BATCH);hasMore=playlist.length>offset+BATCH;source='Country M3U';
 }
 const file='radio-stations.tsv';let previous=[];
 if(offset>0){try{previous=fs.readFileSync(path.join(dataDir,file),'utf8').trimEnd().split('\n').filter(x=>x.split('|||')[4]===country);}catch{}}
 // Deduplicate every returned page without imposing a station-count ceiling.
 // The network response stays bounded per request, and the SDL UI grows only on demand.
 const seen=new Set();let merged=[];
 for(let line of [...previous,...rows]){let parts=line.split('|||'),id=parts[0];if(parts.length!==6||seen.has(id))continue;seen.add(id);merged.push(line);}
 merged.sort(compareStationRow);
 atomic(file,merged.length?merged.join('\n')+'\n':'');
 atomic('radio-status.txt',`Loaded ${merged.length} stations (${source})\n`);
 atomic('radio-pagination.tsv',`${country}|||${offset+BATCH}|||${hasMore?1:0}\n`);
 return {count:rows.length,total:merged.length,source,hasMore};
}
function imageDimensions(buffer){
 if(buffer.length<24)return null;
 if(buffer.subarray(0,8).equals(Buffer.from([137,80,78,71,13,10,26,10]))){
   const width=buffer.readUInt32BE(16),height=buffer.readUInt32BE(20);
   return {width,height,ext:'.png'};
 }
 if(buffer[0]===0xff&&buffer[1]===0xd8){
   let i=2;
   while(i+9<buffer.length){
     if(buffer[i]!==0xff){i++;continue;}
     let marker=buffer[i+1];if(marker===0xd9||marker===0xda)break;
     if(marker===0x00||marker===0x01||marker>=0xd0&&marker<=0xd7){i+=2;continue;}
     if(i+4>buffer.length)break;
     const n=buffer.readUInt16BE(i+2);if(n<2||i+2+n>buffer.length)break;
     if([0xc0,0xc1,0xc2,0xc3].includes(marker)&&n>=7)
       return {width:buffer.readUInt16BE(i+7),height:buffer.readUInt16BE(i+5),ext:'.jpg'};
     i+=2+n;
   }
 }
 return null;
}
async function fetchLogo(url){
 const ac=new AbortController(), timer=setTimeout(()=>ac.abort(),2500);
 try{
  const r=await fetch(url,{signal:ac.signal,redirect:'manual',headers:{Accept:'image/png,image/jpeg'}});
  // No redirects: an arbitrary favicon URL cannot redirect to a local host.
  if(r.status!==200||!r.body||Number(r.headers.get('content-length')||0)>LOGO_MAX_BYTES) return null;
  let parts=[],total=0;
  for await(const chunk of r.body){total+=chunk.length;if(total>LOGO_MAX_BYTES){ac.abort();return null;}parts.push(chunk);}
  const data=Buffer.concat(parts),meta=imageDimensions(data);
  if(!meta||meta.width<8||meta.height<8||meta.width>LOGO_MAX_PIXEL||meta.height>LOGO_MAX_PIXEL)return null;
  return {...meta,data};
 }catch{return null;}finally{clearTimeout(timer);}
}
function trimLogoCache(){
 const entries=[];let total=0;
 for(const name of fs.readdirSync(LOGO_CACHE)){
  if(!/^[a-f0-9-]{16,40}\.(png|jpg|miss)$/.test(name))continue;
  try{const s=fs.lstatSync(path.join(LOGO_CACHE,name));if(!s.isFile())continue;
   total+=s.size;entries.push({name,mtime:s.mtimeMs,size:s.size});}catch{}
 }
 entries.sort((a,b)=>a.mtime-b.mtime);
 while((total>LOGO_CACHE_LIMIT||entries.length>100)&&entries.length){
  const entry=entries.shift();try{fs.unlinkSync(path.join(LOGO_CACHE,entry.name));total-=entry.size;}catch{}
 }
}
async function logos(kind,country,offset){
 if(!['stations','favourites'].includes(kind)||!validCountry(country)||!Number.isSafeInteger(offset)||offset<0||offset%7!==0)
  throw Error('Invalid logo page');
 let rows=[];
 const fn=kind==='favourites'?'radio-favourites.tsv':'radio-stations.tsv';
 try{rows=fs.readFileSync(path.join(dataDir,fn),'utf8').split('\n').filter(Boolean).map(x=>x.split('|||'));}catch{return 0;}
 if(kind==='stations')rows=rows.filter(parts=>parts[4]===country);
 const visible=rows.slice(offset,offset+7);
 fs.mkdirSync(LOGO_CACHE,{recursive:true,mode:0o700});let written=0;
 for(const item of visible){
  const [id,,,,,url]=item;
  if(!/^[a-f0-9-]{16,40}$/.test(id||'')||!validLogoUrl(url))continue;
  const stem=path.join(LOGO_CACHE,id);
  if(['.png','.jpg'].some(ext=>fs.existsSync(stem+ext)))continue;
  const miss=stem+'.miss';
  try{if(Date.now()-fs.statSync(miss).mtimeMs<24*3600*1000)continue;}catch{}
  const result=await fetchLogo(url);
  if(!result){fs.writeFileSync(miss,'', {mode:0o600});continue;}
  const tmp=stem+result.ext+'.tmp-'+process.pid;
  try{fs.writeFileSync(tmp,result.data,{mode:0o600});fs.renameSync(tmp,stem+result.ext);written++;fs.rmSync(miss,{force:true});}
  finally{fs.rmSync(tmp,{force:true});}
 }
 trimLogoCache();return written;
}
async function main(){const [, ,command,country,offset='0']=process.argv;
 try{
  if(command==='countries'){const n=await countries();atomic('radio-status.txt',`Available countries: ${n}\n`);}
  else if(command==='stations'){await stations(String(country||'').toUpperCase(),Number(offset));}
  else if(command==='logos'){await logos(String(country||''),String(offset||'').toUpperCase(),Number(process.argv[5]||0));}
  else throw Error('Usage: radio-backend.js countries | stations CC OFFSET');
 }catch(e){
  // Never clear valid cache when a remote service fails.
  atomic('radio-status.txt',`Radio directory unavailable; retry or use cached list.\n`);
  process.stderr.write('Radio directory unavailable: '+String(e.message).slice(0,100)+'\n');process.exitCode=1;
 }
}
if(require.main===module)main();
module.exports={cleaned,isPublicUrl,validLogoUrl,imageDimensions,parseM3U,stationRows,validCountry,countries,stations,logos,trimLogoCache,limitedFetch};
