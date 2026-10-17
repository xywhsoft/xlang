"""Actual-product crypto and ASN.1/PEM batch acceptance. Not whole crypto/TLS or RC0.
Uses fresh CLI/DLL/SO, real module relocation, standalone products, strict C and native OOM.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--group',choices=('all','crypto'),default='all')
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve()
    assert not out.exists()
    task=Path(__file__).resolve().parents[1];cases=task/'test/cases';win=os.name=='nt'
    cc,nm,inspect=[shutil.which(n) for n in ('gcc','nm','objdump' if win else 'readelf')]
    assert cc and nm and inspect
    names=('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so')
    products=[root/'release'/n for n in names];assert all(f.is_file() for f in products)
    selected=['stdlib_crypto_digest_values','stdlib_crypto_kdf_values','stdlib_crypto_aead_values',
        'stdlib_crypto_sha256_only']
    if a.group=='all': selected+=['stdlib_asn1_pem_values','stdlib_crypto_asn1_threads']
    negatives=['bad_xlir_crypto_no_raise','bad_xlir_crypto_material']
    negatives+=['bad_stdlib_crypto_private_handle']
    if a.group=='all':negatives+=['bad_stdlib_der_private_handle','bad_stdlib_pem_private_handle']
    native=['stdlib_crypto_values_oom_host']
    if a.group=='all':native+=['stdlib_asn1_values_oom_host']
    helpers=['stdlib_product_fixture_host','stdlib_pruning_product_host','stdlib53_pruning_plan',
        'stdlib_codec_template_language_host','xlir_codec_template_contract']
    files={Path(__file__).resolve(),*products,*[cases/(n+'.xl') for n in selected+negatives],
        *[cases/(n+'.c') for n in native+helpers],task/'test/support/module_product_memory.c',
        task/'test/support/module_product_memory.h'}
    for folder in ('src/runtime','src/ir','src/stdlib','src/compile'):
        files.update(f for f in (root/folder).rglob('*') if f.is_file())
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();isolated=out/'isolated';release=isolated/'release';release.mkdir(parents=True)
    relocated=out/'relocated';relocated.mkdir();sdk=out/'sdk';sdk.mkdir()
    rows=[];failures=[];report=dict(Complete=False,Scope=__doc__,Group=a.group,ExplicitSubset=True,Failures=failures,
        FullRC0Acceptance=False,FullCryptoTlsAcceptance=False,Inputs=inputs,Rows=rows)
    for product in products:
        shutil.copy2(product,release/product.name)
        if win and product.suffix=='.dll':shutil.copy2(product,isolated/product.name)
    env={k.upper():v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
        ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
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
            for name in native:
                caseId=product.stem+'.'+name
                binary=isolated/(product.stem+'.'+name+('.exe' if win else ''))
                run(caseId+'.build',[cc,*flags,'-O2','-I'+str(root),'-I'+str(root/'release'),
                    cases/(name+'.c'),*links(product.name),'-o',binary])
                run(caseId+'.run',[binary])
        expected=set()
        for family in ('crypto','asn1'):
            exported=set(re.findall(r'XL6_TEXT_EXPORT\((\w+),',
                (root/f'src/runtime/xl6_{family}_symbols.inc').read_text()))
            declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(\w+)\(',
                (root/f'src/runtime/xl6_{family}_abi.h').read_text()))
            contracted=set(re.findall(r'(?m)^\s*\{"([^"]+)"',
                (root/f'src/ir/xl6_ir_{family}_contracts.inc').read_text()))
            assert exported==declared==contracted,(family,'ABI/contract/export drift')
            expected|=exported
        report['DeclaredResidentEntries']=len(expected)
        for product in products[1:]:
            content=run(product.stem+'.exports',[inspect,'-p',product] if win else [nm,'-D','--defined-only',product])
            actual={v.decode() for v in re.findall(rb'\bxl6(?:Crypto|Der|Pem)\w+',content)}
            assert expected<=actual,('missing actual exports',product.name,sorted(expected-actual))
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
            assert not re.search(r'xlir_crypto_text_hash|xl6_xrt_(?:sha1|sha256|sha384|sha512|hmac_sha|hkdf_sha)',text)
            # Generated code may declare/call resident adapters, never define them.
            assert not re.search(r'(?m)^[^;\n]*\bxl6(?:Crypto|Der|Pem)\w*\([^;\n]*\)\s*\{',text)
            for opt in ('O0','O2'):
                binary=isolated/(name+'.'+opt+('.exe' if win else ''))
                sourceHost='stdlib_codec_template_language_host.c'
                module=work/'module.c';assert module.is_file()
                run(name+'.'+opt+'.build',[cc,*strict,'-'+opt,'-DXLIR_GENERATED_MODULE="'+module.as_posix()+'"',
                    cases/sourceHost,task/'test/support/module_product_memory.c',*links(names[2]),'-o',binary])
                run(name+'.'+opt+'.run',[binary])
        for name in selected:
            try:positive(name)
            except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
        def negative(name):
            target=out/(name+'.c');target.write_bytes(b'KEEP')
            data=run(name,[cli,'cgen',cases/(name+'.xl'),'--emit',target],negative=True)
            assert target.read_bytes()==b'KEEP'
            expected=b'private' if '_private_handle' in name else b'XLIR direct runtime bridge has no exact declared ABI'
            assert expected in data,data
        for name in negatives:
            try:negative(name)
            except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
        if not win:
            planner=isolated/'planner';contract=isolated/'contract'
            observers=[(planner,'stdlib53_pruning_plan.c'),(contract,'xlir_codec_template_contract.c')]
            # Both observers link the real compiler SO, never compiler sources.
            for binary,name in observers:
                run(binary.name+'.build',[cc,*flags,'-O2','-I'+str(root),'-I'+str(root/'release/include'),
                    '-I'+str(root/'lib/tcc'),cases/name,*links(names[2]),'-o',binary])
            for name in selected:
                run(name+'.contract',[contract,isolated/(name+'.xl')])
                if not name.endswith('_only'):continue
                plan=json.loads(run(name+'.plan',[planner,isolated/(name+'.xl')]))
                sourceNames={Path(n).name for n in plan['Sources']}
                if name=='stdlib_crypto_sha256_only':
                    assert 'xl6_crypto_sha256.c' in sourceNames
                    assert not any(s.startswith('xl6_crypto_') and s!='xl6_crypto_sha256.c' for s in sourceNames)
                    assert not any(s.startswith(('xl6_der_', 'xl6_pem_')) for s in sourceNames)
        # Check actual XRT implementation in each standalone saved build too.
        for name in selected:
            if not name.endswith('_only'):continue
            body=out/(name+'.saved/xl6_xrt_impl.c')
            data=run(name+'.features',[cc,*flags,'-I'+str(sdk),'-E','-dM',body])
            found=set(re.findall(rb'^#define (XRT_FEATURE_[A-Z0-9_]+)\b',data,re.M));features[name]=sorted(n.decode() for n in found)
            if name=='stdlib_crypto_sha256_only':
                assert b'XRT_FEATURE_CRYPTO_SHA256' in found
                assert not found&{b'XRT_FEATURE_CRYPTO_SHA1', b'XRT_FEATURE_CRYPTO_MD5',b'XRT_FEATURE_CRYPTO_SHA512',b'XRT_FEATURE_TLS',b'XRT_FEATURE_CRYPTO_AES_GCM'}
        report['Features']=features
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert report['LogsUnchanged'],'Output log IDs must not collide'
        assert report['InputsUnchanged'];report['Complete']=not failures
    except Exception as error:
        report['Error']=repr(error)
    finally:
        save()
    report['FailureCount']=len(failures);save()
    print(json.dumps({k:v for k,v in report.items() if k in ('Complete','Error','FailureCount')},ensure_ascii=False),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
