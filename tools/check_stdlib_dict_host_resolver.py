"""Actual host resolver TU and real Dict/Set cycle plan retirement.

Link every other object from a hash-verified full-source public compiler;
compile the actual xl_api.c inside the test to reach its private resolver.
This does not simulate containers, replace the resolver or certify TCC deletion.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','public-directory','out'): p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--test-root',type=Path)
 a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
 test=a.test_root.resolve() if a.test_root else root
 assert not out.exists(); cc=shutil.which('gcc'); assert cc
 manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
 assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
 selected=[Path(r['Path']) for r in build['Objects'] if Path(r['Path']).name not in
  ('xl_api.o','xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
 assert len(selected)==len(build['Objects'])-2
 fixture=test/'test/cases/stdlib_dict_host_resolver.c'
 files={Path(__file__).resolve(),manifest,fixture,Path(cc),Path(sys.executable)}
 for folder in ('src','lib','release/include'):
  files.update(p for p in (root/folder).rglob('*') if p.is_file())
 for row in build['Inputs']+build['Headers']+build['Objects']:
  assert sha(row['Path'])==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
 runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
 files.add(runtime); inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
 out.mkdir(); rows=[]; products=[]; objects=[]; controls=[]; prefixes=[]; env=dict(os.environ)
 if os.name=='nt': shutil.copy2(runtime,out/'xl.dll')
 else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
 flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-DXL_ENABLE_EXE_COMPILER=1',
  '-DTCC_DISABLE_BUILTIN_VFS','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),
  '-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),'-Wno-unused-function','-Wno-unused-label',
  '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
  '-Wno-missing-field-initializers','-Wno-pointer-sign']
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
 if os.name!='nt': flags+=['-D_GNU_SOURCE']; systems+=['-Wl,-rpath,'+str(runtime.parent)]
 report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Objects=objects,
  NegativeControls=controls,ClosedPrefixes=prefixes,HostResolverPlanProof=True,ModuleTccRetirement=False,
  Full53Acceptance=False,Scope=__doc__)
 def run(label,command,reject=False):
  r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=240)
  data=r.stdout+r.stderr; log=out/(label+'.log'); log.write_bytes(data)
  rows.append(dict(Label=label,Command=list(map(str,command)),Exit=r.returncode,ExpectedRejection=reject))
  assert (r.returncode!=0 if reject else r.returncode==0),(label,data[-5000:])
  if reject: assert b'assert' in data.lower()
  return data.decode(errors='replace')
 try:
  for opt in ('O0','O2'):
   obj=out/(opt+'.o'); run(opt+'-compile',[cc,*flags,'-'+opt,'-c',fixture,'-o',obj]); objects.append(dict(Path=str(obj),Sha256=sha(obj)))
   binary=out/(opt+('.exe' if os.name=='nt' else ''))
   run(opt+'-link',[cc,obj,*selected,runtime,*systems,'-o',binary]); run(opt+'-vectors',[binary,'vectors'])
   data=run(opt+'-oom',[binary,'oom']); match=re.search(r'closed runs=(\d+) first-no-hit=(\d+)',data); assert match
   runs,first=map(int,match.groups()); assert runs==first+1
   points=re.findall(r'point=(\d+) hit=(\d) balanced',data); assert points==[(str(i),str(int(i<first))) for i in range(runs)]
   prefixes.append(dict(Optimization=opt,Runs=runs,FirstNoHit=first)); products.append(dict(Path=str(binary),Sha256=sha(binary)))
  host=root/'src/api/xl_api.c'; body=host.read_text(encoding='utf-8')
  for family in ('Set','Dict'):
   anchor='        if ((adapter=xl6'+family+'OwnershipAdapterV2(ref,policies,policyCount)) != NULL) return adapter;'
   assert body.count(anchor)==1
   altered=out/('host-no-'+family+'.c'); altered.write_text(body.replace(anchor,''),encoding='utf-8')
   source=out/('no-'+family+'.c'); text=fixture.read_text(encoding='utf-8')
   text=text.replace('#include "src/api/xl_api.c"','#include "'+altered.as_posix()+'"')
   source.write_text(text,encoding='utf-8'); obj=out/('no-'+family+'.o')
   run(family+'-compile',[cc,*flags,'-O0','-c',source,'-o',obj]); objects.append(dict(Path=str(obj),Sha256=sha(obj)))
   binary=out/('no-'+family+('.exe' if os.name=='nt' else ''))
   run(family+'-link',[cc,obj,*selected,runtime,*systems,'-o',binary]); run(family+'-reject',[binary,'vectors'],True)
   controls.append(dict(Name=family+'-host-admission-omitted',Compiled=True,Rejected=True,Source=str(altered),
    SourceSha256=sha(altered),Fixture=str(source),FixtureSha256=sha(source),Product=str(binary),ProductSha256=sha(binary)))
  report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
  report['Complete']=report['InputsUnchanged'] and len(controls)==2 and len(prefixes)==2; assert report['Complete']
 finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print('Actual host Dict/Set resolver: O0/O2 cycles, complete OOM prefixes, two compiled omissions rejected')

if __name__=='__main__': main()
