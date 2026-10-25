"""Exercise checked external declaration bindings as actual IR cache inputs.

Function insertion/removal,local edits and imported header/body changes must
preserve current compiler semantics and real raw-fragment reuse. Store/domain
isolation and complete AST/index/IR parity are independently checked natively.
"""
from pathlib import Path
import argparse,hashlib,json,time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize
from check_language_definitions import position
from check_lsp_analysis_configuration import LISTS,PROFILE

def source(helpers=40,prefix=False,local=False,body=1,rename=False,broken=False):
    name='input' if rename else 'value'
    s='\ufeff// 原始模块 中文😀\r\n'
    if prefix:s+='func inserted(int value) as int\r\n return value\r\nend\r\n'
    s+=f'export func compute(int {name}) as int\r\n return '+('"wrong"' if broken else f'{name}+{body}')+'\r\nend\r\n'
    for i in range(helpers):
        s+=f'func helper_{i}(int value) as int\r\n int local=value+{i}\r\n'
        if local and i==5:s+=' int inserted=value+5\r\n'
        s+=' if local > 0 then\r\n'
        if i in [0,helpers-1]:s+=' return helper_1(local)\r\n'
        else:s+=' local=local+1\r\n'*12+' return local\r\n'
        s+=' end\r\n return 0\r\nend\r\n'
    return s

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();server=a.server.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(server):sha(server),str(Path(__file__)):sha(__file__)};clients=[];cases=[];report=dict(status='failed',scope=__doc__,cases=cases,protected=protected)
    try:
        for opened in [False,True]:
            folder=out/('opened' if opened else 'disk');folder.mkdir();entry,dep,extra=[folder/n for n in ['main.xl','dep.xl','extra.xl']];main='import dep\nfunc main(...) as int\n int unique_main=dep.compute(7)\n return unique_main\nend\n';entry.write_text(main,encoding='utf8');protected[str(entry)]=sha(entry)
            c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c);initialize(c,entry,[entry,dep,extra],folder);epoch=1;version=1;current=source();c.open(dep,current,version)
            if opened:c.open(entry,main,1)
            def done(name,**data):cases.append(dict(name=('opened-' if opened else 'disk-')+name,passed=True,**data))
            def check(okay=True):
                until=time.monotonic()+30
                while True:
                    c.request+=1;c.send('xlang/check',dict(textDocument=dict(uri=dep.as_uri()),version=version),c.request);v,_=c.reply(c.request)
                    if v.get('error',{}).get('code')!=-32001:break
                    assert time.monotonic()<until;time.sleep(.001)
                assert 'error' not in v,v;v=v['result'];assert v['version']==version and v['analysisContext']['epoch']==epoch and v['ok']==okay and bool(v['count'])!=okay,v
                if okay:assert next(d for d in v['documents'] if Path(d['path'])==dep)['version']==version
                return v
            first=check();eligible=first['analysis']['irFragments']['captured'];assert eligible==39 and first['analysis']['irFragments']['declined']==3 and not first['analysis']['irFragments']['reused'];done('cold-capture',ir=first['analysis']['irFragments'])
            exact=check();assert exact['analysis']['builds']==first['analysis']['builds'] and exact['analysis']['irFragments']==first['analysis']['irFragments'];done('exact-hit-is-not-new-work')
            for name,options,changed in [('insert-function',dict(prefix=True),0),('insert-local',dict(prefix=True,local=True),1),('change-body',dict(prefix=True,local=True,body=2),0),('change-header',dict(prefix=True,local=True,rename=True),0),('remove-function',dict(local=True),0)]:
                version+=1;current=source(**options);c.change(dep,current,version);v=check();ir=v['analysis']['irFragments'];assert ir['reused']>=eligible-changed and ir['checked']>0 and ir['nodesReused']>100,ir;done(name,ir=ir)
            c.request+=1;c.send('textDocument/definition',dict(textDocument=dict(uri=dep.as_uri()),version=version,position=position(current,current.index('helper_1(local)'))),c.request);nav,_=c.reply(c.request);assert nav['result']['uri']==dep.as_uri() and nav['result']['range']['start']==position(current,current.index('helper_1(int')),nav;done('current-remapped-global-navigation')
            stable=check();c.open(extra,'func incomplete(',1);unrelated=check();assert unrelated['analysis']['builds']==stable['analysis']['builds'] and unrelated['analysis']['irFragments']==stable['analysis']['irFragments'];c.send('textDocument/didClose',dict(textDocument=dict(uri=extra.as_uri())));done('unrelated-topology-retains-current-model')
            version+=1;current=source(broken=True);c.change(dep,current,version);broken=check(False);assert not any(broken['analysis']['irFragments'].values());done('semantic-error-does-not-publish-ir')
            version+=1;current=source();c.change(dep,current,version);recovered=check();assert recovered['analysis']['irFragments']['captured']>0;done('recovery-uses-current-check')
            lists={k:[] for k in LISTS};lists['members']=[p.as_posix() for p in [entry,dep,extra]];context=dict(schema=1,kind='project',project=(folder/'project.xlp').as_posix(),configuration='release',entry=entry.as_posix(),directory=folder.as_posix(),analysisProfile=PROFILE,targetKind='exe',packagePath='',lists=lists);epoch=2;c.call('xlang/configureAnalysis',dict(epoch=epoch,catalog=dict(schema=1,projects=[context],selectedProject=context['project'],packagePath='')));isolated=check();assert not isolated['analysis']['irFragments']['reused'];done('configuration-domain-fence')
            assert not dep.exists() and not extra.exists();c.stop();assert not c.process.returncode
        assert len(cases)==24 and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            (out/f'stderr-{i}.log').write_text(c.stop(),encoding='utf8');(out/f'transcript-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(out/'result.json'))))
if __name__=='__main__':main()
