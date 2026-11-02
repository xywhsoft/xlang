"""Actual receipt-guard omissions and optional canonical-producer refusals.

Requires an intact complete native build. Mutants stay private;the original
collector,type rules and full fresh AST/typed-index/IR oracle remain unchanged.
This establishes neither performance nor GUI/release admission.
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

    def run(name,command,reject=False,refusal=False,index_mismatch=False):
        result=subprocess.run(command,cwd=O,capture_output=True,timeout=240)
        stdout,stderr=O/(name+'.stdout.log'),O/(name+'.stderr.log');stdout.write_bytes(result.stdout);stderr.write_bytes(result.stderr)
        record['commands'].append(dict(name=name,command=command,exit=result.returncode,stdout=str(stdout),stdoutSha256=sha(stdout),stderr=str(stderr),stderrSha256=sha(stderr)));save()
        if index_mismatch:assert result.returncode==1 and b'indexFact(left.fact,right.fact)' in result.stderr,(name,result.returncode,result.stderr[-1800:])
        elif reject:assert result.returncode==1 and b'analysis_index_canonical_guards_test:' in result.stderr,(name,result.returncode,result.stderr[-1800:])
        else:assert not result.returncode,(name,result.returncode,result.stderr[-1800:])
        if refusal:assert b'actual canonical producer refusal' in result.stderr,(name,'producer was not reached')

    def shadow(name,file,text):
        folder=O/name;path=folder/'analysis'/file;path.parent.mkdir(parents=True);path.write_text(text,encoding='utf8',newline='\n');protected[str(path)]=sha(path)
        return folder,path

    def compile(name,reference,folder,source=None):
        command=list(next(c for c in normal['commands'] if '-c' in c and str(reference) in c))
        command.insert(1,'-I'+str(folder))
        if source:
            command.insert(1,'-I'+str(R/'ide/tests'));command[command.index('-c')+1]=str(source)
        command[-1]=str(O/(name+'.o'));run(name+'-compile',command);return O/(name+'.o')

    def link(name,test,objects):
        command=list(next(c for c in normal['commands'] if c[-1]==str(B/(test+'.exe'))));command[1:2]=[str(p) for p in objects];command[-1]=str(O/(name+'.exe'));run(name+'-link',command);return O/(name+'.exe')

    try:
        for test in ['analysis_index_canonical_test','analysis_index_canonical_guards_test']:run(test+'-control',[str(B/(test+'.exe'))])
        record['cases'].append(dict(name='normal-controls',status='passed'))
        for name,file,seam,replacement in [
            ('omit-final-environment','frontend_sem_ir.inc','previous->finalContext!=memo->finalContext ||',''),
            ('omit-capture-pair','frontend_sem_ir.inc','    if(!previous && previousRoot)return 0;',''),
            ('omit-semantic-unit','frontend_sem_ir.inc','entry->unit!=old->unit ||',''),
            ('omit-final-output','frontend_sem_ir.inc','entry->finalInput!=old->finalInput ||',''),
            ('omit-current-root','frontend_sem_ir.inc','memo->finalizedRoot!=root ||',''),
            ('omit-cursor-rejection','frontend_sem_ir.inc','entry->unit->cursor.path ||',''),
            ('omit-row-kind','frontend_index_canonical.inc','row->kind!=fact->node->kind ||',''),
        ]:
            text=(R/'ide/src/analysis'/file).read_text(encoding='utf8');assert text.count(seam)==1,(name,'seam changed')
            folder,path=shadow(name,file,text.replace(seam,replacement,1))
            obj=compile(name,R/'ide/tests/analysis_index_canonical_guards_test.c',folder)
            program=link(name,'analysis_index_canonical_guards_test',[obj]);run(name+'-run',[str(program)],reject=True)
            record['cases'].append(dict(name=name,status='rejected',source=str(path),sha256=sha(path)))
        text=(R/'ide/src/analysis/frontend_index_canonical.inc').read_text(encoding='utf8')
        # Same real alias program,without numeric promotion rewrites. An
        # admission bug must be caught by CURRENT typed-index values,not only
        # by a work-counter expectation or a synthetic receipt fixture.
        program=(R/'ide/tests/analysis_index_canonical_test.c').read_text(encoding='utf8')
        seam='for(unsigned scenario=0;scenario<5;++scenario)if(!editingCanonical(scenario,60,0))return 1;\n    if(!editingCanonical(2,60,1) || !editingCanonical(2,512,0))return 1;'
        assert program.count(seam)==1;program=program.replace(seam,'if(!editingCanonical(4,60,0))return 1;',1)
        seam=' %s local=value+%u\\r\\n",i,type,type,type,i);';assert program.count(seam)==1
        program=program.replace(seam,' %s local=value\\r\\n",i,type,type,type);',1).replace(' local=local+1\\r\\n',' local=local\\r\\n')
        fixture=O/'alias-without-coercion.c';fixture.write_text(program,encoding='utf8',newline='\n');protected[str(fixture)]=sha(fixture)
        obj=compile('alias-control',R/'ide/tests/analysis_index_canonical_test.c',B/'frontend',fixture)
        executable=link('alias-control','analysis_index_canonical_test',[obj]);run('alias-control-run',[str(executable)])
        seam='xlsIndexCanonicalAdmission(scope,slot,1,&count)';assert text.count(seam)==1
        folder,path=shadow('omit-live-canonical-proof','frontend_index_canonical.inc',text.replace(seam,'xlsIndexCanonicalAdmission(scope,slot,0,&count)',1))
        changed=compile('omit-live-canonical-proof',B/'frontend/xl6_sem_index.c',folder)
        executable=link('omit-live-canonical-proof','analysis_index_canonical_test',[obj,changed]);run('omit-live-canonical-proof-run',[str(executable)],index_mismatch=True)
        record['cases'].append(dict(name='real-alias-canonical-staleness',status='rejected',source=str(path),sha256=sha(path),fixture=str(fixture),fixtureSha256=sha(fixture)))
        for name,prefix,suffix in [
            ('refuse-owner','static void* xlsCanonRefuse(size_t a,size_t b){(void)a;(void)b;fputs("actual canonical producer refusal\\n",stderr);return NULL;}\n#define xrtCalloc(a,b) xlsCanonRefuse(a,b)\n','\n#undef xrtCalloc\n'),
            ('refuse-vector','static int xlsCanonRefuse(xarray* a,size_t b){(void)a;(void)b;fputs("actual canonical producer refusal\\n",stderr);return 0;}\n#define xrtArrayResize(a,b) xlsCanonRefuse(a,b)\n','\n#undef xrtArrayResize\n'),
        ]:
            folder,path=shadow(name,'frontend_index_canonical.inc',prefix+text+suffix)
            obj=compile(name,B/'frontend/xl6_sem_index.c',folder)
            program=link(name,'analysis_index_canonical_test',[B/'analysis_index_canonical_test.o',obj])
            run(name+'-run',[str(program),'--expect-no-canonical'],refusal=True)
            record['cases'].append(dict(name=name,status='fallback-passed',source=str(path),sha256=sha(path)))
        verify_build(normal,B)
        for path,digest in protected.items():assert sha(path)==digest,path
        record['status']='passed'
    except BaseException as error:record.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=record['status'],cases=len(record['cases']),commands=len(record['commands']),report=str(O/'result.json'))))


if __name__=='__main__':main()
