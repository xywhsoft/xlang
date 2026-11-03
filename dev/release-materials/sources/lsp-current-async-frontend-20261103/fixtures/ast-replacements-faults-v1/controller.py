"""Exercise actual native AST replacement ownership and four producer omissions.

Compile private semantic producer copies with the identical audited AST layout.
Every omission must reach an invalid XRT pool release in the real checker.
The normal core, root runtime and compiler source assets remain untouched.
"""
from pathlib import Path
import argparse,json,subprocess
from build_lsp_lifecycle_profile import verify_build,sha
from check_native_bridge_catalog import function


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();B=a.build.resolve(strict=True);O=a.output.resolve();O.mkdir(parents=True,exist_ok=False)
    normal=json.loads((B/'build-result.json').read_text(encoding='utf-8-sig'));verify_build(normal,B)
    protected={**normal['inputs'],str(B/'build-result.json'):sha(B/'build-result.json'),str(Path(__file__).resolve()):sha(__file__)}
    report=dict(status='running',scope=__doc__,protected=protected,commands=[],cases=[])
    def save():(O/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,reject=False):
        result=subprocess.run(command,cwd=O,capture_output=True,timeout=240)
        stdout,stderr=O/(name+'.stdout.log'),O/(name+'.stderr.log');stdout.write_bytes(result.stdout);stderr.write_bytes(result.stderr)
        report['commands'].append(dict(name=name,command=command,exit=result.returncode,stdout=str(stdout),stdoutSha256=sha(stdout),stderr=str(stderr),stderrSha256=sha(stderr)));save()
        if reject:assert result.returncode==1 and b'actual invalid pool release' in result.stderr,(name,result.returncode,result.stderr[-2000:])
        else:assert result.returncode==0,(name,result.returncode,result.stderr[-2000:])
    try:
        run('normal',[str(B/'analysis_ast_replacement_test.exe')]);report['cases'].append(dict(name='normal',status='passed'));save()
        sem=B/'frontend/xl6_sem.c';effects=B/'frontend/xl6_sem_c_owned.effects.c'
        for name,producer,old in (
            ('callable','xl6SemCOwnedAdaptCallableExpr','old'),
            ('nullable','xl6SemCOwnedAdaptNullableCallableExpr','old'),
            ('generic','xl6SemCOwnedBindAsyncGeneric','previous'),
            ('contextual','xl6SemBindContextualMethod','old')):
            folder=O/name;folder.mkdir();texts={sem.name:sem.read_text(encoding='utf8'),effects.name:effects.read_text(encoding='utf8')}
            selected=sem.name if name=='contextual' else effects.name
            original=function(texts[selected],producer);seam=f'xlsAstRestoreReplacementOwner(expr,{old});'
            assert original.count(seam)==1,(name,'producer seam changed')
            texts[selected]=texts[selected].replace(original,original.replace(seam,'',1),1)
            for filename,text in texts.items():
                target=folder/filename;target.write_text(text,encoding='utf8',newline='\n');protected[str(target)]=sha(target)
            source=folder/sem.name;obj=folder/'sem.o';program=folder/'fixture.exe'
            command=next(c[:] for c in normal['commands'] if '-c' in c and str(sem) in c);command[command.index(str(sem))]=str(source);command[-1]=str(obj)
            run(name+'-compile',command)
            command=next(c[:] for c in normal['commands'] if c[-1]==str(B/'analysis_ast_replacement_test.exe'))
            command[2:2]=[str(obj)];command[-1]=str(program);run(name+'-link',command);run(name+'-run',[str(program)],reject=True)
            report['cases'].append(dict(name=name,status='actual-producer-omission-rejected',producer=producer));save()
        verify_build(normal,B)
        for path,digest in protected.items():assert sha(path)==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),commands=len(report['commands']),report=str(O/'result.json'))))


if __name__=='__main__':main()
