"""Selectively admit two existing XRT HTTP scanners, preserving all other ABI.

Each declaration/body is copied verbatim from canonical XRT. Exact insertion
anchors and reverse equality keep unrelated native/time changes out.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess
import sync_xrt_snapshot as metadata

def extract(text,name,body):
    # Locate the actual declaration start rather than a preceding comment.
    matches=list(re.finditer(r'XRT_API\s+[^;{}]*?\b'+name+r'\s*\(',text))
    assert len(matches)==1,name
    start=matches[0].start()
    if not body:return text[start:text.index(';',start)+1]
    brace=text.index('{',start);depth=1;end=brace+1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[start:end]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--xrt-root',type=Path,required=True)
    p.add_argument('--revision',required=True,help='Immutable upstream commit supplying these declarations and bodies')
    p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];native=a.xrt_root.resolve()
    revision=subprocess.check_output(['git','rev-parse',a.revision+'^{commit}'],cwd=native,text=True).strip()
    def upstream(relative):return subprocess.check_output(['git','show',revision+':'+relative],cwd=native).decode('utf-8-sig')
    assert not a.out.exists();records=[]
    selected=[('xrtHttpListNext','xrtHttpTokenNext','http.c'),('xrtHttpNameValueNext','xrtHttpParamNext','http_param.c')]
    declarations=upstream('include/xrt/http.h')
    updates=[]
    for relative in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h']:
        path=root/relative;before=path.read_bytes();ending=b'\r\n' if b'\r\n' in before else b'\n'
        text=before.decode('utf-8-sig').replace('\r\n','\n');original=text;insertions=[]
        for name,anchor,source in selected:
            assert not re.search(r'\b'+name+r'\s*\(',text),(relative,name,'already present; inspect existing receipt')
            for body in [False,True] if relative.startswith('lib/') else [False]:
                source_text=upstream('src/http/'+source) if body else declarations
                addition=extract(source_text,name,body)+'\n\n\n\n'
                anchor_match=list(re.finditer(r'XRT_API\s+[^;{}]*?\b'+anchor+r'\s*\([^;{]*(?:\{|;)',text))
                anchor_match=[m for m in anchor_match if m[0].rstrip().endswith('{' if body else ';')]
                assert len(anchor_match)==1,(relative,anchor,body)
                at=anchor_match[0].start();text=text[:at]+addition+text[at:];insertions.append(addition)
        restored=text
        for addition in reversed(insertions):
            assert restored.count(addition)==1
            restored=restored.replace(addition,'',1)
        assert restored==original
        after=text.encode('utf-8').replace(b'\n',ending)
        records.append(dict(Path=relative,BeforeSha256=hashlib.sha256(before).hexdigest(),AfterSha256=hashlib.sha256(after).hexdigest(),AddedBytes=len(after)-len(before)))
        updates.append((path,after))
    for path,data in updates:metadata._write_if_changed(path,data)
    core=set(metadata._api_names(root/'lib/xrt/xrt.h'))
    for product in ['xrt','xruntime']:
        impl=root/f'lib/{product}/{product}.h';decl=root/f'release/include/{product}_decl.h'
        names=set(metadata._api_names(impl));prefix='xrt/single/' if product=='xrt' else 'xrt/extlibs/xruntime/single/'
        value=metadata._metadata(product,prefix+product+'.h',prefix+product+'_decl.h',impl.read_bytes(),decl.read_bytes(),len(names),len(names-core) if product=='xruntime' else None)
        metadata._write_if_changed(root/f'lib/{product}/{product}.snapshot.json',(json.dumps(value,indent=2)+'\n').encode())
    metadata._write_if_changed(root/'src/api/xl_xrt_symbols.inc',metadata.render_symbol_table(root/'lib/xruntime/xruntime.h').encode('utf-8'))
    metadata.sync_capability_matrix(root)
    assert metadata._check_snapshot(root)
    report=dict(Complete=True,SelectiveSync=True,WholeTimeAbiUpgrade=False,OnlyAddedPublicAPIs=[n for n,_,_ in selected],
        UpstreamHead=revision,Records=records)
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
