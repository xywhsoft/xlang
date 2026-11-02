"""Exercise actual context/coordinate guards and geometry producer failures.

Private mutants only,using a complete matched native build. Original parser,
language rules and full fresh-result oracles remain unchanged. This does not
establish performance,GUI or release admission.
"""
from pathlib import Path
import argparse,json,subprocess
from build_lsp_lifecycle_profile import verify_build,sha


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();B=args.build.resolve(strict=True);O=args.output.resolve();O.mkdir(parents=True,exist_ok=False)
    R=Path(__file__).resolve().parents[2];normal=json.loads((B/'build-result.json').read_text());verify_build(normal,B)
    protected={**normal['inputs'],str(B/'build-result.json'):sha(B/'build-result.json'),str(Path(__file__)):sha(Path(__file__))}
    record=dict(status='running',accepted=False,scope=__doc__,protected=protected,commands=[],cases=[])

    def save():(O/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')

    def run(name,command,assertion=None,refusal=False):
        result=subprocess.run(command,cwd=O,capture_output=True,timeout=180)
        stdout,stderr=O/(name+'.stdout.log'),O/(name+'.stderr.log');stdout.write_bytes(result.stdout);stderr.write_bytes(result.stderr)
        record['commands'].append(dict(name=name,command=command,exit=result.returncode,stdout=str(stdout),stdoutSha256=sha(stdout),stderr=str(stderr),stderrSha256=sha(stderr)))
        save()
        if assertion:assert result.returncode==1 and assertion.encode() in result.stderr,(name,result.returncode,result.stderr[-1600:])
        else:assert not result.returncode,(name,result.returncode,result.stderr[-1600:])
        if refusal:assert b'actual geometry producer refusal' in result.stderr,(name,'producer was not reached')

    def compile(name,reference,folder):
        command=list(next(c for c in normal['commands'] if '-c' in c and str(reference) in c))
        command.insert(1,'-I'+str(folder));command[command.index('-o')+1]=str(O/(name+'.o'));run(name+'-compile',command)
        return O/(name+'.o')

    def link(name,test,objects):
        command=list(next(c for c in normal['commands'] if c[-1]==str(B/(test+'.exe'))))
        command[1:2]=[str(p) for p in objects];command[-1]=str(O/(name+'.exe'));run(name+'-link',command)
        return O/(name+'.exe')

    def shadow(name,file,text):
        folder=O/name;path=folder/'analysis'/file;path.parent.mkdir(parents=True);path.write_text(text,encoding='utf8',newline='\n');protected[str(path)]=sha(path)
        return folder,path

    try:
        for test in ['analysis_final_environment_test','analysis_native_borrow_test']:run(test+'-control',[str(B/(test+'.exe'))])
        record['cases'].append(dict(name='normal-controls',status='passed'))
        for name,file,seam,replacement,assertion in [
            ('omit-full-context','frontend_sem.inc','!build || !build->compatible || !build->memo->trackInputs || !build->baseline ||','!build || !build->memo->trackInputs || !build->baseline ||','!xlsSemApplyPosition'),
            ('omit-complete-coordinate','frontend_sem_geometry.inc','XLS_HEADER_ORDER(position.token.endColumn);','','mapped.line==-7'),
            ('omit-ambiguity-check','frontend_sem_geometry.inc','(found && !xlsSemGeometryPositionEqual(result,next->position))','0','!xlsSemGeometryPosition(ambiguous.geometry'),
            ('omit-producer-call-kind','frontend_sem_proof.inc','call->semanticCallKind==XL6_SEM_CALL_DIRECT &&','1 &&','!xlsSemProofOrigin(&unit,2)'),
            ('omit-local-symbol-guard','frontend_sem_proof.inc','node->semanticSymbolId<unit->symbols.floor &&','1 &&','!xlsSemProofOrigin(&unit,2)'),
        ]:
            text=(R/'ide/src/analysis'/file).read_text(encoding='utf8');assert text.count(seam)==1,(name,'seam changed')
            folder,path=shadow(name,file,text.replace(seam,replacement,1))
            obj=compile(name,R/'ide/tests/analysis_final_environment_test.c',folder)
            program=link(name,'analysis_final_environment_test',[obj]);run(name+'-run',[str(program)],assertion=assertion)
            record['cases'].append(dict(name=name,status='rejected',source=str(path),sha256=sha(path)))
        source=(R/'ide/src/analysis/frontend_sem_geometry.inc').read_text(encoding='utf8')
        for name,prefix,suffix in [
            ('refuse-geometry-record','static void* xlsGeometryRefuse(size_t a,size_t b){(void)a;(void)b;fputs("actual geometry producer refusal\\n",stderr);return NULL;}\n#define xrtCalloc(a,b) xlsGeometryRefuse(a,b)\n','\n#undef xrtCalloc\n'),
            ('refuse-geometry-order','static int xlsGeometryRefuse(xarray* a){(void)a;fputs("actual geometry producer refusal\\n",stderr);return 0;}\n#define xrtArraySort(a,b) ((void)(b),xlsGeometryRefuse(a))\n','\n#undef xrtArraySort\n'),
        ]:
            folder,path=shadow(name,'frontend_sem_geometry.inc',prefix+source+suffix)
            obj=compile(name,B/'frontend/xl6_sem.c',folder)
            program=link(name,'analysis_native_borrow_test',[B/'analysis_native_borrow_test.o',obj])
            run(name+'-run',[str(program),'--expect-no-geometry'],refusal=True)
            record['cases'].append(dict(name=name,status='fallback-passed',source=str(path),sha256=sha(path)))
        verify_build(normal,B)
        for p,digest in protected.items():assert sha(p)==digest,p
        record['status']='passed'
    finally:save()
    print(json.dumps(dict(status=record['status'],cases=len(record['cases']),commands=len(record['commands']),report=str(O/'result.json'))))


if __name__=='__main__':main()
