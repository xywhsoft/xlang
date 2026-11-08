"""Verify real native completion visibility/signature producer boundaries.

Normal native query/session facts must pass. Each private C omission or blanket
prefix filter must fail the expected public completion contract. Never install
mutants in the server or root release; preserve the complete normal build.
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
        out,err=O/(name+'.stdout.log'),O/(name+'.stderr.log');out.write_bytes(result.stdout);err.write_bytes(result.stderr)
        report['commands'].append(dict(name=name,command=command,exit=result.returncode,stdout=str(out),stdoutSha256=sha(out),stderr=str(err),stderrSha256=sha(err)));save()
        if reject:assert result.returncode==1 and b'analysis_completion_visibility_test:' in result.stderr,(name,result.returncode,result.stderr[-2000:])
        else:assert result.returncode==0,(name,result.returncode,result.stderr[-2000:])
    try:
        target='analysis_completion_visibility_test';run('normal',[str(B/(target+'.exe'))]);report['cases'].append(dict(name='normal',status='passed'));save()
        sem=B/'frontend/xl6_sem.c';effects=B/'frontend/xl6_sem_c_owned.effects.c';source=sem.read_text(encoding='utf8')
        global_filter='     if (func->flags & XL6_AST_FLAG_C_OWNED_SPECIALIZATION) continue;'
        member_filter='   if (callable && xl6SemAstHasFlag(member,XL6_AST_FLAG_C_OWNED_SPECIALIZATION)) continue;'
        for name,producer,before,after,mode in (
            ('omit-global-filter','xl6SemCompletionNames',global_filter,'','--global'),
            ('omit-member-filter','xl6SemCompletionMembers',member_filter,'','--member'),
            ('blanket-global-prefix','xl6SemCompletionNames',global_filter,'     if (!strncmp(xl6SemFuncName(func),"__xl_c_owned_async_",19)) continue;','--global'),
            ('blanket-member-prefix','xl6SemCompletionMembers',member_filter,'   if (callable && !strncmp(xl6SemMemberName(member),"__xl_c_owned_async_",19)) continue;','--member'),
            ('omit-global-async-result','xl6SemCompletionNames','type=func->kind==XL6_AST_ASYNC_FUNC ?','type=0 ?','--signature'),
            ('omit-member-async-result','xl6SemCompletionMembers','type=member->kind==XL6_AST_ASYNC_FUNC ?','type=0 ?','--member')):
            folder=O/name;folder.mkdir();original=function(source,producer);assert original.count(before)==1,(name,'producer seam changed')
            altered=source.replace(original,original.replace(before,after,1),1)
            private=folder/sem.name;private.write_text(altered,encoding='utf8',newline='\n');private_effects=folder/effects.name;private_effects.write_bytes(effects.read_bytes())
            for path in (private,private_effects):protected[str(path)]=sha(path)
            obj=folder/'sem.o';program=folder/'fixture.exe'
            command=next(c[:] for c in normal['commands'] if '-c' in c and str(sem) in c);command[command.index(str(sem))]=str(private);command[-1]=str(obj);run(name+'-compile',command)
            command=next(c[:] for c in normal['commands'] if c[-1]==str(B/(target+'.exe')));command[2:2]=[str(obj)];command[-1]=str(program);run(name+'-link',command);run(name+'-run',[str(program),mode],reject=True)
            report['cases'].append(dict(name=name,status='actual-native-producer-defect-rejected',producer=producer));save()
        verify_build(normal,B)
        for path,digest in protected.items():assert sha(path)==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),commands=len(report['commands']),report=str(O/'result.json'))))


if __name__=='__main__':main()
