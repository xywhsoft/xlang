"""Actual-product codec/compress/template batch acceptance, not full RC0.

Requires fresh ordinary CLI and both libraries. No substituted native providers,
compiler overrides, source fallback in the isolated compiler or release promotion.
The explicit-class-data policy is recorded separately from the tested API subset.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--group',choices=('all','codec-text','codec-bytes'),default='all')
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve()
    assert not out.exists()
    task=Path(__file__).resolve().parents[1];cases=task/'test/cases';win=os.name=='nt'
    cc,nm,inspect=[shutil.which(n) for n in ('gcc','nm','objdump' if win else 'readelf')]
    assert cc and nm and inspect
    names=('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so')
    products=[root/'release'/n for n in names];assert all(f.is_file() for f in products)
    selected={
        'codec-text':['stdlib_codec_base64_text_only'],
        'codec-bytes':['stdlib_codec_hex_bytes_only','stdlib_codec_base64_bytes_only'],
        'all':['stdlib_codec','stdlib_codec_values','stdlib_compress_values','stdlib_template_values',
            'stdlib_template','stdlib_template_engine',
            'stdlib_template_engine_values','stdlib_template_files','stdlib_template_owner',
            'stdlib_template_privacy','stdlib_native_field_mixin','stdlib_codec_hex_text_only',
            'stdlib_codec_base64_text_only','stdlib_codec_hex_bytes_only','stdlib_codec_base64_bytes_only',
            'stdlib_compress_deflate_only','stdlib_template_core_only',
            'stdlib_array_persistence_language','stdlib_persistence_nominal_language']
    }[a.group]
    negatives=['bad_xlir_base64_text_'+n for n in ('missing_table','wrong_input','wrong_result')] if a.group=='codec-text' else [
        'bad_xlir_byte_codec_'+n for n in ('missing_table','wrong_input','wrong_result')]
    if a.group=='all':
        negatives+=['bad_xlir_base64_text_'+n for n in ('missing_table','wrong_input','wrong_result')]
        negatives+=['bad_xlir_'+n for n in ('codec_value_result','codec_value_no_raise',
            'compress_handle_owner','compress_no_raise','template_raw_context','template_callback_abi')]
    oom=['stdlib_'+n+'_oom' for n in ('codec','compress','template')] if a.group=='all' else []
    native=['stdlib_'+n+'_resident_host' for n in ('codec','compress','template')] if a.group=='all' else ['stdlib_codec_resident_host']
    helpers=['stdlib_product_fixture_host','stdlib_pruning_product_host','stdlib53_pruning_plan',
        'xlir_codec_template_contract','stdlib_codec_template_language_host',
        'stdlib_codec_template_language_oom_host','stdlib_codec_template_oom_host']
    files={Path(__file__).resolve(),*products,*[cases/(n+'.xl') for n in selected+negatives+oom],
        *[cases/(n+'.c') for n in native+helpers],task/'test/support/module_product_memory.c',
        task/'test/support/module_product_memory.h'}
    for folder in ('src/runtime','src/ir','src/stdlib','src/compile'):
        files.update(f for f in (root/folder).rglob('*') if f.is_file())
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();isolated=out/'isolated';release=isolated/'release';release.mkdir(parents=True)
    relocated=out/'relocated';relocated.mkdir();sdk=out/'sdk';sdk.mkdir()
    rows=[];failures=[];report=dict(Complete=False,Scope=__doc__,Group=a.group,ExplicitSubset=True,Failures=failures,
        FullRC0Acceptance=False,ClassDataDecisionPending=False,Inputs=inputs,Rows=rows)
    for product in products:
        shutil.copy2(product,release/product.name)
        if win and product.suffix=='.dll':shutil.copy2(product,isolated/product.name)
    env={k.upper():v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
        ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    # The existing nominal persistence regression explicitly requires its own
    # data directory. Do not inherit a caller's path or alter the host config.
    persistence=isolated/'persistence-data';persistence.mkdir()
    env['XL6_PERSIST_FILE_ROOT']=str(persistence)
    report['FixtureEnvironment']={'XL6_PERSIST_FILE_ROOT':str(persistence)}
    def save():
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,cwd=isolated,negative=False):
        command=list(map(str,command));r=subprocess.run(command,cwd=cwd,env=env,capture_output=True,timeout=300)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=r.returncode,Log=str(log),Sha256=sha(log)));save()
        assert (r.returncode!=0 if negative else r.returncode==0),(name,data[-6000:].decode(errors='replace'))
        return data
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED']
    def links(product):
        return [release/product] if win else ['-L'+str(release),'-l:'+product,'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
    try:
        host=isolated/('host.exe' if win else 'host');observer=isolated/('observer.exe' if win else 'observer')
        for binary,source in ((host,'stdlib_product_fixture_host.c'),(observer,'stdlib_pruning_product_host.c')):
            run(binary.stem+'.build',[cc,*flags,'-O2','-I'+str(root/'release/include'),cases/source,*links(names[2]),'-o',binary])
        for product in products[1:]:
            for name in native+(['stdlib_codec_template_oom_host'] if a.group=='all' else []):
                binary=isolated/(product.stem+'.'+name+('.exe' if win else ''))
                run(binary.stem+'.build',[cc,*flags,'-O2','-I'+str(root),'-I'+str(root/'release'),
                    cases/(name+'.c'),*links(product.name),'-o',binary])
                run(binary.stem+'.run',[binary])
        # Extract ONLY SDK resources from the real build manifest for external GCC.
        # The isolated product above gets no SDK/source search fallback.
        sys.path.insert(0,str(root/'tools'));from gen_tcc_vfs_resources import load_resources
        bd=root/('build/tmp' if win else 'build/linux')
        manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if win else 'tcc_vfs_manifest_linux_runtime.txt')
        dist=root/'res/tcc/win64/x86_64' if win else bd/'tcc-dist'
        for resource in load_resources(manifest,root,dist,bd/'xrt.o',bd):
            if resource.virtual.startswith('include/__xl_runtime/'):
                dest=sdk/Path(resource.virtual).relative_to('include');dest.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(resource.source,dest)
        for header in (root/'release/include').glob('*.h'):shutil.copy2(header,sdk/header.name)
        shutil.copy2(root/'lib/xruntime/xruntime.h',sdk/'xruntime.h')
        cli=release/names[0];generated={};features={}
        strict=[*flags,'-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable',
            '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
            '-Wno-missing-field-initializers',
            '-I'+str(task),'-I'+str(root),'-I'+str(root/'release'),'-I'+str(sdk)]
        def positive(name):
            source=isolated/(name+'.xl');shutil.copy2(cases/source.name,source)
            run(name+'.jit',[cli,'run',source])
            work=out/(name+'.saved');work.mkdir();image=work/('program.exe' if win else 'program')
            run(name+'.aot-build',[observer,release/names[2],source,work,image])
            if name in selected:
                run(name+'.module',[host,release/names[2],source,'module',out/(name+'.module-tmp')])
                independent=relocated/(name+('.exe' if win else ''));shutil.copy2(image,independent)
                imports=run(name+'.imports',[inspect,'-p' if win else '-d',independent],relocated)
                deps=re.findall(rb'DLL Name:\s*([^\s]+)',imports) if win else re.findall(rb'Shared library:\s*\[([^\]]+)\]',imports)
                assert deps and not {d.decode().lower() for d in deps}&{n.lower() for n in names}
                if not win:assert b'(RPATH)' not in imports and b'(RUNPATH)' not in imports
                run(name+'.relocated',[independent],relocated)
            body=out/(name+'.c');repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[cli,'cgen',source,'--emit',body])
            run(name+'.repeat',[cli,'cgen',source,'--backend','xlir','--emit',repeat])
            assert body.read_bytes()==repeat.read_bytes();generated[name]=body
            text=body.read_text(encoding='utf-8')
            assert not re.search(r'xl6_xrt_(?:hex|base64|percent)_(?:encode|decode)|xl6_xrt_template_|\bxte(?:Parse|Make|CreateEngine)\b',text)
            # Generated code may declare/call resident adapters, never define them.
            assert not re.search(r'(?m)^[^;\n]*\bxl6(?:Codec|Template|Deflat|Inflat)\w*\([^;\n]*\)\s*\{',text)
            for opt in ('O0','O2'):
                binary=isolated/(name+'.'+opt+('.exe' if win else ''))
                sourceHost='stdlib_codec_template_language_oom_host.c' if name in oom else 'stdlib_codec_template_language_host.c'
                module=work/'module.c';assert module.is_file()
                run(name+'.'+opt+'.build',[cc,*strict,'-'+opt,'-DXLIR_GENERATED_MODULE="'+module.as_posix()+'"',
                    cases/sourceHost,task/'test/support/module_product_memory.c',*links(names[2]),'-o',binary])
                run(name+'.'+opt+'.run',[binary])
        for name in selected+oom:
            try:positive(name)
            except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
        def negative(name):
            target=out/(name+'.c');target.write_bytes(b'KEEP')
            data=run(name,[cli,'cgen',cases/(name+'.xl'),'--emit',target],negative=True)
            assert target.read_bytes()==b'KEEP'
            expected={
                'bad_xlir_compress_handle_owner':b'has no exact compiler-runtime bridge ABI',
                'bad_xlir_template_raw_context':b'XLIR VFS resource parameter has no exact native field borrow'
            }.get(name,b'XLIR direct runtime bridge has no exact declared ABI')
            assert expected in data,data
        for name in negatives:
            try:negative(name)
            except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
        if not win:
            planner=isolated/'planner';contract=isolated/'contract'
            observers=[(planner,'stdlib53_pruning_plan.c')]
            if a.group=='all':observers.append((contract,'xlir_codec_template_contract.c'))
            for binary,name in observers:
                run(binary.name+'.build',[cc,*flags,'-O2','-I'+str(root),'-I'+str(root/'release/include'),
                    '-I'+str(root/'lib/tcc'),cases/name,*links(names[2]),'-o',binary])
            for name in selected:
                if a.group=='all' and name in ('stdlib_codec_values','stdlib_compress_values',
                    'stdlib_template_privacy','stdlib_template_engine_values','stdlib_template_files','stdlib_native_field_mixin'):
                    run(name+'.contract',[contract,isolated/(name+'.xl')])
                if not name.endswith('_only'):continue
                plan=json.loads(run(name+'.plan',[planner,isolated/(name+'.xl')]))
                sourceNames={Path(n).name for n in plan['Sources']}
                if 'codec_' in name:
                    family='hex' if '_hex_' in name else 'base64'
                    assert 'xl6_codec_'+family+'.c' in sourceNames
                    assert not any('xl6_codec_'+other in s for s in sourceNames for other in ('hex','base64','percent') if other!=family)
                    assert not any(s.startswith(('xl6_compress_','xl6_template_')) for s in sourceNames)
                if name=='stdlib_compress_deflate_only':
                    assert 'xl6_compress_deflate_all.c' in sourceNames
                    assert not any(s.startswith('xl6_compress_inflat') for s in sourceNames)
                if name=='stdlib_template_core_only':
                    assert 'xl6_template_core.c' in sourceNames
                    assert not sourceNames&{'xl6_template_buffer.c','xl6_template_writer.c','xl6_template_file.c','xl6_template_vfs.c'}
        # Check actual XRT implementation in each standalone saved build too.
        for name in selected:
            if not name.endswith('_only'):continue
            body=out/(name+'.saved/xl6_xrt_impl.c')
            data=run(name+'.features',[cc,*flags,'-I'+str(sdk),'-E','-dM',body])
            found=set(re.findall(rb'^#define (XRT_FEATURE_[A-Z0-9_]+)\b',data,re.M));features[name]=sorted(n.decode() for n in found)
            if 'codec_' in name:assert not found&{b'XRT_FEATURE_INFLATE',b'XRT_FEATURE_DEFLATE',b'XRT_FEATURE_TEMPLATE_CORE'}
            if name=='stdlib_compress_deflate_only':assert b'XRT_FEATURE_DEFLATE' in found and b'XRT_FEATURE_INFLATE' not in found
        report['Features']=features
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged'];report['Complete']=not failures
    except Exception as error:
        report['Error']=repr(error)
    finally:
        save()
    report['FailureCount']=len(failures);save()
    print(json.dumps({k:v for k,v in report.items() if k in ('Complete','Error','FailureCount')},ensure_ascii=False),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
