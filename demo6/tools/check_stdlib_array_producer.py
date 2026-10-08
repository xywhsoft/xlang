"""Native external Array producers: precise recipes, independent failures and real language products.
This batch is not the complete 53-module or complete Array-carrier acceptance.
"""
from __future__ import annotations
import argparse, hashlib, json, os, shutil, subprocess, sys, time
from pathlib import Path
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--root',type=Path,required=True)
    p.add_argument('--public',type=Path); p.add_argument('--out',type=Path,required=True)
    p.add_argument('--sanitize',action='store_true'); a=p.parse_args(); assert not (a.sanitize and a.public)
    root=a.root.resolve(); out=a.out.resolve(); cc=shutil.which('gcc'); assert cc and not out.exists()
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable)}
    for folder in ['src','lib','test','release/include']:
        files.update(f for f in (root/folder).rglob('*') if f.suffix in ('.c','.h','.inc','.xl'))
    if a.public:
        build=json.loads((a.public/'build.json').read_text(encoding='utf-8-sig'))
        staged=json.loads((a.public/'artifact_sources.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly'] and staged['DriverSha256'].lower()==sha(build['Driver'])
        for r in build['Inputs']+build['Headers']+build['Objects']+staged['Files']:
            assert sha(r['Path'])==r['Sha256'].lower(); files.add(Path(r['Path']))
        objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name!='xlir_public_source_driver.o']
        cli=Path(build['Driver']); links=[str(root/'release/xl.dll'),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']; suffix='.exe'
        files.update([a.public/'build.json',a.public/'artifact_sources.json',root/'release/xl.dll'])
    else:
        objects=[]; cli=root/'release/xl'; suffix=''
        links=['-L'+str(root/'release'),'-lxlc','-lxl','-Wl,-rpath,'+str(root/'release'),'-pthread','-ldl','-lm']
        files.update(root/('release/'+n) for n in ['xl','libxl.so','libxlc.so'])
    files.add(cli); inputs=[{'Path':str(f),'Sha256':sha(f)} for f in sorted(files)]; out.mkdir(parents=True)
    if a.public: shutil.copy2(root/'release/xl.dll',out/'xl.dll')
    rows=[]; sweeps=[]; report={'Complete':False,'Full53Complete':False,'InputsUnchanged':False,'Platform':sys.platform,'Scope':__doc__,'Inputs':inputs,'Rows':rows,'Sweeps':sweeps}
    def run(name,cmd):
        cmd=list(map(str,cmd)); start=time.monotonic(); r=subprocess.run(cmd,cwd=root,capture_output=True,timeout=180)
        data=r.stdout+r.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':cmd,'Exit':r.returncode,'Seconds':time.monotonic()-start})
        if r.returncode: raise RuntimeError(name+': '+data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include','-Ilib/tcc',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation']
    if not a.public: flags+=['-D_GNU_SOURCE']
    try:
        if a.sanitize:
            native=out/'selected-native-sanitized'
            sources=['test/cases/stdlib_array_producer_host.c','test/support/module_product_memory.c','test/support/array_producer_native.c',
                'src/runtime/xl6_array_producer.c','src/runtime/xl6_sequence.c','src/runtime/xl6_array_primitive_admission.c',
                'src/runtime/xl6_array_erased.c','src/runtime/xl6_array_reference_value.c','src/runtime/xl6_value_admission.c',
                'src/runtime/xl6_string_type.c','src/runtime/xl6_string_value.c','src/runtime/xl6_char_type.c','src/runtime/xl6_char_value.c',
                'src/runtime/xl6_text_chars.c','src/runtime/xl6_text_split.c','src/runtime/xl6_regex_core.c','src/runtime/xl6_regex_set.c',
                'src/runtime/xl6_array_input.c','src/api/xl_foreign_error.c']
            run('selected.sanitize.build',[cc,*flags,'-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                '-fno-pie','-no-pie',*sources,'-pthread','-ldl','-lm','-o',native])
            run('selected.sanitize.vectors',[native])
            for mode in range(5):
                for point in range(4097):
                    text=run(f'selected.sanitize.m{mode}.p{point}',[native,mode,point])
                    if 'hit=0 complete-prefix' in text:
                        sweeps.append({'Lane':'selected-ASan-UBSan','Mode':mode,'FirstNoHit':point,'Runs':point+1}); break
                else: raise AssertionError('sanitized allocation prefix did not close')
        for opt in ['-O0','-O2']:
            memory=out/('memory'+opt+'.o'); run('memory'+opt,[cc,*flags,opt,'-c','test/support/module_product_memory.c','-o',memory])
            binary=out/('native'+opt+suffix)
            run('native.build'+opt,[cc,*flags,opt,'test/cases/stdlib_array_producer_host.c',memory,*objects,*links,'-o',binary])
            run('native.vectors'+opt,[binary])
            for mode in range(5):
                for point in range(4097):
                    text=run(f'native{opt}.m{mode}.p{point}',[binary,mode,point])
                    if 'hit=0 complete-prefix' in text:
                        sweeps.append({'Lane':opt,'Mode':mode,'FirstNoHit':point,'Runs':point+1}); break
                else: raise AssertionError('allocation prefix did not close')
            contract=out/('contract'+opt+suffix)
            run('contract.build'+opt,[cc,*flags,opt,'test/cases/xlir_array_producer_contract.c',*objects,*links,'-o',contract])
            run('contract.run'+opt,[contract])
            # Existing C consumers must use the new resident result, not keep
            # accidentally testing a retired wire-producing bridge symbol.
            for name in ['stdlib_text_queries_host','stdlib_text_split_host','xlir_string_bridge_abi_host',
                         'stdlib_array_input_host','xlir_regex_extended_host']:
                host=out/(name+opt+suffix)
                run(name+'.build'+opt,[cc,*flags,opt,'-DXLIR_NATIVE_PROBE_ONLY',
                    'test/cases/'+name+'.c',memory,*objects,*links,'-o',host])
                run(name+'.run'+opt,[host])
        for name in ['bad_xlir_text_split_type','bad_xlir_text_chars_type','xlir_regex_bad_set_hits',
                     'xlir_string_chars_bad_parameter','xlir_string_chars_bad_result']:
            source=root/('test/cases/'+name+'.xl'); target=out/(name+'.c')
            cmd=([cli,source,'cgen-default',target] if a.public else [cli,'cgen',source,'--emit',target])
            # A wrong declaration must be recognized and rejected before C
            # emission. Merely failing to link a now-unknown V1 symbol is not a gate.
            cmd=list(map(str,cmd)); r=subprocess.run(cmd,cwd=root,capture_output=True,timeout=180)
            data=r.stdout+r.stderr; (out/(name+'.negative.log')).write_bytes(data)
            rows.append({'Name':name+'.negative','Command':cmd,'Exit':r.returncode,'Expected':1})
            assert r.returncode==1 and not target.exists() and 'XLIR'.encode() in data
        fixtures=['stdlib_array_external_producers','stdlib_text_split','stdlib_text_queries','stdlib_regex_set_only','stdlib_pattern_builder','stdlib_array_open_producers']
        for name in fixtures:
            source=root/('test/cases/'+name+'.xl'); prefix=out/(name+'-product')
            if a.public:
                run(name+'.jit',[cli,source,'run',prefix]); run(name+'.aot.build',[cli,source,'exe',prefix]); binary=Path(str(prefix)+'.exe')
            else:
                run(name+'.jit',[cli,'run',source]); binary=Path(str(prefix)+'.product'); run(name+'.aot.build',[cli,'build',source,'-o',binary])
            run(name+'.aot.run',[binary])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs); assert report['InputsUnchanged']; report['Complete']=True
    except Exception as e: report['Failure']=type(e).__name__+': '+str(e); print(report['Failure'],file=sys.stderr)
    finally:
        report['Outputs']=[{'Path':str(f),'Sha256':sha(f)} for f in sorted(out.rglob('*')) if f.is_file()]
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Native Array producers: '+str(len(rows))+' phases; complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
