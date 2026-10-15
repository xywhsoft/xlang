"""Real static-module ownership record and bounded checked retirement driver.

Selected XRT/xruntime and every participating resident implementation are built
from source. This is the generic native foundation, NOT generated entry/VFS or
language AOT graph acceptance. No fabricated RC, resolver success or destructor.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time

SOURCES=('test/cases/stdlib_static_module_native_host.c','test/support/static_module_native.c',
    'src/runtime/xl6_static_module.c','src/runtime/xl6_sequence.c','src/runtime/xl6_stack.c',
    'src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_ownership_plan.c',
    'src/api/xl_foreign_error.c','src/runtime/xl6_ownership_catalog.c',
    'src/runtime/xl6_ownership_domain_class.c','src/runtime/xl6_ownership_domain_code_lease.c',
    'src/runtime/xl6_ownership_domain_sequence.c')

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args();root,out=a.root.resolve(),a.out.resolve()
    cc=shutil.which('gcc');assert cc and not out.exists() and not (a.sanitize and os.name=='nt')
    files={Path(__file__).resolve(),Path(sys.executable).resolve(),Path(cc).resolve(),*[root/name for name in SOURCES]}
    for folder in ('src/runtime','src/api','lib/xrt','lib/xruntime','release/include','test/support'):
        files.update(path for path in (root/folder).rglob('*') if path.is_file() and path.suffix in ('.h','.c','.inc'))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True);rows=[];prefixes=[];controls=[];env=dict(os.environ);suffix='.exe' if os.name=='nt' else ''
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    if a.sanitize:
        flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,NegativeControls=controls,
        Scope=__doc__,SelectedRuntimeInstrumented=a.sanitize,NativeFoundationAcceptance=False,
        GeneratedEntryAcceptance=False,AotGraphRetirementAcceptance=False,ShippingLibraryAcceptance=False,Full53Acceptance=False)
    def run(name,command,reject=False):
        start=time.monotonic()
        try:
            process=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=180)
            data,code=process.stdout+process.stderr,process.returncode
        except subprocess.TimeoutExpired as error:data,code=(error.stdout or b'')+(error.stderr or b''),'timeout'
        log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=code,ExpectedRejection=reject,
            Seconds=time.monotonic()-start,Log=str(log),LogSha256=sha(log)))
        assert code not in (0,'timeout') if reject else code==0,(name,data[-4000:].decode(errors='replace'))
        return data
    try:
        for opt in ('O0','O2'):
            binary=out/(opt+suffix)
            run(opt+'.build',[cc,*flags,'-'+opt,*[root/name for name in SOURCES],*libs,'-o',binary])
            for mode in range(26):
                data=run(opt+'.mode'+str(mode),[binary,str(mode)]);assert b'once-only destructor, checked recovery balanced' in data
            data=run(opt+'.oom',[binary,'--oom']);match=re.search(rb'full prefixes=(\d+) first-no-hit=(\d+) balanced',data);assert match
            runs,first=map(int,match.groups());assert runs==first+1 and first>0
            prefixes.append(dict(Optimization=opt,Runs=runs,FirstNoHit=first))
        original=(root/'src/runtime/xl6_static_module.c').read_text(encoding='utf-8-sig')
        mutants=[
            ('missing-root-pin','xl6OwnershipPlanTryCreateV5(','xl6OwnershipPlanTryCreateV4(',22),
            ('lost-lease-count','*count=module->leases+module->pins+(module->entry ? 1u : 0u);','*count=module->pins+(module->entry ? 1u : 0u);',0),
            ('lost-finalizer','xl6OwnershipPlanFinalizeV2(module->plan);','XL6_OWNERSHIP_STEP_READY;',0),
            ('reopen-closed','!module->closing && module->leases<','module->leases<',0),
            ('lost-null-failure','if(!outcome.ok && !staticFailed(module)){','if(outcome.error && !staticFailed(module)){',4),
            ('early-globals','if(module->phase==STATIC_COMMIT){','if(module->phase==STATIC_COMMIT){ module->contract.graph->finish();',0),
        ]
        for name,old,new,mode in mutants:
            assert original.count(old)==1,(name,old)
            source=out/(name+'.c');source.write_text(original.replace(old,new),encoding='utf-8')
            binary=out/(name+suffix)
            selected=[source if path=='src/runtime/xl6_static_module.c' else root/path for path in SOURCES]
            run(name+'.build',[cc,*flags,'-O0','-I'+str(root/'src/runtime'),*selected,*libs,'-o',binary])
            data=run(name+'.reject',[binary,str(mode)],True);assert b'assert' in data.lower()
            controls.append(name)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,NativeFoundationAcceptance=True,PhaseCount=len(rows))
    except Exception as error:report['Error']=repr(error);print(repr(error),flush=True)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Static module foundation phases: '+str(len(rows))+' complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
