"""Real stream receipts for the online URI/version binding set."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import time
from check_lsp_automatic_diagnostics import AutomaticClient


def digest(bindings):
    rows=sorted((hashlib.sha256(uri.encode()).digest(),version) for uri,version in bindings.items())
    return hashlib.sha256(b'xlang-input-state-v1\0'+struct.pack('>I',len(rows))+b''.join(uri+struct.pack('>i',version) for uri,version in rows)).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [server,Path(__file__),Path(__file__).with_name('check_lsp_automatic_diagnostics.py'),Path(__file__).with_name('check_language_snapshots.py')]}
    clients=[];cases=[];status='failed'
    text='func main(...) as int\n string text = "中文😀"; return 0\nend\n'
    def done(name,**values):cases.append(dict(name=name,passed=True,**values))
    def client(name,automatic=False):
        folder=root/name;folder.mkdir();c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c)
        c.send('initialize',dict(initializationOptions=dict(automaticDiagnostics=automatic,checkOnSave=False,diagnosticDelay=0)),1)
        reply,_=c.reply(1);assert reply['result']['capabilities']['experimental']['inputState'] is True,reply
        c.send('initialized',{});return c,folder
    def receipt(c,bindings):
        r=c.call('xlang/inputState');assert set(r)=={'schema','generation','documents','sha256'} and r['schema']==1,r
        assert r['generation'].isdigit() and r['documents']==len(bindings) and r['sha256']==digest(bindings),r
        return r
    def opened(c,path,body=text,version=1):
        path.write_bytes(b'// unsaved input-state fixture\n');protected[str(path)]=sha(path);c.open(path,body,version)
    try:
        folder=root/'pre-init';folder.mkdir();c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c)
        c.send('xlang/inputState',{},1);reply,_=c.reply(1);assert reply['error']['code']==-32002,reply;c.stop()
        done('requires-initialization')
        c,folder=client('main');r=receipt(c,{});assert r['generation']=='0',r
        done('empty-set-independent-python-SHA256-reference')
        a,b=folder/'入口😀.xl',folder/'dependency.xl';opened(c,a);opened(c,b);bindings={a.as_uri():1,b.as_uri():1};first=receipt(c,bindings)
        done('receipt-follows-two-open-notifications-in-stream')
        other,otherfolder=client('reordered');other.open(b,text,1);other.open(a,text,1)
        reordered=receipt(other,bindings);assert reordered['sha256']==first['sha256'];other.stop()
        done('independent-server-open-order-does-not-change-binding-digest')
        c.change(a,text.replace('return 0','return 1'),2);bindings[a.as_uri()]=2;edited=receipt(c,bindings)
        assert edited['sha256']!=first['sha256'] and int(edited['generation'])>int(first['generation']),edited
        done('accepted-edits-update-versions-and-generation')
        c.change(a,text.replace('return 0','return 1'),3);bindings[a.as_uri()]=3;unchanged=receipt(c,bindings)
        assert unchanged['sha256']!=edited['sha256'],unchanged
        done('same-body-with-new-wire-version-is-a-different-binding')
        c.send('textDocument/didChange',dict(textDocument=dict(uri=a.as_uri(),version=4),contentChanges=[dict(text='x',range=dict(start=dict(line=0,character=0),end=dict(line=0,character=0))),dict(text='bad',range=dict(start=dict(line=999,character=0),end=dict(line=999,character=0)))]))
        assert receipt(c,bindings)==unchanged
        done('rejected-multi-edit-is-atomic-and-not-acknowledged')
        c.change(a,text,1);assert receipt(c,bindings)==unchanged
        c.open(a,text,9);assert receipt(c,bindings)==unchanged
        done('stale-version-and-duplicate-open-do-not-change-receipt')
        c.send('textDocument/didClose',dict(textDocument=dict(uri=a.as_uri())));bindings.pop(a.as_uri());receipt(c,bindings)
        c.open(a,'func main(...) as int\n return 7\nend\n',1);bindings[a.as_uri()]=1;reopened=receipt(c,bindings)
        assert reopened['sha256']==first['sha256'] and int(reopened['generation'])>int(first['generation']),reopened
        done('close-reopen-can-repeat-binding-digest-so-generation-and-model-identity-are-required')
        event_count=len(c.events)
        for i,value in enumerate([None,[],{'unexpected':True}]):
            c.batch([dict(jsonrpc='2.0',method='xlang/inputState',params=value,id=800+i)]);reply,_=c.reply(800+i);assert reply['error']['code']==-32602,reply
        c.send('xlang/inputState',{},'state-中文😀');reply,_=c.reply('state-中文😀');assert reply['result']==reopened,reply
        assert len(c.events)==event_count and all(not e['diagnostics'] for e in c.events) and c.stats()['submittedChecks']==0
        done('strict-parameters-string-request-ID-and-no-analysis-side-effects');c.stop()

        c,folder=client('capacity');bindings={};batch=[]
        for i in range(64):
            path=folder/('文档'+str(i)+'.xl');uri=path.as_uri();bindings[uri]=i-32
            batch.append(dict(jsonrpc='2.0',method='textDocument/didOpen',params=dict(textDocument=dict(uri=uri,languageId='xlang',text=text,version=i-32))))
        batch.append(dict(jsonrpc='2.0',method='xlang/inputState',params={},id=900));c.batch(batch);reply,_=c.reply(900);r=reply['result']
        assert r['documents']==64 and r['sha256']==digest(bindings),r
        done('64-bindings-batched-receipt-and-signed-versions')
        c.open(folder/'overflow.xl',text,1);assert receipt(c,bindings)==r
        done('rejected-65th-document-does-not-advance-state')
        batch=[dict(jsonrpc='2.0',method='textDocument/didClose',params=dict(textDocument=dict(uri=uri))) for uri in reversed(bindings)]
        batch.append(dict(jsonrpc='2.0',method='xlang/inputState',params={},id=901));c.batch(batch);reply,_=c.reply(901);r=reply['result']
        assert r['documents']==0 and r['sha256']==digest({}) and int(r['generation'])==128,r
        done('64-closes-drain-to-empty-in-stream-order');c.stop()

        c,folder=client('active-analysis',True);entry=folder/'large.xl'
        large=''.join(f'func value{i}() as int\n return {i}\nend\n' for i in range(10000))+text
        opened(c,entry,large);before=c.wait_for(lambda s:s['checking']);started=time.perf_counter();r=receipt(c,{entry.as_uri():1});elapsed=(time.perf_counter()-started)*1000;after=c.stats()
        assert before['submittedChecks']==after['submittedChecks']==1 and after['checking'],after
        done('receipt-returns-while-real-native-analysis-is-still-pending',receiptMs=elapsed,generation=r['generation']);c.stop()
        assert all(sha(p)==h for p,h in protected.items());done('source-tool-and-original-fixture-bytes-preserved');status='passed'
    finally:
        for i,c in enumerate(clients):
            stderr=c.stop();(root/f'server-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8');(root/f'server-{i}.stderr').write_text(stderr,encoding='utf-8')
        (root/'result.json').write_text(json.dumps(dict(status=status,scope='bounded URI/version synchronization receipts, not source content or transitive semantic validity',protected=protected,cases=cases),ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(dict(status=status,cases=len(cases),proof=str(root/'result.json'))))


if __name__=='__main__':main()
