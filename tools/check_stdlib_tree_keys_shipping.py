"""Actual-product native-width C keys and immutable byte-key ordering.
Source-free JIT/module/relocated AOT, strict saved C and real provider objects;
Linux genuine SO native lifetime/fault-prefix/IR refusal and plan pruning.
Stripped TCC executable symbols are never treated as pruning evidence.
No replacement providers, source compiler objects, release/full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
POSITIVE=('stdlib_tree_c_scalar_keys','stdlib_tree_bytes_keys','stdlib_tree_generic_owned_keys','stdlib_tree_scalar_pruning','stdlib_tree_char_keys')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','build-report','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();assert not out.exists();task=Path(__file__).resolve().parents[1];win=os.name=='nt'
 cc,nm,inspect=[shutil.which(n) for n in ('gcc','nm','objdump' if win else 'readelf')];assert cc and nm and inspect
 sys.path.insert(0,str(root/'tools'));from gen_tcc_vfs_resources import load_resources
 build=json.loads(a.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
 products=[root/'release'/n for n in (('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so'))]
 recorded={Path(r['Path']).resolve():r['Sha256'] for r in build['Products']};assert all(recorded.get(s)==sha(s) for s in products)
 host=task/'test/cases/stdlib_internal_container_product_host.c';native=task/'test/cases/stdlib_tree_key_native.c';memory=root/'test/support/module_product_memory.c';mutation=task/'test/cases/xlir_tree_key_contract.c'
 cases={n:task/'test/cases'/(n+'.xl') for n in POSITIVE}
 files={Path(__file__).resolve(),a.build_report.resolve(),Path(sys.executable),Path(cc),Path(nm),Path(inspect),host,native,memory,mutation,*cases.values()}
 for r in build['Inputs']+build['Products']:assert sha(r['Path'])==r['Sha256'];files.add(Path(r['Path']))
 inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)];out.mkdir()
 isolated,sdk,tmp,moved=[out/n for n in ('isolated','sdk','tmp','relocated')];release=isolated/'release';release.mkdir(parents=True)
 for d in (sdk,tmp,moved):d.mkdir()
 for s in products:
  shutil.copy2(s,release/s.name)
  if win and s.suffix=='.dll':shutil.copy2(s,isolated/s.name)
 for s in (root/'release/include').glob('*.h'):shutil.copy2(s,sdk/s.name)
 builddir=root/('build/tmp' if win else 'build/linux');manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if win else 'tcc_vfs_manifest_linux_runtime.txt');dist=root/'res/tcc/win64/x86_64' if win else builddir/'tcc-dist'
 for r in load_resources(manifest,root,dist,builddir/'xrt.o',builddir):
  if r.virtual.startswith('include/__xl_runtime/'):
   target=sdk/Path(r.virtual).relative_to('include');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(r.source,target)
 shutil.copy2(root/'lib/xruntime/xruntime.h',sdk/'xruntime.h')
 env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')};env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
 rows=[];proofs=[];prefixes=[];failures=[]
 report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,Proofs=proofs,Prefixes=prefixes,Failures=failures,CompilerSourceObjects=False,ProviderOverrides=False,Full53Acceptance=False)
 def run(tag,cmd,timeout=180):
  cmd=list(map(str,cmd));start=time.monotonic();r=subprocess.run(cmd,cwd=isolated,env=env,capture_output=True,timeout=timeout);data=r.stdout+r.stderr;log=out/(tag+'.log');log.write_bytes(data)
  rows.append(dict(Name=tag,Command=cmd,Exit=r.returncode,Seconds=time.monotonic()-start,TimeoutSeconds=timeout,Log=str(log),LogSha256=sha(log)));assert r.returncode==0,(tag,data[-6000:]);return data.decode(errors='replace')
 flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(sdk)]+([] if win else ['-D_GNU_SOURCE'])
 compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
 try:
  binary=isolated/('observer.exe' if win else 'observer');compiler=release/products[2].name
  links=[compiler] if win else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
  run('host.build',[cc,*flags,'-O2',host,*links,'-o',binary]);assert not any((isolated/n).exists() for n in ('src','lib','res','include'))
  for name in POSITIVE:
   try:
    source=isolated/(name+'.xl');shutil.copy2(cases[name],source);image=isolated/(name+('.exe' if win else ''))
    for mode in ('run','module','aot'):
     work=tmp/(name+'-'+mode);work.mkdir();assert 'internal-container '+mode+' complete' in run(name+'.'+mode,[binary,compiler,source,mode,work,image])
    for opt in ('O0','O2'):
     for file in ('module.c','xl6_xrt_impl.c'):run(name+'.'+opt+'.'+file,[cc,*flags,*compat,'-'+opt,'-c',work/file,'-o',out/(name+'.'+opt+'.'+file+'.o')])
    relocated=moved/image.name;shutil.move(image,relocated);imports=run(name+'.imports',[inspect,'-p' if win else '-d',relocated])
    deps=re.findall(r'DLL Name:\s*(\S+)',imports) if win else re.findall(r'\(NEEDED\).*?\[(.*?)\]',imports)
    assert not any(n.lower() in ('xl.dll','xlc.dll') or 'libxl' in n.lower() for n in deps);assert win or not re.search(r'\((?:RPATH|RUNPATH)\)',imports)
    run(name+'.relocated',[relocated]);proofs.append(dict(Name=name,Jit=True,Module=True,RelocatedAot=True,StrictC=['O0','O2']));print(name+': passed',flush=True)
   except Exception as e:failures.append(dict(Name=name,Error=repr(e)))
  for opt in ('O0','O2'):
   for stem in ('xl6_tree','xl6_tree_bytes_key','xl6_tree_projection'):
    obj=out/(stem+'.'+opt+'.o');run(stem+'.'+opt,[cc,*flags,*compat,'-'+opt,'-I'+str(root),'-I'+str(root/'release'),'-c',root/'src/runtime'/(stem+'.c'),'-o',obj])
    if stem=='xl6_tree':
     symbols=run(stem+'.'+opt+'.undefined',[nm,'-u',obj]);assert 'xrtTypedTree' in symbols and 'xl6TreeBytesKeyCodecV1' not in symbols and 'xl6ReferenceBytesAdmissionV1' not in symbols
  report['CoreTreeHasNoBytesFactoryDependency']=True
  if not win:
   library=root/'release';nativeflags=[*flags,'-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'lib/tcc')];nativeLinks=['-L'+str(library),'-lxlc','-Wl,-rpath,'+str(library),'-pthread','-ldl','-lm']
   binary=out/'mutation';run('mutation.build',[cc,*nativeflags,'-O0',mutation,*nativeLinks,'-o',binary])
   verifiedMutations=0
   for name,kind in (('stdlib_tree_c_scalar_keys','scalar'),('stdlib_tree_bytes_keys','bytes')):
    try:
     text=run('mutation.'+kind,[binary,cases[name],kind],timeout=900);assert re.search(r'tree-key '+kind+r' calls=\d+ mutations=[1-9]\d* codec='+str(int(kind=='bytes')),text),text
     verifiedMutations+=1
    except Exception as e:failures.append(dict(Name='mutation.'+kind,Error=repr(e)))
   report['TypedIrRefusalAndPlanPruning']=verifiedMutations==2
   for opt in ('O0','O2'):
    binary=out/('native.'+opt);run('native.'+opt+'.build',[cc,*nativeflags,'-'+opt,native,memory,*nativeLinks,'-o',binary]);assert 'physical ownership edges balanced' in run('native.'+opt,[binary])
    for operation in ('create','set','clone','first','keys'):
     for point in range(1025):
      text=run('native.'+opt+'.'+operation+'.'+str(point),[binary,operation,point])
      if 'hit=0 complete-prefix balanced' in text:
       prefixes.append(dict(Optimization=opt,Operation=operation,FirstNoHit=point,Runs=point+1));break
     else:raise AssertionError('fault prefix did not close '+operation)
   report['NativeCompleteFaultPrefixes']=True
  report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged'] and len(proofs)==len(POSITIVE) and not failures
 except Exception as e:failures.append(dict(Name='gate',Error=repr(e)))
 report['PhaseCount']=len(rows);report['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(out.rglob('*')) if f.is_file()]
 (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(Complete=report['Complete'],Phases=len(rows),Failures=failures),ensure_ascii=False),flush=True)
 return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
