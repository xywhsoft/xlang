"""Verify final scalar replay write permissions and real original-pass fallback.

Native C omissions must fail the complete boundary fixture. An actual scalar
matcher refusal must preserve current AST/index/IR/source-report parity while
the ordinary original final passes complete. No runtime image is installed.
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

    def run(name,command,reject=False,refusal=False):
        result=subprocess.run(command,cwd=O,capture_output=True,timeout=240)
        stdout,stderr=O/(name+'.stdout.log'),O/(name+'.stderr.log');stdout.write_bytes(result.stdout);stderr.write_bytes(result.stderr)
        record['commands'].append(dict(name=name,command=command,exit=result.returncode,stdout=str(stdout),stdoutSha256=sha(stdout),stderr=str(stderr),stderrSha256=sha(stderr)));save()
        if reject:assert result.returncode==1 and b'analysis_final_environment_test:' in result.stderr,(name,result.returncode,result.stderr[-1800:])
        else:assert not result.returncode,(name,result.returncode,result.stderr[-1800:])
        if refusal:assert b'actual fixed scalar replay refusal' in result.stderr,(name,'producer was not reached')

    def shadow(name,text):
        folder=O/name;path=folder/'analysis/frontend_sem_final.inc';path.parent.mkdir(parents=True);path.write_text(text,encoding='utf8',newline='\n');protected[str(path)]=sha(path)
        return folder,path

    def compile(name,reference,folder):
        command=list(next(c for c in normal['commands'] if '-c' in c and str(reference) in c));command.insert(1,'-I'+str(folder));command[-1]=str(O/(name+'.o'))
        run(name+'-compile',command);return O/(name+'.o')

    def link(name,test,objects):
        command=list(next(c for c in normal['commands'] if c[-1]==str(B/(test+'.exe'))));command[1:2]=[str(p) for p in objects];command[-1]=str(O/(name+'.exe'))
        run(name+'-link',command);return O/(name+'.exe')

    try:
        for test in ['analysis_final_environment_test','analysis_index_canonical_test']:run(test+'-control',[str(B/(test+'.exe'))])
        record['cases'].append(dict(name='normal-controls',status='passed'))
        original=(R/'ide/src/analysis/frontend_sem_final.inc').read_text(encoding='utf8')
        for name,replacements in [
            ('omit-fixed-equality',[('!xlsSemFinalFixedMatches(node,field,value)','0')]),
            ('write-fixed-symbol',[
                ('case 0:node->flags=(unsigned)value;break;','case 3:node->semanticSymbolId=(uint32_t)value;break;\n        case 0:node->flags=(unsigned)value;break;'),
                ('if(reader->write && (XLS_FINAL_PATCH_MASK&(UINT32_C(1)<<field)))xlsSemFinalSet(node,field,value);','if(reader->write)xlsSemFinalSet(node,field,value);')]),
            ('omit-scalar-schema',[('    if(mask&~XLS_FINAL_SCALAR_MASK)return 0;','')]),
        ]:
            text=original
            for seam,replacement in replacements:
                assert text.count(seam)==1,(name,'seam changed');text=text.replace(seam,replacement,1)
            folder,path=shadow(name,text);obj=compile(name,R/'ide/tests/analysis_final_environment_test.c',folder)
            program=link(name,'analysis_final_environment_test',[obj]);run(name+'-run',[str(program)],reject=True)
            record['cases'].append(dict(name=name,status='rejected',source=str(path),sha256=sha(path)))
        seam='    return fields[field]==value;';assert original.count(seam)==1
        text=original.replace(seam,'    (void)fields;(void)field;(void)value;fputs("actual fixed scalar replay refusal\\n",stderr);return 0;',1)
        folder,path=shadow('refuse-fixed-matcher',text);obj=compile('refuse-fixed-matcher',B/'frontend/xl6_sem.c',folder)
        program=link('refuse-fixed-matcher','analysis_index_canonical_test',[B/'analysis_index_canonical_test.o',obj])
        run('refuse-fixed-matcher-run',[str(program),'--expect-fixed-refusal'],refusal=True)
        record['cases'].append(dict(name='refuse-fixed-matcher',status='original-fallback-passed',source=str(path),sha256=sha(path)))
        verify_build(normal,B)
        for path,digest in protected.items():assert sha(path)==digest,path
        record['status']='passed'
    except BaseException as error:record.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=record['status'],cases=len(record['cases']),commands=len(record['commands']),report=str(O/'result.json'))))


if __name__=='__main__':main()
