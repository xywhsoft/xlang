"""Verify live checked-index entity registry admission/isolation over stdio.

Native fixtures verify actual stable declaration/reference IDs and complete
fresh parity. This wire suite covers the real service, source versions,
declaration edits, current navigation, failure recovery and context fencing.
"""
import argparse,hashlib,json,time
from pathlib import Path
from check_lsp_module_semantics import initialize
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_analysis_configuration import LISTS,PROFILE
from check_language_definitions import position


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--server',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(server):sha(server),str(Path(__file__)):sha(__file__)};clients=[];cases=[]
    report=dict(status='failed',scope=__doc__,protected=protected,cases=cases)
    try:
        for opened in [False,True]:
            folder=out/('opened' if opened else 'disk');folder.mkdir();entry,dep,extra=[folder/n for n in ['main.xl','dep.xl','extra.xl']]
            main='import dep\nfunc main(...) as int\n return dep.compute(7)\nend\n';entry.write_bytes(main.encode());protected[str(entry)]=sha(entry)
            c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c);initialize(c,entry,[entry,dep,extra],folder);epoch=1
            def done(name,**data):cases.append(dict(name=('opened-' if opened else 'disk-')+name,passed=True,**data))
            def stats(enabled=True):
                r=c.call('xlang/analysisStats')['entityStore'];assert r['enabled']==enabled,r
                if enabled:assert 0<r['domains'] and 0<r['entities'] and 0<r['bytes']<=r['limit'] and r['admitted']==r['entities'],r
                else:assert not r['domains'] and not r['entities'] and not r['bytes'],r
                return r
            stats(False);done('registry-starts-lazy')
            if opened:c.open(entry,main,1)
            def source(value,pad=False):return ('func inserted() as int\n return 0\nend\n' if pad else '')+f'export func compute(int value) as int\n int local=value+{value}\n return local\nend\n'
            body=source(0);version=1;c.open(dep,body,version)
            def check(okay=True):
                until=time.monotonic()+30
                while True:
                    c.request+=1;c.send('xlang/check',dict(textDocument=dict(uri=dep.as_uri()),version=version),c.request);r,_=c.reply(c.request)
                    if r.get('error',{}).get('code')!=-32001:break
                    assert time.monotonic()<until;time.sleep(.001)
                assert 'error' not in r,r;r=r['result'];assert r['version']==version and r['analysisContext']['epoch']==epoch and r['ok']==okay and bool(r['count'])!=okay,r
                if okay:
                    assert r['semanticDefinitions'],r;row=next(d for d in r['documents'] if Path(d['path'])==dep);assert row['version']==version and row['origin']=='overlay',r
                return r
            check();initial=stats();done('actual-native-check-admits-declarations',stats=initial)
            for v in range(1,7):version+=1;body=source(v);c.change(dep,body,version);check();current=stats();assert current['domains']==initial['domains'] and current['entities']==initial['entities'] and current['hits']>initial['hits'],(initial,current)
            done('six-body-edits-preserve-declaration-registry',stats=current)
            # Definition uses the existing original typed navigation path.
            c.request+=1;c.send('textDocument/definition',dict(textDocument=dict(uri=dep.as_uri()),version=version,position=position(body,body.rindex('local'))),c.request);navigation,_=c.reply(c.request)
            assert navigation['result']['uri']==dep.as_uri() and navigation['result']['range']['start']==position(body,body.index('local')),navigation;done('current-local-navigation-remains-correct')
            version+=1;body=source(7,True);c.change(dep,body,version);check();inserted=stats();assert inserted['domains']==initial['domains'] and inserted['entities']>current['entities'],(inserted,current);done('new-global-declaration-admits-new-identity',stats=inserted)
            version+=1;body=source(8);c.change(dep,body,version);r=check();removed=stats();assert removed['entities']==inserted['entities'],(removed,inserted);done('declaration-removal-keeps-monotonic-id-history')
            c.open(extra,'func incomplete(',1);q=check();assert q['analysis']['builds']==r['analysis']['builds'],q;c.send('textDocument/didClose',dict(textDocument=dict(uri=extra.as_uri())));check();assert stats()['entities']==removed['entities'];done('unrelated-membership-does-not-reintern-model')
            lists={key:[] for key in LISTS};lists['members']=[p.as_posix() for p in [entry,dep,extra]]
            context=dict(schema=1,kind='project',project=(folder/'project.xlp').as_posix(),configuration='release',entry=entry.as_posix(),directory=folder.as_posix(),analysisProfile=PROFILE,targetKind='exe',packagePath='',lists=lists)
            epoch=2;c.call('xlang/configureAnalysis',dict(epoch=epoch,catalog=dict(schema=1,projects=[context],selectedProject=context['project'],packagePath='')));check();release=stats();assert release['domains']==2 and release['entities']>removed['entities'],(release,removed);done('configuration-has-separate-entity-domain',stats=release)
            epoch=3;c.call('xlang/configureAnalysis',dict(epoch=epoch,catalog=dict(schema=1,projects=[context],selectedProject=context['project'],packagePath='')));check();equal=stats();assert equal['domains']==release['domains'] and equal['entities']==release['entities'],(equal,release);done('equal-context-recapture-retains-domain')
            version+=1;c.change(dep,'import absent_entity_dependency\n'+body,version);check(False);assert stats()['entities']==equal['entities'];done('failed-import-does-not-publish-declaration-identities')
            version+=1;c.change(dep,body,version);check();recovered=stats();assert recovered['domains']==equal['domains'] and recovered['entities']==equal['entities'];done('recovery-rebinds-original-current-declarations',stats=recovered)
            c.stop();assert not c.process.returncode and not dep.exists() and not extra.exists()
        assert len(cases)==22 and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            stderr=c.stop();(out/f'stderr-{i}.log').write_text(stderr,encoding='utf8');(out/f'transcript-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(out/'result.json')),ensure_ascii=False))
if __name__=='__main__':main()
