"""Exercise the real bounded semantic root pool over stdio,including eviction.

Actual models remain guarded by current overlays/disk inputs. Cache routing is
separate from semantic fact hits. This is correctness QA,not a latency benchmark.
"""
from pathlib import Path
import argparse,hashlib,json,time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    cases=[];commands=[];protected={str(server):sha(server),str(Path(__file__)):sha(Path(__file__))}
    report=dict(status='failed',scope=__doc__,cases=cases,commands=commands,protected=protected)
    body='\ufeff#doc\r\ndesc: 多根缓存😀\r\n#end\r\nfunc choose(int left,string right) as int\r\n return left\r\nend\r\nfunc main(...) as int\r\n int doneLocal=choose(7,"hello")\r\n return doneLocal\r\nend\r\n'
    try:
        for method in ['xlang/check','textDocument/completion','textDocument/signatureHelp']:
            for count in [2,8,9]:
                folder=out/(method.split('/')[-1]+'-'+str(count));folder.mkdir()
                paths=[folder/f'源-{i}.xl' for i in range(count)]
                for path in paths:path.write_bytes(b'// never replace this disk body\n');protected[str(path)]=sha(path)
                c=AutomaticClient(server,folder/'absent/xl.exe',folder)
                def call(index,version=1,text=body):
                    path=paths[index];params=dict(textDocument=dict(uri=path.as_uri()),version=version)
                    if method.endswith('completion'):params['position']=position(text,text.index('return done')+len('return done'))
                    if method.endswith('signatureHelp'):params.update(position=position(text,text.index('"hello"')+2),context=dict(triggerKind=1,isRetrigger=False))
                    until=time.monotonic()+20
                    while True:
                        c.request+=1;c.send(method,params,c.request);reply,_=c.reply(c.request)
                        if method!='xlang/check' or reply.get('error',{}).get('code')!=-32001:break
                        assert time.monotonic()<until;time.sleep(.002)
                    assert 'error' not in reply,reply
                    value=reply['result'];a=value['analysis'];cache=a['sessionCache']
                    assert value['version']==version and a['workerThreadId']!=a['captureThreadId']
                    assert cache['sessionLimit']==8 and cache['inputStorageLimit']==64*1024*1024 and not cache['inputBudgetExceeded'] and not cache['allocationFallbacks']
                    if method=='xlang/check':assert value['ok'] and value['count']==0 and value['semanticDefinitions']
                    elif method.endswith('completion'):assert any(v['label']=='doneLocal' for v in value['items'])
                    else:assert value['activeParameter']==1 and value['signatures'][value['activeSignature']]['label']=='choose(int left, string right) as int'
                    cases.append(dict(method=method,roots=count,target=index,version=version,analysis=a));return value
                try:
                    c.send('initialize',dict(capabilities={},initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False)),1)
                    init,_=c.reply(1);assert init['result']['serverInfo']['version']=='0.56';c.send('initialized',{})
                    for path in paths:c.open(path,body,1)
                    for i in range(count):
                        value=call(i);assert value['analysis']['builds']==i+1 and value['analysis']['sessionCache']['sessions']==min(i+1,8)
                    if count<=8:
                        for i in list(reversed(range(count)))+list(range(count)):
                            value=call(i);assert value['analysis']['builds']==count and value['analysis']['cacheHits']>0 and value['analysis']['sessionCache']['evictions']==0
                        # Alter an unobserved overlay:current source envelopes change,
                        # while all retained roots still validate their own facts.
                        changed=body+'// unobserved edit 中文😀\r\n';c.change(paths[-1],changed,2)
                        value=call(0);assert value['analysis']['builds']==count and value['analysis']['inputScopeHits']>0
                        value=call(count-1,2,changed);assert value['analysis']['builds']==count+1
                        value=call(0);assert value['analysis']['builds']==count+1
                        c.send('textDocument/didClose',dict(textDocument=dict(uri=paths[-1].as_uri())))
                        value=call(0);assert value['analysis']['sessionCache']['pruned']>=1
                        c.open(paths[-1],body,1);value=call(count-1);assert value['analysis']['sessionCache']['sessions']<=8
                    else:
                        value=call(0);assert value['analysis']['builds']==10 and value['analysis']['sessionCache']['evictions']==2
                        value=call(0);assert value['analysis']['builds']==10 and value['analysis']['cacheHits']==1
                    print(method,count,'roots passed',flush=True)
                finally:
                    stderr=c.stop();(folder/'stderr.log').write_text(stderr,encoding='utf8');(folder/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8');commands.append(dict(server=str(server),exit=c.process.returncode));assert not c.process.returncode
        assert all(sha(path)==h for path,h in protected.items());report['status']='passed'
    finally:(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status='passed',cases=len(cases),proof=str(out/'result.json'))))
if __name__=='__main__':main()
