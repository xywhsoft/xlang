"""Win64 COM C ABI oracle: authoritative SDK versus private TinyCC types.

No COM operation runs. The same source proves every typed slot/record field,
prints sizes/alignments/offsets, and is compiled with both GCC and real TCC.
TCC verifies SDK-derived sizes/alignments/offsets with static assertions; it
does not execute a generated executable. The GCC oracle is executed in both
header modes. This does not bypass antivirus blocking of an earlier artifact.
The actual resident COM component is compiled by both compilers as C.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out','tcc','tcc-dist','sdk-include'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();assert os.name=='nt';root,out,tcc,dist,sdk=[v.resolve() for v in (a.root,a.out,a.tcc,a.tcc_dist,a.sdk_include)];gcc=Path(shutil.which('gcc'));sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();out.mkdir(parents=True,exist_ok=False)
 fixture=root/'test/cases/native_com_win_abi_oracle.c';header=root/'src/runtime/xl6_native_com_win_abi.h';component=root/'src/runtime/xl6_native_com.c';inputs={str(f):sha(f) for f in (Path(__file__),gcc,tcc,fixture,header,component)}
 for f in (root/'src/runtime').glob('*.h'):inputs[str(f)]=sha(f)
 for f in dist.rglob('*'):
  if f.is_file():inputs[str(f)]=sha(f)
 for name in ('windows.h','unknwn.h','objidl.h','oleidl.h','wtypes.h','wtypesbase.h','guiddef.h','winnt.h','windef.h'):inputs[str(sdk/name)]=sha(sdk/name)
 report=dict(scope=__doc__,complete=False,passed=False,windowsArchitecture='x86_64',comOrUiExecuted=False,sourceForwardingChanges=False,inputs=inputs,rows=[]);facts=[]
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,command,accepted=True):
  command=list(map(str,command));start=time.monotonic();r=subprocess.run(command,cwd=root,capture_output=True,timeout=120);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,accepted=accepted,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save();print(name,r.returncode,flush=True);assert (not r.returncode)==accepted,log;return r.stdout
 for opt in ('O0','O2'):
  for mode in ('sdk','internal'):
   exe=out/(mode+'-'+opt+'.exe');flags=['-DXL6_COM_ABI_INTERNAL','-I'+str(root)] if mode=='internal' else []
   run(mode+'-'+opt+'-build',[gcc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(sdk),'-'+opt,*flags,fixture,'-o',exe]);facts.append(run(mode+'-'+opt+'-run',[exe]).replace(b'\r\n',b'\n'))
 assert facts and all(x==facts[0] for x in facts);(out/'matching-facts.txt').write_bytes(facts[0]);report['abiFactsSha256']=sha(out/'matching-facts.txt');report['matchingFactLines']=len(facts[0].splitlines());report['typedVtableSlots']=41
 witness=['#define XL6_COM_ABI_INTERNAL','#include '+json.dumps(str(fixture).replace('\\','/'))]
 for line in facts[0].decode('ascii').splitlines():
  parts=line.split()
  if parts[0]=='type':
   witness+=['_Static_assert(sizeof('+parts[1]+')=='+parts[2]+',"SDK sizeof '+parts[1]+'");','_Static_assert(_Alignof('+parts[1]+')=='+parts[3]+',"SDK align '+parts[1]+'");']
  else:
   ty,field=parts[1].split('.',1);witness+=['_Static_assert(offsetof('+ty+','+field+')=='+parts[2]+',"SDK offsetof '+parts[1]+'");']
 w=out/'sdk-witness.c';w.write_text('\n'.join(witness)+'\n');run('tcc-SDK-witness-object',[tcc,'-B'+str(dist),'-std=c11','-I'+str(root),'-c',w,'-o',out/'tcc-sdk-witness.o']);report['tccLayoutQualification']='real TCC object: exact typed slots and SDK-derived static size/alignment/offset witnesses; not executable runtime'
 common=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'src'),'-I'+str(root/'release/win_x64'),'-I'+str(root/'release/win_x64/include'),'-I'+str(root/'lib/tcc'),'-I'+str(root/'lib/lzma')]
 run('gcc-native-com-object',[gcc,'-Wall','-Wextra','-Werror','-O2',*common,'-c',component,'-o',out/'native-com-gcc.o'])
 run('tcc-native-com-object',[tcc,'-B'+str(dist),*common,'-DXL_STATIC_TCC','-DCONFIG_TCC_STATIC','-DXL6_COMPILER_SOURCE_VFS','-c',component,'-o',out/'native-com-tcc.o'])
 source=header.read_text();changes={
  'wrong-BOOL':('STGMEDIUM*,BOOL);','STGMEDIUM*,BYTE);'),
  'erased-input':('QueryGetData)(IDataObject*,FORMATETC*);','QueryGetData)(IDataObject*,void*);'),
  'slot-order':('HRESULT (STDMETHODCALLTYPE* GetData)(IDataObject*,FORMATETC*,STGMEDIUM*);\n    HRESULT (STDMETHODCALLTYPE* GetDataHere)(IDataObject*,FORMATETC*,STGMEDIUM*);','HRESULT (STDMETHODCALLTYPE* GetDataHere)(IDataObject*,FORMATETC*,STGMEDIUM*);\n    HRESULT (STDMETHODCALLTYPE* GetData)(IDataObject*,FORMATETC*,STGMEDIUM*);')}
 for name,(old,new) in changes.items():
  assert old in source;bad=out/name/'src/runtime';bad.mkdir(parents=True);(bad/header.name).write_text(source.replace(old,new));run('reject-'+name,[gcc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(bad.parents[1]),'-I'+str(root),'-DXL6_COM_ABI_INTERNAL','-fsyntax-only',fixture],False)
 report.update(complete=True,passed=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()));assert report['inputsUnchanged'];save()
if __name__=='__main__':main()
