"""Verify real foreground cursor reuse over stdio; no executor or UI timing claim."""
import argparse
import hashlib
import json
import os
from pathlib import Path
from check_language_snapshots import Client
from check_language_definitions import position


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True)
    entry,dep,extra=root/'main.xl',root/'dep.xl',root/'extra.xl'
    disk=b'// disk stays untouched; completion reads the open overlay\n';entry.write_bytes(disk)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [server,entry,Path(__file__),Path(__file__).with_name('check_language_snapshots.py'),Path(__file__).with_name('check_language_definitions.py')]}
    clients=[];cases=[];request=10
    report=dict(status='failed',scope='actual retained native cursor actor and guarded current-version protocol; not UI latency or changing-function incrementality',protected=protected,cases=cases)

    def done(name,**data):cases.append(dict(name=name,passed=True,**data))

    def client(backend='native'):
        c=Client(server,root/'absent-executor/xl.exe',root);clients.append(c)
        c.send('initialize',dict(capabilities={},initializationOptions=dict(checkOnSave=False,automaticDiagnostics=False,completionBackend=backend)),1)
        value,_=c.reply(1);cap=value['result']['capabilities']['experimental']
        assert cap['retainedCursorCompletion']==(backend=='native'),cap
        c.send('initialized',{});return c

    def params(text,marker,version,path=entry,entry_path=None):
        value=dict(textDocument=dict(uri=path.as_uri()),version=version,position=position(text,text.rindex(marker)+len(marker)))
        if entry_path:value['entryDocument']=dict(uri=entry_path.as_uri())
        return value

    def check_result(reply,version,labels,builds=None,hits=None,invalidations=None,owner=None):
        assert 'error' not in reply,reply
        r=reply['result'];assert r['version']==version and r['available']==bool(labels),r
        assert [x['label'] for x in r['items']]==labels,r
        a=r['analysis'];assert a['workerThreadId']>0 and a['workerThreadId']!=a['captureThreadId'],a
        assert a['adapterThreadId'] not in (a['captureThreadId'],a['workerThreadId']),a
        if builds is not None:assert a['builds']==builds,a
        if hits is not None:assert a['cacheHits']==hits,a
        if invalidations is not None:assert a['invalidations']==invalidations,a
        if owner is not None:assert a['workerThreadId']==owner,a
        return r

    def complete(c,text,marker,version,labels,builds=None,hits=None,invalidations=None,owner=None,path=entry,entry_path=None):
        nonlocal request
        request+=1;c.send('textDocument/completion',params(text,marker,version,path,entry_path),request)
        reply,seen=c.reply(request)
        return check_result(reply,version,labels,builds,hits,invalidations,owner),seen

    try:
        text='func main(...) as int\n int result = 7\n return res\nend\n'
        c=client();c.open(entry,text,1)
        r,_=complete(c,text,'return res',1,['result'],1,0);owner=r['analysis']['workerThreadId']
        done('native-without-executor',workerThreadId=owner)
        for i in range(16):complete(c,text,'return res',1,['result'],1,i+1,owner=owner)
        done('sixteen-validated-hits-on-one-actor')
        c.change(entry,text,2);complete(c,text,'return res',2,['result'],1,17,owner=owner)
        done('new-same-body-version-restamps-cached-facts')
        complete(c,text,'return re',2,['result'],2,17,owner=owner)
        complete(c,text,'return res',2,['result'],3,17,owner=owner)
        done('different-cursor-rebuilds-single-entry-cache')
        text=text.replace('result = 7','result = 8');c.change(entry,text,3)
        complete(c,text,'return res',3,['result'],4,17,owner=owner)
        done('changed-body-recomputes')
        c.open(extra,'func extra() as int\n return 0\nend\n',1)
        r,_=complete(c,text,'return res',3,['result'],5,17,owner=owner);assert r['analysis']['documents']==2
        c.send('textDocument/didClose',dict(textDocument=dict(uri=extra.as_uri())))
        r,_=complete(c,text,'return res',3,['result'],6,17,owner=owner);assert r['analysis']['documents']==1
        done('open-document-set-add-remove-recomputes')
        c.send('textDocument/didClose',dict(textDocument=dict(uri=entry.as_uri())));c.open(entry,text,1)
        complete(c,text,'return res',1,['result'],7,17,owner=owner)
        done('close-reopen-new-identity-recomputes')
        request+=1;c.send('textDocument/completion',params(text,'return res',0),request);reply,_=c.reply(request);assert reply['error']['code']==-32602,reply
        complete(c,text,'return res',1,['result'],7,18,owner=owner)
        done('stale-version-rejection-preserves-cache');c.stop()

        imported='import dep\nfunc main(...) as int\n return dep.value()\nend\n'
        dependency='export func value() as int\n int member = 7\n return mem\nend\n'
        c=client();c.open(entry,imported,10);c.open(dep,dependency,31)
        r,_=complete(c,dependency,'return mem',31,['member'],1,0,path=dep,entry_path=entry)
        owner=r['analysis']['workerThreadId'];assert r['items'][0]['data']['declaration']['uri']==dep.as_uri(),r
        c.change(dep,dependency,32);complete(c,dependency,'return mem',32,['member'],1,1,owner=owner,path=dep,entry_path=entry)
        c.change(entry,imported,11);complete(c,dependency,'return mem',32,['member'],1,2,owner=owner,path=dep,entry_path=entry)
        done('unsaved-import-query-and-entry-versions-remain-distinct')
        dependency=dependency.replace('member = 7','member = 8');c.change(dep,dependency,33)
        complete(c,dependency,'return mem',33,['member'],2,2,owner=owner,path=dep,entry_path=entry)
        imported=imported.replace('func main','int globalFlag = 1\nfunc main');c.change(entry,imported,12)
        complete(c,dependency,'return mem',33,['member'],3,2,owner=owner,path=dep,entry_path=entry)
        done('changed-import-query-or-entry-recomputes');c.stop()

        saved=b'export class Box\n int member\nend\n';dep.write_bytes(saved)
        members='import dep\nfunc main(...) as int\n auto box = dep.Box()\n return box.\nend\n'
        c=client();c.open(entry,members,1)
        r,_=complete(c,members,'return box.',1,['member'],1,0);owner=r['analysis']['workerThreadId']
        complete(c,members,'return box.',1,['member'],1,1,owner=owner)
        replacement=root/'replacement.tmp';replacement.write_bytes(saved);os.replace(replacement,dep)
        complete(c,members,'return box.',1,['member'],2,1,1,owner)
        done('same-bytes-disk-identity-invalidates-cache')
        dep.write_bytes(b'export class Box\n int newMember\nend\n')
        complete(c,members,'return box.',1,['newMember'],3,1,2,owner)
        done('changed-disk-facts-recompute-before-publication');c.stop()

        absent=root/'absent_dep.xl';assert not absent.exists()
        missing=members.replace('import dep','import absent_dep').replace('dep.Box()','absent_dep.Box()')
        c=client();c.open(entry,missing,1)
        r,_=complete(c,missing,'return box.',1,[],1,0);owner=r['analysis']['workerThreadId']
        complete(c,missing,'return box.',1,[],1,1,owner=owner)
        absent.write_bytes(saved);complete(c,missing,'return box.',1,['member'],2,1,1,owner)
        done('new-negative-import-candidate-invalidates-unavailable-projection');c.stop()

        c=client();heavy=''.join('func helper_%d(int value) as int\n return value + 1\nend\n'%i for i in range(1000))
        checked=heavy+'func main(...) as int\n int result = 7\n return result\nend\n'
        c.open(entry,checked,1);c.open(extra,text,1)
        request+=1;background=request;c.check(entry,1,background)
        r,seen=complete(c,text,'return res',1,['result'],1,0,path=extra)
        assert not any(x.get('id')==background for x in seen),seen
        background_reply,_=c.reply(background);assert background_reply['result']['ok'] and not background_reply['result']['count'],background_reply
        assert background_reply['result']['analysis']['workerThreadId']!=r['analysis']['workerThreadId']
        complete(c,text,'return res',1,['result'],1,1,owner=r['analysis']['workerThreadId'],path=extra)
        done('foreground-completion-independent-of-real-background-full-check');c.stop()

        c=client();heavy=heavy+text;c.open(entry,heavy,1);c.open(extra,text,1)
        request+=1;cancelled=request;c.send('textDocument/completion',params(heavy,'return res',1),cancelled)
        c.send('$/cancelRequest',dict(id=cancelled));reply,_=c.reply(cancelled);assert reply['error']['code']==-32800,reply
        complete(c,text,'return res',1,['result'],path=extra)
        done('cancelled-cursor-job-does-not-block-latest-valid-result');c.stop()
        c=client('cli');done('explicit-cli-does-not-advertise-retained-native-cursor');c.stop()
        assert entry.read_bytes()==disk
        assert all(sha(p)==h for p,h in protected.items())
        report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            c.stop();(root/('transcript-%d.json'%i)).write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8')
            (root/('stderr-%d.log'%i)).write_text(c.stderr,encoding='utf-8')
        (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(root/'result.json'))))


if __name__=='__main__':main()
