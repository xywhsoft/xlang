"""Generated source AOT entry, exact runtime closure and actual graph retirement.

Uses an explicitly staged physical private SDK, not shipping embedded VFS.
The original graph fixture bodies and destructor field assertions are unchanged.
The sidecar observes finalizers only; it cannot collect or substitute providers.
"""
from pathlib import Path
import argparse,json,os,re,shutil,subprocess,sys,time
from check_stdlib_slot_map import stage_private_sdk
from xlir_public_build_inputs import sha,verify_public_build

CASES=(('control','stdlib_aot_graph_control',1),('stack','stdlib_stack_ownership',1),
       ('tree','stdlib_tree_ownership',5),('queue','stdlib_concurrent_queue_ownership',6),
       ('direct','cgen_basic',0),('restore-clone','stdlib_graph_clone_restore',0),
       ('restore-construction','stdlib_persistence_restore_construction',0),
       ('restore-collections','stdlib_restore_module_collections',0),
       ('entry-error','xlir_aot_entry_error',1),('entry-return','xlir_aot_entry_return',1),
       ('finalizer-error','xlir_aot_entry_finalizer_error',1))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,public,out=args.root.resolve(),args.public_directory.resolve(),args.out.resolve()
    assert not out.exists();build,pinned,compared=verify_public_build(root,public)
    manifest,sdk,skipped=stage_private_sdk(root);pinned.add(manifest)
    for row in sdk:pinned.update(Path(row[key]) for key in ('Source','Path'))
    windows=os.name=='nt';inspector=shutil.which('objdump' if windows else 'readelf');cc=shutil.which('gcc');assert inspector and cc
    probe=root/'test/support/aot_graph_probe.c'
    task=Path(__file__).resolve().parents[1]
    contract=task/'test/support/static_entry_contract.c'
    fixtures={name:(task if name in ('entry-error','entry-return','finalizer-error') else root)/'test/cases'/(stem+'.xl')
        for name,stem,_ in CASES}
    native=root/'release/xl.dll' if windows else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(sys.executable).resolve(),Path(inspector).resolve(),Path(cc).resolve(),probe,contract,native))
    pinned.update(fixtures.values())
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);relocated=out/'relocated';relocated.mkdir();rows=[];cases=[];failures=[]
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    if not windows:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Cases=cases,Failures=failures,
        ComparedSources=compared,PhysicalPrivateSourceSdk=sdk,UnstagedCompilerImportResources=skipped,
        Scope=__doc__,GeneratedEntryAcceptance=False,Full53Acceptance=False,ShippingLibraryAcceptance=False,
        ProviderOverrides=False,RuntimeInstrumented=False)
    def run(name,command,directory=root):
        start=time.monotonic();command=list(map(str,command))
        try:
            process=subprocess.run(command,cwd=directory,env=env,capture_output=True,timeout=180)
            data,code=process.stdout+process.stderr,process.returncode
        except subprocess.TimeoutExpired as error:data,code=(error.stdout or b'')+(error.stderr or b''),'timeout'
        log=out/(name+'.log');log.write_bytes(data)
        row=dict(Name=name,Command=command,Exit=code,ExpectedExit=0,Directory=str(directory),
            Seconds=time.monotonic()-start,Log=str(log),LogSha256=sha(log));rows.append(row)
        return row,data
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(public/'include')]
    if not windows:flags+=['-D_GNU_SOURCE']
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in
        ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm']
    links=[native,*systems,*([] if windows else ['-Wl,-rpath,'+str(native.parent)])]
    if windows:shutil.copy2(native,out/'xl.dll')
    try:
        for opt in ('O0','O2'):
            binary=out/('contract.'+opt+('.exe' if windows else ''))
            row,data=run(opt+'.contract.build',[cc,*flags,'-'+opt,contract,*objects,*links,'-o',binary]);assert row['Exit']==0,data[-5000:]
            for lane,stem in (('direct','cgen_basic'),('graph','stdlib_stack_ownership')):
                row,data=run(opt+'.contract.'+lane,[binary,root/'test/cases'/(stem+'.xl'),lane]);assert row['Exit']==0,data[-5000:]
                assert b'before output' in data
        report['DeploymentContractAcceptance']=True
    except Exception as error:failures.append(dict(Name='deployment-contract',Error=repr(error)))
    prefix=b'extern "graph_probe_begin" func beginGraphProbe() as bool\nassert(beginGraphProbe())\n'
    for name,stem,count in CASES:
        original=fixtures[name];body=original.read_bytes();directory=out/name;directory.mkdir()
        prefixed=name in ('stack','tree','queue')
        source=directory/(name+'.xl');content=prefix+body if prefixed else body;source.write_bytes(content)
        assert content[len(prefix):]==body if prefixed else content==body
        extra=[]
        if count:
            support=directory/'probe_support.c';support.write_bytes(('#define GRAPH_EXPECTED '+str(count)+'\n'+
                ('#define GRAPH_CONTROL\n' if name=='control' else '')).encode()+probe.read_bytes());extra=[support]
        base=directory/name;image=Path(str(base)+'.exe') if windows else base
        record=dict(Name=name,OriginalFixture=str(original),FixtureSha256=sha(original),ExpectedFinalizers=count,
            Passed=False,IndependentAot=False,Rows=[]);begin=len(rows)
        try:
            row,data=run(name+'.build',[build['Driver'],source,'exe',base,*extra]);assert row['Exit']==0,data[-5000:]
            generated=list(Path(str(base)+'.temps').rglob('module.c'));assert len(generated)==1,generated
            code=generated[0].read_text(encoding='utf-8-sig')
            if name!='direct':
                assert 'xl6StaticModuleCreateV1(&deployment)' in code and 'xl6StaticModuleDrainV1(xlir_entry_module)' in code
                assert code.index('!xl_script_bind_code_owner(')<code.index('ok = xl_script_main_checked(arg, &code)')
                assert 'xl6OwnershipPlanBuildV1(' not in code
            else:
                assert 'xl6StaticModule' not in code and 'xl6OwnershipCatalog' not in code
            record.update(Generated=str(generated[0]),GeneratedSha256=sha(generated[0]))
            row,data=run(name+'.strict-c',[cc,*flags,'-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
                '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
                '-Wno-pointer-sign','-fsyntax-only',generated[0]]);assert row['Exit']==0,data[-5000:]
            independent=relocated/(name+('.exe' if windows else '.aot'));shutil.copy2(image,independent)
            row,data=run(name+'.imports',[inspector,'-p' if windows else '-d',independent],relocated);assert row['Exit']==0
            deps=re.findall(rb'DLL Name:\s*([^\s]+)',data) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]',data)
            assert deps and not {item.decode().lower() for item in deps}&{'xl.dll','xlc.dll','libxl.so','libxlc.so'}
            if not windows:assert b'(RPATH)' not in data and b'(RUNPATH)' not in data
            record.update(IndependentAot=True,Dependencies=[item.decode() for item in deps])
            expectedExit=23 if name=='entry-return' else 1 if name in ('entry-error','finalizer-error') else 0
            row,data=run(name+'.run',[independent],relocated);row['ExpectedExit']=expectedExit
            assert row['Exit']==expectedExit,data[-5000:]
            if name in ('entry-error','finalizer-error'):
                sentinel=b'aot-entry-error-sentinel' if name=='entry-error' else b'aot-entry-finalizer-sentinel'
                assert sentinel in data
            if count:
                match=re.search(rb'standalone graph finalizers=(\d+) expected=(\d+) mask=(\d+)',data);assert match
                actual,expected,mask=map(int,match.groups());record.update(ActualFinalizers=actual,ActualMask=mask)
                assert actual==expected==count and mask==(1<<count)-1
            record['Passed']=True
        except Exception as error:failures.append(dict(Name=name,Error=repr(error)))
        record['Rows']=rows[begin:];cases.append(record);print(name+': '+str(record['Passed']),flush=True)
    report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
    report['Complete']=report['InputsUnchanged'] and not failures and len(cases)==len(CASES) and all(case['Passed'] for case in cases)
    report['GeneratedEntryAcceptance']=report['Complete']
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
