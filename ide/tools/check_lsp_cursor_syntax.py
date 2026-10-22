"""Verify actual cursor syntax reuse, fresh semantic checks and bounded fallback over stdio."""
import argparse,hashlib,json
from pathlib import Path
from check_language_snapshots import Client
from check_language_definitions import position

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    entry,dep,extra=root/'入口.xl',root/'dep.xl',root/'extra.xl';entry.write_bytes(b'// saved entry remains unchanged\n')
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [server,entry,Path(__file__),Path(__file__).with_name('check_language_snapshots.py'),Path(__file__).with_name('check_language_definitions.py')]}
    clients=[];cases=[];request=10
    report=dict(status='failed',scope='actual native entry cursor syntax counters, original semantic rebuilds, projections and explicit native fallback; not UI latency or semantic/IR incrementality',protected=protected,cases=cases)
    def done(name,**data):cases.append(dict(name=name,passed=True,**data))
    def client(backend='native'):
        c=Client(server,root/'absent-executor/xl.exe',root);clients.append(c)
        c.send('initialize',dict(capabilities={},initializationOptions=dict(checkOnSave=False,automaticDiagnostics=False,completionBackend=backend)),1)
        r,_=c.reply(1);assert r['result']['capabilities']['experimental']['retainedCursorSyntax']==(backend=='native'),r
        c.send('initialized',{});return c
    fields=['cursorFullParses','cursorFragmentParses','cursorReusedDeclarations','cursorParsedBytes','cursorSyntaxHits','cursorFreshBuilds']
    def complete(c,text,marker,version,labels,expected=None,path=entry,entry_path=None):
        nonlocal request
        request+=1;params=dict(textDocument=dict(uri=path.as_uri()),version=version,position=position(text,text.rindex(marker)+len(marker)))
        if entry_path is not None:params['entryDocument']=dict(uri=entry_path.as_uri())
        c.send('textDocument/completion',params,request);reply,_=c.reply(request);assert 'error' not in reply,reply
        r=reply['result'];assert r['version']==version and r['available']==bool(labels),r
        assert [v['label'] for v in r['items']]==labels,r
        a=r['analysis'];assert all(type(a[k]) is int and a[k]>=0 for k in fields),a
        assert a['workerThreadId']>0 and a['workerThreadId']!=a['captureThreadId'] and a['adapterThreadId'] not in (a['workerThreadId'],a['captureThreadId']),a
        for k,v in (expected or {}).items():assert a[k]==v,(k,v,a)
        if labels==['result'] and marker.startswith('return re'):
            start=text.rindex('return res')+len('return ')
            assert r['items'][0]['textEdit']==dict(newText='result',range=dict(start=position(text,start),end=position(text,start+3))),r
        return r
    try:
        count=100;prefix=''.join('func helper_%d(int value) as int\n return value + 1\nend\n'%i for i in range(count))
        text=prefix+'func main(...) as int\n int result = 7\n return res\nend\n';version=1;c=client();c.open(entry,text,version)
        r=complete(c,text,'return res',version,['result'],dict(cursorFullParses=1,cursorFragmentParses=0,cursorFreshBuilds=0));done('entry-native-without-executor')
        for i in range(4):complete(c,text,'return res',version,['result'],dict(cursorFullParses=1,cursorFragmentParses=0,cacheHits=i+1))
        done('exact-query-hits-do-not-invent-parser-work')
        where=text.index('result = 7')+len('result = ');version+=1
        c.send('textDocument/didChange',dict(textDocument=dict(uri=entry.as_uri(),version=version),contentChanges=[dict(range=dict(start=position(text,where),end=position(text,where+1)),text='8')]))
        text=text[:where]+'8'+text[where+1:]
        r=complete(c,text,'return res',version,['result'],dict(cursorFullParses=1,cursorFragmentParses=1,cursorReusedDeclarations=100,cursorFreshBuilds=0));done('real-utf16-range-edit-parses-one-function')
        for i in range(6):
            text=prefix+'func main(...) as int\n string 中文 = "😀-%d"\n int result = %d\n return res\nend\n'%(i,i+9);version+=1;c.change(entry,text,version)
            r=complete(c,text,'return res',version,['result'],dict(cursorFullParses=1,cursorFragmentParses=i+2,cursorReusedDeclarations=100*(i+2),cursorFreshBuilds=0))
        assert r['analysis']['cursorParsedBytes']<len(text.encode())+7*256,r
        done('repeated-unicode-edits-reuse-declarations',analysis=r['analysis'])
        version+=1;c.change(entry,text,version);r=complete(c,text,'return res',version,['result'],dict(cursorFullParses=1,cursorFragmentParses=7,cursorFreshBuilds=0));done('same-body-version-reuses-whole-query')
        before=r['analysis']['builds'];c.open(extra,'func extra() as int\n return 0\nend\n',1)
        complete(c,text,'return res',version,['result'],dict(cursorSyntaxHits=1,builds=before+1,cursorFullParses=1))
        c.send('textDocument/didClose',dict(textDocument=dict(uri=extra.as_uri())))
        complete(c,text,'return res',version,['result'],dict(cursorSyntaxHits=2,builds=before+2,cursorFullParses=1));done('input-set-changes-reuse-only-syntax-and-rebuild-semantics')
        complete(c,text,'return re',version,['result'],dict(cursorFullParses=2,cursorFragmentParses=7))
        complete(c,text,'return res',version,['result'],dict(cursorFullParses=3,cursorFragmentParses=7));done('changed-cursor-full-query-parse-removes-old-marker')
        text=text.replace('helper_0(', 'renamed_helper_0(');version+=1;c.change(entry,text,version)
        complete(c,text,'return res',version,['result'],dict(cursorFullParses=4));done('header-edit-full-query-parse')
        text=text.replace('return value + 1','return value + 2',1);version+=1;c.change(entry,text,version)
        complete(c,text,'return res',version,['result'],dict(cursorFullParses=5));done('other-function-edit-does-not-reuse-old-query-marker')
        c.send('textDocument/didClose',dict(textDocument=dict(uri=entry.as_uri())));version=1;c.open(entry,text,version)
        complete(c,text,'return res',version,['result'],dict(cursorFullParses=6));done('reopened-identity-starts-new-parser-cache')
        bad=text.replace(' int result =',' string broken = "unterminated\n int result =',1);version+=1;c.change(entry,bad,version)
        complete(c,bad,'return res',version,[],dict(cursorFullParses=7,cursorFreshBuilds=0))
        version+=1;c.change(entry,text,version);complete(c,text,'return res',version,['result'],dict(cursorFullParses=8,cursorFreshBuilds=0));done('lexical-error-cache-and-recovery-match-fresh-semantics')
        request+=1;c.send('textDocument/completion',dict(textDocument=dict(uri=entry.as_uri()),version=version-1,position=position(text,text.rindex('return res')+10)),request)
        reply,_=c.reply(request);assert reply['error']['code']==-32602,reply;done('stale-version-rejected');c.stop()

        text='import dep\nfunc main(...) as int\n int result = dep.value()\n return res\nend\n';good='export func value() as int\n return 7\nend\n'
        c=client();c.open(entry,text,1);c.open(dep,good,1)
        complete(c,text,'return res',1,['result'],dict(cursorFullParses=1,cursorFreshBuilds=0))
        c.change(dep,good.replace('return 7','return "bad"'),2)
        complete(c,text,'return res',1,[],dict(cursorFullParses=1,cursorSyntaxHits=1,cursorFreshBuilds=0))
        c.change(dep,good.replace('return 7','return 8'),3)
        complete(c,text,'return res',1,['result'],dict(cursorFullParses=1,cursorSyntaxHits=2,cursorFreshBuilds=0));done('changed-dependency-rechecks-types-despite-entry-syntax-hit');c.stop()

        text='func main(...) as int\n int result = 7\n int deepValue = 1'+' + 1'*550+'\n return res\nend\n'
        c=client();c.open(entry,text,1);complete(c,text,'return res',1,['result'],dict(cursorFullParses=1,cursorFreshBuilds=1));done('clone-depth-limit-preserves-valid-native-language-surface');c.stop()

        text='import dep\nfunc main(...) as int\n return dep.value()\nend\n';target='export func value() as int\n int member = 7\n return mem\nend\n'
        c=client();c.open(entry,text,1);c.open(dep,target,1)
        complete(c,target,'return mem',1,['member'],dict(cursorFullParses=0,cursorFragmentParses=0,cursorFreshBuilds=1),path=dep,entry_path=entry)
        done('imported-query-route-reports-fresh-native-work');c.stop()
        c=client('cli');done('cli-does-not-advertise-retained-cursor-syntax');c.stop()
        assert all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            c.stop();(root/('transcript-%d.json'%i)).write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
        (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(root/'result.json')),ensure_ascii=False))
if __name__=='__main__':main()
