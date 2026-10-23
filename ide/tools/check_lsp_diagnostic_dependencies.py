"""Exact shared diagnostic dependency dictionaries over actual stdio."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
from check_lsp_automatic_diagnostics import AutomaticClient


def digest(paths):
    data=bytearray(b'xlang-diagnostic-paths-v1\0'+struct.pack('>I',len(paths)))
    for path in paths:
        raw=path.encode('utf-8');data.extend(struct.pack('>I',len(raw)));data.extend(raw)
    return hashlib.sha256(data).hexdigest()


class DependencyClient(AutomaticClient):
    def __init__(self,*args):
        super().__init__(*args);self.definitions={};self.pending={};self.refs={};self.declarations=[];self.releases=[]
    def next(self):
        message=super().next();method=message.get('method');params=message.get('params',{})
        if method=='xlang/diagnosticDependencies':
            assert params['schema']==1 and re.fullmatch('[0-9a-f]{64}',params['id']),params
            identity=params['id']
            if params['removed']:
                assert set(params)=={'schema','id','removed'} and identity in self.definitions and identity not in self.refs.values(),params
                del self.definitions[identity];self.releases.append(identity)
            else:
                assert set(params)=={'schema','id','removed','part','parts','paths'} and identity not in self.definitions,params
                assert isinstance(params['part'],int) and isinstance(params['parts'],int) and 0<=params['part']<params['parts']<=65536,params
                assert len(json.dumps(params,ensure_ascii=False,separators=(',',':')).encode())<=256*1024,params
                current=self.pending.setdefault(identity,dict(parts=params['parts'],received=0,paths=[]))
                assert current['parts']==params['parts'] and current['received']==params['part'],params
                assert isinstance(params['paths'],list) and all(isinstance(p,str) and p and '\0' not in p and len(p.encode())<=65535 and Path(p).is_absolute() for p in params['paths']),params
                current['paths'].extend(params['paths']);current['received']+=1
                if current['received']==current['parts']:
                    paths=current['paths'];keys=[os.path.normcase(os.path.normpath(p)) for p in paths]
                    assert len(keys)==len(set(keys)) and digest(paths)==identity,(identity,paths)
                    self.definitions[identity]=paths;self.declarations.append(dict(id=identity,parts=current['parts'],count=len(paths)));del self.pending[identity]
        elif method=='textDocument/publishDiagnostics' and 'xlang' in params:
            proof=params['xlang']
            if proof['cleared']:self.refs.pop(params['uri'],None)
            elif 'dependencies' in proof:
                identity=proof['dependencies'];assert identity in self.definitions and identity not in self.pending,params
                self.refs[params['uri']]=identity
        return message
    def paths(self,path):
        return self.definitions[self.refs[path.as_uri()]]


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--server',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--receiver',type=Path,action='append',default=[]);args=parser.parse_args()
    receivers=[p.resolve(strict=True) for p in args.receiver]
    server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False);clients=[];cases=[];protected={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [server,Path(__file__),Path(__file__).with_name('check_lsp_automatic_diagnostics.py'),Path(__file__).with_name('check_language_snapshots.py'),*receivers]};status='failed'
    (root/'driver.py').write_bytes(Path(__file__).read_bytes())
    def done(name,**data):cases.append(dict(name=name,passed=True,**data))
    def client(name,dependency=True,input_proof=True,delay=0,automatic=True):
        folder=root/name;folder.mkdir();c=DependencyClient(server,folder/'absent/xl.exe',folder);clients.append(c)
        capabilities=dict(textDocument=dict(publishDiagnostics=dict(versionSupport=True)),experimental=dict(diagnosticInputProof=input_proof))
        if dependency is not None:capabilities['experimental']['diagnosticDependencyProof']=dependency
        c.send('initialize',dict(capabilities=capabilities,initializationOptions=dict(automaticDiagnostics=automatic,diagnosticDelay=delay)),1);reply,_=c.reply(1)
        assert reply['result']['capabilities']['experimental']['diagnosticDependencyProof'] is True,reply;c.send('initialized',{});return c,folder
    def has(paths,path):return os.path.normcase(os.path.normpath(str(path))) in [os.path.normcase(os.path.normpath(p)) for p in paths]
    good='func main(...) as int\n return 0\nend\n';bad=good.replace('return 0','return missingName')
    imported='import shared\nfunc main(...) as int\n return shared.value()\nend\n';shared='export func value() as int\n return "bad"\nend\n'
    try:
        for name,enabled in [('unnegotiated',None),('disabled',False)]:
            c,folder=client(name,enabled);entry=folder/'entry.xl';c.open(entry,bad);c.quiet()
            assert len(c.latest[entry.as_uri()]['diagnostics'])==1 and not c.declarations and 'dependencies' not in c.latest[entry.as_uri()]['xlang']
            done(name+'-keeps-existing-file-proof-contract');c.stop()

        c,folder=client('multi-entry-中文😀');first=folder/'first.xl';second=folder/'second.xl';common=folder/'shared.xl';independent=folder/'independent.xl';unused=folder/'unused.xl'
        c.batch([dict(jsonrpc='2.0',method='textDocument/didOpen',params=dict(textDocument=dict(uri=p.as_uri(),languageId='xlang',version=1,text=t))) for p,t in [(first,imported),(second,imported),(common,shared),(independent,bad),(unused,good)]])
        c.quiet();paths=c.paths(common)
        assert all(has(paths,p) for p in [first,second,common]) and not has(paths,independent) and not has(paths,unused),paths
        assert len(c.latest[common.as_uri()]['diagnostics'])==1
        done('shared-file-uses-union-of-actual-contributing-entry-inputs',paths=len(paths))
        independent_id=c.refs[independent.as_uri()];before=c.refs[common.as_uri()];events=len(c.events);c.change(first,good,2);c.quiet()
        assert c.refs[common.as_uri()]!=before and not has(c.paths(common),first) and has(c.paths(common),second),c.paths(common)
        assert any(e['uri']==common.as_uri() and not e['xlang']['cleared'] and e['xlang']['dependencies']!=before for e in c.events[events:])
        assert c.refs[independent.as_uri()]==independent_id and independent_id in c.definitions
        done('dependency-only-change-republishes-binding-and-keeps-unrelated-set')
        c.send('textDocument/didClose',dict(textDocument=dict(uri=second.as_uri())));c.quiet()
        assert not has(c.paths(common),first) and not has(c.paths(common),second) and has(c.paths(common),common)
        done('close-removes-contributor-and-releases-only-unused-identities')
        c.change(independent,good,2);c.quiet();assert not c.latest[independent.as_uri()]['diagnostics'] and has(c.paths(independent),independent)
        done('clean-analysis-retains-complete-dependency-proof')
        for p in [first,common,independent,unused]:c.send('textDocument/didClose',dict(textDocument=dict(uri=p.as_uri())))
        c.quiet();assert not c.refs and not c.definitions and not c.pending
        done('last-reference-withdrawal-releases-dictionary-after-diagnostics');c.stop()

        c,folder=client('negative-candidate');entry=folder/'entry.xl';future=folder/'created_dep.xl';text='import created_dep\nfunc main(...) as int\n return created_dep.value()\nend\n'
        c.open(entry,text);c.quiet();assert not future.exists() and has(c.paths(entry),future)
        done('absent-import-candidates-are-invalidation-inputs')
        future.write_text('export func value() as int\n return 1\nend\n',encoding='utf-8')
        c.wait_for(lambda s:not c.latest[entry.as_uri()]['diagnostics'] and s['submittedChecks']>=2);c.quiet();assert not c.latest[entry.as_uri()]['diagnostics'] and has(c.paths(entry),future)
        done('candidate-creation-rechecks-and-rebinds-without-executor');c.stop()

        c,folder=client('manifest');package=folder/'pkg';(package/'src').mkdir(parents=True);manifest=package/'xlang.package.json';api=package/'src/api.xl';entry=folder/'entry.xl'
        manifest.write_text(json.dumps(dict(format='xlang-package-1',name='pkg',version='1.0.0',abiVersion='1',kind='source',artifact=dict(kind='source',xlSources=['src/api.xl']),dependencies=[],links=[],loadlibs=[])),encoding='utf-8');api.write_text(shared,encoding='utf-8')
        c.open(entry,imported.replace('shared','pkg'));c.quiet();assert has(c.paths(entry),manifest) and has(c.paths(entry),api)
        done('package-manifests-and-real-import-files-belong-to-the-set');c.stop()

        c,folder=client('multipart',delay=1500);directory=folder
        while len(str(directory))<6000:directory=directory/('segment-'+'a'*70)
        entry=directory/'entry.xl'
        text=''.join(f'import m{i}\n' for i in range(47))+good
        # Source imports have a native512-byte path limit; negative package
        # resolution candidates remain valid long-path invalidation inputs.
        c.open(entry,text)
        c.wait_for(lambda s:not s['pendingInputs'] and not s['dirtyRoots'] and not s['pendingReport'] and not s['checking'] and not s['observing'],timeout=90);paths=c.paths(entry)
        assert has(paths,entry) and all(has(paths,directory/f'm{i}/xlang.package.json') for i in range(47)),len(paths)
        assert len(c.latest[entry.as_uri()]['diagnostics'])==47
        identity=c.refs[entry.as_uri()];assert any(d['id']==identity and d['parts']>1 for d in c.declarations)
        done('long-negative-import-candidates-use-complete-bounded-multipart-set',paths=len(paths),parts=next(d['parts'] for d in reversed(c.declarations) if d['id']==identity));c.stop()

        for index,invalid in enumerate([1,'true',[],{},None]):
            folder=root/f'invalid-{index}';folder.mkdir();c=DependencyClient(server,folder/'absent/xl.exe',folder);clients.append(c)
            c.send('initialize',dict(capabilities=dict(experimental=dict(diagnosticInputProof=True,diagnosticDependencyProof=invalid))),1);reply,_=c.reply(1);assert reply['error']['code']==-32602,reply
            c.send('initialize',dict(capabilities=dict(experimental=dict(diagnosticInputProof=True,diagnosticDependencyProof=True))),2);reply,_=c.reply(2);assert 'result' in reply,reply;c.send('initialized',{});c.stop()
        done('malformed-capability-types-rejected-and-initialize-recovers')
        folder=root/'missing-base';folder.mkdir();c=DependencyClient(server,folder/'absent/xl.exe',folder);clients.append(c);c.send('initialize',dict(capabilities=dict(experimental=dict(diagnosticDependencyProof=True))),1);reply,_=c.reply(1);assert reply['error']['code']==-32602;c.stop();done('dependency-proof-requires-base-file-proof')
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in protected.items());done('server-and-test-inputs-preserved');status='passed'
    finally:
        for i,c in enumerate(clients):
            stderr=c.stop();(root/f'server-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8');(root/f'server-{i}.stderr').write_text(stderr,encoding='utf-8')
        replays=[];receiver_failure=None
        if status=='passed' and receivers:
            for receiver in receivers:
                for i in range(len(clients)):
                    source=root/f'server-{i}.json';before=hashlib.sha256(source.read_bytes()).hexdigest();command=[str(receiver),'--transcript',str(source)]
                    try:
                        run=subprocess.run(command,capture_output=True,timeout=30)
                        record=dict(command=command,exit=run.returncode,stdout=run.stdout.decode('utf-8','replace'),stderr=run.stderr.decode('utf-8','replace'),inputSha256=before)
                    except (OSError,subprocess.TimeoutExpired) as error:
                        record=dict(command=command,exit=-1,stdout='',stderr=str(error),inputSha256=before)
                    replays.append(record)
                    if record['exit'] or hashlib.sha256(source.read_bytes()).hexdigest()!=before:
                        receiver_failure='C diagnostic receiver failed or changed its input: '+str(command);break
                if receiver_failure:break
            if not receiver_failure and not all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in protected.items()):receiver_failure='Receiver/server/test fingerprint changed.'
            if receiver_failure:status='failed'
            else:done('C-receivers-accept-real-dependency-file-proof-and-current-body-streams',receivers=len(receivers),transcripts=len(clients),replays=len(replays))
        (root/'result.json').write_text(json.dumps(dict(status=status,scope='opt-in exact shared dependency dictionaries and optional native C receiver replay; standard push/file proofs retained, not yet the IDE automatic diagnostic controller',protected=protected,cases=cases,receiverReplays=replays),ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(status=status,cases=len(cases),proof=str(root/'result.json'))))
        if receiver_failure:raise AssertionError(receiver_failure)


if __name__=='__main__':main()
