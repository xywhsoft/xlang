"""Diagnostic Windows public JIT/module/external-output declaration policy.
The negative control links the real prior Cgen and stages the real prior private
header. Explicit object/header overrides are not rebuilt source/shipping proof.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--fixture',type=Path,action='append',required=True)
    args=parser.parse_args(); assert os.name=='nt'
    root,compiler,out=[getattr(args,n).resolve() for n in ('root','compiler','out')]
    assert not out.exists(); debugger=shutil.which('gdb'); assert debugger
    manifest=compiler/'overlay.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and not build['SourceProductAcceptance']
    driver,negative=Path(build['Driver']),Path(build['NegativeDriver'])
    assert sha(driver)==build['DriverSha256'].lower() and sha(negative)==build['NegativeDriverSha256'].lower()
    project=Path(__file__).resolve().parents[1]
    positive=project/'test/cases/stdlib_generated_declaration_gate.xl'
    sidecar=project/'test/cases/stdlib_generated_declaration_gate_c.c'
    library=project/'test/cases/stdlib_generated_declaration_gate_library.xl'
    fixtures=[p.resolve() for p in args.fixture]
    files={Path(__file__).resolve(),Path(debugger),manifest,driver,negative,positive,sidecar,library,*fixtures,root/'release/xl.dll'}
    for row in build['Inputs']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    for folder in (compiler/'include',negative.parent/'include'):
        files.update(p for p in folder.rglob('*') if p.is_file())
    inputs=[{'Path':str(p),'Sha256':sha(p)} for p in sorted(files)]
    out.mkdir(parents=True); rows=[]; controls=[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'SourceProductAcceptance':False,
        'Full53Acceptance':False,'ShippingLibraryAcceptance':False,
        'SharedJitExternalDeclarationGateAcceptance':False,'Inputs':inputs,'Rows':rows,'Controls':controls}
    def save(): (out/'generated.declaration.gate.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,success=True):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start}); save()
        if success and result.returncode: raise RuntimeError(name+': '+data[-6000:].decode(errors='replace'))
        return result.returncode,data.decode(errors='replace')
    try:
        for fixture in fixtures:
            run(fixture.stem+'.positive.jit',[driver,fixture,'run',out/(fixture.stem+'.positive')])
            for mode in ('run','module','exe'):
                prefix=out/(fixture.stem+'.negative.'+mode)
                target=Path(str(prefix)+('.'+mode if mode in ('exe','dll') else '.artifact'))
                target.write_bytes(b'KEEP')
                code,text=run(fixture.stem+'.negative.'+mode,[negative,fixture,mode,prefix],False)
                assert code==1 and 'error: implicit declaration of function' in text and 'xl6SequenceTreeCreateTakeV1' in text
                assert target.read_bytes()==b'KEEP'
                controls.append({'Fixture':str(fixture),'Mode':mode,'CompilationRefused':True,'OutputRetained':True})
            gdb=out/(fixture.stem+'.gdb')
            gdb.write_text('set pagination off\nset confirm off\nset breakpoint pending on\n'
                'handle SIGSEGV stop print nopass\nbreak xl6SequenceTreeCreateTakeV1\ncommands\nsilent\n'
                'printf "UNSAFE_FACTORY_EXECUTION\\n"\nquit 2\nend\n'
                f'run {fixture.as_posix()} run {(out/(fixture.stem+".guard")).as_posix()}\nquit 0\n',encoding='utf-8')
            _,text=run(fixture.stem+'.negative.execution-guard',[debugger,'-batch','-x',gdb,negative])
            assert 'error: implicit declaration of function' in text and 'UNSAFE_FACTORY_EXECUTION' not in text and 'SIGSEGV' not in text
        prefix=out/'library.negative'
        target=Path(str(prefix)+'.dll'); target.write_bytes(b'KEEP')
        code,text=run('library.negative.dll',[negative,library,'dll',prefix],False)
        assert code==1 and 'error: implicit declaration of function' in text and 'xl6SequenceTreeCreateTakeV1' in text
        assert target.read_bytes()==b'KEEP'
        controls.append({'Fixture':str(library),'Mode':'dll','CompilationRefused':True,'OutputRetained':True})
        run('legacy-c.positive.jit',[driver,positive,'run',out/'legacy-c.jit',sidecar])
        prefix=out/'legacy-c.positive'
        run('legacy-c.positive.external',[driver,positive,'exe',prefix,sidecar])
        run('legacy-c.positive.execute',[str(prefix)+'.exe'])
        report['UserCImplicitForwardAccepted']=True
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['SharedJitExternalDeclarationGateAcceptance']=True; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print(f'Windows generated declaration gate: {len(rows)} phases; JIT/module/exe/dll refusal before native execution, C policy restored',flush=True)
if __name__=='__main__': main()
