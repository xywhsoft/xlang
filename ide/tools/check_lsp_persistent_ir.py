"""Real stdio persistent-final-IR ownership and ordinary-edit fallback checks."""
from pathlib import Path
import argparse,hashlib,json,sys

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--baseline',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--tools-root',type=Path,default=Path(__file__).resolve().parent)
    args=parser.parse_args();sys.path.insert(0,str(args.tools_root.resolve(strict=True)))
    from check_lsp_automatic_diagnostics import AutomaticClient
    from check_lsp_module_semantics import source,initialize,checked
    O=args.output.resolve();O.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    variants={'baseline':args.baseline.resolve(strict=True)} if args.baseline else {}
    variants['candidate']=args.server.resolve(strict=True)
    protected={str(p):sha(p) for p in [*variants.values(),Path(__file__),*[args.tools_root/n for n in ['check_lsp_automatic_diagnostics.py','check_lsp_module_semantics.py','check_language_snapshots.py']]]}
    report=dict(status='running',scope=__doc__,protected=protected,queries=[],processes=[]);answers={}
    def publish():(O/'result.json').write_bytes((json.dumps(report,ensure_ascii=False,indent=2)+'\n').encode())
    try:
        for count in [32,512]:
            for topology in ['single','imported']:
                case=f'{count}-{topology}';folder=O/case;folder.mkdir();entry=folder/'main.xl';member=folder/'dep.xl' if topology=='imported' else entry;members=list(dict.fromkeys([entry,member]))
                for p in members:p.write_bytes(b'// immutable unsaved disk placeholder\n');protected[str(p)]=sha(p)
                for variant,server in variants.items():
                    client=AutomaticClient(server,O/'absent/xl.exe',folder)
                    info=None
                    try:
                        initialize(client,entry,members,folder)
                        info=next(x['receive']['result']['serverInfo'] for x in client.transcript if x.get('receive',{}).get('id')==1)
                        if topology=='imported':client.open(entry,'import dep\nfunc main(...) as int\n return dep.compute(3)\nend\n',1)
                        previousWork=None
                        for step,kind in enumerate(['cold','whitespace','whitespace','literal','whitespace','declaration','whitespace','comment','repeat']):
                            repeat=kind=='repeat'
                            if not repeat:
                                payload=source(count,int(step>=3),True);assert payload[0]=='\ufeff';body='\ufeff'+'\r\n'*step+payload[1:]
                                if step>=5:
                                    needle=' int local = value+0\r\n';assert body.count(needle)==1
                                    body=body.replace(needle,needle+' int inserted = 7\r\n',1).replace(' return local\r\n',' return local+inserted\r\n',1)
                                if topology=='single':body+='\nfunc main(...) as int\n return compute(3)\nend\n'
                                if kind=='comment':body+='\r\n// current ordinary comment 中文😀\r\n'
                                if not step:client.open(member,body,1)
                                else:client.change(member,body,step+1)
                            result=checked(client,entry,(8 if repeat else step+1) if topology=='single' else 1)
                            assert result['ok'] and not result['count'] and result['semanticDefinitions'],result
                            facts={k:v for k,v in result.items() if k not in ['analysis','durationMs']};key=(case,step)
                            if key in answers:assert answers[key]==facts,(case,step,variant)
                            else:answers[key]=facts
                            if variant=='candidate':
                                work=result['analysis']['lastBuildPersistentIr'];assert work['verified']==1,work
                                if repeat:assert work==previousWork,work
                                elif kind in ['whitespace','comment']:
                                    assert work['reused']==work['tablesShared']==1 and not work['built'] and not work['captured'],work
                                    assert work['nodesCopied']>count and 0<work['keyBytes']<=work['metadataBytes']<=32*1024*1024,work
                                    assert work['prepared']==int(step in [1,4,6]),work
                                else:assert work['built']==1 and not any(work[k] for k in ['reused','captured','prepared','keyBytes','metadataBytes']),work
                                previousWork=work
                            report['queries'].append(dict(case=case,variant=variant,step=step,kind=kind,result=result));publish()
                    finally:
                        stderr=client.stop();sp=folder/(variant+'-stderr.log');sp.write_bytes(stderr.encode('utf-8'));tp=folder/(variant+'-transcript.json');tp.write_bytes((json.dumps(client.transcript,ensure_ascii=False,indent=2)+'\n').encode())
                        report['processes'].append(dict(case=case,variant=variant,exit=client.process.returncode,serverInfo=info,stderr=str(sp),stderrSha256=sha(sp),transcript=str(tp),transcriptSha256=sha(tp)));publish();assert not client.process.returncode
                    print(case,variant,'passed',flush=True)
        assert len(report['queries'])==36*len(variants) and len(report['processes'])==4*len(variants)
        assert all(sha(p)==h for p,h in protected.items())
        report['status']='passed';publish();print(json.dumps(dict(status='passed',queries=len(report['queries']),processes=len(report['processes']),report=str(O/'result.json'))),flush=True)
    except BaseException as error:report.update(status='failed',error=str(error));publish();raise

if __name__=='__main__':main()
