"""Actual imported cursor syntax reuse with opened/closed configured entries."""
import argparse,hashlib,json
from pathlib import Path
from check_lsp_module_semantics import initialize
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(server):sha(server),str(Path(__file__)):sha(__file__)};cases=[];clients=[];report=dict(status='failed',scope=__doc__+' Original source capture/semantic query retained;not semantic/IR or UI performance acceptance.',cases=cases,protected=protected)
    try:
        for opened in [False,True]:
            folder=root/('opened' if opened else 'disk');folder.mkdir();entry,dep,support=[folder/n for n in ['main.xl','dep.xl','support.xl']];main='import bridge\nfunc main(...) as int\n return bridge.value()\nend\n';entry.write_bytes(main.encode());protected[str(entry)]=sha(entry);c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c);bridge=folder/'bridge.xl';bridge.write_bytes('import dep\nexport func value() as int\n return dep.value()\nend\n'.encode());protected[str(bridge)]=sha(bridge);initialize(c,entry,[entry,dep,support,bridge],folder)
            if opened:c.open(entry,main,1)
            supportBody='export func number() as int\n return 7\nend\n';c.open(support,supportBody,1)
            prefix='\ufeffimport support\r\n'+''.join(f'func helper_{i}(int value) as int\r\n return value+1\r\nend\r\n' for i in range(30))
            def body(i,marker='mem'):
                return prefix+f'export func value() as int\r\n string 中文 = "😀"\r\n int edited = {i}\r\n auto member = support.number()\r\n return {marker}\r\nend\r\n'
            def done(name,**data):cases.append(dict(name=('opened-' if opened else 'disk-')+name,passed=True,**data))
            def complete(text,marker,version,labels=('member',)):
                c.request+=1;c.send('textDocument/completion',dict(textDocument=dict(uri=dep.as_uri()),version=version,position=position(text,text.rindex(marker)+len(marker))),c.request);reply,_=c.reply(c.request);assert 'error' not in reply,reply;r=reply['result'];assert r['version']==version and r['available']==bool(labels) and [v['label'] for v in r['items']]==list(labels),r
                a=r['analysis'];assert a['workerThreadId']>0 and a['workerThreadId']!=a['captureThreadId'] and a['adapterThreadId'] not in (a['workerThreadId'],a['captureThreadId']),a
                if labels:assert r['items'][0]['data']['declaration']['uri']==dep.as_uri(),r
                return r
            text=body(0);version=7;c.open(dep,text,version);r=complete(text,'return mem',version);a=r['analysis'];assert a['cursorFullParses']==1 and not a['cursorFreshBuilds'] and not a['cursorFragmentParses'],a;done('initial-owned-imported-cursor',analysis=a)
            for i in range(20):
                marker=['mem','memb','membe'][i%3];updated=body(i+1,marker);start=text.index(' int edited =');end=text.rindex('\r\nend');replacement=updated[start:updated.rindex('\r\nend')];version+=1;c.send('textDocument/didChange',dict(textDocument=dict(uri=dep.as_uri(),version=version),contentChanges=[dict(range=dict(start=position(text,start),end=position(text,end)),text=replacement)]));text=updated;r=complete(text,'return '+marker,version);a=r['analysis'];assert a['cursorFullParses']==1 and a['cursorFragmentParses']==i+1 and a['cursorReusedDeclarations']==31*(i+1) and not a['cursorFreshBuilds'],a
            done('twenty-body-and-prefix-range-edits',analysis=a)
            version+=1;c.change(dep,text,version);r=complete(text,'return '+marker,version);a=r['analysis'];assert a['cacheHits']==1 and a['cursorFragmentParses']==20,a;done('same-body-version-restamps-retained-candidates')
            c.change(support,'export func number() as string\n return "changed"\nend\n',2);r=complete(text,'return '+marker,version);a=r['analysis'];assert a['cursorSyntaxHits']==1 and 'string' in r['items'][0]['detail'] and not a['cursorFreshBuilds'],r;done('unchanged-cursor-still-rechecks-dependency-types',analysis=a)
            c.send('textDocument/didClose',dict(textDocument=dict(uri=dep.as_uri())));version=1;c.open(dep,text,version);r=complete(text,'return '+marker,version);a=r['analysis'];assert a['cursorFullParses']==2 and not a['cursorFreshBuilds'],a;done('reopened-import-identity-starts-new-parser-cache')
            dot='\ufeffexport class Box\r\n int member\r\nend\r\nexport func value() as int\r\n auto box = Box()\r\n int edited = 1\r\n return box.\r\nend\r\n';version+=1;c.change(dep,dot,version);r=complete(dot,'return box.',version);before=r['analysis']['cursorFragmentParses'];edited=dot.replace('edited = 1','edited = 2');version+=1;c.change(dep,edited,version);r=complete(edited,'return box.',version);assert r['analysis']['cursorFragmentParses']==before+1 and not r['analysis']['cursorFreshBuilds'],r;done('receiver-member-dot-projection-reuses-function-syntax')
            bad='export func value() as int\n string broken = "unterminated\n return mem\nend\n';version+=1;c.change(dep,bad,version);complete(bad,'return mem',version,());version+=1;c.change(dep,edited,version);r=complete(edited,'return box.',version);done('lexical-error-withdrawal-and-recovery')
            deep='export func value() as int\n int member = 7\n int deepValue = 1'+' + 1'*550+'\n return mem\nend\n';version+=1;c.change(dep,deep,version)
            # The next request alone exercises the explicit native fallback.
            fresh=r['analysis']['cursorFreshBuilds'];r=complete(deep,'return mem',version);assert r['analysis']['cursorFreshBuilds']==fresh+1,r;done('deep-copy-limit-preserves-original-native-query',analysis=r['analysis']);c.stop();assert c.process.returncode==0
        assert len(cases)==16 and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            c.stop();(root/f'transcript-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
        (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(root/'result.json')),ensure_ascii=False))
if __name__=='__main__':main()
