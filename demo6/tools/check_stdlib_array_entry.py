#!/usr/bin/env python3
"""Canonical open Array literals and host/argv entry; strict C/JIT/AOT, mutation rejection and complete OOM prefixes."""
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time
from pathlib import Path
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True); p.add_argument('--public',type=Path)
    p.add_argument('--out',type=Path,required=True); a=p.parse_args()
    root,out=a.root.resolve(),a.out.resolve(); tests=Path(__file__).resolve().parents[1]
    cc=shutil.which('gcc'); assert cc and not out.exists() and bool(a.public)==(os.name=='nt')
    names=['stdlib_array_open_producers','stdlib_array_entry_values','stdlib_array_entry_argv','cgen_basic']
    regressions=['cgen_array_literal_expr_elements','cgen_dynamic_container_reference','cgen_dynamic_container_methods',
                 'cgen_dynamic_container_param','cgen_array_negative_index']
    files={Path(__file__),Path(sys.executable),Path(cc)}
    files.update(tests/('test/cases/'+n+'.xl') for n in names+regressions)
    for n in ['stdlib_array_entry_contract.c','stdlib_array_entry_runtime.c','stdlib_array_entry_host.c','stdlib_array_entry_module_host.c',
              'stdlib_dynamic_collection_contract.c','stdlib_collection_admission_host.c','xlir_module_product_oom_host.c']:
        files.add(tests/('test/cases/'+n))
    files.update(tests/('test/support/'+n) for n in ['module_product_memory.c','module_product_memory.h','time_expect.c'])
    for folder in ['src','lib','release/include']:
        files.update(p for p in (root/folder).rglob('*') if p.suffix in ('.c','.h','.inc'))
    objects=[]
    if a.public:
        public=a.public.resolve(); build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        staged=json.loads((public/'artifact_sources.json').read_text(encoding='utf-8-sig'))
        assert staged['DriverSha256'].lower()==sha(build['Driver'])
        for row in build['Inputs']+build['Headers']+build['Objects']+staged['Files']:
            assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
        files.update([public/'build.json',public/'artifact_sources.json',root/'release/xl.dll'])
        objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name!='xlir_public_source_driver.o']
        cli=Path(build['Driver']); links=[str(root/'release/xl.dll'),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
    else:
        cli=root/'release/xl'; links=['-L'+str(root/'release'),'-lxlc','-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
        files.update(root/('release/'+n) for n in ['xl','libxl.so','libxlc.so'])
    files.add(cli); inputs=[{'Path':str(p),'Sha256':sha(p)} for p in sorted(files)]
    out.mkdir(parents=True)
    if a.public: shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    private=out/'include/__xl_runtime'; private.mkdir(parents=True)
    for folder in [root/'src/runtime',root/'release/include']:
        for h in folder.glob('*.h'): shutil.copy2(h,private/h.name)
    rows=[]; sweeps=[]; report={'Complete':False,'InputsUnchanged':False,'Full53Complete':False,
        'Scope':__doc__,'Inputs':inputs,'Rows':rows,'Sweeps':sweeps,'Platform':sys.platform}
    def run(label,cmd,timeout=600):
        cmd=list(map(str,cmd)); start=time.monotonic()
        try: r=subprocess.run(cmd,cwd=root,capture_output=True,timeout=timeout)
        except subprocess.TimeoutExpired as e:
            (out/(label+'.log')).write_bytes((e.stdout or b'')+(e.stderr or b'')); raise
        data=r.stdout+r.stderr; (out/(label+'.log')).write_bytes(data)
        rows.append({'Name':label,'Command':cmd,'Exit':r.returncode,'Seconds':time.monotonic()-start})
        if r.returncode: raise RuntimeError(label+': '+data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def sweep(label,binary,mode=None):
        for point in range(4097):
            log=run(label+'.'+str(point),[binary,*([mode] if mode else []),point])
            assert re.search(r'hit=[01] balanced',log)
            if 'hit=0 balanced' in log:
                sweeps.append({'Lane':label,'FirstNoHit':point,'Runs':point+1}); print(label+': '+str(point),flush=True); return
        raise AssertionError('OOM prefix did not close')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Ilib/tcc','-I'+str(tests),'-I'+str(out/'include'),'-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation']
    if not a.public: flags+=['-D_GNU_SOURCE']
    suffix='.exe' if a.public else ''
    try:
        prior={}
        for level in (0,2):
            opt='-O'+str(level); memory=out/('memory'+opt+'.o')
            run('memory'+opt,[cc,*flags,opt,'-c',tests/'test/support/module_product_memory.c','-o',memory])
            dynamic=out/('dynamic-contract'+opt+suffix)
            run('dynamic-contract.build'+opt,[cc,*flags,opt,tests/'test/cases/stdlib_dynamic_collection_contract.c',memory,*objects,*links,'-o',dynamic])
            run('dynamic-contract.run'+opt,[dynamic])
            native=out/('native'+opt+suffix); contract=out/('contract'+opt+suffix)
            for name,binary in [('runtime',native),('contract',contract)]:
                run(name+'.build'+opt,[cc,*flags,opt,tests/('test/cases/stdlib_array_entry_'+name+'.c'),
                    *([memory] if name=='runtime' else []),*objects,*links,'-o',binary])
            run('native.vectors'+opt,[native]); sweep('native.import'+opt,native,'import'); sweep('native.argv'+opt,native,'argv')
            sweep('native.utf16'+opt,native,'utf16')
            for name in names:
                prefix=out/(name+opt); c,h,ir=[Path(str(prefix)+s) for s in ['.c','.h','.json']]
                run(name+'.contract'+opt,[contract,tests/('test/cases/'+name+'.xl'),c,h,ir])
                text=c.read_text(encoding='utf-8'); entry=json.loads(ir.read_text(encoding='utf-8'))['moduleEntry']
                assert bool(entry['kind'])==(name!='cgen_basic')
                assert ('xl6ArrayEntryAcquireV1(' in text)==bool(entry['kind'])
                assert 'xlir_command_argv_to_arg' not in text
                if name in prior: assert [p.read_bytes() for p in [c,h,ir]]==prior[name]
                else: prior[name]=[p.read_bytes() for p in [c,h,ir]]
                binary=out/(name+opt+suffix)
                host='stdlib_array_entry_host.c' if name.endswith('_values') else 'stdlib_collection_admission_host.c'
                source_args=[c,tests/('test/cases/'+host)] if entry['kind'] else [
                    '-include',c,tests/'test/cases/stdlib_array_entry_module_host.c',memory]
                run(name+'.build'+opt,[cc,*flags,opt,*source_args,*objects,*links,'-o',binary])
                run(name+'.run'+opt,[binary])
                if name=='stdlib_array_open_producers':
                    binary=out/(name+'.oom'+opt+suffix)
                    run(name+'.oom.build'+opt,[cc,*flags,opt,'-DXL6_PRODUCT_USER_ENTRY','-include',c,
                        tests/'test/cases/xlir_module_product_oom_host.c',memory,*objects,*links,'-o',binary])
                    sweep('language'+opt,binary)
        for name in names+regressions:
            if name=='stdlib_array_entry_values': continue
            source=tests/('test/cases/'+name+'.xl'); prefix=out/(name+'-product')
            sidecar=[tests/'test/support/time_expect.c'] if name=='cgen_array_literal_expr_elements' else []
            if a.public:
                run(name+'.jit',[cli,source,'run',prefix,*sidecar]); run(name+'.aot.build',[cli,source,'exe',prefix,*sidecar]); binary=Path(str(prefix)+'.exe')
            else:
                run(name+'.jit',[cli,'run',source,*sidecar]); binary=Path(str(prefix)+'.product'); run(name+'.aot.build',[cli,'build',source,*sidecar,'-o',binary])
            # main argc/scalar regressions intentionally return nonzero, so
            # execute only semantic-zero programs in this gate.
            if name not in ('cgen_main_entry','xlir_entry_scalar'):
                run(name+'.aot.run',[binary,*(['','你','🙂','a"b','C:\\path with space\\'] if name=='stdlib_array_entry_argv' else [])])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as e: report['Failure']=type(e).__name__+': '+str(e); print(report['Failure'],file=sys.stderr)
    finally:
        report['Outputs']=[{'Path':str(p),'Sha256':sha(p)} for p in sorted(out.rglob('*')) if p.is_file()]
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Native Array entry: '+str(len(rows))+' phases; complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
