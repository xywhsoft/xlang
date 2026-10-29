"""Raw ChaCha20 current-source ABI/IR/JIT/module/AOT/pruning acceptance on Windows.

Only fixed public vectors and ordinary argument/lifecycle checks. This source
cohort gate is not shipping-product or cryptographic-security acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public','out'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();root=a.root.resolve();public=a.public.resolve();out=a.out.resolve()
    assert os.name=='nt' and not out.exists();controls=Path(__file__).resolve().parents[1]
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build.get('RuntimeOnly',False)
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc')).resolve();objdump=Path(shutil.which('objdump')).resolve()
    ps=Path(os.environ['WINDIR'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    cases=controls/'test/cases';selected=('stdlib_crypto_chacha20_values','stdlib_crypto_chacha20_only')
    negative=cases/'bad_stdlib_crypto_chacha20_legacy.xl'
    inputs=[Path(__file__).resolve(),public/'build.json',driver,cc,objdump,ps,negative,
        root/'tools/build_xlir_validation_driver.ps1',cases/'xlir_codec_template_contract.c',cases/'stdlib53_pruning_plan.c']
    inputs += [cases/(name+'.xl') for name in selected]
    out.mkdir(parents=True);rows=[];r=dict(Complete=False,Inputs={str(p):sha(p) for p in inputs},Rows=rows,
        Scope=__doc__,ShippingAccepted=False,FullGoalComplete=False)
    env=dict(os.environ,PSModulePath=str(ps.parent/'Modules'))
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args,cwd=root,expected_failure=False):
        args=list(map(str,args));log=out/(name+'.log')
        with log.open('wb') as stream:p=subprocess.run(args,cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=360)
        rows.append(dict(Name=name,Command=args,Exit=p.returncode,Log=str(log),Sha256=sha(log)));save()
        print(name,p.returncode,flush=True)
        assert (p.returncode!=0 if expected_failure else p.returncode==0),str(log)
        return log.read_text(encoding='utf8',errors='replace')
    try:
        binaries={}
        for source in ('xlir_codec_template_contract','stdlib53_pruning_plan'):
            binary=out/(source+'.exe');binaries[source]=binary
            run(source+'.build',[ps,'-NoProfile','-ExecutionPolicy','Bypass','-File',
                root/'tools/build_xlir_validation_driver.ps1','-Driver',cases/(source+'.c'),
                '-Output',binary,'-PublicDirectory',public])
        relocated=out/'relocated';relocated.mkdir()
        for name in selected:
            case=cases/(name+'.xl');prefix=out/name
            for mode in ('emit','run','module','exe'):
                run(name+'.'+mode,[driver,case,mode,prefix])
            generated=Path(str(prefix)+'.c');text=generated.read_text(encoding='utf8')
            assert 'xl6CryptoChaCha20' in text and 'xl6_xrt_chacha20' not in text
            assert not re.search(r'(?m)^[^;\n]*\bxl6Crypto\w*\([^;\n]*\)\s*\{',text)
            for opt in ('O0','O2'):
                run(name+'.syntax-'+opt,[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,'-fsyntax-only',
                    '-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable','-Wno-unused-parameter',
                    '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
                    '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),
                    '-I'+str(public/'include'),'-I'+str(root/'lib/xruntime'),generated])
            image=relocated/(name+'.exe');shutil.copy2(Path(str(prefix)+'.exe'),image)
            imports=run(name+'.imports',[objdump,'-p',image],cwd=relocated)
            assert not re.search(r'DLL Name:\s*(?:xl|xlc)\.dll',imports,re.I)
            run(name+'.standalone',[image],cwd=relocated)
            run(name+'.ir-contract',[binaries['xlir_codec_template_contract'],case])
            plan=json.loads(run(name+'.plan',[binaries['stdlib53_pruning_plan'],case]))
            if name.endswith('_only'):
                sources={Path(p).name for p in plan['Sources']}
                assert 'xl6_crypto_chacha20.c' in sources
                assert not any(p.startswith('xl6_crypto_') and p!='xl6_crypto_chacha20.c' for p in sources)
                assert not any(p.startswith(('xl6_tls_','xl6_x509_','xl6_der_','xl6_pem_')) for p in sources)
        diagnostic=run('legacy-name-rejected',[driver,negative,'emit',out/'legacy'],expected_failure=True)
        assert 'public source compile failed:' in diagnostic and negative.name in diagnostic
        assert '[stdlib] standard library namespace has no such symbol' in diagnostic
        assert 'unresolved reference' not in diagnostic
        assert not (out/'legacy.c').exists() and not (out/'legacy.h').exists()
        r['InputsUnchanged']=all(sha(p)==h for p,h in r['Inputs'].items())
        r['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert r['InputsUnchanged'] and r['LogsUnchanged'];r['Complete']=True
    except Exception as error:r['Error']=repr(error);raise
    finally:
        r['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in out.rglob('*') if p.is_file() and p.name!='results.json'];save()
    return 0

if __name__=='__main__':raise SystemExit(main())
