"""Real retained memo type sharing,root isolation and incremental restoration.

Exact AST/IR/source identity and resource fallback are exercised by native
fixtures. This suite checks actual wire provenance and current semantic work.
"""
from pathlib import Path
import argparse,hashlib,json,time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import source
from check_language_definitions import position

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True);parser.add_argument('--baseline',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--baseline-version',choices=['0.53','0.54','0.55','0.56','0.57','0.58','0.59','0.60','0.61','0.62','0.63','0.64','0.65','0.66','0.67','0.68','0.69','0.70','0.71','0.72','0.73','0.74','0.75','0.76','0.77','0.78','0.79','0.80','0.81','0.82','0.83','0.84','0.85','0.87','0.88','0.89','0.90','0.91','0.92','0.93','0.94','0.95','0.96'],default='0.96')
    args=parser.parse_args();server=args.server.resolve(strict=True);baseline=args.baseline.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    body=source(16,0,True).split('export func compute(')[0]+('func choose(int left,string right) as int\r\n return left\r\nend\r\n'
        'func main(...) as int\r\n int doneLocal=choose(7,"hello")\r\n return doneLocal\r\nend\r\n')
    cases=[];commands=[];protected={str(server):sha(server),str(baseline):sha(baseline),str(Path(__file__)):sha(Path(__file__))};restored={}
    report=dict(status='failed',scope=__doc__,cases=cases,commands=commands,protected=protected)
    try:
        for method in ['xlang/check','textDocument/completion','textDocument/signatureHelp']:
          for variant,exe in [('before',baseline),('after',server)]:
            folder=out/(method.split('/')[-1]+'-'+variant);folder.mkdir();paths=[folder/'根A😀.xl',folder/'根B.xl']
            for p in paths:p.write_bytes(b'// keep disk source untouched\n');protected[str(p)]=sha(p)
            c=AutomaticClient(exe,folder/'absent/xl.exe',folder)
            def call(index,text=body,version=1):
                params=dict(textDocument=dict(uri=paths[index].as_uri()),version=version)
                if method.endswith('completion'):params['position']=position(text,text.index('return done')+len('return done'))
                if method.endswith('signatureHelp'):params.update(position=position(text,text.index('"hello"')+2),context=dict(triggerKind=1,isRetrigger=False))
                until=time.monotonic()+20
                while True:
                    c.request+=1;c.send(method,params,c.request);reply,_=c.reply(c.request)
                    if method!='xlang/check' or reply.get('error',{}).get('code')!=-32001:break
                    assert time.monotonic()<until;time.sleep(.002)
                assert 'error' not in reply,reply
                value=reply['result'];a=value['analysis'];types=a.get('memoTypes',{})
                assert value['version']==version and a['workerThreadId']!=a['captureThreadId']
                if method=='xlang/check' and (variant=='after' or args.baseline_version!='0.53'):assert types['references']==types['uniqueRoots']+types['hits'] and types['hits']>types['uniqueRoots']>0 and types['treeBytes']>0 and types['keyBytes']>0 and types['peakKeyBytes']>0 and types['fallbacks']==0,types
                else:assert 'memoTypes' not in a,a
                if method=='xlang/check':assert value['ok'] and value['count']==0 and value['semanticDefinitions']
                elif method.endswith('completion'):assert any(x['label']=='doneLocal' for x in value['items'])
                else:assert value['activeParameter']==1 and value['signatures'][value['activeSignature']]['label']=='choose(int left, string right) as int'
                cases.append(dict(method=method,variant=variant,target=index,version=version,analysis=a));return dict(a,memoTypes=types)
            try:
                c.send('initialize',dict(capabilities={},initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False)),1)
                init,_=c.reply(1);assert init['result']['serverInfo']['version']==(args.baseline_version if variant=='before' else '0.97');c.send('initialized',{})
                for p in paths:c.open(p,body,1)
                a=call(0);assert a['builds']==1
                again=call(0);assert again['builds']==1 and again['cacheHits']==1 and again['memoTypes']==a['memoTypes']
                b=call(1);assert b['builds']==2
                again=call(0);assert again['builds']==2 and again['memoTypes']==a['memoTypes']
                edited=body.replace('local = value+0','local = value+99999',1);assert edited!=body
                c.change(paths[0],edited,2)
                again=call(1);assert again['builds']==2 and again['inputScopeHits']>0 and again['memoTypes']==b['memoTypes']
                changed=call(0,edited,2);assert changed['builds']==3
                reused=changed['semanticFunctionsReused'] if method=='xlang/check' else changed['lastBuildSemantics']['reused'] if method.endswith('completion') else changed['reused']
                # Prefix coordinate restoration keeps the complete reuse level
                # established by explicit type origins. Active query functions
                # still use the checker;all original gates remain.
                assert reused>0,changed
                if variant=='before':restored[method]=reused
                else:assert reused==(16 if method=='xlang/check' else 15) and reused>=restored[method],(reused,restored[method])
                again=call(0,edited,2);assert again['builds']==3 and again['memoTypes']==changed['memoTypes']
                again=call(1);assert again['builds']==3 and again['memoTypes']==b['memoTypes']
                print(method,variant,'retained roots/current incremental restoration passed;reused',reused,flush=True)
            finally:
                stderr=c.stop();(folder/'stderr.log').write_text(stderr,encoding='utf8');(folder/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8');commands.append(dict(server=str(exe),exit=c.process.returncode));assert not c.process.returncode
        assert len(cases)==48 and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status='passed',cases=len(cases),proof=str(out/'result.json'))))
if __name__=='__main__':main()
