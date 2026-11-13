#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
// User-controlled GitHub Releases updater. Uses stock muOS Archive Manager for
// installation; never writes over a running program or invokes internal APIs.
'use strict';
const fs = require('fs');
const path = require('path');
const https = require('https');
const crypto = require('crypto');
const { spawnSync } = require('child_process');

const REPO = 'PapaZolaRis/MustardTube';
const API = `https://api.github.com/repos/${REPO}/releases?per_page=40`;
const MAX_BYTES = 250 * 1024 * 1024;
const MAX_RESPONSE = 2 * 1024 * 1024;
const MIN_FREE_BUFFER = 16 * 1024 * 1024;
const AUTO_INTERVAL_MS = 24 * 60 * 60 * 1000;
const CURRENT = 'v0.9.11-beta2.4.5';
const APP = process.env.MUSTARDTUBE_APP_DIR || path.resolve(__dirname, '..');
const DATA = process.env.MUSTARDTUBE_DATA_DIR || path.join(APP, 'data');
const STATUS = path.join(DATA, 'ota-status.txt');
const LOCK = path.join(DATA, 'ota-worker.lock');
const CHANNEL = path.join(DATA, 'ota-channel.txt');
const AUTOCHECK = path.join(DATA, 'ota-auto-check.txt');
const CANDIDATE = path.join(DATA, 'ota-candidate.json');
const LAST_CHECK = path.join(DATA, 'ota-last-check.txt');
const BACKUP_NAME = /^MustardTube-settings-v[0-9a-z.\-]+-20[0-9T.Z\-]+\.json$/;
const SAFE_NAME = /^MustardTube-v\d+\.\d+\.\d+(?:-Beta\d+\.\d+\.\d+)?-[a-zA-Z0-9-]+\.muxapp$/;
const USER_FILES = Object.freeze([
  'settings.ini', 'radio-favourites.tsv', 'radio-last-country.txt',
  'quality.txt', 'radio-mix-volume.txt', 'game-mix-volume.txt',
  'radio-hotkeys-enabled.txt', 'watch-history.tsv', 'ota-channel.txt',
  'ota-auto-check.txt'
]);

function version(tag) {
  const m = /^v(\d+)\.(\d+)\.(\d+)(?:-beta(\d+)\.(\d+)\.(\d+))?$/i.exec(String(tag || ''));
  if (!m) return null;
  const nums = m.slice(1).map(x => x == null ? 0 : Number(x));
  if (nums.some(x => !Number.isSafeInteger(x) || x < 0 || x > 100000)) return null;
  return { nums, beta: m[4] != null, tag: tag.toLowerCase() };
}
function newer(a, b) {
  for (let i = 0; i < 3; i++) if (a.nums[i] !== b.nums[i]) return a.nums[i] > b.nums[i];
  if (a.beta !== b.beta) return !a.beta;
  for (let i = 3; i < 6; i++) if (a.nums[i] !== b.nums[i]) return a.nums[i] > b.nums[i];
  return false;
}
function assetIsOfficial(asset, tag) {
  if (!asset || typeof asset.name !== 'string' || !SAFE_NAME.test(asset.name)) return false;
  const v = version(tag);
  if (!v) return false;
  const embedded = /^MustardTube-v(\d+\.\d+\.\d+)(?:-Beta(\d+\.\d+\.\d+))?-/i.exec(asset.name);
  const nameVersion = embedded && version('v' + embedded[1] + (embedded[2] ? '-beta' + embedded[2] : ''));
  if (!nameVersion || nameVersion.tag !== v.tag) return false;
  if (!Number.isSafeInteger(asset.size) || asset.size < 1024 || asset.size > MAX_BYTES) return false;
  if (typeof asset.digest !== 'string' || !/^sha256:[a-f0-9]{64}$/i.test(asset.digest)) return false;
  try {
    const u = new URL(asset.browser_download_url);
    const prefix = `/PapaZolaRis/MustardTube/releases/download/${encodeURIComponent(tag)}/`;
    return u.protocol === 'https:' && u.hostname === 'github.com' && !u.username && !u.password &&
      (u.pathname === prefix + encodeURIComponent(asset.name) ||
       decodeURIComponent(u.pathname) === decodeURIComponent(prefix + encodeURIComponent(asset.name)));
  } catch (_) { return false; }
}
function chooseRelease(releases, channel, installed = CURRENT) {
  const local = version(installed);
  if (!local || !Array.isArray(releases) || !['stable','beta'].includes(channel)) return null;
  let best = null;
  for (const r of releases) {
    if (!r || r.draft || (channel === 'stable' && r.prerelease)) continue;
    const v = version(r.tag_name);
    if (!v || !newer(v, local) || Boolean(r.prerelease) !== Boolean(v.beta)) continue;
    const asset = (Array.isArray(r.assets) ? r.assets : []).find(a => assetIsOfficial(a, r.tag_name));
    if (!asset) continue;
    if (!best || newer(v, best.v)) best = { v, asset, release: r };
  }
  return best;
}
function readChannel() {
  try { return fs.readFileSync(CHANNEL, 'utf8').trim() === 'stable' ? 'stable' : 'beta'; }
  catch (_) { return 'beta'; }
}
function autoCheckEnabled() {
  try { return fs.readFileSync(AUTOCHECK,'utf8').trim() !== 'off'; }
  catch (_) { return true; }
}
function safeStatus(state, detail, release = '', bytes = '', progress = -1) {
  fs.mkdirSync(DATA, { recursive: true, mode: 0o700 });
  const simple = s => String(s).replace(/[^a-zA-Z0-9 _.,:!%+()/-]/g, '').slice(0, 140);
  const pct = state === 'downloading' && Number.isSafeInteger(progress) && progress >= 0 && progress <= 100
    ? progress : -1;
  // Atomic four-theme UI status: renderer polls once per second. No per-chunk UI process.
  const content = `state=${simple(state)}\ndetail=${simple(detail)}\nrelease=${simple(release)}\nsize=${simple(bytes)}\nprogress=${pct}\n`;
  const tmp = `${STATUS}.${process.pid}.tmp`;
  fs.writeFileSync(tmp, content, { mode: 0o600 });
  fs.renameSync(tmp, STATUS);
}
function friendlyError(error) {
  const code = error && error.code || '';
  const message = String(error && error.message || '');
  if (['EAI_AGAIN','ENOTFOUND','ENETUNREACH','EHOSTUNREACH','ENETDOWN','ECONNREFUSED'].includes(code))
    return 'Cannot reach GitHub. Check Wi-Fi and retry.';
  if (code === 'ETIMEDOUT' || code === 'ECONNRESET' || /timed out/i.test(message))
    return 'Connection interrupted or timed out. Retry.';
  if (/^HTTP (403|429)/.test(message)) return 'GitHub is limiting requests. Try later.';
  if (/^HTTP 5\d\d/.test(message)) return 'GitHub updates are temporarily unavailable.';
  if (/^HTTP 404/.test(message)) return 'Release not found. Check GitHub later.';
  if (/^HTTP /.test(message)) return 'Unable to contact update service. Try later.';
  if (/^Unexpected download redirect/.test(message)) return 'Update download link failed validation.';
  // Preserve existing actionable local validation messages; never display a URL.
  return message.replace(/https?:\/\/\S+/g, '[address]').slice(0, 110) || 'Update failed. Please try again.';
}
function atomicText(file, content) {
  fs.mkdirSync(path.dirname(file), { recursive: true, mode: 0o700 });
  const temp = `${file}.${process.pid}.tmp`;
  try {
    fs.writeFileSync(temp, content, { flag:'wx', mode: 0o600 });
    fs.renameSync(temp, file);
  } finally { try { fs.unlinkSync(temp); } catch (_) { /* renamed */ } }
}
function validRedirect(url, initial = false) {
  try {
    const u = new URL(url);
    if (u.protocol !== 'https:' || u.username || u.password || u.port && u.port !== '443') return false;
    return initial ? u.hostname === 'api.github.com' || u.hostname === 'github.com' :
      u.hostname === 'github.com' || u.hostname === 'release-assets.githubusercontent.com' ||
      u.hostname === 'objects.githubusercontent.com' || u.hostname === 'github-releases.githubusercontent.com';
  } catch (_) { return false; }
}
function request(url, redirects = 0) {
  if (redirects > 5 || !validRedirect(url, redirects === 0)) return Promise.reject(new Error('Unexpected download redirect'));
  return new Promise((resolve, reject) => {
    const req = https.get(url, {
      headers: { 'User-Agent': 'MustardTube-updates/2.4.2', 'Accept': 'application/vnd.github+json', 'Accept-Encoding': 'identity' }
    }, res => {
      if ([301, 302, 303, 307, 308].includes(res.statusCode)) {
        const next = new URL(res.headers.location || '', url).toString();
        res.resume();
        return resolve(request(next, redirects + 1));
      }
      if (res.statusCode !== 200) { res.resume(); reject(new Error(`HTTP ${res.statusCode}`)); return; }
      resolve(res);
    });
    req.setTimeout(15000, () => req.destroy(new Error('Network timed out')));
    req.on('error', reject);
  });
}
async function jsonReleases() {
  const res = await request(API);
  const buffers = []; let size = 0;
  for await (const chunk of res) {
    size += chunk.length;
    if (size > MAX_RESPONSE) { res.destroy(); throw new Error('Release response too large'); }
    buffers.push(chunk);
  }
  const json = JSON.parse(Buffer.concat(buffers).toString('utf8'));
  if (!Array.isArray(json)) throw new Error('Unexpected GitHub response');
  return json;
}
function archiveDir() {
  const locations = process.env.MUSTARDTUBE_OTA_TEST_ARCHIVE ? [process.env.MUSTARDTUBE_OTA_TEST_ARCHIVE] :
    ['/mnt/mmc/ARCHIVE', '/mnt/sdcard/ARCHIVE'];
  for (const p of locations) {
    try {
      if (fs.lstatSync(p).isDirectory()) { fs.accessSync(p, fs.constants.W_OK); return p; }
    } catch (_) { /* next SD card */ }
  }
  throw new Error('No writable SD-card ARCHIVE folder');
}
function validArchiveEntries(entries) {
  if (!Array.isArray(entries) || entries.length < 3 || entries.length > 10000) return false;
  const seen = new Set();
  for (const name of entries) {
    if (seen.has(name) || !name.startsWith('MustardTube/') || name.includes('\\') || name.includes('\0') ||
        name.split('/').some(s => s === '..' || s === '.') || name.length > 500) return false;
    seen.add(name);
  }
  return seen.has('MustardTube/mux_launch.sh') && seen.has('MustardTube/bin/mustardtube') &&
    seen.has('MustardTube/licenses/LICENSE');
}
function enoughSpace(dir, bytes) {
  const st = fs.statfsSync(dir);
  const available = BigInt(st.bavail) * BigInt(st.bsize);
  return available >= BigInt(bytes + MIN_FREE_BUFFER);
}
async function fileSha256(file) {
  const hash = crypto.createHash('sha256');
  for await (const chunk of fs.createReadStream(file)) hash.update(chunk);
  return hash.digest('hex');
}
function candidateToData(candidate, channel) {
  return { tag:candidate.v.tag, channel, name:candidate.asset.name,
    digest:candidate.asset.digest.toLowerCase(), size:candidate.asset.size,
    url:candidate.asset.browser_download_url };
}
function validCandidate(data) {
  if (!data || !['beta','stable'].includes(data.channel) ||
      !assetIsOfficial({name:data.name, digest:data.digest,size:data.size,browser_download_url:data.url},data.tag)) return false;
  return !!(version(data.tag) && newer(version(data.tag),version(CURRENT)));
}
function readCandidate() {
  try {
    const data = JSON.parse(fs.readFileSync(CANDIDATE,'utf8'));
    return validCandidate(data) && data.channel === readChannel() ? data : null;
  } catch (_) { return null; }
}
function saveCandidate(candidate, channel) {
  atomicText(CANDIDATE,JSON.stringify(candidateToData(candidate,channel))+'\n');
}
function releaseMatchesPinned(releases, pinned) {
  const found = chooseRelease(releases, pinned.channel);
  // Never silently swap targets between CHECK and PREPARE.
  return found && [found.v.tag,found.asset.name,found.asset.digest.toLowerCase(),found.asset.size,
    found.asset.browser_download_url].every((v,i)=>v===[pinned.tag,pinned.name,pinned.digest,pinned.size,pinned.url][i]) ? found : null;
}
function preferenceDir() {
  // SDL_GetPrefPath(MustardOS, MustardTube) on Linux uses XDG_DATA_HOME,
  // otherwise ~/.local/share. Do not change the runtime preference location.
  const base = process.env.XDG_DATA_HOME || path.join(process.env.HOME || APP,'.local','share');
  return path.join(base,'MustardOS','MustardTube');
}
function userFileMap() {
  return [...USER_FILES.map(name=>[name,path.join(DATA,name)]),
    ['prefs/settings.ini',path.join(preferenceDir(),'settings.ini')]];
}
function backupUserData(dir, tag = CURRENT, now = new Date()) {
  // Non-cache user preferences and history only. Never archive signed stream URLs,
  // session logs, radio credentials, yt-dlp caches or downloaded media.
  const recovery = path.join(dir,'.mustardtube-recovery');
  fs.mkdirSync(recovery,{recursive:true,mode:0o700});
  const files = {};
  let total = 0;
  for (const [name,file] of userFileMap()) {
    let stat;
    try { stat = fs.lstatSync(file); } catch(e) { if(e.code === 'ENOENT') continue; throw e; }
    if (!stat.isFile() || stat.size > 2*1024*1024) throw new Error('User-data backup contains an unsupported file');
    total += stat.size;
    if (total > 5*1024*1024) throw new Error('User-data backup exceeds safety limit');
    const bytes = fs.readFileSync(file);
    files[name] = {sha256:crypto.createHash('sha256').update(bytes).digest('hex'),base64:bytes.toString('base64')};
  }
  const stamp=now.toISOString().replace(/[:.]/g,'-');
  const base=`MustardTube-settings-${tag}-${stamp}.json`;
  const final=path.join(recovery,base);
  if (!enoughSpace(recovery,total*2+MIN_FREE_BUFFER)) throw new Error('Not enough space for settings backup');
  const payload={schema:1, source:tag, made_at:now.toISOString(), files};
  const content=JSON.stringify(payload)+'\n';
  // Do not overwrite existing backups; a second attempt creates its own snapshot.
  const temp=`${final}.${process.pid}.part`;
  try {
    fs.writeFileSync(temp,content,{flag:'wx',mode:0o600});
    fs.renameSync(temp,final);
  } finally { try { fs.unlinkSync(temp); } catch (_) { /* renamed */ } }
  return final;
}
function restoreUserData(backup, destination = DATA, prefs = preferenceDir()) {
  // Explicit developer/admin recovery only. Validate *all* data before writing
  // so corrupt recovery files cannot trigger a partial restore.
  const doc = JSON.parse(fs.readFileSync(backup,'utf8'));
  if (!doc || doc.schema !== 1 || !doc.files || typeof doc.files !== 'object' ||
      Array.isArray(doc.files)) throw new Error('Invalid recovery file');
  const validated = [];
  let total=0;
  for (const [name,info] of Object.entries(doc.files)) {
    if (!(USER_FILES.includes(name) || name === 'prefs/settings.ini') || !info || typeof info.base64 !== 'string' ||
        typeof info.sha256 !== 'string' || !/^[a-f0-9]{64}$/.test(info.sha256)) throw new Error('Invalid recovery content');
    const bytes=Buffer.from(info.base64,'base64');
    total+=bytes.length;
    if(total>5*1024*1024 || crypto.createHash('sha256').update(bytes).digest('hex')!==info.sha256)
      throw new Error('Recovery checksum mismatch');
    const target=name==='prefs/settings.ini'?path.join(prefs,'settings.ini'):path.join(destination,name);
    if(fs.existsSync(target) && !fs.lstatSync(target).isFile()) throw new Error('Recovery target is not a file');
    validated.push({target,bytes});
  }
  for(const {target,bytes} of validated) atomicText(target,bytes);
  return validated.length;
}
async function downloadPinned(release) {
  const dir=archiveDir();
  const asset=release.asset;
  if (!enoughSpace(dir, asset.size+5*1024*1024)) throw new Error('Not enough free space on SD card');
  const final=path.join(dir,asset.name);
  const verifyArchive = file => {
    const checked=spawnSync('unzip',['-tq',file],{encoding:'utf8',timeout:120000,maxBuffer:128*1024});
    if(checked.error || checked.status!==0) throw new Error('Invalid muxapp archive');
    const listed=spawnSync('unzip',['-Z1',file],{encoding:'utf8',timeout:30000,maxBuffer:1024*1024});
    if(listed.error || listed.status!==0 || !validArchiveEntries(listed.stdout.split('\n').filter(Boolean)))
      throw new Error('Unsafe muxapp archive layout');
  };
  if (fs.existsSync(final)) {
    const st=fs.lstatSync(final);
    if (!st.isFile() || st.size !== asset.size || await fileSha256(final)!==asset.digest.slice(7).toLowerCase())
      throw new Error('Different file already uses this release name');
    safeStatus('verifying','Checking existing download',release.v.tag,`${Math.ceil(asset.size/1048576)} MiB`);
    verifyArchive(final);
  } else {
    const temp=path.join(dir,`.mustardtube-${process.pid}-${Date.now()}.part`);
    let fd;
    try {
      fd=fs.openSync(temp,'wx',0o600);
      const res=await request(asset.browser_download_url);
      const hash=crypto.createHash('sha256');
      let count=0, lastStatus=0;
      const deadline=Date.now()+12*60*1000;
      const totalMiB=(asset.size/1048576).toFixed(1);
      const sizeLabel=`${totalMiB} MiB`;
      safeStatus('downloading',`Downloading 0.0 / ${totalMiB} MiB`,release.v.tag,sizeLabel,0);
      for await (const chunk of res) {
        count+=chunk.length;
        if(count>asset.size || Date.now()>deadline) {res.destroy();throw new Error('Oversized or timed-out download');}
        fs.writeSync(fd,chunk);hash.update(chunk);
        const now=Date.now();
        // At most two small atomic status updates per second, regardless of network chunk rate.
        if(now-lastStatus>=500 || count===asset.size) {
          lastStatus=now;
          const pct=Math.floor(count*100/asset.size);
          safeStatus('downloading',`Downloading ${(count/1048576).toFixed(1)} / ${totalMiB} MiB`,release.v.tag,sizeLabel,pct);
        }
      }
      fs.fsyncSync(fd);fs.closeSync(fd);fd=undefined;
      safeStatus('verifying','Verifying SHA-256 and package',release.v.tag,sizeLabel);
      if(count!==asset.size || hash.digest('hex')!==asset.digest.slice(7).toLowerCase()) throw new Error('SHA-256 or size mismatch');
      verifyArchive(temp);
      if(fs.existsSync(final)) throw new Error('Release file appeared during download');
      fs.renameSync(temp,final);
    } finally {
      if(fd!==undefined) fs.closeSync(fd);
      try {fs.unlinkSync(temp);} catch(_) { /* renamed or missing */ }
    }
  }
  // Backup is performed after verification; if backup fails the installed app
  // remains untouched, and the verified archive can be used manually.
  safeStatus('backup','Saving settings before update',release.v.tag);
  backupUserData(dir);
  safeStatus('ready','Verified installer and settings backup ready',release.v.tag,`${Math.ceil(asset.size/1048576)} MiB`);
}
function withLock(fn) {
  fs.mkdirSync(DATA,{recursive:true,mode:0o700});
  try {fs.writeFileSync(LOCK,String(process.pid),{flag:'wx',mode:0o600});}
  catch(e) {
    if(e.code!=='EEXIST') throw e;
    try {
      const pid=Number(fs.readFileSync(LOCK,'utf8'));
      if(Number.isSafeInteger(pid)&&pid>1) {process.kill(pid,0);return Promise.resolve();}
    } catch(err) {if(err.code!=='ESRCH') return Promise.resolve();}
    fs.unlinkSync(LOCK);
    fs.writeFileSync(LOCK,String(process.pid),{flag:'wx',mode:0o600});
  }
  return Promise.resolve().then(fn).finally(()=>{
    try {if(fs.readFileSync(LOCK,'utf8')===String(process.pid))fs.unlinkSync(LOCK);} catch(_) {/* none */}
  });
}
function shouldAutoCheck(now=Date.now()) {
  if(!autoCheckEnabled()) return false;
  try {
    const t=Number(fs.readFileSync(LAST_CHECK,'utf8'));
    return !Number.isFinite(t) || t>now || now-t>=AUTO_INTERVAL_MS;
  } catch(_) {return true;}
}
async function main() {
  const task=process.argv[2];
  if(task==='restore-settings') {
    // Explicit administrator-only recovery, never reachable from an app button.
    const name=process.argv[3] || '';
    if(!BACKUP_NAME.test(name) || path.basename(name)!==name) throw new Error('Specify a recovery backup filename');
    const dir=path.join(archiveDir(),'.mustardtube-recovery');
    const file=path.join(dir,name);
    if(!fs.lstatSync(file).isFile()) throw new Error('Recovery file is not regular');
    const n=restoreUserData(file);
    process.stdout.write(`Restored ${n} user preference files\n`);
    return;
  }
  if(!['check','check-if-due','prepare'].includes(task)) throw new Error('Usage: ota-update.js check|check-if-due|prepare|restore-settings FILE');
  if(task==='check-if-due' && !shouldAutoCheck()) return;
  await withLock(async()=>{
    if(task==='prepare') {
      const pinned=readCandidate();
      if(!pinned) throw new Error('Check for a newer release first');
      safeStatus('checking','Confirming selected release before download',pinned.tag);
      const releases=await jsonReleases();
      const selected=releaseMatchesPinned(releases,pinned);
      if(!selected) throw new Error('Release changed; check again before downloading');
      await downloadPinned(selected);
      return;
    }
    safeStatus('checking','Checking official GitHub Releases');
    const releases=await jsonReleases();
    const channel=readChannel();
    const selected=chooseRelease(releases,channel);
    atomicText(LAST_CHECK,String(Date.now())+'\n');
    if(!selected) {
      try {fs.unlinkSync(CANDIDATE);} catch(_) {/* missing */}
      safeStatus('current','No newer compatible release found');return;
    }
    saveCandidate(selected,channel);
    safeStatus('available','Update available - select Prepare update',selected.v.tag,`${Math.ceil(selected.asset.size/1048576)} MiB`);
  });
}
if(require.main===module) main().catch(e=>{
  // Keep a useful error code in logs, but never log potentially signed URLs.
  console.error('OTA failure code:',e.code || (e.name==='Error'?'ERROR':e.name) || 'ERROR');
  safeStatus('error',friendlyError(e));
  process.exitCode=1;
});
module.exports={version,newer,assetIsOfficial,chooseRelease,validRedirect,validArchiveEntries,enoughSpace,
  safeStatus,friendlyError,archiveDir,candidateToData,validCandidate,releaseMatchesPinned,backupUserData,restoreUserData,shouldAutoCheck,downloadPinned,preferenceDir};
