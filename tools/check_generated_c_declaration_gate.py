"""Diagnostic Linux real-public JIT/external-output declaration policy.
Uses real old-Cgen/native-header missing-prototype controls, not synthetic
symbol resolvers. Object overrides do not constitute shipping/source acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--fixture',type=Path,action='append',required=True)
    args=parser.parse_args(); assert os.name!='nt'
    root,compiler,out=[getattr(args,n).resolve() for n in ('root','compiler','out')]
    assert not out.exists(); debugger=shutil.which('gdb'); assert debugger
    manifest=compiler/'overlay.linux.json'; build=json.loads(manifest.read_text())
    assert build['Complete'] and build['InputsUnchanged'] and not build['SourceProductAcceptance']
    driver,negative=Path(build['Driver']),Path(build['NegativeDriver'])
    assert sha(driver)==build['DriverSha256'] and sha(negative)==build['NegativeDriverSha256']
    project=Path(__file__).resolve().parents[1]
    positive=project/'test/cases/stdlib_generated_declaration_gate.xl'
    sidecar=project/'test/cases/stdlib_generated_declaration_gate_c.c'
    fixtures=[p.resolve() for p in args.fixture]
    files={Path(__file__).resolve(),Path(debugger),manifest,driver,negative,positive,sidecar,*fixtures}
    for row in build['Inputs']+build['Objects']:
        assert sha(row['Path'])==row['Sha256']; files.add(Path(row['Path']))
    files.update(root/'release'/n for n in ('libxl.so','libxlc.so'))
    files.update(p for p in (compiler/'include').rglob('*') if p.is_file())
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
    include=['--inc-path',compiler/'include']
    try:
        for fixture in fixtures:
            run(fixture.stem+'.positive.jit',[driver,'run',fixture,*include])
            for mode in ('run','build'):
                target=out/(fixture.stem+'.negative-artifact')
                target.write_bytes(b'KEEP')
                code,text=run(fixture.stem+'.negative.'+mode,
                    [negative,mode,fixture,*(['-o',target] if mode=='build' else []),*include],False)
                assert code==1 and 'error: implicit declaration of function' in text and 'xl6SequenceTreeCreateTakeV1' in text
                assert target.read_bytes()==b'KEEP'
                controls.append({'Fixture':str(fixture),'Mode':mode,'CompilationRefused':True,'OutputRetained':True})
            gdb=out/(fixture.stem+'.gdb')
            gdb.write_text('set pagination off\nset confirm off\nset breakpoint pending on\n'
                'handle SIGSEGV stop print nopass\nbreak xl6SequenceTreeCreateTakeV1\ncommands\nsilent\n'
                'printf "UNSAFE_FACTORY_EXECUTION\\n"\nquit 2\nend\n'
                f'run run {fixture} --inc-path {compiler/"include"}\nquit 0\n',encoding='utf-8')
            _,text=run(fixture.stem+'.negative.execution-guard',[debugger,'-batch','-x',gdb,negative])
            assert 'error: implicit declaration of function' in text and 'UNSAFE_FACTORY_EXECUTION' not in text and 'SIGSEGV' not in text
        run('legacy-c.positive.jit',[driver,'run',positive,sidecar,*include])
        target=out/'legacy-c.positive'
        run('legacy-c.positive.external',[driver,'build',positive,sidecar,'-o',target,*include])
        run('legacy-c.positive.execute',[target])
        report['UserCImplicitForwardAccepted']=True
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['SharedJitExternalDeclarationGateAcceptance']=True; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print(f'Generated declaration gate: {len(rows)} phases; JIT/external refusal before native execution, legacy C policy restored',flush=True)
if __name__=='__main__': main()
