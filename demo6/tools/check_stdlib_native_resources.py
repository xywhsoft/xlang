"""Audited resident resources: exact native graphs, rollback and forged-policy refusal.
Builds the real selected providers; optional Linux instrumentation covers their
implementations plus the actual XRT/xruntime core, not all other providers.
Shipping embedded VFS, language IR and independent AOT are separate proofs.
"""
from pathlib import Path
import argparse,json,os,re,shutil,subprocess,sys,time
from xlir_public_build_inputs import sha,verify_public_build

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve();assert not out.exists()
    build,pinned,compared=verify_public_build(root,public);cc=shutil.which('gcc');assert cc and not(a.sanitize and os.name=='nt')
    task=Path(__file__).resolve().parents[1];native=task/'test/support/native_resources_native.c'
    text=(root/'src/runtime/xl6_native_resource_policies.inc').read_text(encoding='utf-8-sig')
    policies=re.findall(r'XL6_NATIVE_RESOURCE_POLICY\((?:"(\w+)"|NULL),(\w+),(\w+)\)',text);assert len(policies)==23
    assert len({getter for _,_,getter in policies})==23
    case_text=native.read_text(encoding='utf-8-sig')
    assert {getter for _,_,getter in policies}==set(re.findall(r'\{"[^"]+",(xl6\w+HandlePolicyV1Get),\d+\}',case_text))
    getters={getter for _,_,getter in policies};providers=[]
    for path in sorted((root/'src/runtime').glob('*.c')):
        declared=set(re.findall(r'XL6_DEFINE_NATIVE_(?:LEAF_)?HANDLE_POLICY\((\w+),',path.read_text(encoding='utf-8-sig')))
        if declared & getters:providers.append(str(path.relative_to(root)).replace('\\','/'))
    assert len(providers)==17
    selected=['tools/xrt_single_header_impl.c','src/runtime/xl6_native_handle.c','src/runtime/xl6_ownership_catalog.c',*providers,
        *[str(path.relative_to(root)).replace('\\','/') for path in sorted((root/'src/runtime').glob('xl6_ownership_domain_resource_*.c'))]]
    assert len(selected)==37
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    child_source=root/'test/cases/xlir_process_child.c'
    pinned.update((Path(__file__).resolve(),Path(sys.executable),Path(cc),native,child_source,runtime,*[root/name for name in selected]))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)];out.mkdir(parents=True)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    env=dict(os.environ)
    if a.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:detect_stack_use_after_return=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    san=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if a.sanitize else []
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)])]
    rows=[];proofs=[];suffix='.exe' if os.name=='nt' else ''
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Proofs=proofs,ComparedSources=compared,Scope=__doc__,
        SelectedProvidersInstrumented=a.sanitize,OtherProvidersInstrumented=False,InstrumentedSources=selected if a.sanitize else [],Full53Acceptance=False)
    def run(name,command):
        command=list(map(str,command));start=time.monotonic();r=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=r.returncode,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        assert r.returncode==0,(name,data[-5000:]);return data
    try:
        child=out/('process-child'+suffix)
        run('child.build',[cc,'-std=c11','-O2',*(['-municode'] if os.name=='nt' else ['-D_GNU_SOURCE']),child_source,'-o',child])
        for opt in ('O0','O2'):
            actual=list(objects)
            if a.sanitize:
                actual=[path for path in objects if Path(path).stem not in {Path(name).stem for name in selected}]
                for index,name in enumerate(selected):
                    obj=out/(opt+'.provider'+str(index)+'.o')
                    run(opt+'.provider'+str(index),[cc,*flags,*san,'-'+opt,'-DXL_ENABLE_EXE_COMPILER=1','-DTCC_DISABLE_BUILTIN_VFS',
                        '-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label','-Wno-missing-field-initializers','-c',root/name,'-o',obj]);actual.append(str(obj))
            binary=out/('native.'+opt+suffix);run(opt+'.native.build',[cc,*flags,*san,'-'+opt,native,*actual,*libs,'-o',binary])
            output=run(opt+'.native',[binary,child]).decode()
            matches=re.findall(r'native resource (\S+) nodes=(\d+) prefixes=(\d+) first-no-hit=(\d+) balanced',output);assert len(matches)==23
            assert all(int(runs)==int(first)+1 and int(first)>0 for _,_,runs,first in matches)
            proofs.extend(dict(Optimization=opt,Policy=name,Nodes=int(nodes),Prefixes=int(runs),FirstNoHit=int(first)) for name,nodes,runs,first in matches)
            assert '46 rejecting controls; two concurrent close transitions' in output
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,NativeResourceGraphAcceptance=True)
    except Exception as error:report['Error']=repr(error);print(repr(error),flush=True)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print('native resource phases='+str(len(rows))+' complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
