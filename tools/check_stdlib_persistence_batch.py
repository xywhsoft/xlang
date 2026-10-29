"""Complete declared persistence regression batch against pinned compiled sources.
Collect every independent failure; no release compiler fallback. Each language
fixture has strict C O0/O2, true JIT/AOT, complete synchronous OOM prefixes and
an isolated file root. Graph/field mutation gates remain independent proofs.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,subprocess,sys
from xlir_public_build_inputs import verify_public_build
FIXTURES=('abi','project','readers','configured','sinks','files','writers','visitors',
    'nominal','pretty','pretty-prune','restore','restore-domains','restore-construction','root-default','native-identity')
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,public,out=args.root.resolve(),args.public_directory.resolve(),args.out.resolve()
    assert not out.exists(); build,pinned,compared=verify_public_build(root,public)
    files={Path(__file__),Path(sys.executable),*pinned}
    scripts=('check_persistence_project_language.py','check_persistence_project_plans.py',
        'check_persistence_restore_graph_language.py','xlir_persistence_abi_checks.py')
    files.update(root/'tools'/name for name in scripts)
    files.update(p for p in (root/'test/cases').rglob('*') if p.suffix in ('.xl','.c','.h','.inc'))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir(parents=True); rows=[]; errors=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Errors=errors,
        SourceInputsMatch=True,ProductionFiles=len(compared),Full53Acceptance=False,ShippingArtifact=False,
        Scope=__doc__,Fixtures=list(FIXTURES),WaitQueueComplete=False)
    entries=[(name,'check_persistence_project_language.py',['--fixture',name],'report.json') for name in FIXTURES]
    entries += [('plans','check_persistence_project_plans.py',[],'results.json'),
        ('restore-graph','check_persistence_restore_graph_language.py',['--native-cutover','--oom'],'results.json')]
    def execute(entry):
        name,script,extra,report_file=entry; target=out/name
        command=[sys.executable,str(root/'tools'/script),'--root',str(root),'--public-directory',str(public),
            '--out',str(target),*extra]
        try:
            process=subprocess.run(command,cwd=root,capture_output=True,timeout=1800)
            log=out/(name+'.log'); log.write_bytes(process.stdout+process.stderr)
            result=json.loads((target/report_file).read_text(encoding='utf-8-sig'))
            complete=process.returncode==0 and result['Complete'] and result['InputsUnchanged'] and result['SourceInputsMatch']
            row=dict(Name=name,Command=command,Exit=process.returncode,Complete=complete,Report=str(target/report_file),
                ReportSha256=sha(target/report_file),Log=str(log),LogSha256=sha(log),Phases=len(result['Rows']))
            if not complete: row['Failure']=result.get('Error',result.get('Errors','Incomplete child evidence'))
        except Exception as error: row=dict(Name=name,Command=command,Complete=False,Failure=repr(error))
        print(name+': '+str(row['Complete']),flush=True)
        return row
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for row in pool.map(execute,entries):
            rows.append(row)
            if not row['Complete']: errors.append(dict(Name=row['Name'],Failure=row['Failure']))
            (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
    report['Complete']=not errors and report['InputsUnchanged'] and len(rows)==len(entries)
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
