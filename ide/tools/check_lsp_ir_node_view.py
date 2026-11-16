"""Exercise immutable final IR views through actual stdio and current overlays.

This checks work counters and complete standard result parity,not speed. The
component fixtures separately compare every final IR field with the original
builder and exercise independent ownership,allocation failure and retirement.
"""
from pathlib import Path
import argparse,hashlib,json
from benchmark_lsp_analysis import make_body
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize,checked


def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--baseline',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    variants=([('baseline',args.baseline.resolve(strict=True))]if args.baseline else[])+[('candidate',server)]
    protected={str(p):sha(p)for _,p in variants}
    for name in ['check_lsp_ir_node_view.py','benchmark_lsp_analysis.py','check_lsp_module_semantics.py','check_lsp_automatic_diagnostics.py']:
        p=Path(__file__).with_name(name).resolve();protected[str(p)]=sha(p)
    record=dict(status='running',scope=__doc__.strip(),queries=[],processes=[],protected=protected,performanceQualified=False)
    def save():(out/'result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
    answers={};save()
    try:
        for count in [32,512]:
            for topology,entry_open in [('single',True),('imported',True),('imported',False)]:
                case=f'{count}-{topology}-'+('open'if entry_open else'closed')
                folder=out/case;folder.mkdir();entry=folder/'main.xl';member=entry if topology=='single'else folder/'dep.xl'
                entry_text='import dep\nfunc main(...) as int\n return dep.compute(3)\nend\n'
                entry.write_text('// disk placeholder\n'if topology=='single'else entry_text,encoding='utf8')
                if member!=entry:member.write_text('// disk placeholder\n',encoding='utf8')
                for _,p in dict.fromkeys([(str(entry),entry),(str(member),member)]):protected[str(p)]=sha(p)
                for variant,program in variants:
                    client=AutomaticClient(program,folder/'absent/xl.exe',folder);previous=None
                    try:
                        initialize(client,entry,list(dict.fromkeys([entry,member])),folder,background_workspace_symbols=False)
                        if topology=='imported'and entry_open:client.open(entry,entry_text,1)
                        for step in range(13):
                            repeat=step==10
                            if not repeat:
                                value=0 if step<5 else 1
                                edit='declaration'if step>=8 else'literal'
                                text=make_body(count,value,topology,edit)
                                if step>=3:text=text[0]+'\r\n\r\n'+text[1:]
                                text+=f'\n// current trailing comment {step}\n'
                                if step==2:text+='\n'
                                if step==11:text+='\nfunc invalid(\n'
                                if not step:client.open(member,text,1)
                                else:client.change(member,text,step+1)
                            version=10 if repeat else step+1
                            request=entry if entry_open else member
                            result=checked(client,request,version if topology=='single'or not entry_open else 1)
                            valid=step!=11
                            assert result['ok']==valid and (result['count']==0 if valid else result['count']>0),result
                            facts={k:v for k,v in result.items()if k not in ['analysis','durationMs','checkId']}
                            key=(case,step)
                            if key in answers:assert answers[key]==facts,(case,step,variant)
                            else:answers[key]=facts
                            work=result['analysis']['lastBuildPersistentIr']
                            if variant=='candidate':
                                if step in [1,2,4,6,7,9]:
                                    assert work['reused']==work['tablesShared']==work['certificatesReused']==1 and work['nodesShared']>count,work
                                    assert not any(work[k]for k in ['built','verified','nodesCopied']),work
                                elif step==3:
                                    assert work['reused']==work['verified']==1 and work['nodesCopied']>count,work
                                    assert not work['nodesShared']and not work['certificatesReused'],work
                                elif step in [0,5,8]:
                                    assert work['built']==work['verified']==1 and not work['nodesShared']and not work['certificatesReused'],work
                                elif repeat:assert work==previous,work
                                elif step==11:assert not work['certificatesReused'],work
                            if valid:previous=work
                            record['queries'].append(dict(case=case,variant=variant,step=step,result=result));save()
                    finally:
                        stderr=client.stop();log=folder/(variant+'-stderr.log');log.write_text(stderr,encoding='utf8')
                        transcript=folder/(variant+'-transcript.json');transcript.write_text(json.dumps(client.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
                        record['processes'].append(dict(case=case,variant=variant,exit=client.process.returncode,stderr=str(log),transcript=str(transcript)));save()
                        assert client.process.returncode==0 and not stderr
                    print(case,variant,'passed',flush=True)
        assert len(record['queries'])==78*len(variants)and len(record['processes'])==6*len(variants)
        for p,h in protected.items():assert sha(p)==h,p
        record['status']='passed';save()
    except BaseException as error:record.update(status='failed',error=repr(error));save();raise
    print(json.dumps(dict(status='passed',queries=len(record['queries']),processes=len(record['processes']))))


if __name__=='__main__':main()
