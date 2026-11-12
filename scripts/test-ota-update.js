#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
// Offline GitHub OTA integration tests. Do not use network or change installed apps.
'use strict';
const assert=require('assert');
const fs=require('fs');
const os=require('os');
const path=require('path');
const crypto=require('crypto');
const {spawnSync}=require('child_process');
const EventEmitter=require('events');
const root=fs.mkdtempSync(path.join(os.tmpdir(),'mustardtube-ota242-'));
const app=path.join(root,'app');
const data=path.join(app,'data');
const archive=path.join(root,'ARCHIVE');
const prefs=path.join(root,'prefs','MustardOS','MustardTube');
process.env.XDG_DATA_HOME=path.join(root,'prefs');
fs.mkdirSync(data,{recursive:true});fs.mkdirSync(archive);
process.env.MUSTARDTUBE_APP_DIR=app;
process.env.MUSTARDTUBE_DATA_DIR=data;
process.env.MUSTARDTUBE_OTA_TEST_ARCHIVE=archive;
const https=require('https');
const updater=require('./ota-update.js');
const {version,newer,assetIsOfficial,chooseRelease,validRedirect,validArchiveEntries,
  safeStatus,candidateToData,validCandidate,releaseMatchesPinned,
  backupUserData,restoreUserData,shouldAutoCheck,downloadPinned,friendlyError}=updater;
const digest='sha256:'+'c'.repeat(64);
const tag='v0.9.11-beta2.4.3';
function asset(t,overrides={}) {
 const m=/^v(\d+\.\d+\.\d+)(?:-beta(\d+\.\d+\.\d+))?$/.exec(t);
 const name=`MustardTube-v${m[1]}${m[2]?'-Beta'+m[2]:''}-OTA.muxapp`;
 return {name,size:92000000,digest,browser_download_url:`https://github.com/PapaZolaRis/MustardTube/releases/download/${t}/${name}`,...overrides};
}
function release(t,prerelease,overrides={}) {
 return {tag_name:t,prerelease,draft:false,assets:[asset(t)],...overrides};
}
(async()=>{
 try {
 const base=version('v0.9.11-beta2.4.2');
 assert(base && base.beta && !newer(base,base));
 assert(newer(version(tag),base));
 assert(newer(version('v0.9.11'),base));
 assert(!newer(base,version('v0.9.11')));
 assert(!version('v0.9.11-beta2.4.2-trailing'));
 const rels=[release(tag,true),release('v0.9.11-beta2.4.4',true),release('v0.9.12',false),release('v0.9.13',false,{draft:true})];
 assert.strictEqual(chooseRelease(rels,'beta').v.tag,'v0.9.12');
 assert.strictEqual(chooseRelease(rels,'stable').v.tag,'v0.9.12');
 assert.strictEqual(chooseRelease([release(tag,true)],'stable'),null);
 assert.strictEqual(chooseRelease([release(tag,true)],'beta').v.tag,tag);
 assert(!assetIsOfficial(asset(tag,{digest:null}),tag));
 assert(!assetIsOfficial(asset(tag,{size:800*1024*1024}),tag));
 assert(!assetIsOfficial(asset(tag,{browser_download_url:'http://github.com/a'}),tag));
 assert(!assetIsOfficial(asset(tag,{browser_download_url:'https://evil.example/a'}),tag));
 assert(!assetIsOfficial(asset(tag,{name:'../bad.muxapp'}),tag));
 assert(!assetIsOfficial(asset(tag),'v0.9.11-beta2.4.4'));
 assert(validRedirect('https://release-assets.githubusercontent.com/path'));
 assert(!validRedirect('https://evil.example/file'));
 assert(!validRedirect('http://github.com/a/file'));
 assert(!validRedirect('https://github.com.evil.example/file'));
 const entries=['MustardTube/mux_launch.sh','MustardTube/bin/mustardtube','MustardTube/licenses/LICENSE'];
 assert(validArchiveEntries(entries));
 assert(!validArchiveEntries([...entries,entries[0]]),'duplicate ZIP entries must be rejected');
 assert(!validArchiveEntries([...entries,'MustardTube/../../etc/shadow']));
 assert(!validArchiveEntries(['/tmp/mustard.muxapp']));
 const sel=chooseRelease([release(tag,true)],'beta');
 const pinned=candidateToData(sel,'beta');
 assert(validCandidate(pinned));
 assert(releaseMatchesPinned([release(tag,true)],pinned));
 assert(!releaseMatchesPinned([release('v0.9.11-beta2.4.4',true)],pinned),'must not switch release silently');
 assert(!releaseMatchesPinned([release(tag,true,{assets:[asset(tag,{digest:'sha256:'+'d'.repeat(64)})]})],pinned));
 safeStatus('available','Update ready',tag,'88 MiB');
 assert(fs.readFileSync(path.join(data,'ota-status.txt'),'utf8').includes('state=available\n'));
 assert.strictEqual(friendlyError({code:'EAI_AGAIN',message:'getaddrinfo EAI_AGAIN api.github.com'}),
   'Cannot reach GitHub. Check Wi-Fi and retry.');
 assert.strictEqual(friendlyError({code:'ENOTFOUND',message:'getaddrinfo ENOTFOUND api.github.com'}),
   'Cannot reach GitHub. Check Wi-Fi and retry.');
 assert(!friendlyError({message:'http://example.com/private?token=secret'}).includes('token=secret'));
 safeStatus('downloading','Downloading 43.0 / 88.0 MiB',tag,'88 MiB',49);
 assert(fs.readFileSync(path.join(data,'ota-status.txt'),'utf8').includes('progress=49\n'));
 safeStatus('verifying','Verifying SHA-256',tag,'88 MiB');
 assert(fs.readFileSync(path.join(data,'ota-status.txt'),'utf8').includes('progress=-1\n'));

 assert(shouldAutoCheck());
 fs.writeFileSync(path.join(data,'ota-auto-check.txt'),'off\n');
 assert(!shouldAutoCheck());
 fs.writeFileSync(path.join(data,'ota-auto-check.txt'),'on\n');
 fs.writeFileSync(path.join(data,'ota-last-check.txt'),String(Date.now())+'\n');
 assert(!shouldAutoCheck());
 fs.writeFileSync(path.join(data,'ota-last-check.txt'),String(Date.now()-90000000)+'\n');
 assert(shouldAutoCheck());
 console.log('PASS: release policy, pinned version/digest, safe ZIP entries and bounded auto-check');

 // Per-user backup is content-verified and excludes network session data.
 fs.mkdirSync(prefs,{recursive:true});
 fs.writeFileSync(path.join(prefs,'settings.ini'),'theme=paper\n');
 fs.writeFileSync(path.join(data,'settings.ini'),'theme=acidbloom\n');
 fs.writeFileSync(path.join(data,'radio-favourites.tsv'),'Example FM\tstream\n');
 fs.writeFileSync(path.join(data,'watch-history.tsv'),'test-history\n');
 fs.writeFileSync(path.join(data,'stream-url.txt'),'sensitive-signed-url\n');
 fs.writeFileSync(path.join(data,'youtube.log'),'do-not-backup\n');
 const b=backupUserData(archive,'v0.9.11-beta2.4.2');
 const raw=fs.readFileSync(b,'utf8');
 assert(!raw.includes('sensitive-signed-url')&&!raw.includes('do-not-backup'));
 fs.rmSync(path.join(data,'settings.ini'));
 fs.rmSync(path.join(prefs,'settings.ini'));
 assert.strictEqual(restoreUserData(b,data)>0,true);
 assert.strictEqual(fs.readFileSync(path.join(data,'settings.ini'),'utf8'),'theme=acidbloom\n');
 assert.strictEqual(fs.readFileSync(path.join(prefs,'settings.ini'),'utf8'),'theme=paper\n');
 const admin=spawnSync(process.execPath,[path.join(__dirname,'ota-update.js'),'restore-settings',path.basename(b)],{
   env:{...process.env,MUSTARDTUBE_DATA_DIR:data,MUSTARDTUBE_OTA_TEST_ARCHIVE:archive,XDG_DATA_HOME:process.env.XDG_DATA_HOME},
   encoding:'utf8',timeout:5000});
 assert.strictEqual(admin.status,0,admin.stderr);
 assert(admin.stdout.includes('Restored'));
 const denied=spawnSync(process.execPath,[path.join(__dirname,'ota-update.js'),'restore-settings','../unsafe.json'],{
   env:process.env,encoding:'utf8',timeout:5000});
 assert.notStrictEqual(denied.status,0);
 const bad=path.join(root,'bad.json');
 const badDoc=JSON.parse(raw);badDoc.files['settings.ini'].sha256='0'.repeat(64);
 fs.writeFileSync(bad,JSON.stringify(badDoc));
 assert.throws(()=>restoreUserData(bad,data),/checksum/);
 console.log('PASS: settings/favourites/history recovery with integrity checks and no signed URL leakage');

 // Download integration uses mocked HTTPS stream; the actual unzip utility
 // checks CRC/structure. The working application directory must never change.
 const fixture=path.join(root,'fixture');
 for(const rel of entries) {
   const full=path.join(fixture,rel);
   fs.mkdirSync(path.dirname(full),{recursive:true});
   fs.writeFileSync(full,rel+'\n'+'x'.repeat(2000));
 }
 const zip=path.join(root,'fixture.muxapp');
 const zipped=spawnSync('zip',['-qr',zip,'MustardTube'],{cwd:fixture,encoding:'utf8'});
 assert.strictEqual(zipped.status,0,zipped.stderr);
 const contents=fs.readFileSync(zip);
 const sha=crypto.createHash('sha256').update(contents).digest('hex');
 const finalAsset=asset(tag,{size:contents.length,digest:'sha256:'+sha});
 const candidate={v:version(tag),asset:finalAsset};
 const originalGet=https.get;
 https.get=(url,opts,callback)=>{
   const req=new EventEmitter();req.setTimeout=()=>{};req.destroy=()=>{};
   process.nextTick(()=>{
     const s=fs.createReadStream(zip);
     s.statusCode=200;s.headers={};callback(s);
   });
   return req;
 };
 try {
   await downloadPinned(candidate);
   const final=path.join(archive,finalAsset.name);
   assert(fs.existsSync(final));
   assert.strictEqual(fs.readFileSync(path.join(data,'stream-url.txt'),'utf8'),'sensitive-signed-url\n');
   assert(fs.readFileSync(path.join(data,'ota-status.txt'),'utf8').includes('state=ready\n'));
   assert(fs.readdirSync(path.join(archive,'.mustardtube-recovery')).length>=2);
   await downloadPinned(candidate); // idempotent when file already verified
   fs.writeFileSync(final,'malicious replacement');
   await assert.rejects(downloadPinned(candidate),/Different file/);
   assert(!fs.readdirSync(archive).some(x=>x.endsWith('.part')));
 } finally {https.get=originalGet;}
 console.log('PASS: mocked HTTPS package download, checksum, ZIP CRC/layout, atomic archive placement, idempotency and safe refusal');
 } finally { fs.rmSync(root,{recursive:true,force:true}); }
})().catch(e=>{console.error(e.stack||e);process.exitCode=1;});
