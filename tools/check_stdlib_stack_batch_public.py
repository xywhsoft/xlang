"""Actual Stack batch IR/C/JIT/AOT/products, independent ownership and pruning.
No compiler/provider overrides or source/header fallback for embedded-VFS JIT.
PE export proof is not full static pruning. Not full53 or release promotion.
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
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); assert cc
    inspect=shutil.which('objdump' if windows else 'readelf'); assert inspect
    products=[root/'release'/name for name in (('xl.dll','xlc.dll','xl.exe') if windows else ('libxl.so','libxlc.so','xl'))]
    jit=project/'test/cases/stdlib_thread_shipping_jit_host.c'
    consumer_source=project/'test/cases/stdlib_stack_batch_artifact_host.c'
    memory=project/'test/support/module_product_memory.c'
    strict_host=project/'test/cases/stdlib_semaphore_language_host.c'
    fixtures=[project/'test/cases'/('stdlib_stack_batch_'+name+'.xl') for name in ('public','domains','resources')]
    artifacts=[project/'test/cases'/('stdlib_stack_batch_artifact'+suffix+'.xl') for suffix in ('','_plain','_absent')]
    negatives=[project/'test/fixtures/native_stack_batch_rejected'/name for name in
        ('wrong_element.xl','wrong_array_owner.xl','wrong_depth.xl','legacy_into.xl')]
    files={Path(__file__).resolve(),Path(cc),Path(inspect),manifest,jit,consumer_source,memory,strict_host,
        project/'test/support/module_product_memory.h',*fixtures,*artifacts,*negatives}
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded[path]==sha(path) for path in products)
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256']; files.add(Path(row['Path']))
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); isolated=out/'compiler'; (isolated/'release').mkdir(parents=True); (isolated/'inputs').mkdir()
    consumer=out/'consumer'; consumer.mkdir(); tmp=out/'tmp'; tmp.mkdir()
    headers=out/'headers'; (headers/'__xl_runtime').mkdir(parents=True)
    for path in products: shutil.copy2(path,isolated/'release'/path.name)
    for path in fixtures+artifacts+negatives: shutil.copy2(path,isolated/'inputs'/path.name)
    for path in list((root/'release/include').glob('*.h'))+list((root/'src/runtime').glob('xl6_*.h')):
        shutil.copy2(path,headers/'__xl_runtime'/path.name)
    assert not any((isolated/name).exists() for name in ('src','lib','res','include'))
    env={key:value for key,value in os.environ.items() if not key.upper().startswith('XL6_')
        and key not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[]; errors=[]; proofs=[]; outputs=[]
    report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,'Inputs':inputs,
        'Rows':rows,'Errors':errors,'Proofs':proofs,'Outputs':outputs,'Full53Acceptance':False}
    def save(): (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,cwd=isolated,rejection=None):
        command=list(map(str,command)); result=subprocess.run(command,cwd=cwd,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        text=data.decode(errors='replace')
        if rejection:
            assert result.returncode and rejection in text,(name,text[-6000:])
        else: assert not result.returncode,(name,text[-6000:])
        return text
    def batch(name,action):
        try: action()
        except Exception as error: errors.append({'Name':name,'Error':repr(error)}); save(); print(name+': FAILED',flush=True)
    suffix='.exe' if windows else ''
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release/include'),'-I'+str(project)]
    if not windows: flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    def libraries(product):
        if windows:
            shutil.copy2(product,isolated/product.name); return [isolated/'release'/product.name]
        return ['-L'+str(isolated/'release'),'-l'+product.stem[3:],'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
    cli=isolated/'release'/products[2].name
    def public(fixture):
        name=fixture.stem; source=isolated/'inputs'/fixture.name
        typed=out/(name+'.typed.json'); lowered=out/(name+'.lowered.json')
        generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
        run(name+'.typed',[cli,'ir',source,'--format','json','--emit',typed])
        run(name+'.lowered',[cli,'ir',source,'--lowered','--format','json','--emit',lowered])
        for label,target in [('emit',generated),('repeat',repeat)]: run(name+'.'+label,[cli,'cgen',source,'--emit',target])
        assert generated.read_bytes()==repeat.read_bytes()
        ir=json.loads(lowered.read_text(encoding='utf-8')); typed_ir=json.loads(typed.read_text(encoding='utf-8'))
        calls=[row for row in typed_ir['intrinsicCalls'] if row['runtimeResourceOpName'] in
            ('stack-push-batch','stack-pop-batch','stack-peek-batch','stack-peek-batch-at')]
        assert calls
        for call in calls:
            assert call['admissionName'] in ('verified','generic-dependent') and call['errorAbiName']=='may-raise'
            push=call['runtimeResourceOpName']=='stack-push-batch'
            assert call['resultAbiName']==('value' if push else 'owned')
            args=typed_ir['intrinsicOperands'][call['firstOperand']:call['firstOperand']+call['operandCount']]
            assert [arg['abiName'] for arg in args]==(['borrow','borrow'] if push else
                ['borrow','value','value'] if call['runtimeResourceOpName']=='stack-peek-batch-at' else ['borrow','value'])
        instructions=ir['instructions']; definitions={row['result']:row for row in instructions if row['result']}
        types={row['id']:row for row in ir['types']}; selected=[]
        for index,row in enumerate(instructions):
            if row['runtimeOpName'] not in ('stack-push-batch','stack-pop-batch','stack-peek-batch','stack-peek-batch-at'): continue
            assert row['kindName']=='runtime-call' and not row['targetSymbol'] and not row['hasImmediate']
            owner=types[definitions[row['operands'][0]]['type']]
            push=row['runtimeOpName']=='stack-push-batch'
            array=types[definitions[row['operands'][1]]['type'] if push else row['type']]
            assert owner['params']==array['params'] and owner['typeValueId']!=array['typeValueId']
            assert row['payloadType']==owner['params'][0] and not row['sequenceAdmissionIdentitySet']
            assert instructions[index+1]['kindName']=='error-test' and instructions[index+1]['operands']==[row['result']]
            selected.append(row['runtimeOpName'])
        assert selected
        text=generated.read_text(encoding='utf-8')
        assert 'xlir_stack_' not in text and 'xrtTypedStackPushBatch(' not in text and 'xrtTypedArrayTakeTail(' not in text
        assert len(re.findall(r'v\d+ = xl6StackPushBatchV1\(',text))==selected.count('stack-push-batch')
        assert len(re.findall(r'v\d+ = xl6StackExtractArrayV1\(',text))==len(selected)-selected.count('stack-push-batch')
        for level in ('O0','O2'):
            binary=isolated/(name+'.'+level+suffix)
            run(name+'.'+level+'.build',[cc,*flags,*compat,'-'+level,'-I'+str(headers),
                '-DXLIR_GENERATED_MODULE="'+generated.as_posix()+'"',strict_host,memory,*libraries(products[0]),'-o',binary])
            assert 'lifetimes balanced' in run(name+'.'+level+'.run',[binary])
        binary=out/(name+'.aot'+suffix)
        run(name+'.aot.build',[cli,'build',source,'-o',binary]); run(name+'.aot.run',[binary])
        proofs.append({'Case':name,'CheckedBatchCalls':len(selected),'Operations':sorted(set(selected))})
    for fixture in fixtures: batch(fixture.stem,lambda f=fixture:public(f))
    for product in products[:2]:
        for level in ('O0','O2'):
            name=product.stem+'.'+level; binary=isolated/(name+suffix)
            run(name+'.build',[cc,*flags,'-'+level,'-DTHREAD_SHIPPING_PRODUCT="'+product.name+'"',
                jit,memory,*libraries(product),'-o',binary])
            for fixture in fixtures:
                batch(name+'.'+fixture.stem,lambda f=fixture,n=name,b=binary:
                    run(n+'.'+f.stem,[b,isolated,out/(n+'.'+f.stem+'.temps'),isolated/'inputs'/f.name]))
    def negative(fixture):
        generated=out/(fixture.stem+'.rejected.c'); generated.write_bytes(b'KEEP')
        diagnostic='unknown Stack method' if fixture.stem=='legacy_into' else 'Stack.'
        run('negative.'+fixture.stem,[cli,'cgen',isolated/'inputs'/fixture.name,'--emit',generated],rejection=diagnostic)
        assert generated.read_bytes()==b'KEEP','Rejected program overwrote output'
    for fixture in negatives: batch('negative.'+fixture.stem,lambda f=fixture:negative(f))
    def independent():
        hosts=[]
        for level in ('O0','O2'):
            host=consumer/('host.'+level+suffix)
            run('consumer.'+level+'.build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wshadow','-'+level,
                consumer_source,*([] if windows else ['-ldl']),'-o',host],consumer); hosts.append(host)
        for fixture in artifacts:
            name=fixture.stem; image=consumer/(name+('.dll' if windows else '.so'))
            run(name+'.artifact.build',[cli,'build',isolated/'inputs'/fixture.name,'-shared','-o',image])
            text=run(name+'.imports',[inspect,'-p',image] if windows else [inspect,'-dW',image],consumer)
            needed=re.findall(r'DLL Name:\s*(\S+)',text) if windows else re.findall(r'\(NEEDED\).*?\[([^\]]+)\]',text)
            assert not any(re.match(r'(?:lib)?xlc?\.(?:so|dll)',item,re.I) for item in needed)
            if not windows:
                symbols=run(name+'.symbols',[inspect,'-sW',image],consumer)
                present='xl6StackExtractArrayV1' in symbols
                assert present==(name=='stdlib_stack_batch_artifact')
                segments=run(name+'.segments',[inspect,'-lW',image],consumer)
                stack=[line for line in segments.splitlines() if 'GNU_STACK' in line]
                assert len(stack)==1 and stack[0].split()[-2]=='RW'
            for index,host in enumerate(hosts):
                assert 'checked cleanup passed' in run(name+'.consumer.'+str(index),[host,image],consumer)
            outputs.append({'Path':str(image),'Sha256':sha(image),'Bytes':image.stat().st_size,
                'StaticSymbolPruningVerified':not windows})
    batch('independent',independent)
    report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
    report['Complete']=not errors and report['InputsUnchanged'] and len(proofs)==len(fixtures) and len(outputs)==3
    save(); print(f'Stack batch shipping: {len(rows)} phases, complete={report["Complete"]}',flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
