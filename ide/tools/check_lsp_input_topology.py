"""Real retained semantic models across editor topology changes,including misses.

Every response keeps the current source/version envelope. Adding an overlay
at an original missing/disk candidate must rebuild;unrelated tabs may not.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize
from check_language_definitions import position


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(server):sha(server),str(Path(__file__)):sha(__file__)};cases=[];clients=[];report=dict(status='failed',scope=__doc__,cases=cases,protected=protected)
    try:
        for disk in [False,True]:
            folder=out/('disk' if disk else 'missing');folder.mkdir();entry,active,dependency,extra,another=[folder/n for n in ['main.xl','active.xl','api.xl','extra.xl','another.xl']]
            main='import active\nfunc main(...) as int\n return active.value()\nend\n';entry.write_bytes(main.encode())
            body=('\ufeffimport api\r\nfunc choose(int left,string right) as int\r\n return left\r\nend\r\n'
                  'export func value() as int\r\n int completionLocal=choose(api.value(),"hi")\r\n return completionLocal\r\nend\r\n')
            api='export func value() as int\n return 7\nend\n'
            if disk:dependency.write_bytes(api.encode());protected[str(dependency)]=sha(dependency)
            client=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(client);initialize(client,entry,[entry,active,dependency,extra,another],folder)
            client.open(active,body,1);client.open(extra,'// unrelated malformed ( 中文😀',1)
            methods=['xlang/check','textDocument/completion','textDocument/signatureHelp'] if disk else ['xlang/check']
            def done(name,**details):cases.append(dict(name=('disk-' if disk else 'missing-')+name,passed=True,methods=methods,**details))
            def call(method,okay):
                params=dict(textDocument=dict(uri=active.as_uri()),version=1)
                if method.endswith('completion'):params['position']=position(body,body.rindex('completionLocal')+len('completionL'))
                if method.endswith('signatureHelp'):params.update(position=position(body,body.index('"hi"')+2),context=dict(triggerKind=1,isRetrigger=False))
                until=time.monotonic()+30
                while True:
                    client.request+=1;client.send(method,params,client.request);reply,_=client.reply(client.request)
                    if method!='xlang/check' or reply.get('error',{}).get('code')!=-32001:break
                    assert time.monotonic()<until,reply;time.sleep(.001)
                assert 'error' not in reply,reply
                r=reply['result'];assert r['version']==1 and r['analysisContext']['epoch']==1,r
                if method=='xlang/check':assert r['ok']==okay and bool(r['count'])!=okay,r
                elif method.endswith('completion'):assert bool(r['items'])==okay,r
                else:
                    assert bool(r['signatures'])==okay,r
                    if okay:assert r['signatures'][r['activeSignature']]['label']=='choose(int left, string right) as int' and r['activeParameter']==1,r
                return r
            def trio(okay):return [call(m,okay) for m in methods]
            values=trio(disk);assert all(v['analysis']['builds']==1 for v in values),values;done('initial-original-native-check-and-cursors',analysis=[v['analysis'] for v in values])
            client.open(another,'func not_finished(',1);values=trio(disk)
            assert all(v['analysis']['builds']==1 and v['analysis']['inputScopeHits']==1 for v in values),values
            done('new-unconsulted-overlay-keeps-entire-model',analysis=[v['analysis'] for v in values])
            client.send('textDocument/didClose',dict(textDocument=dict(uri=another.as_uri())));values=trio(disk)
            assert all(v['analysis']['builds']==1 and v['analysis']['inputScopeHits']==2 for v in values),values;done('removed-unconsulted-overlay-keeps-entire-model')
            client.open(dependency,api,7);values=trio(True)
            assert all(v['analysis']['builds']==2 and v['analysis']['inputScopeHits']==2 for v in values),values
            docs={Path(v['path']):v for v in values[0]['documents']};assert docs[dependency]['origin']=='overlay' and docs[dependency]['version']==7,docs
            done('overlay-at-original-miss-or-disk-path-rebuilds-and-restamps',analysis=[v['analysis'] for v in values])
            client.send('textDocument/didClose',dict(textDocument=dict(uri=dependency.as_uri())));values=trio(disk)
            assert all(v['analysis']['builds']==3 for v in values),values;done('removing-observed-overlay-restores-original-disk-or-missing-result')
            client.send('textDocument/didClose',dict(textDocument=dict(uri=extra.as_uri())));client.open(extra,'// new editor identity,unrelated path',1);values=trio(disk)
            assert all(v['analysis']['builds']==3 and v['analysis']['inputScopeHits']==3 for v in values),values;done('reopened-unconsulted-document-identity-keeps-model')
            # The same topology hit must still validate a changed disk entry.
            before=[v['analysis']['builds'] for v in values];replacement=entry.with_suffix('.replacement');replacement.write_bytes(main.encode());os.replace(replacement,entry)
            client.open(another,'// unrelated addition plus atomic root replacement',1);values=trio(disk)
            assert all(v['analysis']['builds']==n+1 for v,n in zip(values,before)),values
            for method,value in zip(methods,values):
                if method=='textDocument/completion':assert value['analysis']['invalidations']>=1,value
            done('metadata-proof-never-bypasses-disk-identity-guard',analysis=[v['analysis'] for v in values])
            assert not active.exists() and not extra.exists() and not another.exists();protected[str(entry)]=sha(entry)
            client.stop();assert client.process.returncode==0
        assert len(cases)==14 and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            stderr=c.stop();(out/f'stderr-{i}.log').write_text(stderr,encoding='utf8');(out/f'transcript-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(out/'result.json')),ensure_ascii=False))


if __name__=='__main__':main()
