"""Migrated public Channel source, owned SSA results and strict global graph reads.
Windows/Linux source compiler and real standalone C/JIT/AOT. Not WaitQueue,
network consumers, the shipping SDK or full53 acceptance.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess

CASES=('cgen_channel','cgen_channel_string','xlir_channel_resource_abi',
    'xlir_channel_captured_output_abi','sem_channel_class_element','cgen_module_global_closure',
    'cgen_global_identifier_collision','xlir_async_storage_address','xlir_task_value_types',
    'stdlib_signal_dispatch_routes','sem_channel_receiver_expr','sem_channel_recv_captured_target',
    'sem_channel_try_recv_captured_target','stdlib_channel_global_absence',
    'stdlib_channel_owned','stdlib_channel_owned_wait','xlir_module_product_globals',
    'cgen_auto_inference_matrix')
NEGATIVE={
    'capacity_type':'channel','class_graph_element':'channel<T>',
    'ctor_arg_count_many':'channel','ctor_named_argument':'channel',
    'function_element':'channel<T>','generator_element':'channel<T>',
    'generic_arity':'expected 1, actual 2','generic_class_element':'channel<T>',
    'owned_extra':'channel receive takes no output variable',
    'owned_retired':'Into methods are retired','owned_timeout_named':'[parse]',
    'owned_timeout_type':'timeoutMs must be int','recv_const_target':'const',
    'recv_named_argument':'channel receive takes no output variable',
    'recv_target_type':'target string, value nullable<int>',
    'recv_timeout_named_argument':'channel receive takes no output variable',
    'recv_timeout_target_type':'target string, value nullable<int>',
    'recv_timeout_type':'timeoutMs must be int','send_named_argument':'channel.send',
    'send_type':'target int, value string','try_recv_const_target':'const',
    'try_recv_named_argument':'channel receive takes no output variable',
    'try_recv_target_type':'target string, value nullable<int>',
    'typed_array_element':'channel<T>','unknown_method':'unknown channel method'}

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def receive_ir(ir,typed):
    types={t['id']:t for t in ir['types']}
    definitions={i['result']:i for i in ir.get('instructions',[]) if i['result']}
    receives=[]
    for call in ir['intrinsicCalls'] if typed else ir['instructions']:
        name=call['runtimeResourceOpName'] if typed else call['runtimeOpName']
        if not name.startswith('channel-'): continue
        assert '-into' not in name,name
        if typed:
            assert call['errorAbiName']=='may-raise'
            if call['admissionName']=='generic-dependent':
                # Logical generic declarations have no physical codec. Their
                # concrete clones are checked below; the private mutation
                # gate separately verifies enclosing declaration authority.
                assert types[call['payloadType']]['cAbi']==0
                assert call['runtimeResourcePayloadAbiName']=='invalid'
            else: assert call['admissionName']=='verified'
        if not name.startswith('channel-owned-'): continue
        result=types[call['resultType'] if typed else call['type']]
        assert result['name']=='__tuple' and len(result['params'])==2
        assert types[result['params'][0]]['displayName']=='int'
        payload=call['payloadType']; value=types[result['params'][1]]
        assert (value['name']=='nullable' and value['params']==[payload]) or value['id']==payload
        if typed:
            operands=ir['intrinsicOperands'][call['firstOperand']:call['firstOperand']+call['operandCount']]
            assert len(operands) in (1,2) and operands[0]['abiName']=='borrow'
            assert all(i['abiName']!='storage-ref' for i in operands)
        else:
            assert call['kindName']=='runtime-call' and call['operandCount'] in (1,2)
            assert all(definitions[v]['kindName']!='storage-address' for v in call['operands'])
        receives.append(call)
    return receives

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path)
    parser.add_argument('--list-negative',action='store_true',help='Read-only diagnostic manifest for suite collectors')
    parser.add_argument('--jobs',type=int,default=4)
    args=parser.parse_args()
    if args.list_negative:
        print(json.dumps({'sem_channel_'+name:marker for name,marker in NEGATIVE.items()})); return
    if not all((args.root,args.public_directory,args.out)): parser.error('--root, --public-directory and --out are required')
    root=args.root.resolve(); public=args.public_directory.resolve(); out=args.out.resolve()
    assert 1<=args.jobs<=8 and not out.exists()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    assert build.get('Complete',os.name=='nt') and build['SourceXrt'] and not build['RuntimeOnly']
    cc=shutil.which('gcc'); assert cc
    driver=Path(build['Driver']); runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    inputs={Path(__file__).resolve(),public/'build.json',Path(cc),runtime,driver}
    names=(*CASES,*('sem_channel_'+n for n in NEGATIVE),'cgen_dev_handfeel_regression')
    inputs.update(root/'test/cases'/(name+'.xl') for name in names)
    inputs.update(root/name for name in ('test/cases/xlir_channel_global_trace_host.c',
        'test/support/module_product_memory.c','test/support/module_product_memory.h'))
    inputs.add(root/'test/cases/xlir_expect_support.c')
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); inputs.add(Path(row['Path']))
    assert sha(driver)==build['DriverSha256'].lower()
    recorded=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(inputs)]
    out.mkdir(); rows=[]; products=[]; env=dict(os.environ)
    if os.name=='nt': shutil.copy2(runtime,out/'xl.dll')
    else:
        env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
        env['XL6_TCC_LIB']=str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
    report=dict(Complete=False,InputsUnchanged=False,Inputs=recorded,Rows=rows,Products=products,
        PositiveFixtures=CASES,NegativeFixtures=NEGATIVE,Scope=__doc__,WaitQueueComplete=False,
        Full53Acceptance=False,ShippingArtifact=False,NativeGraphPrefixSweeps=[],CompileOnly=[])
    def run(name,command,success=True):
        command=list(map(str,command))
        try: result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        except subprocess.TimeoutExpired as failure:
            log=out/(name+'.log'); log.write_bytes((failure.stdout or b'')+(failure.stderr or b''))
            rows.append(dict(Name=name,Command=command,Exit=None,TimedOut=True,ExpectedSuccess=success,
                Log=str(log),LogSha256=sha(log)))
            raise
        log=out/(name+'.log'); log.write_bytes(result.stdout+result.stderr)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,ExpectedSuccess=success,
            Log=str(log),LogSha256=sha(log)))
        assert (result.returncode==0)==success,(name,log.read_bytes()[-8000:])
        print(name+' verified',flush=True)
        return log.read_text(encoding='utf-8',errors='replace')
    def product(path): products.append(dict(Path=str(path),Sha256=sha(path)))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Ilib/tcc','-I'+str(public/'include')]
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    unused=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-const-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    links=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else
        ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)])]
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    def positive(name):
        source=root/'test/cases'/(name+'.xl'); receives=[]
        sidecars=[root/'test/cases/xlir_expect_support.c'] if name=='cgen_auto_inference_matrix' else []
        for mode in ('run','module','ir-typed','ir-lowered'):
            output=out/(name+'-'+mode+('.json' if mode.startswith('ir-') else ''))
            run(name+'-'+mode,[driver,source,mode,output,*sidecars])
            if mode.startswith('ir-'):
                product(output); ir=json.loads(output.read_text(encoding='utf-8-sig'))
                receives=receive_ir(ir,mode=='ir-typed')
                if name=='xlir_async_storage_address' and mode=='ir-lowered':
                    assert ir['asyncMachineCount']==1 and len(receives)==1
                    assert not any(i['kindName']=='storage-address' for i in ir['instructions'])
        generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
        for path in (generated,repeat):
            run(name+'-cgen'+('-repeat' if path==repeat else ''),[driver,source,'cgen-default',path,*sidecars]); product(path)
        assert generated.read_bytes()==repeat.read_bytes()
        text=generated.read_text(encoding='utf-8-sig')
        assert 'xlir_channel_' not in text and 'recvInto' not in text
        if receives:
            count=len(re.findall(r'xl6ChannelReceiveV1\(',text))
            # Async emits both the immediate-ready and resumed execution
            # paths for this same verified instruction; neither is a second
            # language receive. This fixture has exactly one async receive.
            assert count==len(receives)+(1 if name=='xlir_async_storage_address' else 0)
        for opt in ('O0','O2'): run(name+'-gcc-'+opt,[cc,*flags,*unused,'-'+opt,'-fsyntax-only',generated])
        executable=out/(name+'-aot'+('.exe' if os.name=='nt' else ''))
        run(name+'-aot-build',[driver,source,'exe',executable.with_suffix('') if os.name=='nt' else executable,*sidecars]); product(executable)
        run(name+'-aot-run',[executable])
        if name in ('cgen_channel','xlir_channel_captured_output_abi'):
            for n in range(5): run(name+'-repeat-'+str(n),[executable])
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            list(pool.map(positive,CASES))
        for name,marker in NEGATIVE.items():
            output=out/('negative-'+name+'.c')
            text=run('negative-'+name,[driver,root/'test/cases'/('sem_channel_'+name+'.xl'),'cgen-default',output],False)
            assert marker in text and not output.exists(),(name,text)
        # These mixed fixtures retain their unrelated coverage. Compile them
        # here; their external host/template runtime gates remain separate.
        for name in ('cgen_dev_handfeel_regression',):
            source=root/'test/cases'/(name+'.xl'); generated=out/(name+'.c')
            run(name+'-cgen',[driver,source,'cgen-default',generated]); product(generated)
            assert 'recvInto' not in generated.read_text(encoding='utf-8-sig')
            for opt in ('O0','O2'): run(name+'-gcc-'+opt,[cc,*flags,*unused,'-'+opt,'-fsyntax-only',generated])
            report['CompileOnly'].append(name)
        debug=out/'debug-memory.o'
        run('debug-memory-build',[cc,*flags,'-c',root/'test/support/module_product_memory.c','-o',debug]); product(debug)
        generated=out/'stdlib_channel_global_absence.c'
        for opt in ('O0','O2'):
            host=out/('global-trace-'+opt+('.exe' if os.name=='nt' else ''))
            run(opt+'-global-trace-build',[cc,*flags,*unused,'-'+opt,'-include',generated,
                root/'test/cases/xlir_channel_global_trace_host.c',debug,*objects,*links,'-o',host]); product(host)
            text=run(opt+'-global-trace-prefixes',[host])
            proof=re.search(r'prefixes=(\d+) first-no-hit=(\d+) balanced',text); assert proof,text
            count,last=map(int,proof.groups()); assert count==last+1 and last>0
            report['NativeGraphPrefixSweeps'].append(dict(Optimization=opt,Prefixes=count,FirstNoHit=last))
        report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in recorded)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']
    print('Channel migrated source/global graph verified: '+str(len(rows))+' stages',flush=True)

if __name__=='__main__': main()
