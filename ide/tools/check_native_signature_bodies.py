"""Compare actual native signature bodies with original full analysis at scale.

Reuse the built native core, real compiler/link commands and the same C
differential fixture. This is correctness/ownership/budget coverage, not timing.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
from build_lsp_core import dependencies


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-build',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--helpers',type=int,nargs='+',default=[32,512])
    args=parser.parse_args()
    if not args.helpers or any(n<1 or n>512 for n in args.helpers) or len(set(args.helpers))!=len(args.helpers):
        parser.error('helper counts must be unique values in1..512')
    native=args.native_build.resolve(strict=True);output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    source=Path(__file__).resolve().parents[1];fixture=source/'tests/analysis_raw_signature_test.c'
    build_path=native/'build-result.json';build=json.loads(build_path.read_text(encoding='utf-8-sig'))
    assert build['status']=='passed','A completed native build with tests is required'
    commands=build.get('requestedCommands',build['commands']);name='analysis_raw_signature_test'
    compile_command=next(c for c in commands if '-c' in c and Path(c[c.index('-o')+1]).stem==name)
    link_command=next(c for c in commands if '-c' not in c and '-o' in c and Path(c[c.index('-o')+1]).stem==name)
    core=native/'xlang-analysis-core.a';assert sha(core)==build['outputs'][core.name]
    protected=dict(build['inputs']);protected.update({str(core):sha(core),str(build_path):sha(build_path),str(fixture):sha(fixture),str(Path(__file__)):sha(__file__)})
    calls=[];cases=[];report=dict(status='failed',scope=__doc__,commands=calls,cases=cases,protected=protected)
    env=dict(os.environ);env['PATH']=str(Path(compile_command[0]).parent)+os.pathsep+env.get('PATH','')
    def run(label,command):
        cp=subprocess.run(command,cwd=output,env=env,capture_output=True,timeout=240)
        log=output/(label+'.log');log.write_bytes(cp.stdout+cp.stderr)
        calls.append(dict(name=label,command=command,exit=cp.returncode,log=str(log),sha256=sha(log)))
        print(label,cp.returncode,(cp.stdout+cp.stderr).decode('utf-8','replace')[-1800:],flush=True)
        assert cp.returncode==0,label
    try:
        assert all(sha(p)==h for p,h in protected.items())
        for count in args.helpers:
            folder=output/str(count);folder.mkdir();obj=folder/'fixture.o';dep=folder/'fixture.d'
            program=folder/('fixture.exe' if os.name=='nt' else 'fixture')
            command=list(compile_command)
            for option in ['-MF','-MT','-MQ']:
                while option in command:
                    at=command.index(option);del command[at:at+2]
            command=[v for v in command if v not in ['-MMD','-MD']]
            command[command.index('-c')+1]=str(fixture);command[command.index('-o')+1]=str(obj)
            command[1:1]=['-I'+str(source/'tests'),'-MMD','-MT','xlsObject','-MF',str(dep),f'-DXLS_RAW_SIGNATURE_HELPERS={count}u']
            run(f'{count}-compile',command)
            inputs={str(p):sha(p) for p in dependencies(dep.read_text(encoding='utf-8'))}
            ast_headers={p for p in inputs if Path(p).name=='xl6_ast.h'}
            assert ast_headers=={row['header'] for row in build['privateAstConsumers']}
            command=[str(obj) if Path(v).name==name+'.o' else str(core) if Path(v).name==core.name else v for v in link_command]
            command[command.index('-o')+1]=str(program);run(f'{count}-link',command);run(f'{count}-verify',[str(program)])
            assert all(sha(p)==h for p,h in inputs.items());protected.update(inputs)
            cases.append(dict(helpers=count,passed=True,program=str(program),sha256=sha(program),dependency=str(dep),dependencySha256=sha(dep)))
        assert len(cases)==len(args.helpers) and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        (output/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    main()
