"""Verify actual native navigation projection reuse without weakening inputs.

The original typed constructor/query oracle is covered natively. This suite
checks native point preparation and deferred full navigation, exact/no-op/
topology hits, current ranges, body/BOM/reopen/configuration fences and recovery.
"""
from pathlib import Path
import argparse,hashlib,json,time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize
from check_lsp_analysis_configuration import LISTS,PROFILE
from check_language_definitions import position

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();server=a.server.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(server):sha(server),str(Path(__file__)):sha(__file__)};clients=[];cases=[];report=dict(status='failed',scope=__doc__,cases=cases,protected=protected)
    try:
        for opened in [False,True]:
            folder=out/('opened' if opened else 'disk');folder.mkdir();entry,dep,extra=[folder/n for n in ['main.xl','dep.xl','extra.xl']];main='\ufeffimport dep\r\nfunc main(...) as int\r\n int unique_main=dep.compute(7)\r\n return unique_main\r\nend\r\n';entry.write_bytes(main.encode());protected[str(entry)]=sha(entry)
            c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c);initialize(c,entry,[entry,dep,extra],folder);epoch=1;version=1
            if opened:c.open(entry,main,1)
            body='\ufeffexport func compute(int value) as int\r\n return value+1\r\nend\r\n';c.open(dep,body,version)
            def done(name,**data):cases.append(dict(name=('opened-' if opened else 'disk-')+name,passed=True,**data))
            def read_check():
                until=time.monotonic()+30
                while True:
                    c.request+=1;c.send('xlang/check',dict(textDocument=dict(uri=dep.as_uri()),version=version),c.request);r,_=c.reply(c.request)
                    if r.get('error',{}).get('code')!=-32001:break
                    assert time.monotonic()<until;time.sleep(.001)
                assert 'error' not in r,r;r=r['result'];assert r['version']==version and r['analysisContext']['epoch']==epoch,r
                nav=r['analysis']['navigation'];assert set(nav)=={'builds','cacheHits','bytes','residentBytes','limit','deferred','deferredBytes','storeHits','storeMisses','published','retired','points'} and 0<=nav['residentBytes']<=nav['limit']==66*1024*1024,nav
                return r
            def check(expected_hit,okay=True):
                r=read_check();assert r['ok']==okay and bool(r['count'])!=okay,r;nav=r['analysis']['navigation']
                if okay:
                    assert nav['deferred'] and not nav['builds'] and not nav['cacheHits'] and not nav['bytes'] and nav['deferredBytes']>0 and r['semanticDefinitions'],r
                    assert nav['points']['ready'] and nav['points']['builds']==1-expected_hit and nav['points']['cacheHits']==expected_hit and nav['points']['bytes']>0,nav
                    assert next(d for d in r['documents'] if Path(d['path'])==dep)['version']==version,r
                    params=dict(textDocument=dict(uri=dep.as_uri()),version=version,checkId=r['checkId'],position=position(body,body.index('return value')+len('return ')))
                    value=c.call('textDocument/definition',params);assert value['uri']==dep.as_uri() and value['range']['start']==position(body,body.index('value)')),value
                    hover=c.call('textDocument/hover',params);assert hover and hover['contents'],hover
                    current=read_check()['analysis']['navigation'];assert current['deferred'] and not current['builds'] and not current['cacheHits'] and current['residentBytes']>0,current
                    assert current['points']['cacheHits']==1 and current['storeHits']==nav['storeHits']+1 and current['published']==nav['published'],(nav,current)
                else:assert not nav['deferred'] and not nav['deferredBytes'] and not nav['builds'] and not nav['cacheHits'] and not nav['bytes'] and not r['semanticDefinitions'] and nav['points']==dict(ready=False,builds=0,cacheHits=0,bytes=0),r
                return r
            first=check(0);done('cold-model-prepares-native-points-and-defers-full-view',navigation=first['analysis']['navigation'])
            hit=check(1);assert hit['analysis']['builds']==first['analysis']['builds'];done('exact-model-hit-reuses-table')
            version+=1;c.change(dep,body,version);noop=check(1);assert noop['analysis']['builds']==first['analysis']['builds'];done('new-version-same-bytes-uses-current-envelope')
            c.open(extra,'func incomplete(',1);unrelated=check(1);assert unrelated['analysis']['builds']==first['analysis']['builds'] and unrelated['analysis']['inputScopeHits']>0;done('unused-open-topology-hit')
            c.send('textDocument/didClose',dict(textDocument=dict(uri=extra.as_uri())));check(1);done('unused-close-topology-hit')
            version+=1;body=body.replace('value+1','value+2');c.change(dep,body,version);changed=check(0);assert changed['analysis']['builds']>first['analysis']['builds'];done('real-body-edit-never-reuses-old-owner')
            check(1);done('edited-model-exact-hit')
            def navigation():
                path,text,needle,want,vn=(entry,main,'compute(7)','compute(',1) if opened else (dep,body,'value+2','value)',version)
                v=c.call('textDocument/definition',dict(textDocument=dict(uri=path.as_uri()),version=vn,position=position(text,text.index(needle))));assert v['uri']==dep.as_uri() and v['range']['start']==position(body,body.index(want)),v
            navigation();done('current-imported-definition-with-bom' if opened else 'current-local-definition-with-bom')
            version+=1;body=body.removeprefix('\ufeff');c.change(dep,body,version);check(0);navigation();done('bom-removal-rebuilds-coordinate-view')
            c.send('textDocument/didClose',dict(textDocument=dict(uri=dep.as_uri())));version=1;c.open(dep,body,version);check(0);navigation();done('reopen-identity-fence')
            version+=1;c.change(dep,body.replace('value+2','"wrong"'),version);check(0,False);done('semantic-error-does-not-reuse-navigation')
            version+=1;c.change(dep,body,version);check(0);navigation();done('recovery-current-owner-and-ranges')
            lists={k:[] for k in LISTS};lists['members']=[p.as_posix() for p in [entry,dep,extra]];context=dict(schema=1,kind='project',project=(folder/'project.xlp').as_posix(),configuration='release',entry=entry.as_posix(),directory=folder.as_posix(),analysisProfile=PROFILE,targetKind='exe',packagePath='',lists=lists);epoch=2;c.call('xlang/configureAnalysis',dict(epoch=epoch,catalog=dict(schema=1,projects=[context],selectedProject=context['project'],packagePath='')));check(0);done('configuration-owner-fence')
            check(1);done('new-context-exact-hit')
            c.stop();assert not c.process.returncode and not dep.exists() and not extra.exists()
        assert len(cases)==28 and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            (out/f'stderr-{i}.log').write_text(c.stop(),encoding='utf8');(out/f'transcript-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(out/'result.json'))))
if __name__=='__main__':main()
