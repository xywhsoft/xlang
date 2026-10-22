"""Negotiated automatic diagnostic input provenance over actual stdio."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
from check_lsp_automatic_diagnostics import AutomaticClient


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    sha=lambda value:hashlib.sha256(value).hexdigest()
    protected={str(p):sha(p.read_bytes()) for p in [server,Path(__file__),Path(__file__).with_name('check_lsp_automatic_diagnostics.py'),Path(__file__).with_name('check_language_snapshots.py')]}
    cases=[];clients=[];status='failed'
    good='func main(...) as int\n string text = "中文😀"; return 0\nend\n'
    bad=good.replace('return 0','return missingName')
    def done(name,**data):cases.append(dict(name=name,passed=True,**data))
    def client(name,proof=None,versions=True,automatic=True):
        folder=root/name;folder.mkdir();c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c)
        capabilities=dict(textDocument=dict(publishDiagnostics=dict(versionSupport=versions)))
        if proof is not None:capabilities['experimental']=dict(diagnosticInputProof=proof)
        c.send('initialize',dict(capabilities=capabilities,initializationOptions=dict(checkOnSave=False,automaticDiagnostics=automatic,diagnosticDelay=0)),1)
        reply,_=c.reply(1);assert reply['result']['capabilities']['experimental']['diagnosticInputProof'] is True,reply
        c.send('initialized',{});return c,folder
    def opened(c,path,text,version=1,uri=None):
        path.write_bytes(b'// overlay must remain unsaved\n');protected[str(path)]=sha(path.read_bytes())
        c.send('textDocument/didOpen',dict(textDocument=dict(uri=uri or path.as_uri(),languageId='xlang',text=text,version=version)))
    def envelope(params,cleared=False):
        value=params['xlang'];assert value['schema']==1 and value['cleared'] is cleared,value
        assert re.fullmatch(r'0|[1-9][0-9]*',value['generation']),value
        assert all('xlang' not in d for d in params['diagnostics']),params
        if cleared:assert 'document' not in value and not params['diagnostics'],params
        else:assert set(value)=={'schema','generation','cleared','document'},value
        return value
    def overlay(params,path,text,version):
        value=envelope(params);document=value['document']
        assert set(document)=={'path','origin','version','normalizedSize','normalizedSha256'},document
        assert Path(document['path'])==path and document['origin']=='overlay' and document['version']==version,document
        assert document['normalizedSize']==len(text.encode()) and document['normalizedSha256']==sha(text.encode()),document
        return value
    try:
        for name,proof in [('unnegotiated',None),('explicit-false',False)]:
            c,folder=client(name,proof);entry=folder/'entry.xl';opened(c,entry,bad);c.quiet()
            assert len(c.latest[entry.as_uri()]['diagnostics'])==1 and all('xlang' not in e for e in c.events),c.events
            c.send('textDocument/didClose',dict(textDocument=dict(uri=entry.as_uri())));c.quiet()
            assert not c.latest[entry.as_uri()]['diagnostics'] and all('xlang' not in e for e in c.events),c.events
            done(name+'-preserves-standard-notifications');c.stop()

        c,folder=client('overlay',True);entry=folder/'入口😀.xl';opened(c,entry,bad,7);c.quiet()
        params=c.latest[entry.as_uri()];first=overlay(params,entry,bad,7)
        assert params['version']==7 and len(params['diagnostics'])==1,params
        done('negotiated-unicode-overlay-proof-without-executor')
        response=c.call('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=7));c.quiet()
        group=next(g for g in response['documents'] if Path(g['path'])==entry)
        assert first['document']=={k:v for k,v in group.items() if k!='diagnostics'},group
        done('proof-equals-verified-native-result-metadata')
        c.change(entry,good,8);c.quiet();params=c.latest[entry.as_uri()];second=overlay(params,entry,good,8)
        assert not params['diagnostics'] and int(second['generation'])>int(first['generation']),params
        done('clean-result-has-current-proof-and-is-not-withdrawal')
        c.change(entry,bad,9);c.quiet();overlay(c.latest[entry.as_uri()],entry,bad,9)
        c.send('textDocument/didClose',dict(textDocument=dict(uri=entry.as_uri())));c.quiet()
        cleared=envelope(c.latest[entry.as_uri()],True);assert c.latest[entry.as_uri()].get('version',9)==9
        done('close-clears-own-uri-without-reusing-file-proof')
        alternate=re.sub(r'%[0-9A-F]{2}',lambda m:m[0].lower(),entry.as_uri())
        opened(c,entry,bad,1,alternate);c.quiet();again=overlay(c.latest[alternate],entry,bad,1)
        assert int(again['generation'])>int(cleared['generation']) and c.latest[alternate]['version']==1
        done('slot-reopen-version-reset-and-exact-URI-spelling');c.stop()

        c,folder=client('no-versions',True,False);entry=folder/'entry.xl';opened(c,entry,bad,5);c.quiet()
        overlay(c.latest[entry.as_uri()],entry,bad,5);assert 'version' not in c.latest[entry.as_uri()]
        c.send('textDocument/didClose',dict(textDocument=dict(uri=entry.as_uri())));c.quiet()
        envelope(c.latest[entry.as_uri()],True);assert 'version' not in c.latest[entry.as_uri()]
        done('standard-version-support-is-independent-of-opted-in-proof');c.stop()

        c,folder=client('disk',True);entry=folder/'entry.xl';dep=folder/'dependency.xl'
        imported='import dependency\nfunc main(...) as int\n return dependency.value()\nend\n'
        deptext='export func value() as int\n return "错误😀"\nend\n'
        raw=b'\xef\xbb\xbf'+deptext.replace('\n','\r\n').encode();dep.write_bytes(raw)
        opened(c,entry,imported);c.quiet();params=c.latest[dep.as_uri()];value=envelope(params);document=value['document'];disk=document['input']
        assert document['origin']=='disk' and 'version' not in document and 'version' not in params,document
        assert Path(document['path'])==dep and document['normalizedSize']==len(deptext.encode()) and document['normalizedSha256']==sha(deptext.encode()),document
        assert disk['origin']=='disk' and Path(disk['path'])==dep and disk['rawSize']==len(raw) and disk['rawSha256']==sha(raw),disk
        metadata=disk['metadata'];assert all(re.fullmatch(r'[0-9a-f]{16}',metadata[k]) for k in ['device','identity']),metadata
        assert len(params['diagnostics'])==1 and len(json.dumps(value))<3000,value
        done('BOM-CRLF-disk-has-normalized-and-raw-proofs-with-native-identity')
        response=c.call('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=1));c.quiet()
        group=next(g for g in response['documents'] if Path(g['path'])==dep)
        assert document=={k:v for k,v in group.items() if k!='diagnostics'},group
        done('disk-proof-equals-exact-verified-native-input-group')
        replacement=folder/'replacement.tmp';replacement.write_bytes(raw);os.replace(replacement,dep)
        c.wait_for(lambda s:c.latest[dep.as_uri()]['xlang'].get('document',{}).get('input',{}).get('metadata',{}).get('identity') not in [None,metadata['identity']])
        params=c.latest[dep.as_uri()];updated=envelope(params)['document']
        assert updated['normalizedSha256']==document['normalizedSha256'] and updated['input']['rawSha256']==disk['rawSha256'] and len(params['diagnostics'])==1,params
        done('same-byte-disk-replacement-publishes-new-identity-proof')
        opened(c,dep,'export func value() as int\n return 0\nend\n',3);c.quiet()
        params=c.latest[dep.as_uri()];overlay(params,dep,'export func value() as int\n return 0\nend\n',3)
        assert not params['diagnostics'],params
        done('opening-disk-dependency-rebinds-proof-to-current-overlay');c.stop()

        c,folder=client('manual',True,automatic=False);entry=folder/'entry.xl';opened(c,entry,bad);c.quiet();assert not c.events
        response=c.call('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=1));assert response['count']==1,response
        assert c.events and all('xlang' not in e for e in c.events),c.events
        done('manual-mode-keeps-existing-batch-contract');c.stop()

        for i,invalid in enumerate([1,'true',[],{},None]):
            folder=root/('invalid-'+str(i));folder.mkdir();c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c)
            c.send('initialize',dict(capabilities=dict(experimental=dict(diagnosticInputProof=invalid))),1)
            reply,_=c.reply(1);assert reply['error']['code']==-32602,reply
            c.send('initialize',dict(capabilities=dict(experimental=dict(diagnosticInputProof=True))),2)
            reply,_=c.reply(2);assert reply['result']['capabilities']['experimental']['diagnosticInputProof'] is True,reply
            c.send('initialized',{});c.stop()
        done('invalid-capability-types-rejected-and-initialize-can-recover')
        assert all(sha(Path(p).read_bytes())==h for p,h in protected.items())
        done('source-tools-and-unsaved-original-bytes-preserved');status='passed'
    finally:
        for i,c in enumerate(clients):
            stderr=c.stop();(root/f'server-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8');(root/f'server-{i}.stderr').write_text(stderr,encoding='utf-8')
        report=dict(status=status,scope='negotiated per-file automatic diagnostic metadata; not a complete IDE input synchronization or semantic validity proof',protected=protected,cases=cases)
        (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(dict(status=status,cases=len(cases),proof=str(root/'result.json'))))


if __name__=='__main__':main()
