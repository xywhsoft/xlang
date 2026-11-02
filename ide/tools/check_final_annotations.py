"""Validate native final-phase omissions and real isolated allocation refusals.

Matching complete native/public ABI and compiler options remain fixed. Mutants
change one private include;allocation wrappers observe the actual producer call
site. Normal AST/IR parity and direct native boundary tests are independent.
This is correctness evidence,not performance/GUI/release admission.
"""
from pathlib import Path
import argparse,json,subprocess
from build_lsp_lifecycle_profile import verify_build,sha


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();build=args.build.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    normal=json.loads((build/'build-result.json').read_text());verify_build(normal,build)
    root=Path(__file__).resolve().parents[2]
    source=root/'ide/src/analysis/frontend_sem_final.inc';original=source.read_text(encoding='utf8')
    protected={**normal['inputs'],str(build/'build-result.json'):sha(build/'build-result.json'),str(Path(__file__).resolve()):sha(__file__)}
    for name,digest in normal['outputs'].items():protected[str(Path(name) if Path(name).is_absolute() else build/name)]=digest
    report=dict(status='running',accepted=False,scope=__doc__,protected=protected,commands=[],cases=[])
    def save():(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
    def run(name,command,assertion=None,refusal=False):
        result=subprocess.run(command,cwd=out,capture_output=True,timeout=180)
        stdout,stderr=out/(name+'.stdout.log'),out/(name+'.stderr.log');stdout.write_bytes(result.stdout);stderr.write_bytes(result.stderr)
        report['commands'].append(dict(name=name,command=command,exit=result.returncode,stdout=str(stdout),stdoutSha256=sha(stdout),stderr=str(stderr),stderrSha256=sha(stderr),expected='assertion' if assertion else 'zero'));save()
        if assertion:assert result.returncode==1 and assertion.encode() in result.stderr,(name,result.returncode,result.stderr[-2000:])
        else:assert not result.returncode,(name,result.returncode,result.stderr[-2000:])
        if refusal:assert b'observed final allocation refusals:' in result.stdout,(name,'producer call not observed')
    def compile(source,reference,target,include=None):
        command=list(next(c for c in normal['commands'] if '-c' in c and str(reference) in c));command[command.index(str(reference))]=str(source)
        if include:command.insert(1,'-I'+str(include))
        for flag in ['-MF','-MT','-o']:
            if flag in command:command[command.index(flag)+1]=str(target.with_suffix('.d' if flag=='-MF' else '.o'))
        run(target.stem+'-compile',command);return target.with_suffix('.o')
    def link(name,test,objects,extra=()):
        command=list(next(c for c in normal['commands'] if '-o' in c and Path(c[c.index('-o')+1]).name==test+'.exe'))
        command[1:2]=[str(p) for p in objects];target=out/(name+'.exe');command[command.index('-o')+1]=str(target);command+=list(extra)
        run(name+'-link',command);return target
    def variant(name,body):
        folder=out/name;path=folder/'analysis/frontend_sem_final.inc';path.parent.mkdir(parents=True);path.write_text(body,encoding='utf8',newline='\n');protected[str(path)]=sha(path);return folder,path
    try:
        for test in ['analysis_final_annotations_test','analysis_final_environment_test']:
            run(test+'-control',[str(build/(test+'.exe'))]);report['cases'].append(dict(name=test+'-control',status='passed'))
        for name,seam,replacement,test,assertion in [
            ('omit-type-abi','!xlsSemKeyNumber(key,type->abiId) ||','0 ||','analysis_final_environment_test','different(&ctx,&key)'),
            ('omit-root-region','state->currentRegion==1 &&','1 &&','analysis_final_environment_test','!xlsSemFinalPlanBefore'),
            ('omit-region-rebase','value=(uint64_t)reader->start+value-reader->oldStart;','/* Fault: retain retired absolute region. */','analysis_final_annotations_test','sameAst')]:
            assert original.count(seam)==1,name
            folder,changed=variant(name,original.replace(seam,replacement))
            if test=='analysis_final_environment_test':
                obj=compile(root/'ide/tests'/(test+'.c'),root/'ide/tests'/(test+'.c'),out/(name+'.o'),folder);objects=[obj]
            else:
                obj=compile(build/'frontend/xl6_sem.c',build/'frontend/xl6_sem.c',out/(name+'.o'),folder);objects=[build/(test+'.o'),obj]
            exe=link(name,test,objects);run(name+'-run',[str(exe)],assertion)
            report['cases'].append(dict(name=name,status='rejected',source=str(changed),sha256=sha(changed)))
        for name,function in [('refuse-final-context','xrtCalloc'),('refuse-final-key','xrtStrBufReserve')]:
            prefix='int xlsFinalTestInRefusal;\n'
            if function=='xrtCalloc':
                seam='        memo->finalContext=xrtCalloc(1,sizeof(*memo->finalContext));'
                replacement='        xlsFinalTestInRefusal=1;\n'+seam+'\n        xlsFinalTestInRefusal=0;'
                prototype='void* __real_xrtCalloc(size_t,size_t);'
                wrapper='void* __wrap_xrtCalloc(size_t count,size_t bytes){if(xlsFinalTestInRefusal){++denied;return NULL;}return __real_xrtCalloc(count,bytes);}'
            else:
                seam='    xstrbuf key={0};\n    if(!xlsSemFinalEnvironment(ctx,&key) ||'
                replacement='    xstrbuf key={0};\n    xlsFinalTestInRefusal=1;int encoded=xlsSemFinalEnvironment(ctx,&key);xlsFinalTestInRefusal=0;\n    if(!encoded ||'
                prototype='bool __real_xrtStrBufReserve(xstrbuf*,size_t);'
                wrapper='bool __wrap_xrtStrBufReserve(xstrbuf* buffer,size_t bytes){if(xlsFinalTestInRefusal){++denied;return false;}return __real_xrtStrBufReserve(buffer,bytes);}'
            assert original.count(seam)==1,name
            folder,changed=variant(name,prefix+original.replace(seam,replacement))
            spy=folder/'allocation.c';spy.write_text('#include <xrt.h>\n#include <stdio.h>\n#include <stdlib.h>\nextern int xlsFinalTestInRefusal;\nstatic unsigned denied;\n'+prototype+'\n'+wrapper+'\nstatic void verify(void){if(!denied)abort();printf("observed final allocation refusals:%u\\n",denied);}\n__attribute__((constructor)) static void setup(void){atexit(verify);}\n',encoding='utf8',newline='\n');protected[str(spy)]=sha(spy)
            obj=compile(build/'frontend/xl6_sem.c',build/'frontend/xl6_sem.c',out/(name+'.o'),folder)
            allocation=compile(spy,root/'ide/tests/analysis_final_annotations_test.c',out/(name+'-allocation.o'))
            exe=link(name,'analysis_final_annotations_test',[build/'analysis_final_annotations_test.o',obj,allocation],['-Wl,--wrap='+function])
            run(name+'-run',[str(exe),'--expect-no-final-cache'],refusal=True)
            report['cases'].append(dict(name=name,status='fallback-passed',source=str(changed),sha256=sha(changed),allocationWrapper=str(spy),wrapperSha256=sha(spy)))
        verify_build(normal,build)
        for path,digest in protected.items():assert sha(path)==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),commands=len(report['commands']),report=str(out/'result.json'))))


if __name__=='__main__':main()
