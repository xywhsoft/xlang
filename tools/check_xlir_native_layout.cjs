const fs=require('fs'),path=require('path'),cp=require('child_process'),crypto=require('crypto');
const option=name=>{const i=process.argv.indexOf(name);if(i<0||!process.argv[i+1])throw Error('Required '+name);return path.resolve(process.argv[i+1]);};
process.chdir(path.resolve(__dirname,'..'));
const output=option('--directory'),base=option('--native-directory'),before=option('--baseline-directory');
const gccOnly=process.argv.includes('--gcc-only');
const observedBefore=process.argv.includes('--observed-before-directory')?option('--observed-before-directory'):null;
if(fs.existsSync(output))throw Error('Preserve layout evidence');
const hash=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex').toUpperCase();
const reportPath=path.join(base,'results.json'),candidate=JSON.parse(fs.readFileSync(reportPath));
if(!candidate.Complete||!candidate.InputsUnchanged)throw Error('Verified native candidate required');
for(const i of candidate.Inputs)if(hash(i.Path)!==i.Sha256)throw Error('Candidate changed '+i.Path);
const copy=(a,b)=>{fs.mkdirSync(path.dirname(b),{recursive:true});fs.copyFileSync(a,b);};
const source=path.resolve('test/cases/xlir_native_layout.c'),test=path.join(output,'test.c');copy(source,test);copy(__filename,path.join(output,'runner.cjs'));
const inputs=[{Path:source,Sha256:hash(source),Frozen:test},{Path:__filename,Sha256:hash(__filename),Frozen:path.join(output,'runner.cjs')},
 {Path:reportPath,Sha256:hash(reportPath)},{Path:path.join(before,'results.json'),Sha256:hash(path.join(before,'results.json'))}];
const lanes=[],skipped=[],unavailable=new Set();
for(const [revision,native] of [['before',before],['after',base]]){
 const vendor=path.join(native,'vendor/xrt');
 // Both revisions use their immutable full source/header snapshots. No edits
 // to the candidate and no regenerated live/release single header are needed.
 for(const [name,compiler,flags] of [['gcc','E:/software/w64devkit/bin/gcc.exe',['-std=c11','-Wall','-Wextra','-Werror','-O0']],
  ['gcc_o2','E:/software/w64devkit/bin/gcc.exe',['-std=c11','-Wall','-Wextra','-Werror','-O2']],
  ['tcc','E:/software/tcc/tcc.exe',['-std=c11','-Wall','-Werror']]]){
  inputs.push({Path:compiler,Sha256:hash(compiler)});
  for(const [variant,extra] of [['core',[]],['dial',['-DXRT_MODULE_NET_TCP_DIAL']],['future',['-DXRT_MODULE_NET_TCP_FUTURE']],['both',['-DXRT_MODULE_NET_TCP_DIAL','-DXRT_MODULE_NET_TCP_FUTURE']]]){
   const prefix=revision+'_'+name+'_'+variant,exe=path.join(output,prefix+'.exe');
   if((gccOnly&&name==='tcc')||unavailable.has(name)){skipped.push({Revision:revision,Compiler:name,Variant:variant,Reason:'Compiler execution unverified; preserve quarantine gap, no retry'});continue;}
   if(revision==='before'&&observedBefore){
    const observedTest=path.join(observedBefore,'test.c'),log=path.join(observedBefore,prefix+'.run.log');
    if(hash(observedTest)!==hash(test))throw Error('Observed baseline test differs');
    const layout=JSON.parse(fs.readFileSync(log,'utf8'));
    inputs.push(...[observedTest,log,path.join(observedBefore,'runner.cjs'),path.join(observedBefore,'interrupted_attempt.json')].map(p=>({Path:p,Sha256:hash(p)})));
    lanes.push({Revision:revision,Compiler:name,Variant:variant,Build:null,Run:null,Layout:layout,ObservedOutputReused:true,ExitCodeNotPersisted:true,Evidence:log});continue;
   }
   const build=cp.spawnSync(compiler,[...flags,'-DXRT_MODULE_NET_TCP',...extra,'-I'+path.join(vendor,'include'),'-I'+path.join(vendor,'src'),test,'-o',exe],{windowsHide:true,encoding:'utf8',timeout:30000});
   fs.writeFileSync(path.join(output,prefix+'.build.log'),(build.stdout||'')+(build.stderr||''));
   const run=build.status===0?cp.spawnSync(exe,[],{windowsHide:true,encoding:'utf8',timeout:10000}):null;
   fs.writeFileSync(path.join(output,prefix+'.run.log'),(run?.stdout||'')+(run?.stderr||''));
   const layout=run?.stdout?JSON.parse(run.stdout):null;
   let binary=null,binaryError;try{if(fs.existsSync(exe))binary={Path:exe,Sha256:hash(exe)};}catch(error){binaryError=error.message;}
   if(build.error||run?.error||binaryError)unavailable.add(name);
   lanes.push({Revision:revision,Compiler:name,Variant:variant,Build:build.status,Run:run?.status??null,Error:build.error?.message||run?.error?.message||binaryError,
    Layout:layout,Binary:binary});
   fs.writeFileSync(path.join(output,'progress.json'),JSON.stringify(lanes,null,2)+'\n');
  }
 }
}
const unchanged=inputs.every(i=>hash(i.Path)===i.Sha256)&&candidate.Inputs.every(i=>hash(i.Path)===i.Sha256);
const after=lanes.filter(l=>l.Revision==='after'),prior=lanes.filter(l=>l.Revision==='before');
const count=gccOnly?8:12,selected=unchanged&&after.length===count&&after.every(l=>l.Build===0&&l.Run===0)&&
 prior.length===count&&prior.every(l=>l.Layout&&l.Layout.Stream>l.Layout.StreamLimit&&(l.ObservedOutputReused||(l.Build===0&&l.Run===1)));
const result={Complete:!gccOnly&&selected&&skipped.length===0,SelectedLanesPassed:selected,BaselineOversizeObserved:prior.length===count&&prior.every(l=>l.Layout.Stream>l.Layout.StreamLimit),
 InputsUnchanged:unchanged,Inputs:inputs,Lanes:lanes,Skipped:skipped,Native:{Path:reportPath,Sha256:hash(reportPath)},
 Scope:'Actual 64-bit layouts across TCP core/dial/future/both for selected compilers. Original budgets retained; observed baseline outputs do not invent lost exit codes. TCC layout probes skipped after quarantine are not passed by GCC or by separate native runtime tests. Not runtime semantics or a 32-bit/platform claim.'};
fs.writeFileSync(path.join(output,'results.json'),JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify({Complete:result.Complete,SelectedLanesPassed:selected,After:after,BaselineLayouts:prior.length,Skipped:skipped}));if(!result.Complete)process.exitCode=1;
