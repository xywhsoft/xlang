"""Apply only the audited WebSocket/TLS event-ownership delta to the pinned ABI.

The canonical and pinned runtimes deliberately keep different time contracts.
Context patches retain every unrelated pinned declaration and implementation.
"""
from pathlib import Path
import argparse,difflib,hashlib,json,re,subprocess,tempfile
import sync_xrt_snapshot as metadata

def chunks(text):
    markers=list(re.finditer(r'/\* (?:public|internal|source): ([^\n]+?) \*/',text))
    return {m[1]:text[m.start():markers[i+1].start() if i+1<len(markers) else len(text)] for i,m in enumerate(markers)}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--xrt-root',type=Path,required=True);p.add_argument('--base',default='6447bb29b0c822a52ff355f8a9bc9ab067ce3305');p.add_argument('--revision',required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];assert not a.out.exists()
    def upstream(rev,file):return subprocess.check_output(['git','show',rev+':'+file],cwd=a.xrt_root).decode('utf-8-sig').replace('\r\n','\n')
    original=chunks(upstream(a.base,'single/xrt.h'));current=chunks(upstream(a.revision,'single/xrt.h'))
    selected=['include/xrt/tls_stream.h','include/xrt/websocket_stream.h','src/internal/xrt_tls_stream.h','src/internal/xrt_websocket_stream.h','src/tls/stream.c','src/websocket/stream.c','src/websocket/stream_ref.c']
    updates=[];rows=[]
    for file in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h']:
        q=root/file;before=q.read_bytes();text=before.decode('utf-8-sig').replace('\r\n','\n');pinned=chunks(text)
        for n in selected:
            if n not in pinned:continue
            patch=''.join(difflib.unified_diff(original[n].splitlines(True),current[n].splitlines(True),'a/section.h','b/section.h',n=4))
            if not patch:continue
            with tempfile.TemporaryDirectory(prefix='ws-section-',dir=a.out.parent) as tmp:
                s=Path(tmp)/'section.h';s.write_text(pinned[n],encoding='utf-8',newline='\n')
                run=subprocess.run(['git','apply','--unsafe-paths','--whitespace=nowarn','-'],cwd=tmp,input=patch.encode(),capture_output=True)
                if run.returncode:raise RuntimeError(file+' '+n+'\n'+run.stderr.decode(errors='replace'))
                changed=s.read_text(encoding='utf-8')
            assert text.count(pinned[n])==1
            if n=='src/websocket/stream.c' and 'src/websocket/stream_ownership.c' not in pinned:changed+=current['src/websocket/stream_ownership.c']
            text=text.replace(pinned[n],changed,1)
        # Pinned microsecond contract must remain exactly unchanged.
        for name in ['xrtNetEngineAfter','xrtTaskNetAfter','xrtWsStreamConfigInit']:
            def signatures(s):return re.findall(r'XRT_API[^;{}]*?\b'+name+r'\s*\([^;{]*(?:;|\{)',s,re.S)
            assert signatures(before.decode('utf-8-sig'))==signatures(text),(file,name)
        after=text.encode();updates.append((q,after));rows.append(dict(Path=file,Before=hashlib.sha256(before).hexdigest(),After=hashlib.sha256(after).hexdigest()))
    for q,b in updates:metadata._write_if_changed(q,b)
    core=set(metadata._api_names(root/'lib/xrt/xrt.h'))
    for product in ['xrt','xruntime']:
        impl=root/f'lib/{product}/{product}.h';decl=root/f'release/include/{product}_decl.h';names=set(metadata._api_names(impl));prefix='xrt/single/' if product=='xrt' else 'xrt/extlibs/xruntime/single/'
        value=metadata._metadata(product,prefix+product+'.h',prefix+product+'_decl.h',impl.read_bytes(),decl.read_bytes(),len(names),len(names-core) if product=='xruntime' else None)
        metadata._write_if_changed(root/f'lib/{product}/{product}.snapshot.json',(json.dumps(value,indent=2)+'\n').encode())
    metadata._write_if_changed(root/'src/api/xl_xrt_symbols.inc',metadata.render_symbol_table(root/'lib/xruntime/xruntime.h').encode());metadata.sync_capability_matrix(root)
    assert metadata._check_snapshot(root)
    a.out.write_text(json.dumps(dict(Complete=True,Base=a.base,Revision=a.revision,WholeTimeAbiUpgrade=False,Rows=rows),indent=2)+'\n',encoding='utf-8');print('WebSocket owned snapshot synchronized')
if __name__=='__main__':main()
