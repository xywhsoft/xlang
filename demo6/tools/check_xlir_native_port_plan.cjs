const fs=require('fs'),path=require('path'),cp=require('child_process'),crypto=require('crypto');
const value=name=>{const at=process.argv.indexOf(name);if(at<0||!process.argv[at+1])throw Error('Required '+name);return process.argv[at+1];};
const root=path.resolve(__dirname,'..');process.chdir(root);
const output=path.resolve(value('--directory')),stage=path.join(output,'stage'),base=path.resolve(value('--native-directory'));
const linux=process.argv.includes('--linux');
const wsl=p=>'/mnt/'+p[0].toLowerCase()+p.slice(2).replace(/\\/g,'/');
if(fs.existsSync(output))throw Error('Preserve evidence');
const report=JSON.parse(fs.readFileSync(path.join(base,'results.json')));
if(!report.Complete||!report.InputsUnchanged)throw Error('Verified complete native candidate required');
const hash=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex').toUpperCase();
for(const entry of report.Inputs)if(hash(entry.Path)!==entry.Sha256)throw Error('Changed candidate '+entry.Path);
const copy=(a,b)=>{fs.mkdirSync(path.dirname(b),{recursive:true});fs.copyFileSync(a,b);};
const files=[
 ['test/cases/xlir_native_port_plan.c','test.c'],
 ['tools/xlir_native_port_impl.c','runtime.c'],
 ['src/runtime/xl6_ownership_plan.c','src/runtime/xl6_ownership_plan.c'],
 ['src/runtime/xl6_ownership_plan.h','src/runtime/xl6_ownership_plan.h'],
 ['src/runtime/xl6_code_lease.h','src/runtime/xl6_code_lease.h'],
 ['src/runtime/xl6_code_owner.h','src/runtime/xl6_code_owner.h'],
 [path.join(base,'vendor/xrt/single/xrt.h'),'lib/xrt/xrt.h']
];
const inputs=files.map(([a,b])=>{const p=path.resolve(a),frozen=path.join(stage,b);copy(p,frozen);return {Path:p,Sha256:hash(p),Frozen:frozen};});
copy(__filename,path.join(output,'runner.cjs'));inputs.push({Path:__filename,Sha256:hash(__filename)});
const lanes=[];
for(const [name,compiler,flags] of (linux?[
 ['linux_gcc','gcc',['-D_GNU_SOURCE','-std=c11','-Wall','-Wextra','-Werror','-O0']],
 ['linux_gcc_o2','gcc',['-D_GNU_SOURCE','-std=c11','-Wall','-Wextra','-Werror','-O2']],
 ['linux_tcc','tcc',['-D_GNU_SOURCE','-std=c11','-Wall','-Werror']]]:[
 ['gcc','E:/software/w64devkit/bin/gcc.exe',['-std=c11','-Wall','-Wextra','-Werror','-O0']],
 ['gcc_o2','E:/software/w64devkit/bin/gcc.exe',['-std=c11','-Wall','-Wextra','-Werror','-O2']],
 ['tcc','E:/software/tcc/tcc.exe',['-std=c11','-Wall','-Werror']]])){
 const exe=path.join(output,name+(linux?'':'.exe')),start=Date.now();
 function command(args,tag,timeout){
  const out=fs.openSync(path.join(output,name+'.'+tag+'.out.log'),'w'),err=fs.openSync(path.join(output,name+'.'+tag+'.err.log'),'w');
  const program=tag==='build'?compiler:(linux?wsl(exe):exe);
  const r=cp.spawnSync(linux?'wsl.exe':program,linux?['-d','Ubuntu','--cd',wsl(stage),'--exec',program,...args]:args,{cwd:stage,windowsHide:true,stdio:['ignore',out,err],timeout});
  fs.closeSync(out);fs.closeSync(err);return {Exit:r.status,Error:r.error?.message,Signal:r.signal};
 }
 const build=command([...flags,'-I.', 'test.c','src/runtime/xl6_ownership_plan.c','runtime.c',...(linux?['-lpthread','-ldl','-lm']:['-lws2_32']),'-o',linux?wsl(exe):exe],'build',120000);
 const binary=build.Exit===0&&fs.existsSync(exe)?{Path:exe,Sha256:hash(exe)}:null;
 const run=build.Exit===0?command([],'run',60000):null;
 const row={Compiler:name,Build:build,Binary:binary,Run:run,Seconds:(Date.now()-start)/1000,Complete:build.Exit===0&&run?.Exit===0};
 lanes.push(row);console.log(JSON.stringify(row));fs.writeFileSync(path.join(output,'progress.json'),JSON.stringify(lanes,null,2)+'\n');
}
const unchanged=inputs.every(x=>hash(x.Path)===x.Sha256&&(!x.Frozen||hash(x.Frozen)===x.Sha256));
const result={Complete:unchanged&&lanes.every(x=>x.Complete),InputsUnchanged:unchanged,Inputs:inputs,Lanes:lanes,
 Native:{Path:path.join(base,'results.json'),Sha256:hash(path.join(base,'results.json'))},
 Scope:'Real unchanged XLIR ownership planner retaining Post and OS request cases and adding pending readiness generations shared with delivered events, real backend destruction, external delivered roots, policy rejection and finalizer resurrection. Not kqueue native runtime, full transport/service graph, generated module, DLL unload or final language/release proof.'};
fs.writeFileSync(path.join(output,'results.json'),JSON.stringify(result,null,2)+'\n');if(!result.Complete)process.exitCode=1;
