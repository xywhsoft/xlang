"""Fresh Thread products: actual embedded-VFS JIT, CLI IR/C and independent
artifacts. Both resident products are used without source/header/TCC fallbacks.
Standalone images are retained until process exit; wait is not OS-thread join.
ELF full symbol-table pruning is limited to three controlled Thread fixtures;
PE exports alone are explicitly not treated as full static-provider pruning.
Independent failures are batched. This is not full53 or release promotion.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve()
    manifest=args.build_report.resolve(); assert not out.exists(); windows=os.name=='nt'
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    project=Path(__file__).resolve().parents[1]
    cc,objdump=shutil.which('gcc'),shutil.which('objdump'); assert cc and objdump
    readelf=None if windows else shutil.which('readelf'); assert windows or readelf
    products=[root/'release'/name for name in (('xl.dll','xlc.dll','xl.exe') if windows else ('libxl.so','libxlc.so','xl'))]
    jit=project/'test/cases/stdlib_thread_shipping_jit_host.c'
    consumer_source=project/'test/cases/stdlib_thread_artifact_host.c'
    memory=project/'test/support/module_product_memory.c'
    strict_host=project/'test/cases/stdlib_semaphore_language_host.c'
    fixtures=[project/'test/cases'/name for name in ('stdlib_thread_resident.xl','stdlib_thread_collections.xl',
        'stdlib_thread_oom_create.xl','stdlib_thread_oom_policy.xl','stdlib_thread_oom_stack.xl',
        'stdlib_sync_resident.xl','stdlib_tree_sync_values.xl','stdlib_semaphore_resident.xl','stdlib_tree_semaphore_values.xl')]
    artifacts=[project/'test/cases'/('stdlib_thread_artifact_'+name+'.xl') for name in ('resident','policy','absent')]
    abi=project/'test/cases/xlir_thread_resource_abi.xl'
    files={Path(__file__).resolve(),Path(cc),Path(objdump),manifest,jit,consumer_source,memory,strict_host,
        project/'test/support/module_product_memory.h',abi,*fixtures,*artifacts}
    if readelf: files.add(Path(readelf))
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded[path]==sha(path) for path in products)
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256']; files.add(Path(row['Path']))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); isolated=out/'compiler'; (isolated/'release').mkdir(parents=True); (isolated/'inputs').mkdir()
    consumer=out/'consumer'; consumer.mkdir(); tmp=out/'tmp'; tmp.mkdir()
    for path in products: shutil.copy2(path,isolated/'release'/path.name)
    for path in fixtures+artifacts+[abi]: shutil.copy2(path,isolated/'inputs'/path.name)
    assert not any((isolated/name).exists() for name in ('src','lib','res','include'))
    env={key:value for key,value in os.environ.items() if not key.upper().startswith('XL6_')
        and key not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[]; errors=[]; outputs=[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Inputs':inputs,'Rows':rows,
        'Errors':errors,'Artifacts':outputs,'ExternalXlangHeadersUsedForJit':False,
        'ExternalTccDistributionUsed':False,'ExternalXlangSourcesUsedForCliArtifacts':False,
        'TargetSystemLibcHeadersCrtAndLinkerRequired':not windows,'ThreadShippingApiVfsAcceptance':False,
        'ThreadCliIrCgenAcceptance':False,'IndependentThreadArtifactAcceptance':False,
        'ThreadElfProviderPruningAcceptance':False,'PublicPeArtifactExportsAcceptance':False,
        'SafeImageUnloadAcceptance':False,'OsThreadJoinAcceptance':False,
        'ShippingLibraryAcceptance':False,'Full53Acceptance':False}
    def save(): (out/'thread.shipping.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,cwd=isolated):
        command=list(map(str,command)); result=subprocess.run(command,cwd=cwd,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert not result.returncode,(name,data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def batch(name,action):
        try: action()
        except Exception as error: errors.append({'Name':name,'Error':repr(error)}); save(); print(name+': FAILED',flush=True)
    def no_imports(text):
        needed=re.findall(r'DLL Name:\s*(\S+)',text) if windows else re.findall(r'\(NEEDED\).*?\[([^\]]+)\]',text)
        assert not any(re.match(r'(?:lib)?xlc?\.(?:so|dll)',name,re.I) for name in needed),needed
        return needed
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(project)]
    if not windows: flags+=['-D_GNU_SOURCE']
    suffix='.exe' if windows else ''
    def libraries(product):
        if windows:
            shutil.copy2(product,isolated/product.name)
            return [isolated/'release'/product.name]
        return ['-L'+str(isolated/'release'),'-l'+product.stem[3:],'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
    def genuine_jit(product,level):
        name=product.stem+'.'+level; binary=isolated/(name+suffix)
        run(name+'.build',[cc,*flags,'-'+level,'-DTHREAD_SHIPPING_PRODUCT="'+product.name+'"',
            jit,memory,*libraries(product),'-o',binary])
        for fixture in fixtures:
            def check(f=fixture):
                text=run(name+'.'+f.stem,[binary,isolated,out/(name+'.'+f.stem+'.temps'),isolated/'inputs'/f.name])
                assert 'embedded VFS, source image retired and lifetimes balanced' in text
            batch(name+'.'+fixture.stem,check)
    cli=isolated/'release'/products[2].name
    def exact_abi():
        typed=out/'thread.typed.json'; lowered=out/'thread.lowered.json'
        source=isolated/'inputs'/abi.name
        run('abi.typed',[cli,'ir',source,'--format','json','--emit',typed])
        run('abi.lowered',[cli,'ir',source,'--lowered','--format','json','--emit',lowered])
        types=json.loads(typed.read_text(encoding='utf-8')); ops=json.loads(lowered.read_text(encoding='utf-8'))
        resources=[row for row in types['types'] if row['runtimeResourceAbiName']=='thread']; assert len(resources)==1
        resource=resources[0]; assert resource['cAbi']==26 and resource['ownershipAbi']==2 and resource['nullAbi']==2
        assert resource['typeValueId']!='0x0000000000000000'
        counts={'thread-create':4,'thread-wait':2,'thread-wait-timeout':1,'thread-exit-code':2,'thread-state':1,'thread-stop':1}
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
                assert instruction['kindName']=='runtime-call' and not instruction['targetSymbol'] and not instruction['hasImmediate']
                assert instruction['operandCount']==(2 if kind=='thread-wait-timeout' else 1)
                assert bool(instruction['calleeType'])==(kind=='thread-create')
        captures=[row for row in ops['closureCaptures'] if row['type']==resource['id']]
        assert len(captures)==1 and captures[0]['abiName']=='retain-xvalue'
        generated=out/'thread.abi.c'; repeat=out/'thread.abi.repeat.c'
        for label,target in [('emit',generated),('repeat',repeat)]: run('abi.'+label,[cli,'cgen',source,'--emit',target])
        assert generated.read_bytes()==repeat.read_bytes(); text=generated.read_text(encoding='utf-8')
        helpers=('xl6ThreadCreateV1','xl6ThreadWaitV1','xl6ThreadWaitTimeoutV1','xl6ThreadExitCodeV1','xl6ThreadStateV1','xl6ThreadStopV1')
        for helper,count in zip(helpers,counts.values()): assert len(re.findall(r'v\d+ = '+helper+r'\(',text))==count
        assert text.count(resource['typeValueId']+'ULL')==sum(counts.values())
        assert 'xlir_thread_' not in text and 'xrtThreadCreate(' not in text and 'xl6ClosureRuntimeV2Get' in text
        # Legacy generated unused helpers get the existing narrow exemptions;
        # the new shipping JIT and independent native hosts have no exemptions.
        compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
            '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
        headers=out/'headers'; (headers/'__xl_runtime').mkdir(parents=True)
        for path in list((root/'release/include').glob('*.h'))+list((root/'src/runtime').glob('xl6_*.h')):
            shutil.copy2(path,headers/'__xl_runtime'/path.name)
        for level in ('O0','O2'):
            binary=isolated/('abi.'+level+suffix)
            run('abi.'+level+'.build',[cc,*flags,*compat,'-'+level,'-I'+str(headers),'-DXLIR_SCRIPT_FRAGMENT',
                '-DXLIR_GENERATED_MODULE="'+generated.as_posix()+'"',strict_host,memory,*libraries(products[0]),'-o',binary])
            assert 'lifetimes balanced' in run('abi.'+level+'.run',[binary])
        report['ThreadCliIrCgenAcceptance']=True
    def independent():
        hosts=[]
        for level in ('O0','O2'):
            binary=consumer/('host.'+level+suffix)
            run('consumer.'+level+'.build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-'+level,
                consumer_source,*([] if windows else ['-ldl']),'-o',binary],consumer)
            text=run('consumer.'+level+'.imports',[objdump,'-p',binary] if windows else [readelf,'-dW',binary],consumer)
            no_imports(text); hosts.append(binary)
        def artifact(fixture):
            name=fixture.stem; generated=isolated/(name+('.dll' if windows else '.so'))
            text=run(name+'.compile',[cli,'build',isolated/'inputs'/fixture.name,'-shared','-o',generated,'--save-temps'])
            paths=re.findall(r'temporary files kept: (.+)',text); assert len(paths)==1,text
            temp=Path(paths[0].strip()).resolve(); assert temp.is_relative_to(out)
            implementation=temp/'xl6_xrt_impl.c'; assert implementation.is_file()
            selected=implementation.read_text(encoding='utf-8')
            uses_thread=fixture.stem!='stdlib_thread_artifact_absent'; policy=fixture.stem=='stdlib_thread_artifact_policy'
            if uses_thread: assert '#define XRT_MODULE_THREAD' in selected
            assert 'XRT_MODULE_ALL' not in selected and 'XRUNTIME_MODULE_ALL' not in selected
            module=consumer/generated.name; shutil.copy2(generated,module)
            record={'Path':str(module),'Sha256':sha(module),'Bytes':module.stat().st_size,
                'SelectedXrtSource':str(implementation),'SelectedXrtSha256':sha(implementation)}; outputs.append(record)
            # CODE_LEASE itself legitimately needs XRT thread even in the
            # no-Thread control. This proves language providers, not pruning
            # the kernel required by common runtime/module lifecycle.
            record['XrtThreadKernelSelected']='#define XRT_MODULE_THREAD' in selected
            dependencies=run(name+'.imports',[objdump,'-p',module] if windows else [readelf,'-dW',module],consumer)
            record['Needed']=no_imports(dependencies)
            # Full ELF symtab, not just dynamic exports. PE has no equivalent
            # guaranteed table in these TCC products: expose only export proof.
            symbols=run(name+'.symbols',[objdump,'-p',module] if windows else [readelf,'-sW',module],consumer)
            if not windows:
                segments=run(name+'.segments',[readelf,'-lW',module],consumer)
                stack=[line for line in segments.splitlines() if 'GNU_STACK' in line]
                assert len(stack)==1 and stack[0].split()[-2]=='RW'; record['StackFlags']='RW'
                defined={line.split()[-1] for line in symbols.splitlines() if re.match(r'\s*\d+:',line)
                    and len(line.split())>=8 and line.split()[6]!='UND'}
            else:
                # New GNU objdump includes +base/ordinal/hint columns. The
                # name is the last column; ordinal columns are not symbols.
                defined={line.split()[-1] for line in symbols.splitlines() if re.match(r'\s*\[\s*\d+\]',line)}
            if not windows:
                for symbol,expected in [('xl6ThreadCreateV1',uses_thread),('xl6SequenceThreadCreateV1',policy)]:
                    assert (symbol in defined)==expected,(name,symbol,expected)
            assert {'xl_script_module_init','xl_script_module_unit_checked','xl6_export_threadProbe'}.issubset(defined)
            for binary in hosts:
                result=run(name+'.'+binary.stem,[binary,module],consumer)
                assert 'checked cleanup passed; image retained until process exit' in result
        for fixture in artifacts: batch(fixture.stem,lambda f=fixture: artifact(f))
        assert not any(path.name in {p.name for p in products} for path in consumer.iterdir())
    try:
        for product in products[:2]:
            for level in ('O0','O2'): batch(product.stem+'.'+level,lambda p=product,l=level: genuine_jit(p,l))
        batch('exact-abi',exact_abi); batch('independent',independent)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; assert not errors,errors
        report['Complete']=True; report['ThreadShippingApiVfsAcceptance']=True
        report['IndependentThreadArtifactAcceptance']=True
        report['ThreadElfProviderPruningAcceptance']=not windows
        report['PublicPeArtifactExportsAcceptance']=windows
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Thread shipping: '+str(len(rows))+' phases; real products, embedded VFS and independent artifacts passed',flush=True)
if __name__=='__main__': main()
