// Focused native binding/XLIR planner contract gate. This deliberately does
// not require or claim a complete transport graph or a passing release gate.
const fs=require('fs'),path=require('path'),cp=require('child_process'),crypto=require('crypto'),assert=require('assert/strict');
const option=name=>{const i=process.argv.indexOf(name);assert(i>=0&&process.argv[i+1],'Required '+name);return process.argv[i+1];};
const root=path.resolve(__dirname,'..'),runtime=path.resolve(option('--runtime-directory'));
const output=path.resolve(option('--directory')),stage=path.join(output,'stage');assert(!fs.existsSync(output),'Preserve earlier evidence');
const copy=(a,b)=>{fs.mkdirSync(path.dirname(b),{recursive:true});fs.copyFileSync(a,b);};
const hash=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex').toUpperCase();
const walk=d=>fs.readdirSync(d,{withFileTypes:true}).flatMap(e=>e.name==='__pycache__'?[]:e.isDirectory()?walk(path.join(d,e.name)):[path.join(d,e.name)]);
const wsl=p=>'/mnt/'+p[0].toLowerCase()+p.slice(2).replaceAll('\\','/');
fs.mkdirSync(stage,{recursive:true});copy(__filename,path.join(output,'runner.cjs'));
const origins=[];
for(const name of ['src','include','config','tools','tests','single','LICENSE']){
 const source=path.join(runtime,name),files=fs.statSync(source).isDirectory()?walk(source):[source];
 for(const p of files){const relative=path.relative(runtime,p),dest=path.join(stage,relative);copy(p,dest);
  const frozen=relative.replaceAll('\\','/').startsWith('single/')||relative.replaceAll('\\','/')==='include/xrt/features.h'?
   path.join(output,'original_generated',relative):dest;
  if(frozen!==dest)copy(p,frozen);origins.push({Path:p,Relative:relative,Sha256:hash(p),Frozen:frozen});}
}
function command(exe,args,name,linux=false,timeout=120000){
 const stdout=path.join(output,name+'.out.log'),stderr=path.join(output,name+'.err.log'),out=fs.openSync(stdout,'w'),err=fs.openSync(stderr,'w'),start=Date.now();
 const call=linux?['wsl.exe',['-d','Ubuntu','--cd',wsl(stage),'--exec',exe,...args]]:[exe,args];
 const r=cp.spawnSync(...call,{cwd:stage,windowsHide:true,stdio:['ignore',out,err],timeout});fs.closeSync(out);fs.closeSync(err);
 return {Exit:r.status,Signal:r.signal,Error:r.error?.message,Seconds:(Date.now()-start)/1000,Stdout:stdout,Stderr:stderr};
}
const generation=command('C:/Program Files/Python312/python.exe',['-B','tools/amalgamate.py'],'generation');assert.equal(generation.Exit,0);
const copies=[['test/cases/xlir_native_callback_plan.c','collector.c'],
 ...['xl6_ownership_plan.c','xl6_ownership_plan.h','xl6_code_lease.h','xl6_code_owner.h'].map(n=>['src/runtime/'+n,'src/runtime/'+n])];
for(const [source,relative] of copies){const p=path.join(root,source),frozen=path.join(stage,relative);copy(p,frozen);origins.push({Path:p,Relative:relative,Sha256:hash(p),Frozen:frozen});}
copy(path.join(stage,'single/xrt.h'),path.join(stage,'lib/xrt/xrt.h'));
const inputs=walk(stage).map(p=>({Path:p,Sha256:hash(p)}));inputs.push({Path:__filename,Sha256:hash(__filename)});
fs.writeFileSync(path.join(output,'inputs.json'),JSON.stringify({Inputs:inputs,Origins:origins},null,2)+'\n');
const lanes=[];
for(const [linux,name] of [[false,'gcc'],[false,'gcc_o2'],[false,'tcc'],[true,'gcc'],[true,'gcc_o2'],[true,'tcc'],[true,'sanitizer']]){
 const prefix=(linux?'linux_':'windows_')+name,binary=path.join(output,prefix+(linux?'':'.exe'));
 const compiler=linux?(name==='tcc'?'/usr/bin/tcc':'/usr/bin/gcc'):(name==='tcc'?'E:/software/tcc/tcc.exe':'E:/software/w64devkit/bin/gcc.exe');
 const toolHash=linux?cp.execFileSync('wsl.exe',['-d','Ubuntu','--exec','sha256sum',compiler],{encoding:'utf8',windowsHide:true}).slice(0,64).toUpperCase():hash(compiler);
 const args=['-std=c11','-Wall','-Wextra','-Werror',...(name==='gcc_o2'?['-O2']:name==='sanitizer'?['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined']:[]),
 '-DXRT_MODULE_MEMORY_DEBUG','-DXRT_MODULE_NET_TCP','-DXRT_MODULE_NET_UDP',...(linux?['-D_GNU_SOURCE','-DXRT_MODULE_NET_PORT_URING']:[]),
 '-I.','-Isingle','collector.c','src/runtime/xl6_ownership_plan.c',...(linux?['-pthread','-ldl','-lm']:['-lws2_32']),'-o',linux?wsl(binary):binary];
 const build=command(compiler,args,prefix+'.build',linux,180000);
 const run=build.Exit===0?(name==='sanitizer'?command('/usr/bin/env',['ASAN_OPTIONS=abort_on_error=1:detect_leaks=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1',wsl(binary)],prefix+'.run',linux,60000):
 command(linux?wsl(binary):binary,[],prefix+'.run',linux,60000)):{Exit:null,NotRun:true};
 const signature=run.Exit===0&&fs.readFileSync(run.Stdout,'utf8').match(/Callback collection: cycles=4 shared=2 restored=4 active-busy=4 wrong-policy=4 context-drops=4 finalizers=4 freed=4 exact-memory=ok; full transport graph incomplete/);
 const network=run.Exit===0&&fs.readFileSync(run.Stdout,'utf8').match(/Callback ownership: cases=(\d+) drops=.*actual binding references, exact owner edges and memory; full transport graph incomplete/);
 const row={Platform:linux?'linux':'windows',Compiler:name,Tool:{Path:compiler,Sha256:toolHash},Build:build,Arguments:args,Run:run,Signature:signature?signature[0]:null,
  NetworkCases:network?Number(network[1]):null,Binary:build.Exit===0?{Path:binary,Sha256:hash(binary)}:null,
  Complete:build.Exit===0&&run.Exit===0&&!!signature&&!!network&&Number(network[1])===(linux?6:4)};
 lanes.push(row);console.log(JSON.stringify(row));fs.writeFileSync(path.join(output,'progress.json'),JSON.stringify(lanes,null,2)+'\n');
 if(build.Exit!==0)console.log(fs.readFileSync(build.Stderr,'utf8'));if(run.Exit!==0&&run.Stderr)console.log(fs.readFileSync(run.Stderr,'utf8'));
}
const changed=[...inputs,...origins].filter(r=>hash(r.Path)!==r.Sha256||(r.Frozen&&hash(r.Frozen)!==r.Sha256)).map(r=>r.Path);
const complete=!changed.length&&lanes.length===7&&lanes.every(l=>l.Complete);
const report={Complete:complete,InputsUnchanged:!changed.length,Changed:changed,
 Inputs:inputs,Origins:origins,Generation:generation,Lanes:lanes,RuntimeSource:runtime,
 NativeCallbackBindingCollectionAdapterVerified:complete,CompleteNativeTransportGraphVerified:false,LanguageModuleAcceptance:false,
 Scope:'Exact binding policy admission, active refusal, real shared/cyclic references, external roots, abort/restore, finalizer resurrection and outside-freeze Drop with the unchanged XLIR planner. Existing native TCP/UDP/inherited-listener callback tests also execute. This focused gate does not certify full transport/service/DLL/FFI/release/default-Cgen completion.'};
fs.writeFileSync(path.join(output,'results.json'),JSON.stringify(report,null,2)+'\n');if(!report.Complete)process.exitCode=1;
