"""Real references with negotiated,source-bound per-document jump proofs.

Full standard Location parity comes from explicit source tokens. Proof hashes
are independently calculated from current open text or original disk bytes;
this is correctness coverage,not a timing run or fullworkspace acceptance.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
from urllib.parse import unquote
from check_language_snapshots import Client
from check_language_definitions import position
from check_lsp_analysis_configuration import LISTS,PROFILE


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--expected-version')
    args=parser.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    root=out/'工程😀';root.mkdir();entry=root/'main.xl';other=root/'未打开.xl';shared=root/'shared.xl';unused=root/'unused.xl'
    main_text='\ufeffimport shared\r\nfunc main(...) as int\r\n string 中文="😀"; return shared.compute(7)\r\nend\r\n'
    body='\ufeffexport func compute(int value) as int\r\n return value+1\r\nend\r\n'
    consumer='import shared\nfunc main(...) as int\n return shared.compute(8)+shared.compute(9)\nend\n'
    entry.write_bytes(main_text.encode());shared.write_bytes(body.encode());other.write_bytes(consumer.encode())
    lists={k:[] for k in LISTS};lists['members']=[p.as_posix() for p in [entry,shared,other]]
    scope=dict(schema=1,kind='project',project=(root/'project.xlp').as_posix(),configuration='debug',entry=entry.as_posix(),directory=root.as_posix(),analysisProfile=PROFILE,targetKind='exe',packagePath=None,lists=lists)
    packet=lambda epoch:dict(epoch=epoch,catalog=dict(schema=1,projects=[copy.deepcopy(scope)],selectedProject=scope['project'],packagePath=None))
    clients=[];cases=[];rid=10;versions={};overlays={};disk={entry:main_text,shared:body,other:consumer}
    sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();protected={str(server):sha(server),str(entry):sha(entry)}
    report=dict(status='failed',scope=__doc__,cases=cases,protected=protected)
    def done(name,**data):cases.append(dict(name=name,**data))
    def call(c,method,params,code=None):
        nonlocal rid
        rid+=1;c.send(method,params,rid);r,_=c.reply(rid)
        if code is not None:assert r.get('error',{}).get('code')==code,r;return r
        assert 'error' not in r,r;return r['result']
    def start(proof,invalid=False):
        c=Client(server,root/'absent-xl.exe',root);clients.append(c);cap={} if proof is None else dict(experimental=dict(referenceInputProof=proof));options=dict(checkOnSave=False,automaticDiagnostics=False,analysisConfiguration=packet(1));c.send('initialize',dict(capabilities=cap,initializationOptions=options),1);r,_=c.reply(1)
        if invalid:
            assert r.get('error',{}).get('code')==-32602,r;c.send('initialize',dict(capabilities=dict(experimental=dict(referenceInputProof=True)),initializationOptions=options),2);r,_=c.reply(2)
        assert r['result']['capabilities']['referencesProvider'] and r['result']['capabilities']['experimental']['referenceInputProof'],r
        if args.expected_version:assert r['result']['serverInfo']['version']==args.expected_version,r
        c.send('initialized',{});return c
    def location(path,text,at):return dict(uri=path.as_uri(),range=dict(start=position(text,at),end=position(text,at+7)))
    def wanted(include):
        result=[]
        for p in [entry,other,shared]:
            text=overlays.get(p,disk[p]);start=0
            while True:
                at=text.find('compute',start)
                if at<0:break
                if p!=shared or include:result.append(location(p,text if p in overlays else text.lstrip('\ufeff'),at if p in overlays else at-(1 if text.startswith('\ufeff') else 0)))
                start=at+7
        return sorted(result,key=lambda v:(unquote(v['uri']).casefold(),v['range']['start']['line'],v['range']['start']['character']))
    def query(c,include=True,proof=True):
        text=overlays[entry];result=call(c,'textDocument/references',dict(textDocument=dict(uri=entry.as_uri()),position=position(text,text.index('compute(7)')),context=dict(includeDeclaration=include)))
        plain=[{k:v for k,v in item.items() if k!='xlang'} for item in result];assert plain==wanted(include),(plain,wanted(include))
        for item in result:
            if not proof:assert 'xlang' not in item;continue
            extension=item['xlang'];assert extension['schema']==1;d=extension['document'];p=next(p for p in disk if p.as_uri()==item['uri']);assert Path(d['path'])==p
            source=overlays.get(p,disk[p].lstrip('\ufeff'));normal=source.replace('\r\n','\n').replace('\r','\n').encode();assert d['normalizedSize']==len(normal) and d['normalizedSha256']==hashlib.sha256(normal).hexdigest(),d
            if p in overlays:assert d['origin']=='overlay' and d['version']==versions[p] and 'input' not in d,d
            else:
                raw=disk[p].encode();post=raw[3:] if raw.startswith(b'\xef\xbb\xbf') else raw;i=d['input'];assert d['origin']=='disk' and 'version' not in d and Path(i['path'])==p and i['origin']=='disk' and i['rawSize']==len(raw) and i['textSize']==len(post),d
                assert i['rawSha256']==hashlib.sha256(raw).hexdigest() and i['textSha256']==hashlib.sha256(post).hexdigest(),i
                meta=i['metadata'];assert int(meta['device'],16)>=0 and int(meta['identity'],16)>0 and int(meta['modifiedUs'])>0,meta
        return result
    try:
        for flag in [None,False,True]:
            overlays.clear();versions.clear();c=start(flag);c.open(entry,main_text,1);overlays[entry]=main_text;versions[entry]=1
            query(c,proof=flag is True);done(f'capability-{flag}-full-standard-location-parity')
            query(c,False,proof=flag is True);done(f'capability-{flag}-include-declaration-false');c.stop()
        for bad in [1,'yes',{},None]:
            # Explicit null differs from absent capability.
            if bad is None:
                c=Client(server,root/'absent-xl.exe',root);clients.append(c);c.send('initialize',dict(capabilities=dict(experimental=dict(referenceInputProof=None))),1);r,_=c.reply(1);assert r.get('error',{}).get('code')==-32602,r;c.stop();done('reject-null-proof-capability');continue
            c=start(bad,True);c.stop();done(f'reject-invalid-proof-capability-{type(bad).__name__}-then-recover')
        overlays.clear();versions.clear();c=start(True);c.open(entry,main_text,1);overlays[entry]=main_text;versions[entry]=1;query(c);done('BOM-CRLF-disk-and-overlay-exact-normalized-raw-postBOM-proofs')
        for revision in [-2147483648,-7,0,2147483647]:
            c.stop();c=start(True);c.open(entry,main_text,revision);versions[entry]=revision;query(c);done('signed-wire-version',version=revision)
        c.stop();c=start(True);c.open(entry,main_text,1);versions[entry]=1
        overlay='\ufeff'+body.lstrip('\ufeff').replace('value+1','value+2');c.open(shared,overlay,1);overlays[shared]=overlay;versions[shared]=1;query(c);done('first-line-BOM-overlay-declaration-and-proof')
        changed='\ufeff\n'+overlay.lstrip('\ufeff');c.change(shared,changed,2);overlays[shared]=changed;versions[shared]=2;query(c);done('changed-shared-overlay-proof-and-all-location-ranges')
        c.send('textDocument/didClose',dict(textDocument=dict(uri=shared.as_uri())));overlays.pop(shared);versions.pop(shared);before=query(c);done('closed-overlay-restores-real-disk-proof')
        replacement=shared.with_suffix('.replacement');replacement.write_bytes(shared.read_bytes());replacement.replace(shared);after=query(c);old=next(v['xlang']['document']['input']['metadata']['identity'] for v in before if v['uri']==shared.as_uri());new=next(v['xlang']['document']['input']['metadata']['identity'] for v in after if v['uri']==shared.as_uri());assert old!=new,(old,new);done('same-bytes-new-file-identity-gets-fresh-proof')
        disk[other]='\n'+consumer;other.write_bytes(disk[other].encode());query(c);done('unopened-unimported-source-edit-updates-proof-and-ranges')
        c.open(unused,'// unrelated😀',1);query(c);done('unrelated-overlay-never-appears-in-location-proofs');c.send('textDocument/didClose',dict(textDocument=dict(uri=unused.as_uri())))
        dirty='\n'+consumer;c.open(other,dirty,1);overlays[other]=dirty;versions[other]=1;query(c);done('unopened-member-becomes-unsaved-overlay')
        call(c,'xlang/configureAnalysis',packet(2));query(c);done('configuration-epoch-refresh-preserves-proof-source-binding')
        c.send('textDocument/didClose',dict(textDocument=dict(uri=other.as_uri())));overlays.pop(other);versions.pop(other);query(c);done('member-close-restores-disk-proof')
        other.write_bytes(b'func broken(\n');call(c,'textDocument/references',dict(textDocument=dict(uri=entry.as_uri()),position=position(main_text,main_text.index('compute(7)')),context=dict(includeDeclaration=True)),-32803);done('source-error-never-publishes-partial-proof-locations');other.write_bytes(disk[other].encode());query(c);done('recovery-after-source-error')
        c.stop();assert all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            if c.process.poll() is None:c.stop()
            (out/f'transcript-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),'utf8')
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),evidence=str(out/'result.json'))))


if __name__=='__main__':main()
