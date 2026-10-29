"""Normative independent AOT graph-exit gate, not a known-failure waiver.

Reuses unchanged Stack/Tree/Queue graph fixture bodies and counts their actual
destructors at process exit. A real acyclic destructor calibrates the observer.
Cycles that are not retired fail the gate; scanning or successful compilation
cannot certify retirement. No collector, adapter or provider is substituted.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys

CASES=(('control','stdlib_aot_graph_control',1),('stack','stdlib_stack_ownership',1),
       ('tree','stdlib_tree_ownership',5),('queue','stdlib_concurrent_queue_ownership',6))

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists()
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    windows=os.name=='nt';inspector=shutil.which('objdump' if windows else 'readelf');assert inspector
    task=Path(__file__).resolve().parents[1]
    products=[root/'release'/name for name in (('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so'))]
    files={Path(__file__).resolve(),Path(sys.executable).resolve(),Path(inspector).resolve(),args.build_report.resolve(),
           task/'test/support/aot_graph_probe.c',*[task/'test/cases'/(stem+'.xl') for _,stem,_ in CASES]}
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded[path]==sha(path) for path in products)
    for row in build['Inputs']+build['Products']:assert sha(row['Path'])==row['Sha256'];files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True);isolated=out/'compiler';(isolated/'release').mkdir(parents=True)
    relocated=out/'relocated';relocated.mkdir();tmp=out/'tmp';tmp.mkdir()
    for product in products:
        shutil.copy2(product,isolated/'release'/product.name)
        if windows:shutil.copy2(product,isolated/product.name)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[];cases=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Cases=cases,Failures=failures,
        Scope=__doc__,ExplicitSubset=True,AotGraphRetirementAcceptance=False,ProviderOverrides=False,
        EmbeddedVfsWithoutFallback=True,FieldAssertionsPreserved=True,Full53Acceptance=False,RuntimeInstrumented=False)
    def run(name,command,directory=isolated):
        try:
            process=subprocess.run(list(map(str,command)),cwd=directory,env=env,capture_output=True,timeout=180)
            data,code=process.stdout+process.stderr,process.returncode
        except subprocess.TimeoutExpired as error:data,code=(error.stdout or b'')+(error.stderr or b''),'timeout'
        log=out/(name+'.log');log.write_bytes(data)
        row=dict(Name=name,Command=list(map(str,command)),Exit=code,ExpectedExit=0,
            Directory=str(directory),Log=str(log),LogSha256=sha(log));rows.append(row)
        return row,data
    cli=isolated/'release'/products[0].name
    probe=(task/'test/support/aot_graph_probe.c').read_bytes()
    prefix=b'extern "graph_probe_begin" func beginGraphProbe() as bool\nassert(beginGraphProbe())\n'
    for name,stem,count in CASES:
        original=task/'test/cases'/(stem+'.xl');body=original.read_bytes();directory=isolated/name;directory.mkdir()
        source=directory/(name+'.xl');content=body if name=='control' else prefix+body;source.write_bytes(content)
        assert content==body if name=='control' else content[len(prefix):]==body
        support=directory/'probe_support.c';support.write_bytes(('#define GRAPH_EXPECTED '+str(count)+'\n'+('#define GRAPH_CONTROL\n' if name=='control' else '')).encode()+probe)
        image=directory/(name+('.exe' if windows else '.aot'));record=dict(Name=name,OriginalFixture=str(original),FixtureSha256=sha(original),
            ExpectedFinalizers=count,ExpectedMask=(1<<count)-1,Rows=[],Passed=False,IndependentAot=False)
        begin=len(rows)
        try:
            row,data=run(name+'.build',[cli,'build',source,support,'-o',image]);assert row['Exit']==0,data[-3000:]
            independent=relocated/image.name;shutil.copy2(image,independent)
            row,data=run(name+'.imports',[inspector,'-p' if windows else '-d',independent],relocated);assert row['Exit']==0
            deps=re.findall(rb'DLL Name:\s*([^\s]+)',data) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]',data)
            assert deps and not {item.decode().lower() for item in deps}&{product.name.lower() for product in products}
            if not windows:assert b'(RPATH)' not in data and b'(RUNPATH)' not in data
            record.update(IndependentAot=True,Dependencies=[item.decode() for item in deps])
            row,data=run(name+'.run',[independent],relocated)
            match=re.search(rb'standalone graph finalizers=(\d+) expected=(\d+) mask=(\d+)',data);assert match
            actual,expected,mask=map(int,match.groups());assert expected==count
            record.update(ActualFinalizers=actual,ActualMask=mask)
            assert row['Exit']==0 and actual==count and mask==(1<<count)-1,(row['Exit'],actual,count,mask)
            record['Passed']=True
        except Exception as error:failures.append(dict(Name=name,Error=repr(error)))
        record['Rows']=rows[begin:];cases.append(record);print(name+': '+str(record['Passed']),flush=True)
    assert not any((isolated/name).exists() for name in ('src','lib','res','include'))
    report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
    report['ObserverControlPassed']=cases[0]['Passed']
    report['Complete']=report['InputsUnchanged'] and not failures and len(rows)==12 and all(case['Passed'] for case in cases)
    report['AotGraphRetirementAcceptance']=report['Complete']
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
