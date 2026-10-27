"""Real standard project references over guarded unopened and unimported roots.

Expected full Location arrays come from explicit source tokens,independently of
the LS index. Covers overlays,disk identity,configuration,cancel/publication
fences and foreground protocol responsiveness;not final latency acceptance.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
from urllib.parse import unquote
import time
from check_language_snapshots import Client
from check_language_definitions import position
from check_lsp_analysis_configuration import LISTS, PROFILE


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    root=out/'工程😀';root.mkdir();entry=root/'main.xl';other=root/'未打开.xl';shared=root/'shared.xl';shadow=root/'shadow.xl';unused=root/'unused.xl'
    main_text='\ufeffimport shared\r\nfunc main(...) as int\r\n string text="😀"\r\n return shared.compute(7)\r\nend\r\n'
    body='export func compute(int value) as int\n return value+1\nend\n'
    other_text='import shared\nfunc main(...) as int\n return shared.compute(8)+shared.compute(9)\nend\n'
    edited='import shared\nfunc main(...) as int\n return shared.compute(8)+shared.compute(9)+shared.compute(10)\nend\n'
    one='import shared\nfunc main(...) as int\n return shared.compute(8)\nend\n'
    shadow_text='func compute(int value) as int\n return value\nend\nfunc main(...) as int\n return compute(7)\nend\n'
    for p,text in [(entry,main_text),(other,other_text),(shared,body),(shadow,shadow_text)]:p.write_bytes(text.encode())
    members=[entry,other,shared,shadow,entry];lists={name:[] for name in LISTS};lists['members']=[p.as_posix() for p in members]
    scope=dict(schema=1,kind='project',project=(root/'project.xlp').as_posix(),configuration='debug',entry=entry.as_posix(),directory=root.as_posix(),analysisProfile=PROFILE,targetKind='exe',packagePath=None,lists=lists)
    packet=lambda epoch,context=scope:dict(epoch=epoch,catalog=dict(schema=1,projects=[copy.deepcopy(context)],selectedProject=context['project'],packagePath=None))
    cases=[];clients=[];rid=20;sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(p):sha(p) for p in [server,Path(__file__)]};report=dict(status='failed',scope=__doc__,cases=cases,protected=protected)

    def done(name,**kw):cases.append(dict(name=name,passed=True,**kw))
    def send(c,method,params):
        nonlocal rid
        rid+=1;c.send(method,params,rid);return rid
    def collect(c,ids):
        pending=set(ids);rows={}
        while pending:
            row=c.next()
            if 'id' in row:
                assert row['id'] in pending,('unexpected/duplicate reply',row,pending)
                pending.remove(row['id']);rows[row['id']]=row
        return rows
    def call(c,method,params,code=None):
        id=send(c,method,params);row=collect(c,[id])[id]
        if code is not None:assert row['error']['code']==code,row;return row['error']
        assert 'error' not in row,row
        return row['result']
    def start(config=True):
        c=Client(server,out/'absent-xl.exe',root);clients.append(c);options=dict(checkOnSave=False,automaticDiagnostics=False)
        if config:options['analysisConfiguration']=packet(1)
        c.send('initialize',dict(capabilities={},initializationOptions=options),1);r,_=c.reply(1);assert r['result']['serverInfo']['version']=='0.93' and r['result']['capabilities']['referencesProvider'] is True,r;c.send('initialized',{});c.open(entry,main_text,1);return c
    def params(path=entry,text=main_text,include=True,needle='compute(7)',**extra):return dict(textDocument=dict(uri=path.as_uri()),position=position(text,text.index(needle)),context=dict(includeDeclaration=include),**extra)
    def location(path,text,at,size=7):return dict(uri=path.as_uri(),range=dict(start=position(text,at),end=position(text,at+size)))
    def expected(other_body=other_text,dep_body=body,include=True,with_other=True):
        rows=[location(entry,main_text,main_text.index('compute(7)'))]
        if with_other:
            at=0
            while True:
                at=other_body.find('compute(',at)
                if at<0:break
                rows.append(location(other,other_body,at));at+=7
        if include:rows.append(location(shared,dep_body,dep_body.index('compute')))
        return sorted(rows,key=lambda r:(unquote(r['uri']).casefold(),r['range']['start']['line'],r['range']['start']['character']))
    def references(c,want,**kw):
        got=call(c,'textDocument/references',params(**kw));assert got==want,(got,want);return got
    try:
        c=start();references(c,expected());stats=call(c,'xlang/referenceStats',{});assert (stats['roots'],stats['captured'],stats['reused'],stats['locations'])==(3,3,0,4),stats;assert (stats['memberWorkers'],stats['memberTasks'],stats['memberPeak'],stats['memberThreads'])==(4,2,2,2),stats;done('cold-complete-unopened-unimported-project',work=stats)
        references(c,expected(include=False),include=False);stats=call(c,'xlang/referenceStats',{});assert (stats['captured'],stats['reused'])==(1,2),stats;assert not any(stats[k] for k in ['memberTasks','memberPeak','memberThreads']),stats;done('exclude-declaration-and-retained-shards',work=stats)
        other.write_bytes(edited.encode());references(c,expected(edited));stats=call(c,'xlang/referenceStats',{});assert (stats['captured'],stats['reused'])==(2,1),stats;done('unopened-member-disk-edit',work=stats)
        replacement=root/'replacement.tmp';replacement.write_bytes(edited.encode());os.replace(replacement,other);references(c,expected(edited));stats=call(c,'xlang/referenceStats',{});assert (stats['captured'],stats['reused'])==(2,1),stats;done('same-byte-disk-identity-replacement',work=stats)
        c.open(other,one,3);references(c,expected(one));done('unsaved-unimported-member-overlay')
        c.open(unused,'broken unused',1);references(c,expected(one));stats=call(c,'xlang/referenceStats',{});assert stats['reused']==2,stats;done('unrelated-overlay-topology-does-not-rebuild-members',work=stats)
        c.change(unused,'still broken unused',2);references(c,expected(one));done('unrelated-overlay-body-does-not-change-bindings')
        c.send('textDocument/didClose',dict(textDocument=dict(uri=unused.as_uri())));references(c,expected(one));done('unrelated-overlay-close')
        c.send('textDocument/didClose',dict(textDocument=dict(uri=other.as_uri())));references(c,expected(edited));done('used-overlay-close-reanalyzes-disk-member')
        changed=body.replace('value+1','value+2');shared.write_bytes(changed.encode());references(c,expected(edited,changed));stats=call(c,'xlang/referenceStats',{});assert (stats['captured'],stats['reused'])==(2,1),stats;done('declaration-source-change-rechecks-actual-consumers',work=stats)
        bom='\ufeff'+changed;c.open(shared,bom,1);references(c,expected(edited,bom));done('declaration-overlay-bom-positions')
        references(c,expected(edited,bom),path=shared,text=bom,needle='compute');done('query-origin-is-imported-open-member')
        c.send('textDocument/didClose',dict(textDocument=dict(uri=shared.as_uri())));references(c,expected(edited,changed));done('declaration-overlay-close')
        references(c,[],needle='func');done('non-occurrence-empty-result')
        for name,p in [('missing-context',dict(textDocument=dict(uri=entry.as_uri()),position=dict(line=0,character=0))),('wrong-include-kind',params(include=1)),('old-version',params(version=0)),('closed-document',params(path=unused)),('surrogate-interior',dict(textDocument=dict(uri=entry.as_uri()),position=dict(line=2,character=15),context=dict(includeDeclaration=True)))]:
            if name=='surrogate-interior':p['position']=position(main_text,main_text.index('😀'));p['position']['character']+=1
            call(c,'textDocument/references',p,-32602);done(name)
        other.write_bytes(b'func main(\n');call(c,'textDocument/references',params(),-32803);done('failed-member-does-not-publish-partial-project')
        other.write_bytes(edited.encode());references(c,expected(edited,changed));done('source-error-recovery')
        reduced=copy.deepcopy(scope);reduced['lists']['members']=[p.as_posix() for p in [entry,shared,shadow]];call(c,'xlang/configureAnalysis',packet(2,reduced));references(c,expected(edited,changed,with_other=False));done('membership-epoch-removes-old-consumer')
        call(c,'xlang/configureAnalysis',packet(3));references(c,expected(edited,changed));done('membership-epoch-restores-all-members')
        # Cold large unsaved source guarantees cancellable work is live;the
        # source parser itself remains cooperative only at existing boundaries.
        heavy=''.join(f'func helper_{i}(int value) as int\n return value+1\nend\n' for i in range(1500))+main_text.lstrip('\ufeff')
        c.change(entry,heavy,2);a=send(c,'textDocument/references',params(text=heavy));b=send(c,'textDocument/references',params(text=heavy));c.send('$/cancelRequest',dict(id=a));ping=send(c,'xlang/inputState',{});c.change(entry,main_text,3);rows=collect(c,[a,b,ping]);assert rows[a]['error']['code']==-32800 and rows[b]['error']['code']==-32801 and 'result' in rows[ping],rows;done('parallel-ids-cancel-and-content-fence-with-live-owner')
        references(c,expected(edited,changed));done('recovery-after-cancelled-project-work')
        other.write_bytes(heavy.replace('import shared','import shared').encode());a=send(c,'textDocument/references',params());c.send('workspace/didChangeWatchedFiles',dict(changes=[dict(uri=other.as_uri(),type=2)]));rows=collect(c,[a]);assert rows[a]['error']['code']==-32801,rows;done('watched-file-fence-with-automatic-diagnostics-disabled')
        other.write_bytes(edited.encode());references(c,expected(edited,changed));done('watched-cancel-recovery')
        other.write_bytes(heavy.encode());a=send(c,'textDocument/references',params());b=send(c,'xlang/configureAnalysis',packet(4,reduced));rows=collect(c,[a,b]);assert rows[a]['error']['code']==-32801 and 'result' in rows[b],rows;done('configuration-fence-during-project-analysis')
        other.write_bytes(edited.encode());references(c,expected(edited,changed,with_other=False));done('configuration-fence-recovery')
        wide=copy.deepcopy(scope);extra_locations=[]
        for index in range(40):
            member=root/f'member-{index}.xl';member.write_bytes(one.encode());wide['lists']['members'].append(member.as_posix());extra_locations.append(location(member,one,one.index('compute(')))
        call(c,'xlang/configureAnalysis',packet(5,wide));want=sorted(expected(edited,changed)+extra_locations,key=lambda r:(unquote(r['uri']).casefold(),r['range']['start']['line'],r['range']['start']['character']))
        references(c,want);stats=call(c,'xlang/referenceStats',{});assert (stats['roots'],stats['captured'],stats['reused'])==(43,43,0),stats;cold=stats;done('beyond-32-members-complete-cold-query',work=stats)
        references(c,want);stats=call(c,'xlang/referenceStats',{});assert (stats['roots'],stats['captured'],stats['reused'])==(43,1,42),stats;assert stats['keyActorBuilds']==cold['keyActorBuilds'] and stats['keyActorHits']>cold['keyActorHits'] and stats['keyActorSessions']<=2 and stats['keyNavigationHits']==1 and stats['keyNavigationBuilds']==0,stats;done('beyond-32-members-hot-query-keeps-all-shards-and-key-worker',work=stats)
        other.write_bytes(heavy.encode());a=send(c,'textDocument/references',params());started=time.perf_counter();b=send(c,'textDocument/completion',dict(textDocument=dict(uri=entry.as_uri()),position=position(main_text,main_text.index('compute(7)')+4)))
        rows=collect(c,[a,b]);assert 'error' not in rows[a] and 'error' not in rows[b],rows;assert any(v['label']=='compute' for v in rows[b]['result']['items']),rows[b]
        want=sorted(expected(heavy,changed)+extra_locations,key=lambda r:(unquote(r['uri']).casefold(),r['range']['start']['line'],r['range']['start']['character']));assert rows[a]['result']==want,rows[a];done('foreground-semantic-completion-during-project-analysis',combinedMs=(time.perf_counter()-started)*1000)
        other.write_bytes(edited.encode())
        c.stop();c=start(False);references(c,expected(edited,changed,with_other=False));done('standalone-entry-import-closure-without-project');c.stop()
        assert all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for index,c in enumerate(clients):
            if c.process.poll() is None:c.stop()
            (out/f'transcript-{index}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),'utf8')
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),evidence=str(out/'result.json'))))


if __name__=='__main__':main()
