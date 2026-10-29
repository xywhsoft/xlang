"""Apply the single reviewed HTTP size-query fix without upgrading the snapshot."""
from pathlib import Path
import argparse, hashlib, json, re, subprocess
import sync_xrt_snapshot as metadata

def implementation(text):
    match=list(re.finditer(r'XRT_API\s+bool\s+xrtHttpContentEncodingWrite\s*\([^;{}]*\)\s*\{',text))
    assert len(match)==1
    start=match[0].start();end=match[0].end();depth=1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[start:end]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xrt-root',type=Path,required=True)
    parser.add_argument('--commit',required=True,help='The reviewed size-query fix commit')
    parser.add_argument('--out',type=Path,required=True);a=parser.parse_args()
    root=Path(__file__).resolve().parents[1];native=a.xrt_root
    head=subprocess.check_output(['git','rev-parse',a.commit+'^{commit}'],cwd=native,text=True).strip()
    beforeSource=subprocess.check_output(['git','show',head+'^:src/http/http_content_encoding.c'],cwd=native).decode()
    afterSource=subprocess.check_output(['git','show',head+':src/http/http_content_encoding.c'],cwd=native).decode()
    old=implementation(beforeSource);new=implementation(afterSource);assert old!=new
    assert not a.out.exists()
    records=[];updates=[]
    for relative in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h']:
        path=root/relative;before=path.read_bytes();text=before.decode('utf-8-sig').replace('\r\n','\n')
        assert implementation(text)==old
        changed=text.replace(old,new,1);assert changed.replace(new,old,1)==text
        after=changed.encode().replace(b'\n',b'\r\n' if b'\r\n' in before else b'\n')
        updates.append((path,after));records.append(dict(Path=relative,BeforeSha256=hashlib.sha256(before).hexdigest(),AfterSha256=hashlib.sha256(after).hexdigest()))
    for path,data in updates:metadata._write_if_changed(path,data)
    core=set(metadata._api_names(root/'lib/xrt/xrt.h'))
    for product in ['xrt','xruntime']:
        impl=root/f'lib/{product}/{product}.h';decl=root/f'release/include/{product}_decl.h';names=set(metadata._api_names(impl))
        prefix='xrt/single/' if product=='xrt' else 'xrt/extlibs/xruntime/single/'
        value=metadata._metadata(product,prefix+product+'.h',prefix+product+'_decl.h',impl.read_bytes(),decl.read_bytes(),len(names),len(names-core) if product=='xruntime' else None)
        metadata._write_if_changed(root/f'lib/{product}/{product}.snapshot.json',(json.dumps(value,indent=2)+'\n').encode())
    metadata.sync_capability_matrix(root)
    assert metadata._check_snapshot(root)
    report=dict(Complete=True,SelectiveSync=True,WholeTimeAbiUpgrade=False,UpstreamCommit=head,Records=records)
    a.out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
