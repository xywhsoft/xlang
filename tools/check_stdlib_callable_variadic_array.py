#!/usr/bin/env python3
"""Native boxed-call frame Array producer: canonical IR, strict C, real runtime, JIT/AOT and complete OOM prefixes.

Windows links all frozen current-source objects; the DLL supplies TCC only.
Linux links the current-source shared runtime/compiler. This gate does not
replace the independent artifact component/linkage/pruning gate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--public',type=Path)
    parser.add_argument('--producers',action='store_true',help='Ordinary/closure/dynamic/bound native Array producers')
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    root,out=args.root.resolve(),args.out.resolve()
    tests=Path(__file__).resolve().parents[1]
    cc=shutil.which('gcc')
    if not cc or out.exists() or (os.name=='nt') != bool(args.public):
        parser.error('Require gcc, fresh output and --public only on Windows')
    names=['stdlib_variadic_native_producers','stdlib_variadic_native_producers_unused'] if args.producers else [
        'stdlib_callable_variadic_array','stdlib_callable_variadic_array_unused']
    contract_name='stdlib_variadic_native_producers_contract.c' if args.producers else 'stdlib_callable_variadic_array_contract.c'
    regressions=['cgen_dynamic_method_arguments','cgen_dynamic_method_bound_lifetime',
        'cgen_dynamic_method_inheritance','cgen_dynamic_method_overload','cgen_generic_dynamic_method',
        'protocol_dynamic_dispatch','stdlib_variadic_dispatch_reachability'] if args.producers else []
    files={Path(__file__),Path(sys.executable),Path(cc)}
    files.update(tests/('test/cases/'+name+'.xl') for name in names)
    files.update(tests/('test/cases/'+name+'.xl') for name in regressions)
    for name in [contract_name,'stdlib_callable_variadic_array_runtime.c',
                 'xlir_callable_bridge_mutations.h','stdlib_collection_admission_host.c','xlir_module_product_oom_host.c']:
        files.add(tests/('test/cases/'+name))
    files.update(tests/('test/support/'+name) for name in ['module_product_memory.c','module_product_memory.h'])
    for folder in ['src','lib','release/include']:
        files.update(p for p in (root/folder).rglob('*') if p.suffix in ('.c','.h','.inc'))
    objects=[]
    if args.public:
        public=args.public.resolve()
        build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
        staged=json.loads((public/'artifact_sources.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs']+build['Headers']+build['Objects']+staged['Files']:
            assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
            files.add(Path(row['Path']))
        files.update([public/'build.json',public/'artifact_sources.json',root/'release/xl.dll'])
        objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name!='xlir_public_source_driver.o']
        cli=Path(build['Driver']); files.add(cli)
        links=[str(root/'release/xl.dll'),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:
        cli=root/'release/xl'
        links=['-L'+str(root/'release'),'-lxlc','-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
        files.update(root/('release/'+name) for name in ['xl','libxl.so','libxlc.so'])
    inputs=[{'Path':str(p.resolve()),'Sha256':sha(p)} for p in sorted(files)]
    out.mkdir(parents=True)
    if args.public: shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    private=out/'include/__xl_runtime'; private.mkdir(parents=True)
    for folder in [root/'src/runtime',root/'release/include']:
        for header in folder.glob('*.h'): shutil.copy2(header,private/header.name)
    rows=[]; sweeps=[]
    report={'Scope':__doc__,'Complete':False,'InputsUnchanged':False,'Platform':sys.platform,
            'Rows':rows,'Sweeps':sweeps,'Inputs':inputs,'Full53Complete':False,
            'ProducerKinds':['ordinary','closure','dynamic','bound','default','nested'] if args.producers else ['boxed-callable']}
    env=dict(os.environ)
    def run(name,command,timeout=180):
        command=[str(p) for p in command]; start=time.monotonic()
        try: process=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (out/(name+'.log')).write_bytes((error.stdout or b'')+(error.stderr or b''))
            rows.append({'Name':name,'Command':command,'Exit':None,'TimedOut':True}); raise
        data=process.stdout+process.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':process.returncode,'Seconds':time.monotonic()-start})
        if process.returncode: raise RuntimeError(name+': '+data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def sweep(label,binary):
        for point in range(4097):
            log=run(label+'.'+str(point),[binary,point])
            assert re.search(r'hit=[01] balanced',log)
            if 'hit=0 balanced' in log:
                sweeps.append({'Lane':label,'FirstNoHit':point,'Runs':point+1})
                print(label+': first no-hit '+str(point),flush=True); return
        raise AssertionError('OOM prefix did not close: '+label)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
           '-Ilib/tcc','-I'+str(out/'include'),'-Wno-unused-function','-Wno-unused-variable',
           '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
           '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation']
    if not args.public: flags+=['-D_GNU_SOURCE']
    suffix='.exe' if args.public else ''
    try:
        first={}
        for level in (0,2):
            memory=out/('memory-O'+str(level)+'.o')
            run('memory.build-O'+str(level),[cc,*flags,'-O'+str(level),'-c',tests/'test/support/module_product_memory.c','-o',memory])
            native=out/('native-O'+str(level)+suffix)
            run('native.build-O'+str(level),[cc,*flags,'-O'+str(level),tests/'test/cases/stdlib_callable_variadic_array_runtime.c',
                memory,*objects,*links,'-o',native])
            run('native.vectors-O'+str(level),[native]); sweep('native.O'+str(level),native)
            contract=out/('contract-O'+str(level)+suffix)
            run('contract.build-O'+str(level),[cc,*flags,'-O'+str(level),tests/('test/cases/'+contract_name),
                *objects,*links,'-o',contract])
            for name in names:
                source=tests/('test/cases/'+name+'.xl'); prefix=out/(name+'-O'+str(level))
                c,h,ir=[Path(str(prefix)+s) for s in ['.c','.h','.json']]
                log=run(name+'.contract-O'+str(level),[contract,source,c,h,ir]+(['--emit-only'] if level else []),600)
                assert re.search(r'native producers: arrays=\d+ methods=\d+ mutations=\d+ selected=[01]' if args.producers else
                    r'native frame plans: arrays=\d+ dicts=\d+ fixed=\d+ mutations=\d+ selected=[01]',log)
                text=c.read_text(encoding='utf-8'); unused=name.endswith('_unused')
                calls=re.findall(r'\bvariadic = xl6ArrayCallFramePackV1\(' if args.producers else
                    r'\ba\d+ = xl6ArrayCallFramePackV1\(',text)
                assert bool(calls)!=unused
                assert 'variadicIndex < frame->ArgumentCount' not in text
                assert 'xrtValueArrayAppend(variadic, positionalValues[positionalIndex])' not in text
                plans=json.loads(ir.read_text(encoding='utf-8'))['dynamicMethods' if args.producers else 'callableBridges']; assert plans
                assert all('variadicCarrierAbi' in b and 'variadicElementIdentity' in b for b in plans)
                repeat=[Path(str(p)+'.repeat') for p in [c,h,ir]]
                run(name+'.repeat-O'+str(level),[contract,source,*repeat,'--emit-only'])
                assert all(a.read_bytes()==b.read_bytes() for a,b in zip([c,h,ir],repeat))
                current=[p.read_bytes() for p in [c,h,ir]]
                if not level: first[name]=current
                else: assert current==first[name]
                binary=out/(name+'-O'+str(level)+suffix)
                run(name+'.build-O'+str(level),[cc,*flags,'-O'+str(level),c,tests/'test/cases/stdlib_collection_admission_host.c',
                    *objects,*links,'-o',binary])
                executed=run(name+'.run-O'+str(level),[binary])
                if args.producers and not unused:
                    normalized=executed.replace('\r\n','\n')
                    assert 'native 42 null\n[42,null]\n' in normalized
                    assert 'a\0你 ' in normalized
                if not unused:
                    module=out/(name+'.oom-O'+str(level)+suffix)
                    run(name+'.oom.build-O'+str(level),[cc,*flags,'-O'+str(level),'-DXL6_PRODUCT_USER_ENTRY','-include',c,
                        tests/'test/cases/xlir_module_product_oom_host.c',memory,
                        *objects,*links,'-o',module])
                    sweep('language.O'+str(level),module)
                print(name+' O'+str(level)+' passed',flush=True)
        for name in names:
            source=tests/('test/cases/'+name+'.xl'); prefix=out/(name+'-product')
            if args.public:
                run(name+'.jit',[cli,source,'run',prefix]); run(name+'.aot.build',[cli,source,'exe',prefix])
                product=Path(str(prefix)+'.exe')
            else:
                run(name+'.jit',[cli,'run',source]); product=Path(str(prefix)+'.product')
                run(name+'.aot.build',[cli,'build',source,'-o',product])
            assert product.is_file(); run(name+'.aot.run',[product])
        for name in regressions:
            source=tests/('test/cases/'+name+'.xl'); generated=out/(name+'.c')
            command=[cli,source,'cgen-default',generated] if args.public else [cli,'cgen',source,'--emit',generated]
            run(name+'.cgen',command)
            emitted=generated.read_text(encoding='utf-8')
            if name=='cgen_dynamic_method_inheritance': assert 'xlir_vdispatch_' not in emitted
            if name=='stdlib_variadic_dispatch_reachability':
                assert 'xlir_vdispatch_' in emitted and 'xlir_pdispatch_' in emitted
            for level in (0,2):
                binary=out/(name+'-O'+str(level)+suffix)
                run(name+'.build-O'+str(level),[cc,*flags,'-O'+str(level),generated,tests/'test/cases/stdlib_collection_admission_host.c',
                    *objects,*links,'-o',binary])
                run(name+'.run-O'+str(level),[binary])
            prefix=out/(name+'-regression')
            if args.public:
                run(name+'.jit',[cli,source,'run',prefix]); run(name+'.aot.build',[cli,source,'exe',prefix])
                product=Path(str(prefix)+'.exe')
            else:
                run(name+'.jit',[cli,'run',source]); product=Path(str(prefix)+'.product')
                run(name+'.aot.build',[cli,'build',source,'-o',product])
            assert product.is_file(); run(name+'.aot.run',[product])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Failure']=type(error).__name__+': '+str(error); print(report['Failure'],file=sys.stderr)
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Native callable frame: '+str(len(rows))+' phases; complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__': raise SystemExit(main())
