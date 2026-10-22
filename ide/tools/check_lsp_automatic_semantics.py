"""Real standard semantic requests without a preceding xlang/check or xl.exe."""
import argparse
import hashlib
import json
from pathlib import Path
from check_language_snapshots import Client
from check_language_definitions import position


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    entry=root/'入口😀.xl';other=root/'other.xl';dep=root/'dep.xl'
    disk=b'// analysis must use unsaved text\n'
    for path in (entry,other,dep):path.write_bytes(disk)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in (server,entry,other,dep,Path(__file__),Path(__file__).with_name('check_language_snapshots.py'),Path(__file__).with_name('check_language_definitions.py'))}
    clients=[];cases=[];rid=10
    good='func main(...) as int\n int result = 7\n return result\nend\n'
    overload='func pick(int value) as int\n return value\nend\nfunc pick(string value) as int\n return 2\nend\nfunc main(...) as int\n auto first = pick(1)\n auto second = pick("😀")\n return first + second\nend\n'
    heavy=''.join('func helper_%d(int value) as int\n return value + 1\nend\n'%i for i in range(3000))+'func main(...) as int\n return helper_2999(7)\nend\n'
    report=dict(status='failed',scope='standard on-demand full semantic analysis, bounded owner queue and lifecycle; no semantic-only latency or changed-cursor incrementality claim',protected=protected,cases=cases)

    def done(name,**data):cases.append(dict(name=name,passed=True,**data))
    def client(backend=None):
        c=Client(server,root/'absent-executor/xl.exe',root);clients.append(c)
        options=dict(checkOnSave=False,automaticDiagnostics=False)
        if backend:options['diagnosticsBackend']=backend
        c.send('initialize',dict(capabilities=dict(textDocument=dict(documentSymbol=dict(hierarchicalDocumentSymbolSupport=True))),initializationOptions=options),1)
        r,_=c.reply(1);assert r['result']['capabilities']['experimental']['automaticSemanticQueries']==(backend!='cli'),r
        c.send('initialized',{});return c
    def params(path=entry,text=good,needle='result',**extra):
        return dict(textDocument=dict(uri=path.as_uri()),position=position(text,text.rindex(needle)),**extra)
    def send(c,method,p):
        nonlocal rid
        rid+=1;c.send(method,p,rid);return rid
    def collect(c,ids):
        pending=set(ids);results={};seen=[]
        while pending:
            r=c.next();seen.append(r)
            if 'id' in r:
                assert r['id'] in pending,('unexpected or duplicate response',r,pending)
                pending.remove(r['id']);results[r['id']]=r
        return results,seen
    def call(c,method,p):
        id=send(c,method,p);rows,seen=collect(c,[id]);r=rows[id];assert 'error' not in r,r;return r['result'],seen
    def expect(c,method,p,code):
        id=send(c,method,p);rows,_=collect(c,[id]);assert rows[id]['error']['code']==code,rows[id]
    def location(path,text,at,length):return dict(uri=path.as_uri(),range=dict(start=position(text,at),end=position(text,at+length)))
    def barrier(c):return call(c,'xlang/analysisStats',{})
    def absent(c):
        barrier(c)
        assert not c.messages.qsize(),list(c.messages.queue)

    try:
        c=client();c.open(entry,overload)
        ids=[send(c,'textDocument/definition',params(text=overload,needle='pick')),
             send(c,'textDocument/hover',params(text=overload,needle='pick')),
             send(c,'textDocument/documentSymbol',dict(textDocument=dict(uri=entry.as_uri())))]
        rows,_=collect(c,ids)
        assert all('error' not in r for r in rows.values()),rows
        assert rows[ids[0]]['result']==location(entry,overload,overload.index('pick',overload.index('pick')+1),4),rows
        assert 'pick' in json.dumps(rows[ids[1]]['result']) and 'string' in json.dumps(rows[ids[1]]['result']),rows
        assert [x['name'] for x in rows[ids[2]]['result']]==['pick','pick','main'],rows
        done('cold-standard-definition-hover-outline-without-custom-check')
        r,_=call(c,'xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=1))
        assert r['analysis']['builds']==1 and r['analysis']['cacheHits']==1,r
        done('same-document-waiters-share-one-real-semantic-build',analysis=r['analysis'])
        c.change(entry,good,2)
        r,_=call(c,'textDocument/definition',params(version=2));assert r==location(entry,good,good.index('result'),6),r
        done('unsaved-edit-analyzes-new-body-automatically')
        expect(c,'textDocument/definition',params(version=1),-32602)
        expect(c,'textDocument/definition',params(checkId=999),-32801)
        expect(c,'textDocument/definition',params(checkId=None),-32602)
        done('supplied-stale-version-and-batch-remain-strict')
        c.stop()

        c=client();imported='import dep\nfunc main(...) as int\n return dep.value()\nend\n';dependency='\n\nexport func value() as int\n return 7\nend\n'
        c.open(entry,imported,3);c.open(dep,dependency,9)
        r,_=call(c,'textDocument/definition',params(text=imported,needle='value'))
        assert r==location(dep,dependency,dependency.index('value'),5),r
        r,_=call(c,'textDocument/documentSymbol',dict(textDocument=dict(uri=dep.as_uri())))
        assert r[0]['name']=='value' and r[0]['selectionRange']['start']['line']==2,r
        done('unsaved-import-navigation-and-dependent-outline-reuse')
        c.change(dep,'\n'+dependency,10)
        r,_=call(c,'textDocument/definition',params(text=imported,needle='value'))
        assert r==location(dep,'\n'+dependency,('\n'+dependency).index('value'),5),r
        done('dependency-edit-invalidates-and-reanalyzes-standard-request')
        c.stop()

        c=client();c.open(entry,good);different='func main(...) as int\n int other = 3\n return other\nend\n';c.open(other,different)
        ids=[send(c,'textDocument/definition',params()),send(c,'textDocument/definition',params(other,different,'other'))]
        rows,_=collect(c,ids)
        assert rows[ids[0]].get('result')==location(entry,good,good.index('result'),6),rows
        assert rows[ids[1]].get('result')==location(other,different,different.index('other'),5),rows
        done('different-entry-documents-serialize-and-return-own-facts');c.stop()

        c=client();broken='func main(...) as int\n return "bad"\nend\n';c.open(entry,broken)
        ids=[send(c,'textDocument/definition',params(text=broken,needle='main')),
             send(c,'textDocument/hover',params(text=broken,needle='main')),
             send(c,'textDocument/documentSymbol',dict(textDocument=dict(uri=entry.as_uri())))]
        rows,seen=collect(c,ids)
        assert [rows[id].get('result','missing') for id in ids]==[None,None,[]],rows
        assert any(m.get('method')=='textDocument/publishDiagnostics' and m['params']['diagnostics'] for m in seen),seen
        r,_=call(c,'xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=1))
        assert not r['ok'] and r['count']==1 and r['analysis']['builds']==1 and r['analysis']['cacheHits']==1,r
        absent(c);done('source-errors-publish-diagnostics-return-empty-and-do-not-spin');c.stop()

        c=client();oversized=''.join('func helper_%d(int value) as int\n return value + 1\nend\n'%i for i in range(6000))+'func main(...) as int\n return 0\nend\n';c.open(entry,oversized)
        id=send(c,'textDocument/definition',params(text=oversized,needle='main'));rows,_=collect(c,[id])
        assert rows[id]['error']['code']==-32603 and 'index exceeds tree limit' in rows[id]['error']['message'],rows
        c.change(entry,good,2);r,_=call(c,'textDocument/definition',params());assert r==location(entry,good,good.index('result'),6),r
        done('resource-failure-remains-error-and-current-body-recovers');c.stop()

        for event in ('cancel','change','close','shutdown'):
            c=client();c.open(entry,heavy)
            id=send(c,'textDocument/documentSymbol',dict(textDocument=dict(uri=entry.as_uri())))
            if event=='cancel':c.send('$/cancelRequest',dict(id=id))
            elif event=='change':c.change(entry,good,2)
            elif event=='close':c.send('textDocument/didClose',dict(textDocument=dict(uri=entry.as_uri())))
            else:
                shutdown=send(c,'shutdown',{});rows,_=collect(c,[id,shutdown]);assert rows[shutdown].get('result','missing') is None,rows
            if event!='shutdown':rows,_=collect(c,[id])
            assert rows[id]['error']['code']==(-32801 if event=='change' else -32800),rows
            if event in ('cancel','change'):
                if event=='cancel':c.change(entry,good,2)
                r,_=call(c,'textDocument/definition',params());assert r==location(entry,good,good.index('result'),6),r
            elif event=='close':
                c.open(entry,good,1);r,_=call(c,'textDocument/definition',params());assert r==location(entry,good,good.index('result'),6),r
            done('pending-'+event+'-has-one-response-and-new-identity-recovers');c.stop()

        c=client();c.open(entry,heavy)
        ids=[send(c,'textDocument/documentSymbol',dict(textDocument=dict(uri=entry.as_uri()))) for _ in range(65)]
        rows,_=collect(c,ids);assert rows[ids[-1]]['error']['code']==-32001,rows[ids[-1]]
        assert all(isinstance(rows[id].get('result'),list) and len(rows[id]['result'])==3001 for id in ids[:-1]),rows[ids[0]]
        r,_=call(c,'xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=1));assert r['analysis']['builds']==1 and r['analysis']['cacheHits']==1,r
        done('64-canonical-waiters-are-bounded-and-coalesce-one-build');c.stop()

        c=client();c.open(entry,heavy);id=send(c,'textDocument/documentSymbol',dict(textDocument=dict(uri=entry.as_uri())))
        c.send('textDocument/definition',params(text=heavy,needle='helper_2999'),id);rows,_=collect(c,[id]);assert rows[id]['error']['code']==-32600,rows
        c.send('$/cancelRequest',dict(id='unknown'));c.change(entry,good,2)
        r,_=call(c,'textDocument/definition',params());assert r==location(entry,good,good.index('result'),6),r
        oversized_id='q'*65536;c.send('textDocument/hover',params(),oversized_id);rows,_=collect(c,[oversized_id]);assert rows[oversized_id]['error']['code']==-32602
        done('duplicate-and-oversized-IDs-rejected-unknown-cancel-harmless');c.stop()

        c=client('cli');c.open(entry,good);expect(c,'textDocument/definition',params(),-32801)
        done('explicit-cli-backend-retains-manual-semantics');c.stop()
        assert all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            c.stop();(root/f'transcript-{i:02}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8');(root/f'stderr-{i:02}.log').write_text(c.stderr,encoding='utf-8')
        (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(root/'result.json'))))


if __name__=='__main__':main()
