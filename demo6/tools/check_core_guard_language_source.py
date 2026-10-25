"""Original core fixtures through one real public compilation per process.

Current complete source cohort only. No compiler/runtime object replacement,
fixture rewriting, private entry hooks or public-network/security tests.
Native assertion helpers are an ordinary no-XRT source package, also linked
into the independent AOT executable; no host pointer is embedded in AOT.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,threading,time

CASES=('cgen_container_ops','stdlib_array_algebra_language','stdlib_array_dynamic_algebra_language',
       'stdlib_array_clone_language','cgen_typed_container_safe_index',
       'cgen_typed_container_safe_index_objects_functions','xlir_safe_index_gate',
       'cgen_copy_clone','cgen_class_custom_copy_clone','cgen_generic_class_copy_clone',
       'stdlib_class_identity_checked','stdlib_class_identity_inherited',
       'cgen_async_string_ownership','cgen_async_exception_propagation','xlir_task_spawn_closure_abi',
       'cgen_channel','stdlib_channel_receive_ir','stdlib_channel_cases_receive_only',
       'stdlib_wait_queue_owned','stdlib_wait_queue_values','stdlib_tree_query_selection',
       'cgen_sync','stdlib_sync_resident','stdlib_tree_sync_values','stdlib_template_core_only',
       'stdlib_template_values','stdlib_codec_values','stdlib_nullable_projection_char',
       'stdlib_nullable_projection_f32','stdlib_nullable_projection_u64')
NEGATIVES=('bad_xlir_template_raw_context','bad_xlir_channel_nullable_receiver',
           'bad_generic_channel_unsupported','sem_wait_queue_capacity_required')

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','runtime','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=2)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists() and args.jobs>0
    manifest=args.public_directory.resolve()/'build.json'
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly']
    windows=os.name=='nt';runtime=args.runtime.resolve();cc=Path(shutil.which('gcc')).resolve()
    driver_name='xlir_public_source_driver.c' if windows else 'xlir_public_source_linux_driver.c'
    drivers=[Path(row['Path']) for row in build['Inputs'] if Path(row['Path']).name==driver_name]
    assert len(drivers)==1;compiled_root=drivers[0].parents[2]
    inputs={str(manifest):sha(manifest),str(Path(__file__).resolve()):sha(__file__),
            str(Path(sys.executable).resolve()):sha(sys.executable),str(cc):sha(cc)}
    production=[]
    for row in build['Inputs']+build['Headers']+build['Objects']:
        p=Path(row['Path']).resolve();assert sha(p)==row['Sha256'].lower(),p
        inputs[str(p)]=sha(p)
        if p.is_relative_to(compiled_root):
            relative=p.relative_to(compiled_root)
            if relative.parts[0] in ('src','lib','res','include') or relative.parts[:2]==('release','include'):
                current=root/relative
                assert p.read_bytes().replace(b'\r\n',b'\n')==current.read_bytes().replace(b'\r\n',b'\n'),current
                inputs[str(current)]=sha(current);production.append(str(relative))
    assert inputs.get(str(runtime))==sha(runtime),'Use the cohort runtime dependency'
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
             ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    sdk=Path(build['Driver']).parent/'include'
    host_source=root/'test/cases/xlir_core_guard_source_host.c'
    support=root/'test/fixtures/core_guard_assertions_support.xl'
    files=[host_source,support,*[root/'test/cases'/(name+'.xl') for name in CASES+NEGATIVES],
           *sorted((root/'test/fixtures/core_guard_assertions').rglob('*'))]
    for p in files:
        if p.is_file():inputs[str(p)]=sha(p)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
           '-I'+str(root/'release/include'),'-I'+str(sdk)]
    if not windows:flags.append('-D_GNU_SOURCE')
    links=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else
           ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    env=dict(os.environ)
    out.mkdir(parents=True);(root/'build/test').mkdir(parents=True,exist_ok=True)
    # Source-cohort executables resolve private native source assets beside
    # their own executable, just like the canonical public source driver.
    # This is the actual cohort SDK, not a replacement implementation.
    shutil.copytree(sdk,out/'include')
    for p in sorted((out/'include').rglob('*')):
        if p.is_file():
            original=sdk/p.relative_to(out/'include');assert sha(p)==sha(original)
            inputs[str(p)]=sha(p)
    if windows:shutil.copy2(runtime,out/runtime.name)
    library=root/'res/tcc/win64/x86_64' if windows else Path(build['NativeRoot'])/'build/linux/tcc-dist'
    lock=threading.RLock();rows=[];products=[];proofs=[];failures=[]
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,Proofs=proofs,Failures=failures,
                ProductionFiles=len(production),SourceInputsMatch=True,OriginalFixtures=True,
                SourceObjectOverrides=False,CompileOncePerFixture=True,ShippingAccepted=False,FullGoalComplete=False,
                Scope=__doc__)
    def save():
        with lock:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):
        with lock:products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command):
        command=list(map(str,command));actual=command
        if command[0]==str(cc):
            response=out/(name+'.rsp')
            response.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n',encoding='utf8')
            product(response);actual=[str(cc),'@'+str(response)]
        log=out/(name+'.log');started=time.monotonic()
        try:
            with log.open('wb') as stream:
                code=subprocess.run(actual,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=600).returncode
        except subprocess.TimeoutExpired:code='timeout'
        with lock:
            rows.append(dict(Name=name,Command=command,ActualCommand=actual,Exit=code,Log=str(log),
                             Sha256=sha(log),Seconds=time.monotonic()-started));save()
        print(name,code,flush=True);assert code==0,(name,str(log))
        return log.read_text(encoding='utf-8',errors='replace')
    save();host=out/('core-host.exe' if windows else 'core-host')
    try:
        run('host-build',[cc,*flags,'-O2',host_source,*objects,*links,'-o',host]);product(host)
    except Exception as error:failures.append(dict(Name='host-build',Error=repr(error)));save();return 1
    def check(name,negative):
        try:
            directory=out/name;directory.mkdir();source=root/'test/cases'/(name+'.xl')
            helpers=str(support) if re.search(r'extern\s+"expect_(?:i64|str)"',source.read_text(encoding='utf-8-sig')) else '-'
            text=run(name+'-host',[host,root,source,directory,sdk,runtime,library,helpers,'negative' if negative else 'positive'])
            if negative:
                assert 'core-host: checked-rejection' in text
                assert not (directory/'rejected.c').exists() and not (directory/'rejected.h').exists()
            else:
                for marker in ('load','compile','typed','lowered','cgen','cgen-repeat','header','aot-build','jit','module','checked-retirement'):
                    assert text.count('core-host: '+marker+'\n')==1,(name,marker)
                generated=directory/'module.c'
                assert generated.read_bytes()==(directory/'module.repeat.c').read_bytes()
                assert not any(p in generated.read_text(encoding='utf-8-sig') for p in
                    ('xlir_network_services','xlir_special_queue_create','xlir_sync_handle_ops','xlir_avl_handle_ops'))
                unused=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
                        '-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers']
                for opt in ('O0','O2'):
                    run(name+'-'+opt+'-strict',[cc,*flags,*unused,'-'+opt,'-fsyntax-only',generated])
                run(name+'-aot',[directory/'program.exe'])
            for p in sorted(directory.rglob('*')):
                if p.is_file():product(p)
            with lock:proofs.append(dict(Name=name,Negative=negative,NativeAssertionPackage=helpers!='-'));save()
        except Exception as error:
            with lock:failures.append(dict(Name=name,Error=repr(error)));save()
    with ThreadPoolExecutor(max_workers=args.jobs) as workers:
        list(workers.map(lambda item:check(*item),[(n,False) for n in CASES]+[(n,True) for n in NEGATIVES]))
    report['InputsUnchanged']=all(sha(p)==h for p,h in inputs.items())
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
    report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
    report['ExpectedStages']=1+len(CASES)*4+len(NEGATIVES)
    report['Complete']=not failures and report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged'] and len(rows)==report['ExpectedStages']
    save();print(json.dumps(dict(Complete=report['Complete'],Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
