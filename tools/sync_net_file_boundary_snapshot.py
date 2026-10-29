"""Selective NetFile borrowed-ownership/EOF correction; no time ABI upgrade.
Applies only unique bounded hunks from the reviewed independent XRT files and
proves exact reversibility of all four pinned headers. Metadata is regenerated
from these pinned products, never by copying a newer whole upstream header.
"""
from pathlib import Path
import argparse,difflib,hashlib,json,re,subprocess,sys

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
 p.add_argument('--xrt-root',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
 p.add_argument('--native-base',default='79f5da3c3c0859508329e42a2db79afd8f9e6fde')
 p.add_argument('--pinned-base',default='2ae572dab008754d2d8fdf34e5eb22df165edc65')
 a=p.parse_args();root=a.root.resolve();native=a.xrt_root.resolve();assert not a.out.exists()
 sha=lambda b:hashlib.sha256(b).hexdigest().upper()
 def hunks(name):
  old=subprocess.check_output(['git','show',a.native_base+':'+name],cwd=native).decode('utf8').replace('\r\n','\n')
  new=(native/name).read_text(encoding='utf-8-sig')
  lines=[]
  for tag,i,j,k,l in difflib.SequenceMatcher(a=old.splitlines(keepends=True),b=new.splitlines(keepends=True),autojunk=False).get_opcodes():
   if tag!='equal':
    # Include context so an insertion can be uniquely matched and undone.
    before=''.join(old.splitlines(keepends=True)[max(0,i-3):min(len(old.splitlines(keepends=True)),j+3)])
    after=''.join(new.splitlines(keepends=True)[max(0,k-3):min(len(new.splitlines(keepends=True)),l+3)])
    lines.append((before,after))
  return old,new,lines
 headerOld,headerNew,_=hunks('include/xrt/net_file.h')
 strip=lambda s:re.sub(r'\s+','',re.sub(r'/\*.*?\*/','',s,flags=re.S))
 assert strip(headerOld)==strip(headerNew),'This backport must not change any public declaration'
 # Public declaration block contains neither implementation includes nor its
 # standalone include guard; no symbol or type ABI changes in this backport.
 first=headerOld.index('/*\n\t打开可提交');last=headerOld.index('\n#endif',first)
 newFirst=headerNew.index('/*\n\t打开可提交');newLast=headerNew.index('\n#endif',newFirst)
 declarations=(headerOld[first:last],headerNew[newFirst:newLast])
 sourceOld,sourceNew,_=hunks('src/network/file.c')
 first=sourceOld.index('#if defined(XRT_FEATURE_NET_FILE)');newFirst=sourceNew.index('#if defined(XRT_FEATURE_NET_FILE)')
 implementation=(sourceOld[first:],sourceNew[newFirst:])
 _,_,portChanges=hunks('src/network/port_iocp.c')
 assert len(portChanges)==1,'Review any additional IOCP changes before syncing'
 changes=[declarations,implementation,*portChanges]
 records=[];pending=[]
 priorPath=root/'lib/xrt/backports/net_file_boundary.json'
 prior=json.loads(priorPath.read_text(encoding='utf8')) if priorPath.exists() else {'Records':[]}
 known={r['Path']:r for r in prior['Records']}
 for name in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h']:
  path=root/name;raw=path.read_bytes();ending=b'\r\n' if b'\r\n' in raw else b'\n'
  baselineText=subprocess.check_output(['git','show',a.pinned_base+':'+name],cwd=root).decode('utf8').replace('\r\n','\n')
  original=baselineText;body=original
  product='xruntime' if 'xruntime' in name else 'xrt'
  baseline=json.loads(subprocess.check_output(['git','show',a.pinned_base+':lib/'+product+'/'+product+'.snapshot.json'],cwd=root).decode())
  identity='implementation' if '/lib/' in '/'+name else 'declarations'
  baselineRaw=original.encode('utf8').replace(b'\n',ending)
  assert sha(baselineRaw)==baseline[identity]['sha256'],name
  selected=changes if '/lib/' in '/'+name else [declarations]
  for before,after in selected:
   assert body.count(before)==1,(name,'non-unique source block')
   body=body.replace(before,after,1)
  restored=body
  for before,after in reversed(selected):
   assert restored.count(after)==1,(name,'non-unique reverse block')
   restored=restored.replace(after,before,1)
  assert restored==original,(name,'out-of-scope change')
  data=body.encode('utf8').replace(b'\n',ending)
  # Resume/review only from the pinned baseline, this exact new product or a
  # previously recorded product. Never overwrite unrecorded workspace edits.
  allowed={sha(baselineRaw),sha(data)}
  if name in known:allowed.add(known[name]['AfterSha256'])
  assert sha(raw) in allowed,(name,'unrecorded workspace changes')
  pending.append((path,data))
  records.append(dict(Path=name,BeforeSha256=sha(baselineRaw),AfterSha256=sha(data),ChangedBytes=len(data)-len(baselineRaw)))
 for path,data in pending:path.write_bytes(data)
 sys.path.insert(0,str(root/'tools'));import sync_xrt_snapshot as metadata
 from gen_xrt_symbol_table import _api_names
 for product,impl,decl,upstream,upstreamDecl in [
  ('xrt','lib/xrt/xrt.h','release/include/xrt_decl.h','xrt/single/xrt.h','xrt/single/xrt_decl.h'),
  ('xruntime','lib/xruntime/xruntime.h','release/include/xruntime_decl.h','xrt/extlibs/xruntime/single/xruntime.h','xrt/extlibs/xruntime/single/xruntime_decl.h')]:
  names=set(_api_names(root/impl));core=set(_api_names(root/'lib/xrt/xrt.h'))
  value=metadata._metadata(product,upstream,upstreamDecl,(root/impl).read_bytes(),(root/decl).read_bytes(),len(names),len(names-core) if product=='xruntime' else None)
  (root/('lib/'+product+'/'+product+'.snapshot.json')).write_text(json.dumps(value,indent=2)+'\n',encoding='utf8')
 from gen_xrt_xlang_matrix import sync
 sync(root)
 assert metadata._check_snapshot(root)
 result=dict(Complete=True,SelectiveSync=True,PublicAbiChanged=False,WholeTimeAbiUpgrade=False,NativeBase=a.native_base,PinnedBase=a.pinned_base,
  PreservedPinnedMicrosecondsUntil=True,IndependentMillisecondsForUnchanged=True,Records=records,
  Sources=[dict(Path=n,Sha256=sha((native/n).read_bytes())) for n in ('include/xrt/net_file.h','src/network/file.c','src/network/port_iocp.c')])
 a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
 print(json.dumps(result,indent=2))
if __name__=='__main__':main()
