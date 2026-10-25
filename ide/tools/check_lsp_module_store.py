"""Shared ordinary module syntax across real native check/cursor/signature actors.

Original overlay/disk loading,project/configuration and cursor projections stay
authoritative. Native fixtures independently compare complete AST and IR.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time
from check_lsp_module_semantics import initialize
from check_lsp_analysis_configuration import LISTS, PROFILE
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(server):sha(server),str(Path(__file__)):sha(__file__)};cases=[];clients=[]
    report=dict(status='failed',scope=__doc__,protected=protected,cases=cases)
    try:
        for opened in [False,True]:
            folder=out/('opened' if opened else 'disk');folder.mkdir()
            entry,bridge,dep,right=[folder/n for n in ['main.xl','bridge.xl','dep.xl','right.xl']]
            main='import bridge\nfunc main(...) as int\n return bridge.value()\nend\n'
            entry.write_bytes(main.encode());bridge.write_bytes(b'import dep\nexport func value() as int\n return dep.value()\nend\n')
            for path in [entry,bridge]:protected[str(path)]=sha(path)
            client=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(client)
            initialize(client,entry,[entry,bridge,dep,right],folder)
            if opened:client.open(entry,main,1)
            text=('\ufeffimport right\r\nexport func value() as int\r\n'
                  ' auto completionLocal=right.compute(7,"hi")\r\n return completionLocal\r\nend\r\n')
            def support(value,kind='int'):
                return (f'export func compute(int left,string right) as {kind}\r\n'
                        f' int local=left+{value}\r\n return '+('local' if kind=='int' else '"changed"')+'\r\nend\r\n')
            client.open(dep,text,7);client.open(right,support(0),1)
            epoch=1
            def done(name,**details):cases.append(dict(name=('opened-' if opened else 'disk-')+name,passed=True,**details))
            def bank():
                s=client.call('xlang/analysisStats')['moduleStore']
                assert s['enabled'] and 0<s['registryBytes']<=s['bytes']<=s['limit'] and s['modules']>=2,s
                assert s['lookups']==s['hits']+s['misses'],s
                return s
            def call(method,body=text,version=7,kind='int',okay=True):
                params=dict(textDocument=dict(uri=dep.as_uri()),version=version)
                if method.endswith('completion'):params['position']=position(body,body.rindex('completionLocal')+len('completionL'))
                if method.endswith('signatureHelp'):params.update(position=position(body,body.index('"hi"')+2),context=dict(triggerKind=1,isRetrigger=False))
                until=time.monotonic()+30
                while True:
                    client.request+=1;client.send(method,params,client.request);reply,_=client.reply(client.request)
                    if method!='xlang/check' or reply.get('error',{}).get('code')!=-32001:break
                    assert time.monotonic()<until,reply;time.sleep(.001)
                assert 'error' not in reply,reply
                r=reply['result'];assert r['version']==version and r['analysisContext']['epoch']==epoch,r
                a=r['analysis'];assert a['workerThreadId']!=a['captureThreadId'],a
                if method=='xlang/check':assert r['ok']==okay and bool(r['count'])!=okay,r
                elif method.endswith('completion'):
                    item=next(v for v in r['items'] if v['label']=='completionLocal');assert kind in item['detail'] and item['data']['declaration']['uri']==dep.as_uri(),r
                else:assert r['activeParameter']==1 and r['signatures'][r['activeSignature']]['label']==f'right.compute(int left, string right) as {kind}',r
                return r
            call('xlang/check');cold=bank();assert cold['published']>=2,cold
            call('textDocument/completion');cursor=bank();assert cursor['hits']>cold['hits'],(cold,cursor)
            call('textDocument/signatureHelp');signature=bank();assert signature['hits']>cursor['hits'],(cursor,signature)
            done('three-real-actors-share-ordinary-imports',cold=cold,cursor=cursor,signature=signature)
            for version in range(2,6):
                client.change(right,support(version),version)
                call('xlang/check');first=bank();assert first['published']>signature['published'],(signature,first)
                call('textDocument/completion');second=bank();call('textDocument/signatureHelp');signature=bank()
                assert second['hits']>first['hits'] and signature['hits']>second['hits'] and signature['published']==first['published'],(first,second,signature)
            done('four-dependency-edits-publish-once-then-share',stats=signature)
            client.change(right,support(7,'string'),6)
            call('xlang/check',kind='string',okay=False);call('textDocument/completion',kind='string');call('textDocument/signatureHelp',kind='string')
            done('same-module-identity-still-rechecks-current-types',stats=bank())
            client.send('textDocument/didClose',dict(textDocument=dict(uri=right.as_uri())));client.open(right,support(8),1)
            call('xlang/check');call('textDocument/completion');call('textDocument/signatureHelp')
            done('reopened-overlay-source-identity-rebuilds-semantics',stats=bank())
            lists={key:[] for key in LISTS};lists['members']=[p.as_posix() for p in [entry,bridge,dep,right]]
            context=dict(schema=1,kind='project',project=(folder/'project.xlp').as_posix(),configuration='release',entry=entry.as_posix(),directory=folder.as_posix(),analysisProfile=PROFILE,targetKind='exe',packagePath='',lists=lists)
            epoch=2;client.call('xlang/configureAnalysis',dict(epoch=epoch,catalog=dict(schema=1,projects=[context],selectedProject=context['project'],packagePath='')))
            before=bank();r=call('xlang/check');assert r['analysisContext']['configuration']=='release',r
            call('textDocument/completion');call('textDocument/signatureHelp');after=bank()
            assert after['hits']>before['hits'] and after['published']==before['published'],(before,after)
            done('configuration-has-private-semantics-and-shared-raw-syntax',stats=after)
            incomplete=text.replace('return completionLocal','return right.')
            client.change(dep,incomplete,8);reply=client.call('textDocument/completion',dict(textDocument=dict(uri=dep.as_uri()),version=8,position=position(incomplete,incomplete.index('right.\r\n')+6)))
            assert [v['label'] for v in reply['items']]==['compute'],reply
            assert bank()['published']==after['published']
            client.change(dep,text,9);call('xlang/check',version=9)
            done('incomplete-query-projection-never-enters-ordinary-store',stats=bank())
            assert not dep.exists() and not right.exists()
            client.stop();assert client.process.returncode==0
        assert len(cases)==12 and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            stderr=c.stop();(out/f'stderr-{i}.log').write_text(stderr,encoding='utf8');(out/f'transcript-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(out/'result.json')),ensure_ascii=False))


if __name__=='__main__':main()
