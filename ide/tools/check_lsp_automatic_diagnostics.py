"""Actual default automatic diagnostics, multiple roots and input lifetimes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import time
from check_language_snapshots import Client
from check_language_definitions import position


class AutomaticClient(Client):
    def __init__(self,*args):
        super().__init__(*args);self.latest={};self.events=[];self.logs=[];self.request=100
    def next(self):
        value=super().next()
        if value.get('method')=='textDocument/publishDiagnostics':
            params=value['params'];self.latest[params['uri']]=params;self.events.append(params)
        if value.get('method')=='window/logMessage':self.logs.append(value['params'])
        return value
    def batch(self,messages):
        frames=[]
        for message in messages:
            self.transcript.append(dict(send=message));data=json.dumps(message,ensure_ascii=False,separators=(',',':')).encode()
            frames.append(f'Content-Length: {len(data)}\r\n\r\n'.encode()+data)
        self.process.stdin.write(b''.join(frames));self.process.stdin.flush()
    def call(self,method,params=None):
        self.request+=1;self.send(method,params or {},self.request);reply,_=self.reply(self.request)
        assert 'error' not in reply,reply
        return reply['result']
    def stats(self):return self.call('xlang/diagnosticStats')
    def wait_for(self,predicate,timeout=12):
        until=time.monotonic()+timeout
        while True:
            stats=self.stats()
            if predicate(stats):return stats
            assert time.monotonic()<until,stats
            time.sleep(.02)
    def quiet(self):
        return self.wait_for(lambda s:not s['pendingInputs'] and not s['dirtyRoots'] and not s['pendingReport'] and not s['checking'] and not s['observing'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in (server,Path(__file__),Path(__file__).with_name('check_language_snapshots.py'),Path(__file__).with_name('check_language_definitions.py'))}
    clients=[];cases=[]
    report=dict(status='failed',scope='default native automatic push diagnostics, entry contribution ownership, bounded owner scheduling and real input changes; not final semantic incrementality or IDE display latency',protected=protected,cases=cases)
    good='func main(...) as int\n int result = 7\n return result\nend\n'
    bad='func main(...) as int\n string text = "😀"; return missingName\nend\n'
    dep_good='export func value() as int\n return 7\nend\n'
    dep_bad='export func value() as int\n return "bad"\nend\n'
    placeholder=b'// native diagnostics must use the open overlay\n'
    def done(name,**data):cases.append(dict(name=name,passed=True,**data))
    def client(name,options=None,versions=True,expected=True,initialize=True):
        folder=root/name;folder.mkdir();c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c)
        if initialize:
            opts=dict(checkOnSave=False);opts.update(options or {})
            c.send('initialize',dict(capabilities=dict(textDocument=dict(publishDiagnostics=dict(versionSupport=versions))),initializationOptions=opts),1)
            reply,_=c.reply(1);assert reply['result']['capabilities']['experimental']['automaticDiagnostics']==expected,reply
            c.send('initialized',{})
        return c,folder
    def opened(c,path,text,version=1,uri=None):
        if not path.exists():path.write_bytes(placeholder);protected[str(path)]=sha(path)
        if uri is None:c.open(path,text,version)
        else:c.send('textDocument/didOpen',dict(textDocument=dict(uri=uri,text=text,version=version,languageId='xlang')))
    def count(c,path,uri=None):
        value=c.latest.get(uri or path.as_uri());return None if value is None else len(value['diagnostics'])
    def no_clear(c,path,at):return not any(v['uri']==path.as_uri() and not v['diagnostics'] for v in c.events[at:])
    def close(c,path,uri=None):c.send('textDocument/didClose',dict(textDocument=dict(uri=uri or path.as_uri())))
    try:
        c,folder=client('independent');first=folder/'入口😀.xl';second=folder/'second.xl'
        opened(c,first,bad);stats=c.quiet()
        assert count(c,first)==1 and stats['submittedChecks']==1 and stats['retainedRoots']==1,stats
        assert stats['workerThreadId']!=stats['ownerThreadId'] and stats['lastAnalysis']['builds']==1 and stats['lastAnalysis']['workerThreadId']!=stats['ownerThreadId'],stats
        diagnostic=c.latest[first.as_uri()]['diagnostics'][0];assert diagnostic['range']['start']==position(bad,bad.index('missingName')),diagnostic
        done('default-open-checks-native-unicode-overlay-without-executor',stats=stats)
        at=len(c.events);opened(c,second,bad);stats=c.quiet()
        assert count(c,first)==count(c,second)==1 and no_clear(c,first,at) and stats['retainedRoots']==2,stats
        done('second-entry-preserves-first-entry-error')
        c.change(second,good,2);c.stats();at=len(c.events);stats=c.quiet()
        assert count(c,second)==0 and count(c,first)==1 and no_clear(c,first,at),stats
        assert all(v.get('version')==2 for v in c.events[at:] if v['uri']==second.as_uri()),c.events[at:]
        done('edit-clears-only-current-contribution-and-rejects-late-version')
        close(c,first);stats=c.quiet();assert count(c,first)==0 and stats['retainedRoots']==1 and stats['publishedFiles']==1,stats
        lower=re.sub(r'%[0-9A-F]{2}',lambda m:m[0].lower(),first.as_uri());opened(c,first,bad,1,lower);c.quiet();assert count(c,first,lower)==1,c.events
        close(c,first,lower);opened(c,first,bad,1);c.quiet();assert count(c,first)==1 and count(c,first,lower)==0,c.events
        done('close-reopen-identity-and-exact-percent-URI-binding');c.stop()

        c,folder=client('shared');a,b,dep=folder/'a.xl',folder/'b.xl',folder/'common.xl'
        imported='import common\nfunc main(...) as int\n return common.value()\nend\n'
        opened(c,a,imported);opened(c,b,imported);opened(c,dep,dep_bad,7);stats=c.quiet()
        assert stats['retainedRoots']==3 and count(c,dep)==1 and count(c,a)==count(c,b)==0,stats
        done('shared-unsaved-dependency-errors-are-deduplicated')
        c.change(dep,dep_good,8);c.stats();at=len(c.events);stats=c.quiet()
        assert count(c,dep)==0 and not stats['failedRoots'],stats
        assert all(v.get('version')==8 for v in c.events[at:] if v['uri']==dep.as_uri()),c.events[at:]
        done('dependency-edit-rechecks-every-affected-root-at-current-version')
        c.change(a,good,2);stats=c.quiet();before=stats['submittedChecks'];c.change(dep,dep_bad,9);c.stats();at=len(c.events);stats=c.quiet()
        assert count(c,dep)==1 and stats['submittedChecks']==before+2,stats
        assert not any(v['uri']==a.as_uri() for v in c.events[at:]),c.events[at:]
        done('unrelated-root-is-neither-rechecked-nor-republished',scheduledChecks=stats['submittedChecks']-before)
        c.send('textDocument/didSave',dict(textDocument=dict(uri=dep.as_uri()),text=dep_bad));stats=c.quiet()
        assert count(c,dep)==1 and not stats['failedRoots'],stats
        done('save-uses-native-snapshots-without-launching-cli');c.stop()

        c,folder=client('disk');entry,dep=folder/'entry.xl',folder/'disk_dep.xl'
        dep.write_text(dep_good,encoding='utf-8');imported='import disk_dep\nfunc main(...) as int\n return disk_dep.value()\nend\n'
        opened(c,entry,imported);c.quiet();assert count(c,dep)==0,c.latest
        dep.write_text(dep_bad,encoding='utf-8');stats=c.wait_for(lambda s:count(c,dep)==1 and not s['checking'] and not s['pendingReport'])
        assert stats['lastAnalysis']['invalidations']>=1,stats
        done('unopened-disk-edit-is-observed-and-reanalyzed')
        before=stats['lastAnalysis']['invalidations'];replacement=folder/'replacement.tmp';replacement.write_bytes(dep.read_bytes());os.replace(replacement,dep)
        stats=c.wait_for(lambda s:s.get('lastAnalysis',{}).get('invalidations',0)>before and count(c,dep)==1 and not s['checking'])
        done('same-byte-disk-identity-replacement-invalidates',invalidations=stats['lastAnalysis']['invalidations'])
        dep.write_text(dep_good,encoding='utf-8');c.send('workspace/didChangeWatchedFiles',dict(changes=[dict(uri=dep.as_uri(),type=2)]));c.quiet();assert count(c,dep)==0,c.latest
        done('standard-watched-file-notification-rechecks-current-inputs');c.stop()

        c,folder=client('disk-race',dict(diagnosticDelay=0));a,b,dep=folder/'a.xl',folder/'b.xl',folder/'disk_dep.xl'
        dep.write_text(dep_good,encoding='utf-8');opened(c,a,imported);c.quiet()
        dep.write_text(dep_bad,encoding='utf-8');opened(c,b,imported);stats=c.quiet()
        assert stats['retainedRoots']==2 and not stats['failedRoots'] and count(c,dep)==1 and count(c,a)==count(c,b)==0,stats
        assert not any('Conflicting' in v['message'] for v in c.logs),c.logs
        done('new-entry-observing-new-disk-state-invalidates-stale-contributions');c.stop()

        c,folder=client('negative');entry,dep=folder/'entry.xl',folder/'created_dep.xl'
        text='import created_dep\nfunc main(...) as int\n return created_dep.value()\nend\n';opened(c,entry,text);c.quiet();assert count(c,entry)==1,c.latest
        dep.write_text(dep_good,encoding='utf-8');c.wait_for(lambda s:count(c,entry)==0 and s['retainedRoots']==1 and not s['checking'] and not s['pendingReport'])
        done('negative-import-candidate-creation-recovers-without-manual-check');c.stop()

        c,folder=client('debounce',dict(diagnosticDelay=250));entry=folder/'entry.xl'
        messages=[dict(jsonrpc='2.0',method='textDocument/didOpen',params=dict(textDocument=dict(uri=entry.as_uri(),text=bad,version=1,languageId='xlang')))]
        for version in range(2,22):messages.append(dict(jsonrpc='2.0',method='textDocument/didChange',params=dict(textDocument=dict(uri=entry.as_uri(),version=version),contentChanges=[dict(text=good if version==21 else bad)])))
        c.batch(messages);stats=c.quiet();assert stats['submittedChecks']==1 and count(c,entry)==0 and c.latest[entry.as_uri()]['version']==21,stats
        done('queued-edit-burst-debounces-to-one-real-check',checks=stats['submittedChecks']);c.stop()

        c,folder=client('request-flood',dict(diagnosticDelay=0));entry=folder/'entry.xl'
        messages=[dict(jsonrpc='2.0',method='textDocument/didOpen',params=dict(textDocument=dict(uri=entry.as_uri(),text=bad,version=1,languageId='xlang')))]
        ids=set(range(1000,1200));messages.extend(dict(jsonrpc='2.0',method='xlang/diagnosticStats',params={},id=i) for i in sorted(ids));c.batch(messages);scheduled=False
        while ids:
            reply=c.next()
            if reply.get('id') in ids:
                ids.remove(reply['id']);scheduled|=bool(ids and reply['result']['submittedChecks'])
        assert scheduled,'Ordinary queued requests starved automatic checking.'
        c.quiet();assert count(c,entry)==1;done('ordinary-request-queue-does-not-starve-background-checks');c.stop()

        c,folder=client('watch-reset');entry=folder/'entry.xl';opened(c,entry,bad);before=c.quiet()['submittedChecks']
        c.send('workspace/didChangeWatchedFiles',dict(changes=[dict(uri=(folder/f'change-{i}.xl').as_uri(),type=2) for i in range(129)]));stats=c.quiet()
        assert count(c,entry)==1 and stats['retainedRoots']==1 and not stats['failedRoots'] and stats['submittedChecks']==before+1,stats
        assert stats['lastAnalysis']['builds']==1 and stats['lastAnalysis']['cacheHits']>=1,stats
        done('oversized-watcher-batch-resynchronizes-with-bounded-state');c.stop()

        c,folder=client('cancel',dict(diagnosticDelay=0));entry=folder/'entry.xl'
        heavy=''.join('func helper_%d(int value) as int\n return value + 1\nend\n'%i for i in range(3000))+'func main(...) as int\n return helper_2999(7)\nend\n'
        opened(c,entry,heavy);c.send('textDocument/definition',dict(textDocument=dict(uri=entry.as_uri()),position=position(heavy,heavy.rindex('helper_2999'))),40);c.send('$/cancelRequest',dict(id=40))
        reply,_=c.reply(40);assert reply['error']['code']==-32800,reply
        stats=c.quiet();assert stats['submittedChecks']==1 and stats['lastAnalysis']['builds']==1 and count(c,entry)==0,stats
        done('cancelling-navigation-keeps-needed-automatic-check',stats=stats);c.stop()

        c,folder=client('supersede',dict(diagnosticDelay=0));entry=folder/'entry.xl';opened(c,entry,heavy)
        c.wait_for(lambda s:s['checking']);c.change(entry,bad,2);c.stats();at=len(c.events);stats=c.quiet()
        assert count(c,entry)==1 and all(v.get('version')==2 for v in c.events[at:] if v['uri']==entry.as_uri()),c.events[at:]
        c.change(entry,heavy,3);c.wait_for(lambda s:s['checking']);close(c,entry);opened(c,entry,good,1);c.stats();at=len(c.events);stats=c.quiet()
        assert count(c,entry)==0 and stats['retainedRoots']==1 and not stats['failedRoots'],stats
        assert all(v.get('version')==1 for v in c.events[at:] if v['uri']==entry.as_uri()),c.events[at:]
        done('active-check-change-close-and-slot-reuse-discard-old-results');c.stop()

        c,folder=client('shutdown',dict(diagnosticDelay=0));entry=folder/'entry.xl';opened(c,entry,heavy);c.wait_for(lambda s:s['checking'])
        c.send('shutdown',{},900);reply,_=c.reply(900);assert reply.get('result','missing') is None,reply;at=len(c.events)
        c.send('xlang/diagnosticStats',{},901);reply,_=c.reply(901);assert reply['error']['code']==-32600 and len(c.events)==at,reply
        c.send('exit');c.process.stdin.close();assert c.process.wait(timeout=5)==0
        done('shutdown-cancels-active-work-without-late-publications');c.stop()

        c,folder=client('completion',dict(diagnosticDelay=0));entry=folder/'entry.xl'
        text=''.join('func helper_%d(int value) as int\n return value + 1\nend\n'%i for i in range(1000))+'func main(...) as int\n int dynamicValue = 7\n return dynamicV\nend\n'
        opened(c,entry,text);c.send('textDocument/completion',dict(textDocument=dict(uri=entry.as_uri()),position=position(text,text.index('return dynamicV')+len('return dynamicV'))),41)
        reply,_=c.reply(41);value=reply['result'];assert value['available'] and {v['label'] for v in value['items']}=={'dynamicValue'},reply
        stats=c.quiet();assert count(c,entry)==1 and value['analysis']['workerThreadId']!=stats['lastAnalysis']['workerThreadId'],(value,stats)
        done('incomplete-code-completion-is-independent-of-background-diagnostics');c.stop()

        c,folder=client('resource',dict(diagnosticDelay=0));entry=folder/'entry.xl'
        oversized=''.join('func helper_%d(int value) as int\n return value + 1\nend\n'%i for i in range(6000))+'func main(...) as int\n return 0\nend\n'
        opened(c,entry,oversized);stats=c.wait_for(lambda s:s['failedRoots']==1 and not s['checking'])
        assert stats['submittedChecks']==1 and any('index exceeds tree limit' in v['message'] for v in c.logs),stats
        operations=stats['completedOperations'];stats=c.wait_for(lambda s:s['completedOperations']>=operations+2)
        assert stats['submittedChecks']==1 and not stats['retainedRoots'],stats
        c.change(entry,good,2);stats=c.quiet();assert not stats['failedRoots'] and count(c,entry)==0 and stats['submittedChecks']==2,stats
        done('resource-errors-log-once-per-input-state-and-recover-on-edit');c.stop()

        c,folder=client('no-version',versions=False);entry=folder/'entry.xl';opened(c,entry,bad);c.quiet();assert count(c,entry)==1
        c.change(entry,good,2);c.quiet();assert count(c,entry)==0 and all('version' not in v for v in c.events),c.events
        done('diagnostic-version-support-is-negotiated');c.stop()

        c,folder=client('manual',dict(automaticDiagnostics=False),expected=False);entry=folder/'entry.xl';opened(c,entry,bad);stats=c.quiet()
        assert not stats['submittedChecks'] and not c.events,stats
        value=c.call('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=1));assert value['count']==1 and count(c,entry)==1,value
        done('explicit-manual-mode-preserves-batch-check-contract');c.stop()

        c,folder=client('validation',initialize=False)
        for request,options in enumerate([dict(automaticDiagnostics='bad'),dict(diagnosticDelay=-1),dict(diagnosticsBackend='cli',automaticDiagnostics=True)],1):
            c.send('initialize',dict(capabilities={},initializationOptions=options),request);reply,_=c.reply(request);assert reply['error']['code']==-32602,reply
        c.send('initialize',dict(capabilities={},initializationOptions=dict(diagnosticsBackend='cli',checkOnSave=False)),4);reply,_=c.reply(4);assert not reply['result']['capabilities']['experimental']['automaticDiagnostics'],reply
        c.send('initialized',{});done('invalid-options-rejected-and-explicit-cli-remains-manual');c.stop()
        assert all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for index,c in enumerate(clients):
            c.stop();(root/f'transcript-{index:02}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8');(root/f'stderr-{index:02}.log').write_text(c.stderr,encoding='utf-8')
        (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(root/'result.json'))))


if __name__=='__main__':main()
