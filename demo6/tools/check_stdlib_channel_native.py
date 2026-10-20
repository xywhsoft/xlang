"""Native resident Channel ownership foundation, not public language cutover.
Fresh source objects, strict O0/O2, exact statuses, schema, UTF-8/NUL, nullable
presence, graph retirement, operation pins, real buffered/rendezvous threads,
complete allocation prefixes and an actual demand-linked native closure.
No WaitQueue backend, shipping artifact, full53 or whole-runtime sanitizer claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true')
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    assert build.get('Complete',os.name=='nt') and build['SourceXrt'] and not build['RuntimeOnly'] and not out.exists()
    cc=shutil.which('gcc'); nm=shutil.which('nm'); assert cc and nm
    assert not a.sanitize or os.name!='nt'
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    host=root/'test/cases/stdlib_channel_native.c'; provider=root/'src/runtime/xl6_channel.c'
    planner=root/'test/cases/stdlib_channel_native_plan.c'; memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),public/'build.json',Path(cc),Path(nm),runtime,host,provider,planner,memory}
    for folder in ('src','lib','release/include','test/support'):
        files.update(f for f in (root/folder).rglob('*') if f.suffix in ('.c','.h','.inc'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(parents=True); rows=[]; products=[]; prefixes=[]; controls=[]
    report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,
        Products=products,Prefixes=prefixes,NegativeControls=controls,PublicLanguageCutover=False,
        WaitQueueComplete=False,Full53Acceptance=False,ShippingArtifact=False,
        SelectedClosureSanitizers=a.sanitize,WholeRuntimeInstrumented=False)
    env=dict(os.environ)
    if os.name=='nt': shutil.copy2(runtime,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    def save(): (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,refused=False):
        command=list(map(str,command));invocation=command;options={}
        if command[0]==cc:
            response=out/(name+'.rsp')
            response.write_text('\n'.join('"'+token.replace('\\','/').replace('"','\\"')+'"' for token in command[1:])+'\n',encoding='utf-8')
            product(response);invocation=[cc,'@'+str(response)]
        if os.name!='nt':
            import resource
            options['preexec_fn']=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        result=subprocess.run(invocation,cwd=root,env=env,capture_output=True,timeout=600,**options)
        data=result.stdout+result.stderr; log=out/(name+'.log'); log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Invocation=invocation,Exit=result.returncode,
            ExpectedRejection=refused,Log=str(log),LogSha256=sha(log)));save()
        print(name+': '+str(result.returncode),flush=True)
        if refused: assert result.returncode!=0 and (b'assertion' in data or b'Assertion' in data),(name,data[-6000:])
        else: assert result.returncode==0,(name,data[-6000:])
        return data.decode(errors='replace')
    def product(path): products.append(dict(Path=str(path),Sha256=sha(path)))
    def execute(binary,lane):
        run(lane+'-vectors',[binary]); run(lane+'-lifetimes',[binary,'repeat'])
        for operation in ('create','send'):
            for point in range(4097):
                result=run(lane+'-'+operation+'-'+str(point),[binary,operation,point])
                if 'hit=0 complete-prefix balanced' in result:
                    prefixes.append(dict(Lane=lane,Operation=operation,FirstNoHit=point,Runs=point+1)); break
            else: raise AssertionError('Unclosed allocation prefix')
    ext='.exe' if os.name=='nt' else ''
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Isrc/runtime','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include')]
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=[runtime,*systems]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    others=[f for f in objects if Path(f).name!='xl6_channel.o']
    try:
        for opt in ('O0','O2'):
            binary=out/('channel-'+opt+ext)
            run(opt+'-build',[cc,*flags,'-'+opt,host,provider,memory,*others,*links,'-o',binary]); product(binary)
            execute(binary,opt)
            probe=out/('probe-'+opt+'.o'); probed=out/('probe-'+opt+ext)
            run(opt+'-probe-body',[cc,*flags,'-'+opt,'-DxrtTypeCopyValue=xl6ChannelProbeCopy',
                '-DxrtChannelTrySend=xl6ChannelProbeSend','-DxrtChannelUnit=xl6ChannelProbeUnit','-c',provider,'-o',probe]); product(probe)
            run(opt+'-probe-build',[cc,*flags,'-'+opt,host,probe,memory,*others,*links,'-o',probed]); product(probed)
            run(opt+'-logical-failures',[probed,'logical'])
        body=provider.read_text(encoding='utf-8')
        mutations={
            'false-null-absence':('case XCHANNEL_EMPTY: *status=XL6_CHANNEL_EMPTY_V1; return true;',
                'case XCHANNEL_EMPTY: *status=XL6_CHANNEL_OK_V1; return true;'),
            'unreported-failure':('if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);',
                'if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);'),
            'overwrite-owned':('if (!empty) { xrtSetErrorKind(XERR_STATE); return false; }',
                '(void)empty;'),
            'omit-edge':('if(owner->layout.transfer<3)return true;',
                'if(owner->layout.transfer!=UINT32_MAX)return true;'),
        }
        for tag,(old,new) in mutations.items():
            assert old in body
            bad=out/(tag+'.c'); bad.write_text(body.replace(old,new),encoding='utf-8'); product(bad)
            binary=out/(tag+ext)
            if tag=='unreported-failure':
                obj=out/(tag+'.o')
                run(tag+'-probe',[cc,*flags,'-O0','-DxrtTypeCopyValue=xl6ChannelProbeCopy',
                    '-DxrtChannelTrySend=xl6ChannelProbeSend','-DxrtChannelUnit=xl6ChannelProbeUnit','-c',bad,'-o',obj]); product(obj)
                run(tag+'-build',[cc,*flags,'-O0',host,obj,memory,*others,*links,'-o',binary])
            else: run(tag+'-build',[cc,*flags,'-O0',host,bad,memory,*others,*links,'-o',binary])
            product(binary)
            run(tag+'-reject',[binary,'logical'] if tag=='unreported-failure' else [binary],refused=True)
            controls.append(dict(Name=tag,ActualCompiledProvider=str(bad),ActualExecution=True))
        planBinary=out/('planner'+ext)
        run('planner-build',[cc,*flags,'-Ilib/tcc',planner,*objects,*links,'-o',planBinary]); product(planBinary)
        plan=json.loads(run('actual-native-plan',[planBinary]).strip()); report['SelectedPlan']=plan
        assert 'src/runtime/xl6_channel.c' in plan['Sources'] and 'src/api/xl_channel_bridge.c' not in plan['Sources']
        assert 'XRT_MODULE_CHANNEL' in plan['NativeModules']
        native=out/'selected-native.c'
        modules=plan['NativeModules']+['XRT_MODULE_MEMORY_DEBUG','XRT_MODULE_THREAD']
        assert not any(n in ('XRT_MODULE_ALL','XRUNTIME_MODULE_ALL') for n in modules)
        native.write_text(''.join('#define '+n+'\n' for n in sorted(set(modules)))+
            '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8'); product(native)
        instrumentation=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
        for opt in ('O0','O2'):
            selected=[]
            for index,source in enumerate([native,*[root/s for s in plan['Sources']],memory]):
                obj=out/('selected-'+opt+'-'+str(index)+'.o')
                run('selected-'+opt+'-compile-'+str(index),[cc,*flags,*instrumentation,'-'+opt,'-c',source,'-o',obj]); product(obj); selected.append(obj)
            binary=out/('selected-'+opt+ext)
            run('selected-'+opt+'-link',[cc,*flags,*instrumentation,'-'+opt,host,*selected,*systems,'-o',binary]); product(binary)
            execute(binary,'selected-'+opt)
            symbols=run('selected-'+opt+'-symbols',[nm,'--defined-only',binary])
            assert re.search(r'\bxl6ChannelReceiveV1\b',symbols)
            assert not re.search(r'\b(?:xl6(?:SpscQueue|MpscQueue|MpmcQueue|Tree|Regex)|xrt(?:ChannelSelect|TypedMPMCQueue))\w*\b',symbols)
        report['LifetimesPerLane']=1000; report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        save()
    assert report['Complete'] and report['InputsUnchanged']; print('Native owned Channel foundation verified',flush=True)
if __name__=='__main__': main()
