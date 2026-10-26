"""Actual automatic diagnostics backed by the persistent reverse owner graph.

Check shared/independent roots,negative candidates,manifests,wire dependency
dictionaries and current publications. No UI/latency/large-project acceptance.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
from check_lsp_diagnostic_dependencies import DependencyClient


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [server,Path(__file__),*[Path(__file__).with_name(n) for n in
        ['check_lsp_diagnostic_dependencies.py','check_lsp_automatic_diagnostics.py','check_language_snapshots.py','check_language_definitions.py']]]}
    clients=[];cases=[];status='failed'
    def client(name):
        folder=out/name;folder.mkdir();c=DependencyClient(server,folder/'absent/xl.exe',folder);clients.append((name,c))
        capabilities=dict(textDocument=dict(publishDiagnostics=dict(versionSupport=True)),
            experimental=dict(diagnosticInputProof=True,diagnosticDependencyProof=True))
        c.send('initialize',dict(capabilities=capabilities,initializationOptions=dict(automaticDiagnostics=True,checkOnSave=False,diagnosticDelay=0)),1)
        reply,_=c.reply(1);assert reply['result']['serverInfo']['version']=='0.77';c.send('initialized',{})
        return c,folder
    def opened(c,path,body):
        path.write_bytes(b'// must remain unsaved\n');protected[str(path)]=sha(path);c.open(path,body,1)
    def count(c,path):return len(c.latest[path.as_uri()]['diagnostics'])
    def indexed(stats,roots):
        assert stats['dependencyIndexed'] and stats['dependencyRoots']==stats['retainedRoots']==roots and not stats['dependencyFallbacks'],stats
        assert stats['dependencyPaths']>=roots and stats['dependencyEdges']>=roots and stats['dependencyBytes']>0,stats
    def watched(c,path,kind=2):c.send('workspace/didChangeWatchedFiles',dict(changes=[dict(uri=path.as_uri(),type=kind)]))
    def done(name,stats):cases.append(dict(name=name,passed=True,stats=stats))
    good='export func value() as int\n return 7\nend\n';bad='export func value() as int\n return "wrong"\nend\n'
    try:
        c,folder=client('shared');a,b,independent,shared=[folder/n for n in ['a.xl','b.xl','independent.xl','shared.xl']]
        shared.write_text(bad,encoding='utf8');body='import shared\nfunc main(...) as int\n return shared.value()\nend\n'
        opened(c,a,body);opened(c,b,body);opened(c,independent,'func main(...) as int\n return absentLocal\nend\n')
        stats=c.quiet();indexed(stats,3);assert count(c,shared)==count(c,independent)==1
        assert shared.as_posix().lower() in [p.replace('\\','/').lower() for p in c.paths(shared)]
        done('three-real-roots-have-persistent-shared-and-independent-owners',stats)
        before=stats;at=len(c.events);watched(c,folder/'unobserved-中文😀.xl');stats=c.quiet();indexed(stats,3)
        assert stats['dependencyLookups']>before['dependencyLookups'] and stats['dependencyGeneration']==before['dependencyGeneration'] and stats['submittedChecks']==before['submittedChecks']
        assert not c.events[at:],c.events[at:];done('unobserved-watched-path-is-indexed-miss-without-owner-rebuild-or-republication',stats)
        before=stats;at=len(c.events);shared.write_text(good,encoding='utf8');watched(c,shared);stats=c.quiet();indexed(stats,3)
        assert count(c,shared)==0 and count(c,independent)==1 and stats['submittedChecks']>=before['submittedChecks']+2
        assert stats['dependencyLookups']>before['dependencyLookups'] and stats['dependencyGeneration']>before['dependencyGeneration']
        assert not any(e['uri']==independent.as_uri() for e in c.events[at:]);done('shared-input-rechecks-both-owners-and-preserves-independent-root',stats)
        c.send('textDocument/didClose',dict(textDocument=dict(uri=a.as_uri())));stats=c.quiet();indexed(stats,2);assert count(c,shared)==0 and count(c,independent)==1
        done('closing-one-root-updates-graph-with-shared-owner-preserved',stats)
        c.open(a,body,1);stats=c.quiet();indexed(stats,3);done('reopening-new-document-identity-recreates-owner-edges',stats)
        before=stats;at=len(c.events);c.change(independent,'func main(...) as int\n return 0\nend\n',2);stats=c.quiet();indexed(stats,3)
        assert count(c,independent)==0 and c.latest[independent.as_uri()]['version']==2 and stats['submittedChecks']==before['submittedChecks']+1
        assert not any(e['uri']==shared.as_uri() for e in c.events[at:]);done('independent-edit-rechecks-one-owner-and-current-version',stats)
        c.stop();assert c.process.returncode==0

        c,folder=client('missing');entry,future=folder/'entry.xl',folder/'late_dep.xl'
        opened(c,entry,'import late_dep\nfunc main(...) as int\n return late_dep.value()\nend\n');stats=c.quiet();indexed(stats,1)
        assert count(c,entry)>0 and any(Path(p).name=='late_dep.xl' for p in c.paths(entry));done('missing-import-retains-negative-resolution-candidate',stats)
        before=stats;future.write_text(good,encoding='utf8');watched(c,future,1);stats=c.quiet();indexed(stats,1)
        assert count(c,entry)==count(c,future)==0 and stats['submittedChecks']>before['submittedChecks'] and stats['dependencyLookups']>before['dependencyLookups']
        done('negative-candidate-creation-rebuilds-owner-and-publishes-new-source',stats)
        before=stats;replacement=folder/'replacement.tmp';replacement.write_bytes(future.read_bytes());os.replace(replacement,future);watched(c,future);stats=c.quiet();indexed(stats,1)
        assert count(c,future)==0 and stats['submittedChecks']>before['submittedChecks'] and stats['dependencyGeneration']>before['dependencyGeneration']
        done('same-byte-file-identity-replacement-still-rebuilds',stats);c.stop();assert c.process.returncode==0

        c,folder=client('manifest');entry=folder/'entry.xl';package=folder/'pkg';(package/'src').mkdir(parents=True)
        manifest,api=package/'xlang.package.json',package/'src/api.xl'
        spec=dict(format='xlang-package-1',name='pkg',version='1.0.0',abiVersion='1',kind='source',
            artifact=dict(kind='source',xlSources=['src/api.xl']),dependencies=[],links=[],loadlibs=[])
        manifest.write_text(json.dumps(spec),encoding='utf8');api.write_text(bad,encoding='utf8')
        opened(c,entry,'import pkg\nfunc main(...) as int\n return pkg.value()\nend\n');stats=c.quiet();indexed(stats,1)
        assert count(c,api)==1 and any(Path(p).name=='xlang.package.json' for p in c.paths(api));done('actual-package-manifest-is-a-reverse-dependency',stats)
        before=stats;spec['version']='1.0.1';manifest.write_text(json.dumps(spec),encoding='utf8');watched(c,manifest);stats=c.quiet();indexed(stats,1)
        assert count(c,api)==1 and stats['submittedChecks']>before['submittedChecks'] and stats['dependencyLookups']>before['dependencyLookups'] and stats['dependencyGeneration']>before['dependencyGeneration']
        done('manifest-edit-invalidates-and-replaces-owner-closure',stats)
        before=stats;api.write_text(good,encoding='utf8');watched(c,api);stats=c.quiet();indexed(stats,1)
        assert count(c,api)==count(c,entry)==0 and stats['submittedChecks']>before['submittedChecks'];done('package-source-repair-publishes-current-diagnostics',stats)
        c.stop();assert c.process.returncode==0
        assert len(cases)==12 and all(sha(p)==h for p,h in protected.items());status='passed'
    finally:
        for name,c in clients:
            if c.process.poll() is None:c.stop()
            (out/name/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
        (out/'result.json').write_text(json.dumps(dict(status=status,scope=__doc__,cases=cases,protected=protected),ensure_ascii=False,indent=2),encoding='utf8')
    print(f'dependency graph passed:{len(cases)} real automatic diagnostic scenarios;evidence:{out}',flush=True)


if __name__=='__main__':main()
