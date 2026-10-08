"""Resident typed SPSC/MPSC/MPMC prerequisite, not language/53-module acceptance.
Exact state results, schema, prefix ownership, parallel roles and physical graphs.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time
from pathlib import Path

PROVIDERS=['src/runtime/xl6_queue_'+f+'.c' for f in ['spsc','mpsc','mpmc']]
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--public',type=Path)
    parser.add_argument('--runtime',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    cc=shutil.which('gcc'); nm=shutil.which('nm'); assert cc and nm and not out.exists()
    assert bool(args.public)!=bool(args.runtime) and not (args.public and args.sanitize)
    files={Path(__file__).resolve(),Path(cc),Path(nm),Path(sys.executable)}
    for folder in ['src','lib','release/include','test/support']:
        files.update(f for f in (root/folder).rglob('*') if f.suffix in ('.c','.h','.inc'))
    files.add(root/'test/cases/stdlib_concurrent_queue_native_host.c')
    files.add(root/'test/cases/stdlib_concurrent_queue_tcc_host.c')
    if args.public:
        build=json.loads((args.public/'build.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
        exclude={Path(f).with_suffix('.o').name for f in PROVIDERS}|{'xlir_public_source_driver.o'}
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in exclude]
        # Fresh providers are compiled at each optimization, never hidden by an
        # older source build or a DLL exporting another implementation.
        dll=root/'release/xl.dll'; files.add(dll)
        links=[str(dll),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']; suffix='.exe'
    else:
        assert sys.platform=='linux'
        runtime=args.runtime.resolve(); objects=[]; suffix=''
        files.update(runtime/n for n in ['libxl.so','libxlc.so'])
        links=['-L'+str(runtime),'-lxlc','-lxl','-Wl,-rpath,'+str(runtime),'-pthread','-ldl','-lm']
    inputs=[{'Path':str(f),'Sha256':sha(f)} for f in sorted(files)]
    out.mkdir(parents=True)
    if args.public: shutil.copy2(dll,out/'xl.dll')
    rows=[]; sweeps=[]; selections=[]
    report={'Complete':False,'LanguageQueueMigrationComplete':False,'Full53Complete':False,
            'InputsUnchanged':False,'Scope':__doc__,'Platform':sys.platform,
            'Inputs':inputs,'Rows':rows,'Sweeps':sweeps,'Selections':selections}
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start})
        if result.returncode: raise RuntimeError(name+': '+data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
           '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
           '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
           '-Wno-pointer-sign','-Wno-format-truncation']
    if not args.public: flags+=['-D_GNU_SOURCE']
    def vectors_and_faults(lane,binary):
        run(lane+'.vectors',[binary]); run(lane+'.threads',[binary,'threads'])
        for family in range(3):
            for operation in range(5):
                for point in range(4097):
                    text=run(f'{lane}.f{family}.m{operation}.p{point}',[binary,family,operation,point])
                    if 'hit=0 complete-prefix' in text:
                        sweeps.append({'Lane':lane,'Family':family,'Operation':operation,'FirstNoHit':point,'Runs':point+1}); break
                else: raise AssertionError('allocation prefix did not close')
        print(lane+': vectors, threads and fifteen complete fault prefixes passed',flush=True)
    try:
        for optimization in ['-O0','-O2']:
            provider_objects=[]
            for source in PROVIDERS:
                target=out/(Path(source).stem+optimization+'.o')
                run(target.stem+'.compile',[cc,*flags,optimization,'-c',source,'-o',target])
                text=run(target.stem+'.symbols',[nm,'-u',target])
                family=Path(source).stem.rsplit('_',1)[1].upper()
                actual=set(re.findall(r'\bU\s+(xrtTyped(?:SPSC|MPSC|MPMC)Queue\w+)',text))
                assert actual and all(symbol.startswith('xrtTyped'+family+'Queue') for symbol in actual)
                assert not re.search(r'\b(?:xrtChannel|xrtTQueue)\w*',text)
                selections.append({'Scope':'Resident-provider','Optimization':optimization,'Family':family,'Required':sorted(actual),'Complete':True})
                provider_objects.append(target)
            binary=out/('queue'+optimization+suffix)
            run('build'+optimization,[cc,*flags,optimization,'test/cases/stdlib_concurrent_queue_native_host.c',
                'test/support/module_product_memory.c',*provider_objects,*objects,*links,'-o',binary])
            vectors_and_faults(optimization,binary)
            tcc_binary=out/('queue-tcc'+optimization+suffix)
            run('tcc.build'+optimization,[cc,*flags,optimization,'test/cases/stdlib_concurrent_queue_tcc_host.c',
                'test/support/module_product_memory.c',*provider_objects,*objects,*links,'-o',tcc_binary])
            run('tcc.abi'+optimization,[tcc_binary])
        if args.sanitize:
            native=out/'queue-sanitized'
            sources=['test/cases/stdlib_concurrent_queue_native_host.c','test/support/module_product_memory.c',
                'test/support/concurrent_queue_native.c',*PROVIDERS,'src/runtime/xl6_sequence.c',
                'src/runtime/xl6_array_primitive_admission.c','src/runtime/xl6_array_reference_admission.c',
                'src/runtime/xl6_array_reference_value.c','src/runtime/xl6_array_erased.c','src/runtime/xl6_array_future_admission.c',
                'src/runtime/xl6_array_c_scalar_admission.c','src/runtime/xl6_nullable_i64.c',
                'src/runtime/xl6_value_admission.c','src/runtime/xl6_string_type.c','src/runtime/xl6_char_type.c',
                'src/runtime/xl6_ownership_plan.c','src/api/xl_foreign_error.c']
            run('sanitize.build',[cc,*flags,'-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                '-fno-pie','-no-pie',*sources,'-pthread','-ldl','-lm','-o',native])
            vectors_and_faults('ASan-UBSan',native)
        # Inspect each selected XRT TU, not just unresolved provider names: a
        # component annotation must really omit the other typed algorithms.
        for family in ['SPSC','MPSC','MPMC']:
            target=out/('native-'+family+'.o')
            run('select.'+family+'.compile',[cc,*flags,'-O0','-DXL6_QUEUE_NATIVE_'+family+'_ONLY',
                '-c','test/support/concurrent_queue_native.c','-o',target])
            text=run('select.'+family+'.symbols',[nm,'-g','--defined-only',target])
            actual=set(re.findall(r'\b[TRD]\s+(xrtTyped(?:SPSC|MPSC|MPMC)Queue\w+)',text))
            assert actual and all(symbol.startswith('xrtTyped'+family+'Queue') for symbol in actual)
            assert not re.search(r'\b(?:xrtChannel|xrtTQueue)\w*',text)
            selections.append({'Scope':'Actual-selected-XRT-TU','Family':family,'Defined':sorted(actual),'Complete':True})
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Failure']=type(error).__name__+': '+str(error); print(report['Failure'],file=sys.stderr)
    finally:
        report['Outputs']=[{'Path':str(f),'Sha256':sha(f)} for f in sorted(out.rglob('*')) if f.is_file()]
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'Typed queue kernel: {len(rows)} phases; complete={report["Complete"]}',flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
