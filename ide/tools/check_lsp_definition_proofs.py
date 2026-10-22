"""Actual cold definition Locations with negotiated target input metadata."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    sha=lambda b:hashlib.sha256(b).hexdigest()
    protected={str(p):sha(p.read_bytes()) for p in [server,Path(__file__),Path(__file__).with_name('check_lsp_automatic_diagnostics.py'),Path(__file__).with_name('check_language_definitions.py'),Path(__file__).with_name('check_language_snapshots.py')]}
    clients=[];cases=[];status='failed'
    overload='func pick(int value) as int\n return value\nend\nfunc pick(string value) as int\n return 7\nend\nfunc main(...) as int\n string 文本 = "😀"; return pick("x")\nend\n'
    imported='import proof_dep\nfunc main(...) as int\n string 文本 = "😀"; return proof_dep.value()\nend\n'
    depbody='export func value() as int\n string 文本 = "😀"\n return 7\nend\n'
    overlay='\n\nexport func value() as int\n return 8\nend\n'
    def done(name,**values):cases.append(dict(name=name,passed=True,**values))
    def client(name,proof=None,initialize=True):
        folder=root/name;folder.mkdir();c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c)
        if initialize:
            cap={} if proof is None else dict(experimental=dict(definitionInputProof=proof))
            c.send('initialize',dict(capabilities=cap,initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False)),1)
            reply,_=c.reply(1);assert reply['result']['capabilities']['experimental']['definitionInputProof'] is True,reply;c.send('initialized',{})
        return c,folder
    def opened(c,path,text,version=1):
        if not path.exists():path.write_bytes(b'// actual unsaved definition input\n')
        protected[str(path)]=sha(path.read_bytes());c.open(path,text,version)
    def query(c,entry,text,needle,version=1):
        return c.call('textDocument/definition',dict(textDocument=dict(uri=entry.as_uri()),position=position(text,text.index(needle)),version=version))
    def metadata(location,path,origin):
        assert set(location)=={'uri','range','xlang'} and set(location['xlang'])=={'schema','document'} and location['xlang']['schema']==1,location
        value=location['xlang']['document'];assert len(value)==5 and Path(value['path'])==path and value['origin']==origin and 'diagnostics' not in value and 'text' not in value,value
        assert re.fullmatch(r'[0-9a-f]{64}',value['normalizedSha256']),value
        return value
    try:
        for name,proof in [('generic',None),('explicit-false',False)]:
            c,folder=client(name,proof);entry=folder/'入口😀.xl';opened(c,entry,overload)
            location=query(c,entry,overload,'pick("x")');assert set(location)=={'uri','range'} and location['uri']==entry.as_uri() and location['range']['start']==dict(line=3,character=5),location
            done(name+'-preserves-standard-Location-without-prior-check');c.stop()

        c,folder=client('overlay',True);entry=folder/'入口😀.xl';opened(c,entry,overload,7)
        location=query(c,entry,overload,'pick("x")',7);value=metadata(location,entry,'overlay')
        assert value['version']==7 and value['normalizedSize']==len(overload.encode()) and value['normalizedSha256']==sha(overload.encode()) and location['range']['start']==dict(line=3,character=5),location
        done('cold-overload-has-current-target-overlay-proof-without-executor')
        assert all(query(c,entry,overload,'pick("x")',7)==location for _ in range(15))
        done('repeated-Locations-preserve-proof-and-range')
        assert c.call('textDocument/definition',dict(textDocument=dict(uri=entry.as_uri()),position=dict(line=overload.count('\n'),character=0),version=7)) is None
        done('no-symbol-returns-JSON-null-without-fabricated-proof')
        bad='func main(...) as int\n return missingName\nend\n';c.change(entry,bad,8)
        assert query(c,entry,bad,'missingName',8) is None and len(c.latest[entry.as_uri()]['diagnostics'])==1
        done('source-error-is-null-plus-real-diagnostic')
        c.change(entry,overload,9);value=metadata(query(c,entry,overload,'pick("x")',9),entry,'overlay');assert value['version']==9
        done('edit-recovery-publishes-only-current-target-version');c.stop()

        c,folder=client('dependency',True);entry=folder/'entry.xl';dep=folder/'proof_dep.xl';unused=folder/'unused.xl'
        opened(c,entry,imported);opened(c,dep,overlay,4);opened(c,unused,'func broken(\n',17)
        location=query(c,entry,imported,'value()');value=metadata(location,dep,'overlay')
        assert value['version']==4 and value['normalizedSha256']==sha(overlay.encode()) and location['uri']==dep.as_uri() and location['range']['start']==dict(line=2,character=12),location
        done('unsaved-dependency-proof-uses-target-version-and-excludes-unused-overlay')
        changed='\n\n'+overlay;c.change(dep,changed,5);location=query(c,entry,imported,'value()');value=metadata(location,dep,'overlay')
        assert value['version']==5 and value['normalizedSha256']==sha(changed.encode()) and location['range']['start']['line']==4,location
        done('dependency-edits-rebind-proof-and-definition-coordinate');c.stop()

        c,folder=client('disk',True);entry=folder/'entry.xl';dep=folder/'proof_dep.xl';raw=b'\xef\xbb\xbf'+depbody.replace('\n','\r\n').encode();dep.write_bytes(raw);protected[str(dep)]=sha(raw);opened(c,entry,imported)
        location=query(c,entry,imported,'value()');value=metadata(location,dep,'disk');disk=value['input'];identity=disk['metadata']['identity']
        assert value['normalizedSize']==len(depbody.encode()) and value['normalizedSha256']==sha(depbody.encode()) and disk['rawSize']==len(raw) and disk['rawSha256']==sha(raw),value
        assert 'version' not in value and re.fullmatch(r'[0-9a-f]{16}',identity) and len(json.dumps(location))<3500,location
        done('closed-BOM-CRLF-dependency-carries-both-byte-proofs-and-identity')
        replacement=folder/'replacement.tmp';replacement.write_bytes(raw);os.replace(replacement,dep)
        while True:
            event=c.next()
            if event.get('method')=='xlang/diagnosticsInvalidated':
                assert event['params']['uri']==entry.as_uri() and event['params']['version']==1,event;break
        value=metadata(query(c,entry,imported,'value()'),dep,'disk');assert value['input']['metadata']['identity']!=identity and value['input']['rawSha256']==sha(raw),value
        done('observed-same-byte-replacement-requires-new-target-identity')
        opened(c,dep,overlay,3);value=metadata(query(c,entry,imported,'value()'),dep,'overlay');assert value['version']==3 and 'input' not in value,value
        done('opened-target-replaces-disk-proof-with-overlay-proof');c.stop()

        c,folder=client('many-inputs',True);entry=folder/'entry.xl';lines=[]
        for i in range(128):
            path=folder/f'input_{i}.xl';path.write_text('export func value() as int\n return 7\nend\n',encoding='utf-8');protected[str(path)]=sha(path.read_bytes());lines.append(f'import input_{i}\n')
        text=''.join(lines)+'func main(...) as int\n return input_127.value()\nend\n';opened(c,entry,text)
        location=query(c,entry,text,'value()');value=metadata(location,folder/'input_127.xl','disk')
        assert len(json.dumps(location))<3500 and value['input']['rawSha256']==sha((folder/'input_127.xl').read_bytes()),location
        done('128-actual-inputs-return-only-target-file-metadata');c.stop()

        for i,invalid in enumerate([1,'true',[],{},None]):
            c,folder=client('invalid-'+str(i),initialize=False)
            c.send('initialize',dict(capabilities=dict(experimental=dict(definitionInputProof=invalid)),initializationOptions=dict(automaticDiagnostics=False)),1)
            reply,_=c.reply(1);assert reply['error']['code']==-32602,reply
            c.send('initialize',dict(capabilities=dict(experimental=dict(definitionInputProof=True)),initializationOptions=dict(automaticDiagnostics=False)),2)
            reply,_=c.reply(2);assert reply['result']['capabilities']['experimental']['definitionInputProof'] is True,reply;c.send('initialized',{});c.stop()
        done('invalid-capability-types-reject-and-valid-initialize-recovers')
        assert all(sha(Path(p).read_bytes())==h for p,h in protected.items());done('source-tool-and-original-unsaved-file-bytes-preserved');status='passed'
    finally:
        for i,c in enumerate(clients):
            stderr=c.stop();(root/f'server-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8');(root/f'server-{i}.stderr').write_text(stderr,encoding='utf-8')
        (root/'result.json').write_text(json.dumps(dict(status=status,scope='negotiated definition target-file proof over real native cold queries; not full IDE latency or transitive instantaneous file observation',protected=protected,cases=cases),ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(dict(status=status,cases=len(cases),proof=str(root/'result.json'))))


if __name__=='__main__':main()
