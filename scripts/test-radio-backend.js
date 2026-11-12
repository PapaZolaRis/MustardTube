'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'mt-radio-test-'));
process.env.MUSTARDTUBE_DATA_DIR=dir;
const radio=require('./radio-backend.js');
(async()=>{
 const good='https://streams.example.org/radio.mp3';
 for(const bad of ['file:///etc/passwd','http://localhost/admin','http://127.0.0.1','http://192.168.2.1','http://10.0.0.2','http://169.254.169.254/latest','http://[::1]/','https://x.local/live','https://user:password@example.org/live','javascript:alert(1)'])assert(!radio.isPublicUrl(bad),bad);
 assert(radio.isPublicUrl(good));
 assert(radio.validLogoUrl('https://cdn.example.org/logo.png'));
 assert(!radio.validLogoUrl('http://127.0.0.1/logo.png'));
 assert(!radio.validLogoUrl('http://cdn.example.org/logo.png'));
 assert(!radio.validLogoUrl('https://localhost/logo.png'));
 assert(!radio.validCountry('../../etc'));assert(radio.validCountry('MY'));
 const rows=radio.stationRows([{name:'Test | Radio\nName',stationuuid:'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa',url_resolved:good,codec:'MP3',favicon:'https://cdn.example.org/logo.png'},
   {name:'Unsafe',url_resolved:'http://127.0.0.1',codec:'MP3'},
   {name:'Duplicate',url_resolved:good,codec:'MP3'}],'US');
 assert.equal(rows.length,1);assert(!rows[0].includes('\n'));
 assert(rows[0].endsWith('|||https://cdn.example.org/logo.png'));
 const png=fs.readFileSync(path.join(__dirname,'..','tests','fixtures','logo24.png'));
 assert.deepEqual({width:radio.imageDimensions(png).width,height:radio.imageDimensions(png).height},{width:24,height:24});
 let fallback=radio.parseM3U('#EXTM3U\n#EXTINF:-1,Station One\nhttps://station.example.org/live.aac\n#EXTINF:-1,Unsafe\nhttp://127.0.0.1:8080/private\n','US');
 assert.equal(fallback.length,1);assert(fallback[0].includes('Station One'));
 const original=global.fetch;
 let mode='ok',calls=[];
 global.fetch=async (url)=>{
  calls.push(url);
  if(mode==='fallback' && url.includes('radio-browser'))throw Error('offline');
  let body;
  if(url.includes('/json/countrycodes'))body=JSON.stringify([{name:'GB',stationcount:100},{name:'US',stationcount:235},{name:'XX',stationcount:0}]);
  else if(url.includes('bycountrycodeexact')){
    const q=new URL(url);const offset=Number(q.searchParams.get('offset'));
    body=JSON.stringify(Array.from({length:Math.max(0,Math.min(20,235-offset))},(_,i)=>({name:'Station '+(offset+i),stationuuid:String(offset+i).padStart(32,'a'),codec:'MP3',url_resolved:'https://radio.example.org/'+(offset+i)})));
  }else body='#EXTM3U\n#EXTINF:-1,Playlist Station\nhttps://stream.example.org/live.mp3\n';
  return new Response(body,{status:200,headers:{'content-length':String(Buffer.byteLength(body))}});
 };
 assert.equal(await radio.countries(),2);
 assert(calls.some(u=>u.includes('/json/countrycodes')&&u.includes('order=name')));
 assert.equal(fs.readFileSync(path.join(dir,'radio-countries.tsv'),'utf8').trim().split('\n').length,2);
 let r=await radio.stations('US',0);assert.equal(r.total,20);
 assert(calls.some(u=>u.includes('bycountrycodeexact')&&u.includes('order=name')));
 r=await radio.stations('US',20);assert.equal(r.total,40);
 assert.equal(fs.readFileSync(path.join(dir,'radio-stations.tsv'),'utf8').trim().split('\n').length,40);
 for(let offset=40;offset<240;offset+=20)r=await radio.stations('US',offset);
 assert.equal(r.total,235);
 assert.equal(r.hasMore,false);
 assert.equal(fs.readFileSync(path.join(dir,'radio-stations.tsv'),'utf8').trim().split('\n').length,235);
 assert.equal(fs.readFileSync(path.join(dir,'radio-pagination.tsv'),'utf8').trim(),'US|||240|||0');
 // Background images are optional, capped, and cached by station ID.
 fs.writeFileSync(path.join(dir,'radio-stations.tsv'), rows[0]+'\n');
 global.fetch=async (url)=>new Response(png,{status:200,headers:{'content-length':String(png.length),'content-type':'image/png'}});
 assert.equal(await radio.logos('stations','US',0),1);
 assert.equal(await radio.logos('stations','US',0),0);
 assert(fs.existsSync(path.join(dir,'radio-logo-cache','aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa.png')));
 global.fetch=async (url)=>{
  calls.push(url);
  if(mode==='fallback' && url.includes('radio-browser'))throw Error('offline');
  let body;
  if(url.includes('/json/countrycodes'))body='[]';
  else if(url.includes('bycountrycodeexact'))body='[]';
  else body='#EXTM3U\n#EXTINF:-1,Playlist Station\nhttps://stream.example.org/live.mp3\n';
  return new Response(body,{status:200,headers:{'content-length':String(Buffer.byteLength(body))}});
 };
 mode='fallback';r=await radio.stations('GB',0);assert.equal(r.source,'Country M3U');assert.equal(r.total,1);
 assert(fs.readFileSync(path.join(dir,'radio-stations.tsv'),'utf8').includes('Playlist Station'));
 assert(calls.some(x=>x.includes('countries/gb.m3u')));
 // An M3U with over 80 valid entries must keep all entries across pages.
 global.fetch=async (url)=>{
  if(url.includes('/json/'))throw Error('Directory offline');
  const playlist='#EXTM3U\n'+Array.from({length:107},(_,i)=>
    `#EXTINF:-1,Playlist Station ${String(i).padStart(3,'0')}\nhttps://stream.example.org/${i}.mp3\n`).join('');
  return new Response(playlist,{status:200,headers:{'content-length':String(Buffer.byteLength(playlist))}});
 };
 for(let offset=0;offset<120;offset+=20)r=await radio.stations('GB',offset);
 assert.equal(r.total,107);
 assert.equal(r.hasMore,false);
 assert.equal(fs.readFileSync(path.join(dir,'radio-stations.tsv'),'utf8').trim().split('\n').length,107);
 await assert.rejects(()=>radio.stations('../ZZ',0));
 global.fetch=original;
 fs.rmSync(dir,{recursive:true,force:true});
 console.log('PASS: country directory, station filtering, 20-item pagination, M3U fallback, cached files, URL validation');
})().catch(e=>{console.error(e);fs.rmSync(dir,{recursive:true,force:true});process.exit(1);});
