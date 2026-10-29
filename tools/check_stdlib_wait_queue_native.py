"""Independent typed MPMC WaitQueue native foundation, not language cutover.
Real selected native closure, strict O0/O2, UTF-8/NUL/string/Future/null slots,
cycle retirement, four producers/consumers, close/drain, allocation prefixes,
scripted spurious wakes and post-publication notification error, compiled bad
providers and optional selected ASan/UBSan. No shipping/full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from xlir_public_build_inputs import verify_public_build
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm');assert cc and nm and not out.exists() and (not a.sanitize or os.name!='nt')
    build,pinned,_=verify_public_build(root,public)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    planner=root/'test/cases/stdlib_wait_queue_native_plan.c';host=root/'test/cases/stdlib_wait_queue_native.c'
    provider=root/'src/runtime/xl6_wait_queue.c';body=root/'src/runtime/xl6_concurrent_queue_impl.h'
    memory=root/'test/support/module_product_memory.c'
    pinned.update((Path(__file__).resolve(),Path(cc),Path(nm),runtime,planner,host,provider,body,memory))
    for folder in ('src/runtime','src/api','lib','release/include','test/support'):
        pinned.update((root/folder).rglob('*.h'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(pinned)]
    out.mkdir();rows=[];products=[];controls=[];env=dict(os.environ);ext='.exe' if os.name=='nt' else ''
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Isrc/runtime','-Ilib/tcc','-I'+str(public/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=[runtime,*systems]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,NegativeControls=controls,
        PublicLanguageCutover=False,Full53Acceptance=False,ShippingArtifact=False,SelectedNativeSanitized=a.sanitize,Scope=__doc__)
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)))
    def run(name,cmd,bad=False):
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=300)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,cmd)),Exit=r.returncode,ExpectedRejection=bad,Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if bad else r.returncode==0),(name,data[-7000:])
        if bad:assert b'assert' in data.lower(),(name,r.returncode)
        return data.decode(errors='replace')
    try:
        executable=out/('planner'+ext);run('planner-build',[cc,*flags,'-O0',planner,*objects,*links,'-o',executable]);product(executable)
        plan=json.loads(run('actual-native-plan',[executable]));report['SelectedPlan']=plan
        assert 'src/runtime/xl6_wait_queue.c' in plan['Sources'] and 'src/runtime/xl6_channel.c' not in plan['Sources']
        assert 'src/runtime/xl6_queue_mpmc.c' not in plan['Sources']
        assert 'XRUNTIME_MODULE_TYPED_QUEUE_MPMC' in plan['NativeModules'] and 'XRT_MODULE_CHANNEL' not in plan['NativeModules']
        native=out/'native.c';modules=sorted(set(plan['NativeModules']+['XRT_MODULE_MEMORY_DEBUG','XRT_MODULE_THREAD','XRT_MODULE_FUTURE']))
        native.write_text(''.join('#define '+n+'\n' for n in modules)+'#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n')
        product(native);sources=[native,*[root/name for name in plan['Sources']],memory];cached={}
        for opt in ('O0','O2'):
            providers=[]
            for i,source in enumerate(sources):
                obj=out/(opt+'-'+str(i)+'.o');run(obj.stem,[cc,*flags,'-'+opt,'-c',source,'-o',obj]);product(obj);providers.append(obj)
            cached[opt]=providers
            binary=out/('native-'+opt+ext)
            run(opt+'-link',[cc,*flags,'-'+opt,host,*providers,*systems,'-o',binary]);product(binary)
            assert 'WaitQueue Future cycle:' in run(opt+'-vectors',[binary]);report[opt+'Prefixes']=run(opt+'-prefixes',[binary,'prefixes'])
            assert report[opt+'Prefixes'].count('first-no-hit=')==2
            symbols=run(opt+'-symbols',[nm,'--defined-only',binary])
            names={line.split()[-1].lstrip('_') for line in symbols.splitlines() if line.split()}
            assert 'xrtChannelCreate' not in names and 'xl6MpmcQueueCreateTakeV1' not in names
            assert {'xl6WaitQueueReceiveV1','xrtTypedMPMCQueueTryPop'}<=names
            probe=out/(opt+'-probe.o')
            probes=['-Dxl6CondSignalV1=xl6WaitProbeSignal','-Dxl6CondWaitUntilNative=xl6WaitProbeUntil']
            clockProbe='#include "release/include/xl_clock.h"\nXL6_DeadlineV1 xl6WaitProbeDeadlineAfter(uint64_t);\n#define xl6DeadlineAfterMillisecondsV1 xl6WaitProbeDeadlineAfter\n'
            probeSource=out/(opt+'-probe.c');probeSource.write_text(clockProbe+'#include "src/runtime/xl6_wait_queue.c"\n');product(probeSource)
            run(opt+'-probe-build',[cc,*flags,'-'+opt,*probes,'-c',probeSource,'-o',probe]);product(probe)
            replacements=providers[:];replacements[plan['Sources'].index('src/runtime/xl6_wait_queue.c')+1]=probe
            mutex=out/(opt+'-mutex-probe.o')
            run(opt+'-mutex-probe-build',[cc,*flags,'-'+opt,'-DxrtMutexUnit=xl6WaitProbeMutexUnit','-c',root/'src/runtime/xl6_mutex.c','-o',mutex]);product(mutex)
            replacements[plan['Sources'].index('src/runtime/xl6_mutex.c')+1]=mutex
            logical=out/('logical-'+opt+ext);run(opt+'-logical-link',[cc,*flags,'-'+opt,host,*replacements,*systems,'-o',logical]);product(logical)
            run(opt+'-logical-execute',[logical,'logical'])
        original=body.read_text();prefix=provider.read_text().split('#include "src/runtime/xl6_concurrent_queue_impl.h"')[0]
        changes={
            'false-absence':('actual=XL6_CHANNEL_EMPTY_V1;ok=true;break;','actual=XL6_CHANNEL_OK_V1;ok=true;break;'),
            'ignored-notify-error':('if(result==XQUEUE_OK && !xl6CondSignalV1(owner->available,identity))xl6ErrorScopeCaptureFailure(&scope);','if(result==XQUEUE_OK)(void)xl6CondSignalV1(owner->available,identity);'),
            'reset-deadline':('xwaitresult waited=xl6CondWaitUntilNative',
                'deadline=xl6DeadlineAfterMillisecondsV1((uint64_t)milliseconds);\n        xwaitresult waited=xl6CondWaitUntilNative'),
        }
        for tag,(before,after) in changes.items():
            assert original.count(before)==1;mutant=out/(tag+'.c');mutant.write_text(clockProbe+prefix+original.replace(before,after));product(mutant)
            obj=out/(tag+'.o');run(tag+'-build',[cc,*flags,'-O0',*probes,'-c',mutant,'-o',obj]);product(obj)
            providers=cached['O0'][:];providers[plan['Sources'].index('src/runtime/xl6_wait_queue.c')+1]=obj
            providers[plan['Sources'].index('src/runtime/xl6_mutex.c')+1]=out/'O0-mutex-probe.o'
            binary=out/(tag+ext);run(tag+'-link',[cc,*flags,'-O0',host,*providers,*systems,'-o',binary]);product(binary)
            run(tag+'-refused',[binary] if tag=='false-absence' else [binary,'logical'],True);controls.append(tag)
        if a.sanitize:
            sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];providers=[]
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            for i,source in enumerate(sources):
                obj=out/('san-'+str(i)+'.o');run(obj.stem,[cc,*sf,'-O1','-c',source,'-o',obj]);product(obj);providers.append(obj)
            binary=out/'san';run('san-link',[cc,*sf,'-O1',host,*providers,*systems,'-fsanitize=address,undefined','-no-pie','-o',binary]);product(binary)
            assert 'WaitQueue Future cycle:' in run('san-vectors',[binary]);report['SanPrefixes']=run('san-prefixes',[binary,'prefixes'])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=report['InputsUnchanged'];assert report['Complete']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('WaitQueue native MPMC foundation verified',flush=True)
if __name__=='__main__':main()
