"""Real imported function-header fragments retain other parsed declarations.

The native fixture proves actual chunk identity and full fresh AST/index/IR.
This suite checks configured unsaved project inputs and exact wire work.
"""
from pathlib import Path
import argparse,hashlib,json,time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize,source


def program(helpers,generation):
    body=source(helpers,0,True)
    if generation>=3:
        original='func identity(int value) as int\r\n return value\r\nend'
        assert original in body
        body=body.replace(original,'func identity(int input) as int\r\n return input\r\nend',1)
    name='spare' if generation>=4 else 'unused'
    arg='longer_input' if generation>=6 else 'input' if generation else 'value'
    default='=' if generation==5 else '=2' if generation>=2 else ''
    return body+f'#doc\r\ndesc:\r\n unused helper😀\r\n#end\r\nfunc {name}(int {arg}{default}) as int\r\n return {arg}\r\nend\r\n'


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(server):sha(server)};cases=[];commands=[]
    report=dict(status='failed',scope=__doc__,cases=cases,commands=commands,protected=protected)
    try:
        for helpers in [16,96]:
            folder=out/str(helpers);folder.mkdir();entry,dep=[folder/f for f in ['main.xl','dep.xl']]
            main='import dep\r\nfunc main(...) as int\r\n return dep.compute(7)\r\nend\r\n'
            for path in [entry,dep]:path.write_bytes(b'// real analysis uses unsaved overlays\n');protected[str(path)]=sha(path)
            c=AutomaticClient(server,folder/'absent/xl.exe',folder)
            def check():
                until=time.monotonic()+30
                while True:
                    c.request+=1;c.send('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=1),c.request);r,_=c.reply(c.request)
                    if r.get('error',{}).get('code')!=-32001:break
                    assert time.monotonic()<until;time.sleep(.002)
                assert 'error' not in r,r
                value=r['result'];assert value['version']==1 and value['analysisContext']['epoch']==1
                assert value['analysis']['workerThreadId']!=value['analysis']['captureThreadId'];return value
            try:
                initialize(c,entry,[entry,dep],folder);c.open(entry,main,1)
                for generation in range(7):
                    text=program(helpers,generation)
                    if not generation:c.open(dep,text,1)
                    else:c.change(dep,text,generation+1)
                    r=check();a=r['analysis'];work=a['lastBuildModuleSyntax'];copied=a['lastBuildMaterialization'];assert copied['calls'] and copied['pooledCopies']==copied['calls'] and copied['nodes'] and not copied['heapCopies'] and not copied['poolFallbacks'];assert r['ok']==(generation!=5) and (r['count']==0)==(generation!=5),r
                    if generation in [1,2,3,4,6]:assert work['full']==0 and work['fragments']==1 and work['reused']==helpers+2 and work['bytes']<256,work
                    else:assert work['full']==1,work
                    if generation in [1,2,4]:assert a['semanticReads']['reused']>=helpers,a
                    if generation==3:assert a['semanticReads']['rejected']>=helpers//2,a
                    documents={Path(v['path']):v for v in r['documents']};assert documents[dep]['version']==generation+1 and documents[dep]['origin']=='overlay',r
                    cases.append(dict(helpers=helpers,generation=generation,phase='changed',analysis=a))
                    repeated=check();assert repeated['analysis']['builds']==a['builds'] and repeated['analysis']['lastBuildModuleSyntax']==work
                    cases.append(dict(helpers=helpers,generation=generation,phase='exact',analysis=repeated['analysis']))
            finally:
                stderr=c.stop();(folder/'stderr.log').write_text(stderr,encoding='utf8');(folder/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
                commands.append(dict(server=str(server),exit=c.process.returncode));assert not c.process.returncode
        assert len(cases)==28 and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(out/'result.json'))))


if __name__=='__main__':main()
