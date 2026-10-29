"""Isolated current-source Thread language, exact IR/ABI and lifetime acceptance.
All compiler/API/native providers and XRT are rebuilt from source. Only genuine
TCC is reused dynamically. No shipping DLL/VFS, static pruning, OS-thread join,
image dlclose or complete 53-module acceptance is inferred from this proof.
Independent failures are collected in one batch; all inputs are hash-pinned.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,compiler,out=[getattr(args,name).resolve() for name in ('root','compiler','out')]
    assert not out.exists(); windows=os.name=='nt'; project=Path(__file__).resolve().parents[1]
    cc=shutil.which('gcc'); assert cc
    manifest=compiler/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    assert len(build['Objects'])==len(build['Sources'])
    names={Path(path).stem for path in build['Sources']}
    assert {'xl_api','xl_api_symbols','xrt_single_header_impl','xl6_ir','xl6_ir_lower',
        'xl6_ir_cgen','xl6_compile_unit','xl6_thread','xl6_array_thread_admission',
        'xl6_closure','xl6_tree','xl6_tree_admission','xl6_sequence','xl6_queue_spsc_admission',
        'xl6_queue_mpsc_admission','xl6_queue_mpmc_admission'}.issubset(names)
    fixtures=[project/'test/cases'/name for name in ('stdlib_thread_resident.xl','stdlib_thread_collections.xl')]
    oom=[project/'test/cases'/('stdlib_thread_oom_'+name+'.xl') for name in ('create','policy','stack')]
    fragments=[project/'test/cases'/name for name in ('cgen_thread.xl','cgen_thread_default.xl',
        'xlir_thread_resource_abi.xl','cgen_sync.xl')]
    regressions=[project/'test/cases'/name for name in ('stdlib_sync_resident.xl','stdlib_tree_sync_values.xl',
        'stdlib_semaphore_resident.xl','stdlib_tree_semaphore_values.xl')]
    negatives=[project/'test/cases'/('sem_thread_'+name+'.xl') for name in ('ctor_args','ctor_named_argument',
        'entry_signature','method_args','wait_timeout_type','unknown_method','receiver_expr','copy',
        'projection_copy','nullable_receiver')]
    contract=project/'test/cases/xlir_thread_domain_contract.c'
    policy_host=project/'test/cases/stdlib_thread_policy_host.c'
    host=project/'test/cases/stdlib_semaphore_language_host.c'
    oom_host=project/'test/cases/stdlib_semaphore_language_oom_host.c'
    jit_host=project/'test/cases/stdlib_thread_language_jit_host.c'
    memory=project/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(cc),manifest,Path(build['Driver']),contract,policy_host,host,oom_host,jit_host,
        memory,project/'test/support/module_product_memory.h',*fixtures,*oom,*fragments,*regressions,*negatives}
    for row in build['Inputs']+build['Objects']+build['Headers']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    files.update(path for path in (compiler/'include').rglob('*') if path.is_file())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); tmp=out/'tmp'; tmp.mkdir(); env=dict(os.environ,TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    if not windows: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    rows=[]; errors=[]; report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,
        'LanguageCgenAcceptance':False,'ShippingLibraryAcceptance':False,'Full53Acceptance':False,
        'StandaloneLinkPruningAcceptance':False,'Inputs':inputs,'Rows':rows,'Errors':errors}
    def save(): (out/'thread.language.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,success=True):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert (result.returncode==0)==success,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def batch(name,action):
        try: action()
        except Exception as error:
            errors.append({'Name':name,'Error':repr(error)}); save(); print(name+': FAILED',flush=True)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(compiler/'include')]
    # Existing generated static helpers are not all used. New native providers
    # were independently verified with no warning exclusions in thread.native.
    compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        {'xlir_public_source_driver.o','xlir_public_source_linux_driver.o'}]
    if windows:
        suffix='.exe'; shutil.copy2(root/'release/xl.dll',out/'xl.dll')
        libraries=[*objects,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:
        suffix=''; flags+=['-D_GNU_SOURCE']; native=Path(build['NativeRoot'])
        libraries=[*objects,'-L'+str(native/'release'),'-lxl','-Wl,-rpath,'+str(native/'release'),
            '-Wl,--export-dynamic','-pthread','-ldl','-lm']
    def contracts(level):
        binary=out/('contract.'+level+suffix)
        # Legacy compiler-internal header shadows the XRT `bytes` typedef.
        run('contract.'+level+'.build',[cc,*[f for f in flags if f!='-Wshadow'],*compat,
            '-'+level,contract,*libraries,'-o',binary])
        for fixture,recursive in [(fixtures[0],0),(fixtures[1],1),(oom[0],0),(oom[1],1),(oom[2],1)]:
            batch(fixture.stem+'.'+level+'.contract',lambda f=fixture,r=recursive:
                run(f.stem+'.'+level+'.contract',[binary,f,r]))
    def genuine_jit(level):
        binary=out/('jit.'+level+suffix)
        run('jit.'+level+'.build',[cc,*flags,'-'+level,jit_host,memory,*libraries,'-o',binary])
        for fixture in fixtures+oom+regressions:
            batch(fixture.stem+'.'+level+'.jit-lifetime',lambda f=fixture:
                run(f.stem+'.'+level+'.jit-lifetime',[binary,root,compiler/'include',
                    out/(f.stem+'.'+level+'.temps'),f]))
    def native_policy(level):
        binary=out/('policy.'+level+suffix)
        run('policy.'+level+'.build',[cc,*flags,'-'+level,policy_host,memory,*libraries,'-o',binary])
        assert 'lifetimes balanced' in run('policy.'+level+'.run',[binary])
    def exact_abi():
        fixture=fragments[2]; typed=out/'thread.typed.json'; lowered=out/'thread.lowered.json'
        run('thread.typed',[build['Driver'],fixture,'ir-typed',typed])
        run('thread.lowered',[build['Driver'],fixture,'ir-lowered',lowered])
        types=json.loads(typed.read_text(encoding='utf-8'))
        ops=json.loads(lowered.read_text(encoding='utf-8'))
        resources=[row for row in types['types'] if row['runtimeResourceAbiName']=='thread']
        assert len(resources)==1; resource=resources[0]
        assert resource['cAbi']==26 and resource['ownershipAbi']==2 and resource['nullAbi']==2
        assert resource['typeValueId']!='0x0000000000000000'
        counts={'thread-create':4,'thread-wait':2,'thread-wait-timeout':1,
            'thread-exit-code':2,'thread-state':1,'thread-stop':1}
        for kind,count in counts.items():
            calls=[row for row in types['intrinsicCalls'] if row['runtimeResourceOpName']==kind]
            instructions=[row for row in ops['instructions'] if row['runtimeOpName']==kind]
            assert len(calls)==count and len(instructions)==count
            for call in calls:
                assert call['kindName']=='runtime-resource' and call['admissionName']=='verified'
                operands=types['intrinsicOperands'][call['firstOperand']:call['firstOperand']+call['operandCount']]
                assert [row['abiName'] for row in operands]==(['borrow','value'] if kind=='thread-wait-timeout' else ['borrow'])
                assert call['errorAbiName']==('may-raise' if kind=='thread-create' else 'no-raise')
                assert call['resultAbiName']==('owned' if kind=='thread-create' else 'value')
            for instruction in instructions:
                assert instruction['kindName']=='runtime-call' and instruction['targetSymbol']==0 and not instruction['hasImmediate']
                assert instruction['operandCount']==(2 if kind=='thread-wait-timeout' else 1)
                assert bool(instruction['calleeType'])==(kind=='thread-create')
        captures=[row for row in ops['closureCaptures'] if row['type']==resource['id']]
        assert len(captures)==1 and captures[0]['abiName']=='retain-xvalue'
        selected=min((row for row in ops['instructions'] if row['runtimeOpName']=='thread-create'),key=lambda row:row['sourceLine'])
        conditional=[row for row in ops['instructions'] if row['sourceLine']==selected['sourceLine'] and row['type']==selected['calleeType']]
        assert sum(row['kindName']=='retain' for row in conditional)==2
        assert sum(row['kindName']=='move' for row in conditional)==1
        generated=out/'thread.abi.c'
        run('thread.abi-c',[build['Driver'],fixture,'cgen-default',generated])
        text=generated.read_text(encoding='utf-8')
        helpers=('xl6ThreadCreateV1','xl6ThreadWaitV1','xl6ThreadWaitTimeoutV1',
            'xl6ThreadExitCodeV1','xl6ThreadStateV1','xl6ThreadStopV1')
        for helper,count in zip(helpers,counts.values()):
            assert len(re.findall(r'v\d+ = '+helper+r'\(',text))==count
        assert text.count(resource['typeValueId']+'ULL')==sum(counts.values())
        assert 'xlir_thread_' not in text and 'xrtThreadCreate(' not in text
    def language(fixture):
        name=fixture.stem; generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
        for label,target in (('emit',generated),('repeat',repeat)):
            run(name+'.'+label,[build['Driver'],fixture,'cgen-default',target])
        assert generated.read_bytes()==repeat.read_bytes()
        text=generated.read_text(encoding='utf-8')
        assert 'xlir_thread_' not in text and 'xrtThreadCreate(' not in text
        constructors=re.findall(r'(v\d+) = (?:xl6ThreadCreateV1|xl6SequenceThreadCreateV1)\([^;]*\);\s*([^\n]+)',text)
        if fixture in fixtures+oom: assert constructors
        for value,test in constructors: assert re.match(r'if \('+value+r' != NULL\)',test),(name,value,test)
        if fixture in (fixtures[1],oom[1],oom[2]): assert 'xl6SequenceThreadCreateV1(' in text
        batch(name+'.jit',lambda: run(name+'.jit',[build['Driver'],fixture,'run',out/(name+'.jit')]))
        for level in ('O0','O2'):
            def aot():
                binary=out/(name+'.'+level+suffix)
                extra=['-DXLIR_SCRIPT_FRAGMENT'] if fixture in fragments else []
                selected=oom_host if fixture in oom else host
                run(name+'.'+level+'.build',[cc,*flags,*compat,'-'+level,*extra,
                    '-DXLIR_GENERATED_MODULE="'+generated.as_posix()+'"',selected,memory,*libraries,'-o',binary])
                assert 'lifetimes balanced' in run(name+'.'+level+'.run',[binary])
            batch(name+'.'+level+'.aot',aot)
    def negative(fixture):
        target=out/(fixture.stem+'.guard.c'); target.write_bytes(b'KEEP')
        run(fixture.stem+'.refuse',[build['Driver'],fixture,'cgen-default',target],False)
        assert target.read_bytes()==b'KEEP'
    try:
        for level in ('O0','O2'):
            batch('contracts.'+level,lambda l=level: contracts(l))
            batch('genuine-jit.'+level,lambda l=level: genuine_jit(l))
            batch('native-policy.'+level,lambda l=level: native_policy(l))
        batch('exact-abi',exact_abi)
        for fixture in fixtures+oom+fragments+regressions: batch(fixture.stem,lambda f=fixture: language(f))
        for fixture in negatives: batch(fixture.stem,lambda f=fixture: negative(f))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; assert not errors,errors
        report['Complete']=True; report['LanguageCgenAcceptance']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Thread language: '+str(len(rows))+' phases; actual JIT/AOT lifetimes balanced',flush=True)
if __name__=='__main__': main()
