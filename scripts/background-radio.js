#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
/* MustardTube Beta 2.2 - experimental audio-only process supervisor.
 * Short-lived Node controller: only the separately packaged ffplay-radio stays alive.
 * No PortMaster, no stdin/keyboard ownership, no framebuffer/SDL video display.
 * Never persist radio URLs in state or logs. Do not kill processes using a stale PID.
 * Device-side PipeWire mixing and game-launch survival MUST still be validated. */
'use strict';
const fs=require('node:fs');
const path=require('node:path');
const {spawn,spawnSync}=require('node:child_process');
const base=process.env.MUSTARDTUBE_APP_DIR || path.resolve(__dirname,'..');
const data=process.env.MUSTARDTUBE_DATA_DIR || path.join(base,'data');
const selected=path.join(data,'radio-selected.txt');
const statePath=path.join(data,'radio-background-state.json');
const volumePath=path.join(data,'radio-mix-volume.txt');
const hotkeysPath=path.join(data,'radio-hotkeys-enabled.txt');
const DEFAULT_RADIO_MIX=75; // Tested balance: FFplay 55%, PipeWire stream 75%.
const player=path.join(base,'bin','ffplay-radio');
const hotkeys=path.join(base,'bin','radio-hotkeys');
const command=process.argv[2] || 'status';
const fail=(m,code=1)=>{process.stderr.write(`background radio: ${m}\n`);process.exitCode=code;};
const writeAtomic=(file,body)=>{
  fs.mkdirSync(data,{recursive:true,mode:0o700});
  const tmp=file+`.tmp-${process.pid}`;
  try {fs.writeFileSync(tmp,body,{mode:0o600,flag:'wx'});fs.renameSync(tmp,file);}
  finally {try{fs.unlinkSync(tmp);}catch{}}
};
const clear=()=>{try{fs.unlinkSync(statePath);}catch(e){if(e.code!=='ENOENT')throw e;}};
// One persistent RADIO stream control. The game and master sinks are untouched.
function readVolume(){
  try {
    const raw=fs.readFileSync(volumePath,'utf8').trim();
    if(!/^(100|[1-9]?\d)$/.test(raw))return DEFAULT_RADIO_MIX;
    return Number(raw);
  }catch{return DEFAULT_RADIO_MIX;}
}
function parseVolume(v){
  if(typeof v!=='string'||!/^(100|[1-9]?\d)$/.test(v))return null;
  return Number(v);
}
// Disabled by default until users test button conflicts on their device.
// The app-owned low-memory event helper never installs global muOS hotkeys.
function hotkeysEnabled(){
  try{return ['1','2'].includes(fs.readFileSync(hotkeysPath,'utf8').trim());}catch{return false;}
}
function adjustVolume(delta){
  const n=readVolume();
  const target=Math.max(0,Math.min(100,n+delta));
  writeAtomic(volumePath,String(target)+'\n');
  const s=active();
  if(s && !applyVolume(s.pid,target))return fail('volume saved; live stream adjustment unavailable',4);
  process.stdout.write(`Radio volume: ${target}%\n`);
}
function restoreGameAudio(){
  // No additional resident process. A short-lived, identity-validated
  // restore must succeed before its saved original volume is discarded.
  // Failure retains the recovery record for the next safe attempt.
  const session=path.join(data,'game-volume.session');
  try{
    const deferred=[0,1,2,3,4,5,6,7].some(i=>fs.existsSync(path.join(data,`game-volume.deferred-${i}`)));
    if(!fs.existsSync(session) && !deferred)return true;
    for(let attempt=0;attempt<2;attempt++){
      const result=spawnSync(hotkeys,['--restore-game',base],{
        env:{...process.env,XDG_RUNTIME_DIR:'/run',PIPEWIRE_RUNTIME_DIR:'/run'},
        stdio:'ignore',timeout:2500
      });
      if(result.status===0 && !result.error)return true;
      if(result.status===3 && !result.error)return 'pending';
    }
    process.stderr.write('background radio: game volume could not be restored; recovery state retained\n');
    return false;
  }catch{
    process.stderr.write('background radio: game volume restore unavailable; recovery state retained\n');
    return false;
  }
}
/* If the game exited before radio stopped, use the SAME existing helper
 * executable as a bounded (180-second) one-shot recovery watcher.  It never
 * changes a device/master sink, never installs an OS daemon, and only matches
 * the recorded emulator binary + a unique PipeWire output stream. */
function startDeferredGameRecovery(){
  try {
    const st=fs.statSync(hotkeys);
    if(!st.isFile() || !(st.mode&0o111))return;
    const child=spawn(hotkeys,['--recover-game',base],{
      detached:true,stdio:'ignore',env:{...process.env,XDG_RUNTIME_DIR:'/run',
        PIPEWIRE_RUNTIME_DIR:'/run',MUSTARDTUBE_APP_DIR:base,MUSTARDTUBE_DATA_DIR:data}
    });
    child.on('error',()=>{});
    child.unref();
  }catch{}
}
function startAppOwnedMonitor(pid,started){
  // If absent, radio still plays; the feature remains safely optional.
  try{
    const st=fs.statSync(hotkeys);
    if(!st.isFile() || !(st.mode&0o111))return;
    const child=spawn(hotkeys,['--monitor',base,String(pid),String(started)],{
      detached:true,stdio:'ignore',env:{...process.env,MUSTARDTUBE_APP_DIR:base,
        MUSTARDTUBE_DATA_DIR:data}
    });
    child.on('error',()=>{});
    child.unref();
  }catch{}
}
function applyVolume(pid,level){
  const result=spawnSync('wpctl',['set-volume','--pid',String(pid),`${level}%`],{
    env:{...process.env,XDG_RUNTIME_DIR:'/run',PIPEWIRE_RUNTIME_DIR:'/run'},
    stdio:'ignore',timeout:1500
  });
  return result.status===0 && !result.error;
}

// proc starttime (field 22) defeats PID reuse. The process must still be the
// same packaged binary; only then can its dedicated process group be stopped.
function procIdentity(pid) {
  try{
    const raw=fs.readFileSync(`/proc/${pid}/stat`,'utf8').trim();
    const match=raw.match(/^\d+ \(.*\) (.*)$/);
    if(!match)return null;
    const fields=match[1].trim().split(/\s+/);
    if(fields[0]==='Z'||fields[0]==='X')return null;
    return {started:fields[19]||null, group:Number(fields[2])};
  }catch{return null;}
}
function playerPathValid(){
  try{
    const stat=fs.statSync(player);
    if(!stat.isFile() || (stat.mode & 0o111)===0)return false;
    const magic=Buffer.alloc(20);const fd=fs.openSync(player,'r');
    try{fs.readSync(fd,magic,0,20,0);}finally{fs.closeSync(fd);}
    // ELF 64-bit little endian, e_machine = 183 (AArch64) on device;
    // host tests can inject a custom player with explicit opt-in.
    return (magic[0]===127 && magic.subarray(1,4).toString()==='ELF' &&
      magic.readUInt16LE(18)===183) || process.env.MT_RADIO_TEST_PLAYER==='1';
  }catch{return false;}
}
function active(){
  try{
    const s=JSON.parse(fs.readFileSync(statePath,'utf8'));
    if(!Number.isSafeInteger(s.pid)||s.pid<=1 || !/^\d+$/.test(s.started)||
       typeof s.station!=='string' || s.station.length>112 ||
       s.player!==fs.realpathSync(player) || !playerPathValid())return null;
    const identity=procIdentity(s.pid);
    if(!identity || identity.started!==s.started || identity.group!==s.pid)return null;
    if(fs.realpathSync(`/proc/${s.pid}/exe`)!==s.player && process.env.MT_RADIO_TEST_PLAYER!=='1')return null;
    // Must be process-group leader before we signal -pid.
    return s;
  }catch{return null;}
}
function stop(allowDeferred=false){
  const s=active();
  if(s){
    try {process.kill(-s.pid,'SIGTERM');}
    catch(e){if(e.code!=='ESRCH')fail('unable to stop player: '+e.code);}
  }
  // Keep the radio-stop path independent of mixer restoration failures.
  // The saved original game volume remains available for a later retry.
  const restored=restoreGameAudio();
  if(restored==='pending' && allowDeferred)startDeferredGameRecovery();
  clear();
}
function validatedRadio(text){
  const rows=text.split(/\r?\n/);
  const station=(rows[0]||'').replace(/[\x00-\x1f\x7f]/g,' ').trim().slice(0,110);
  const address=(rows[1]||'').trim();
  if(!station || address.length>740 || /[\s\\|#]/.test(address))throw Error('invalid station');
  const u=new URL(address);
  if(!['http:','https:'].includes(u.protocol)||u.username||u.password||!u.hostname||
     /^(localhost|.*\.(local|lan|home|internal))$/i.test(u.hostname))throw Error('invalid stream address');
  if(/^\d+\.\d+\.\d+\.\d+$/.test(u.hostname)){
    const b=u.hostname.split('.').map(Number);
    if(b.some(x=>x>255)||b[0]===0||b[0]===10||b[0]===127||b[0]>=224||
       b[0]===169&&b[1]===254||b[0]===172&&b[1]>=16&&b[1]<=31||
       b[0]===192&&b[1]===168||b[0]===100&&b[1]>=64&&b[1]<=127)throw Error('private address');
  }
  return {station,address};
}
if(command==='status'){
  const s=active();
  if(s){process.stdout.write(s.station+'\n');}
  else {clear();process.exitCode=1;}
}else if(command==='stop'){
  stop(true);process.stdout.write('Stopped background radio.\n');
}else if(command==='get-volume'){
  process.stdout.write(readVolume()+'\n');
}else if(command==='set-volume'){
  const level=parseVolume(process.argv[3]);
  if(level===null)fail('radio volume must be an integer from 0 to 100',2);
  else {
    try {
      writeAtomic(volumePath,String(level)+'\n');
      const s=active();
      if(s && !applyVolume(s.pid,level))fail('volume saved; live stream adjustment unavailable',4);
      else process.stdout.write(`Radio volume: ${level}%\n`);
    }catch{fail('cannot save radio volume',3);}
  }
}else if(command==='get-hotkeys'){
  let mode='0';try{mode=fs.readFileSync(hotkeysPath,'utf8').trim();}catch{}
  process.stdout.write(['0','1','2'].includes(mode)?mode+'\n':'0\n');
}else if(command==='set-hotkeys'){
  if(!['0','1','2'].includes(process.argv[3]))fail('hotkeys mode must be 0, 1 or 2',2);
  else{
    try{
      writeAtomic(hotkeysPath,process.argv[3]+'\n');
      // Disabling game controls must restore NOW, even while the radio and
      // emulator remain active. Do not wait for the 1Hz monitor check.
      const restored=process.argv[3]==='2' || restoreGameAudio();
      // The radio may have started before the user enabled this option.
      // Reattach the standalone monitor if its earlier launch failed or ended.
      // A nonblocking flock inside the helper guarantees at most one monitor.
      if(process.argv[3]==='1'||process.argv[3]==='2'){
        const s=active();
        if(s)startAppOwnedMonitor(s.pid,s.started);
      }
      process.stdout.write(`In-game controls: ${['OFF','RADIO','RADIO+GAME'][Number(process.argv[3])]}\n`);
      if(!restored)process.exitCode=4; // Saved mode, pending game restore.
    }catch{fail('cannot save hotkeys preference',3);}
  }
}else if(command==='adjust-volume'){
  const delta=Number(process.argv[3]);
  if(!Number.isInteger(delta)||![5,-5].includes(delta))fail('adjustment must be +/-5',2);
  else try{adjustVolume(delta);}catch{fail('cannot adjust volume',3);}
}else if(command==='apply-volume'){
  // Short-lived helper started only when a stream begins. PipeWire registers
  // the ALSA playback stream asynchronously, so do not block the launcher.
  const expectedPid=Number(process.argv[3]);
  let tries=0;
  const attempt=()=>{
    const s=active();
    if(!s||s.pid!==expectedPid)return;
    if(applyVolume(s.pid,readVolume()))return;
    if(++tries<30)setTimeout(attempt,100);
  };
  attempt();
}else if(command==='start'){
  let radio;
  try{radio=validatedRadio(fs.readFileSync(selected,'utf8'));}
  catch(e){fail('no valid selected station');}
  finally{try{fs.unlinkSync(selected);}catch{}}
  if(radio){
    if(!playerPathValid()){
      fail('audio-only ARM64 player not bundled in this source build',3);
    }else{
      stop(false);
      // Dedicated process group; MUST NOT use the foreground Wasabi binary.
      const env={...process.env,SDL_VIDEODRIVER:'dummy',SDL_AUDIODRIVER:'alsa',XDG_RUNTIME_DIR:'/run',PIPEWIRE_RUNTIME_DIR:'/run'};
      const args=['-nodisp','-vn','-loglevel','error','-autoexit','-volume','55',
                  '-i',radio.address];
      const child=spawn(player,args,{detached:true,stdio:'ignore',env});
      let finished=false;
      child.once('error',err=>{if(!finished){finished=true;fail(`player launch failed (${err.code})`);}});
      child.once('spawn',()=>{
        if(finished)return;
        finished=true;
        const identity=procIdentity(child.pid);
        if(!identity || !identity.started || identity.group!==child.pid){
          fail('cannot establish detached player identity');
          try{process.kill(child.pid,'SIGTERM');}catch{}return;
        }
        try{
          writeAtomic(statePath,JSON.stringify({pid:child.pid,started:identity.started,
            station:radio.station,player:fs.realpathSync(player)})+'\n');
          // Run a bounded, detached startup mixer helper only until the
          // PipeWire ALSA stream becomes visible (no resident Node service).
          try {
            const helper=spawn(process.execPath,[__filename,'apply-volume',String(child.pid)],
              {detached:true,stdio:'ignore',env});
            helper.on('error',()=>{}); // Missing helper must never stop radio.
            helper.unref();
          }catch{}
          // No resident input helper unless the user has explicitly opted in.
          if(hotkeysEnabled())startAppOwnedMonitor(child.pid,identity.started);
          child.unref();process.stdout.write(`Started background radio: ${radio.station}\n`);
        }catch(e){try{process.kill(-child.pid,'SIGTERM');}catch{}fail('cannot save player state');}
      });
    }
  }
}else{
  fail('usage: background-radio.js start|stop|status|get-volume|set-volume N|adjust-volume +/-5|get-hotkeys|set-hotkeys 0|1|2',2);
}
