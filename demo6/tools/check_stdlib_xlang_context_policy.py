"""Audited Context-only resident policy, exact IR, real calls and owning faults.

Full selected compiler/runtime providers, genuine dynamic TCC, no functional
provider replacement. Optional instrumentation covers the actual API, handle
take and policy units, not every provider. Shipping embedded VFS is separate.
"""
from pathlib import Path
import argparse,json,os,re,shutil,subprocess,sys,time
from xlir_public_build_inputs import sha,verify_public_build

def fields(text,name):
    text=re.sub(r'/\*.*?\*/|//[^\n]*','',text,flags=re.S)
    body=re.search(r'(?:typedef\s+)?struct\s+'+name+r'\s*\{([^}]+)\}',text).group(1)
    body=re.sub(r'^\s*#.*$','',body,flags=re.M)
    return [re.sub(r'\s+','',part) for part in body.split(';') if part.strip()]

def audited_layout(text,packages,public_package):
    expected={
        'XL_StringList':['char**items','size_tcount'],
        'XL_CSymbol':['char*name','void*value'],
        'XL_LoadedLibrary':['char*path','HMODULEmodule','void*module'],
        'XL_Context':['char*'+name for name in ('baseDir','workDir','tempDir','tccDllPath','tccLibPath','tccIncludePath',
            'tccWinIncludePath','tccWinapiIncludePath','tccLibSearchPath','tccLibtcc1Path')]+
            ['intsaveTemps','char*lastError','intlastExitCode']+
            ['XL_StringList'+name for name in ('includePaths','libraryPaths','linkLibraries','resourceFiles')]+
            ['XL_CSymbol*symbols','size_tsymbolCount','XL_LoadedLibrary*loadedLibraries','size_tloadedLibraryCount',
             'unsignedintactiveCalls','intdestroyRequested','XL6_BuildInputs*buildInputs',
             'XL6_CompiledPackageSetcompiledPackages']}
    native={
        'XL6_CompiledPackageSet':['XL_CompiledPackage*items','size_tcount'],
        'XL_PackageEntry':['constchar*name','constvoid*address'],
        'XL_CompiledPackage':['size_tsize','unsignedintversion','unsignedintpointerBytes','uint64_tcompilerAbi',
            'constvoid*runtimeIdentity','constchar*name','constchar*packageVersion','constchar*abiVersion',
            'constunsignedchar*interfaceData','size_tinterfaceSize','uint64_tinterfaceHash',
            'constXL_PackageEntry*entries','size_tentryCount',
            'conststructXL_CompiledPackage*const*dependencies','size_tdependencyCount']}
    combined=packages+'\n'+public_package
    return all(fields(text,name)==layout for name,layout in expected.items()) and \
        all(fields(combined,name)==layout for name,layout in native.items())

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve();assert not out.exists()
    build,pinned,compared=verify_public_build(root,public);cc=shutil.which('gcc');assert cc and not (a.sanitize and os.name=='nt')
    task=Path(__file__).resolve().parents[1];ir=task/'test/support/xlang_context_policy_ir.c';native=task/'test/support/xlang_context_policy_native.c'
    fixtures=[root/'test/cases'/name for name in ('stdlib_nested_sdk.xl','stdlib_string_bridge_boundaries.xl','cgen_basic.xl')]
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    tcc=root/'res/tcc/win64/x86_64' if os.name=='nt' else Path(build['NativeRoot'])/'build/linux/tcc-dist'
    selected=('src/api/xl_api.c','src/runtime/xl6_native_handle.c','src/runtime/xl6_xlang_context_policy.c','src/runtime/xl6_buffer_core.c')
    pinned.update((Path(__file__).resolve(),Path(sys.executable).resolve(),Path(cc).resolve(),ir,native,runtime,*fixtures))
    pinned.update(root/name for name in selected)
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)];out.mkdir(parents=True)
    # Real default contexts locate their declaration SDK beside the host.
    # Supplying the TCC core/library path alone is deliberately insufficient.
    shutil.copytree(public/'include',out/'include')
    rows=[];prefixes=[];env=dict(os.environ);windows=os.name=='nt';suffix='.exe' if windows else ''
    if windows:shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,
        ComparedSources=compared,Scope=__doc__,SelectedApiProvidersInstrumented=a.sanitize,
        OtherProvidersInstrumented=False,ContextPolicyAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),'-I'+str(public/'include')]
    if not windows:flags+=['-D_GNU_SOURCE']
    san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if a.sanitize else []
    if a.sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm']
    links=[runtime,*systems,*([] if windows else ['-Wl,-rpath,'+str(runtime.parent)])]
    def run(name,command):
        start=time.monotonic();command=list(map(str,command))
        actual_command=command
        response=None
        if windows and command[0]==cc:
            response=out/(name+'.rsp')
            response.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"' for arg in command[1:])+'\n',encoding='utf-8')
            actual_command=[cc,'@'+str(response)]
        result=subprocess.run(actual_command,cwd=root,env=env,capture_output=True,timeout=180);data=result.stdout+result.stderr
        log=out/(name+'.log');log.write_bytes(data);rows.append(dict(Name=name,Command=command,Exit=result.returncode,
            Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        if response:rows[-1].update(ConsumedCommand=actual_command,Response=str(response),ResponseSha256=sha(response))
        assert result.returncode==0,(name,data[-5000:].decode(errors='replace'));return data
    try:
        source=(root/'src/api/xl_api.c').read_text(encoding='utf-8-sig')
        packages=(root/'src/package/xl6_compiled_package.h').read_text(encoding='utf-8-sig')
        public_package=(root/'release/include/xl_package.h').read_text(encoding='utf-8-sig')
        assert audited_layout(source,packages,public_package)
        assert not audited_layout(source.replace('struct XL_Context {','struct XL_Context { xvalue* owned;'),packages,public_package)
        assert not audited_layout(source.replace('char** items;','xvalue** items;'),packages,public_package)
        assert not audited_layout(source,packages,public_package.replace('const void* address;','xvalue* address;'))
        assert fields((root/'lib/xrt/xrt.h').read_text(encoding='utf-8-sig'),'xbuffer')==['bytesData','size_tSize','size_tCapacity']
        report['AuditedContextAndNestedNativeLayouts']=True
        for opt in ('O0','O2'):
            actual=objects
            if a.sanitize:
                actual=[obj for obj in objects if Path(obj).stem not in {Path(s).stem for s in selected}]
                for index,name in enumerate(selected):
                    obj=out/(opt+'.provider'+str(index)+'.o')
                    run(opt+'.provider'+str(index),[cc,*flags,*san,'-'+opt,'-DXL_ENABLE_EXE_COMPILER=1','-DTCC_DISABLE_BUILTIN_VFS',
                        '-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label','-Wno-missing-field-initializers',
                        '-c',root/name,'-o',obj]);actual.append(str(obj))
            binary=out/('native.'+opt+suffix)
            run(opt+'.native.build',[cc,*flags,*san,'-'+opt,native,*actual,*links,'-o',binary])
            data=run(opt+'.native',[binary,runtime,tcc]);match=re.search(rb'prefixes=(\d+) first-no-hit=(\d+) balanced',data);assert match
            runs,first=map(int,match.groups());assert runs==first+1 and first>0
            prefixes.append(dict(Optimization=opt,Runs=runs,FirstNoHit=first))
            buffer=re.search(rb'buffer take prefixes=(\d+) first-no-hit=(\d+) balanced',data);assert buffer
            runs,first=map(int,buffer.groups());assert runs==first+1 and first>0
            prefixes.append(dict(Optimization=opt,Lane='buffer',Runs=runs,FirstNoHit=first))
            contract=out/('ir.'+opt+suffix)
            run(opt+'.ir.build',[cc,*flags,*san,'-'+opt,ir,*actual,*links,'-o',contract])
            for fixture in fixtures[:2]:run(opt+'.ir.'+fixture.stem,[contract,fixture,fixtures[2]])
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,ContextPolicyAcceptance=True)
    except Exception as error:report['Error']=repr(error);print(repr(error),flush=True)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Context policy phases='+str(len(rows))+' complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
