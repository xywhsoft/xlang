"""Retire superseded internal text emitters without changing public string APIs.
Actual old-product reachability control; each retired namespace rejected before
C output. Raw externs cannot obtain a compiler-authored implementation. Exact
public NUL/Unicode/size/function-value JIT/module/relocated AOT, strict O0/O2 C,
and genuine SO closed bridge-kind/native plan checks. No provider substitution.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','build-report','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--baseline-build-report',type=Path)
 a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();assert not out.exists();task=Path(__file__).resolve().parents[1];win=os.name=='nt'
 cc,inspect=[shutil.which(n) for n in ('gcc','objdump' if win else 'readelf')];assert cc and inspect
 spec=task/'test/cases/stdlib_retired_text_bridges.json';table=json.loads(spec.read_text(encoding='utf-8'));assert len(table)==26
 source=task/'test/cases/stdlib_text_resident_contracts.xl';host=task/'test/cases/stdlib_internal_container_product_host.c';contract=task/'test/cases/xlir_retired_text_contract.c'
 build=json.loads(a.build_report.read_text(encoding='utf-8-sig'));old=json.loads(a.baseline_build_report.read_text(encoding='utf-8-sig')) if a.baseline_build_report else None
 builds=[build]+([old] if old else []);assert all(b['Complete'] and b['InputsUnchanged'] and b['Exit']==0 for b in builds)
 products=[root/'release'/n for n in (('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so'))]
 recorded={Path(r['Path']).resolve():r['Sha256'].lower() for r in build['Products']};assert all(recorded.get(f)==sha(f) for f in products)
 oldcli=next(Path(r['Path']) for r in old['Products'] if Path(r['Path']).name==products[0].name) if old else None
 files={Path(__file__).resolve(),Path(sys.executable),Path(cc),Path(inspect),a.build_report.resolve(),spec,source,host,contract}
 if a.baseline_build_report:files.add(a.baseline_build_report.resolve())
 for b in builds:
  for r in b['Inputs']+b['Products']:assert sha(r['Path'])==r['Sha256'].lower();files.add(Path(r['Path']))
 inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)];out.mkdir();isolated=out/'isolated';release=isolated/'release';release.mkdir(parents=True)
 sdk,tmp,moved=[out/n for n in ('sdk','tmp','relocated')]
 for d in (sdk,tmp,moved):d.mkdir()
 for f in products:
  shutil.copy2(f,release/f.name)
  if win and f.suffix=='.dll':shutil.copy2(f,isolated/f.name)
 for f in (root/'release/include').glob('*.h'):shutil.copy2(f,sdk/f.name)
 sys.path.insert(0,str(root/'tools'));from gen_tcc_vfs_resources import load_resources
 bd=root/('build/tmp' if win else 'build/linux');manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if win else 'tcc_vfs_manifest_linux_runtime.txt');dist=root/'res/tcc/win64/x86_64' if win else bd/'tcc-dist'
 for r in load_resources(manifest,root,dist,bd/'xrt.o',bd):
  if r.virtual.startswith('include/__xl_runtime/'):
   target=sdk/Path(r.virtual).relative_to('include');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(r.source,target)
 shutil.copy2(root/'lib/xruntime/xruntime.h',sdk/'xruntime.h')
 env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')};env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
 rows=[];failures=[];controls=[];report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Failures=failures,Controls=controls,Scope=__doc__,Full53Acceptance=False,ProviderOverrides=False,BaselineRequested=old is not None)
 def run(tag,cmd):
  cmd=list(map(str,cmd));start=time.monotonic();r=subprocess.run(cmd,cwd=isolated,env=env,capture_output=True,timeout=180);log=out/(tag+'.log');log.write_bytes(r.stdout+r.stderr)
  rows.append(dict(Name=tag,Command=cmd,Exit=r.returncode,Seconds=time.monotonic()-start,Log=str(log),LogSha256=sha(log)));return r.returncode,(r.stdout+r.stderr).decode(errors='replace')
 def ok(tag,cmd):
  code,text=run(tag,cmd);assert code==0,(tag,text[-6000:]);return text
 def arguments(entry):
  return ', '.join('"x"' if p['Type']=='string' else 'c_bool(false)' if p['Type']=='c_bool' else '1' for p in entry['Parameters'])
 try:
  # This old producer is immutable evidence, not a replacement compiler.
  if old:
   baseline=isolated/'legacy-control.xl';baseline.write_text('func main(...) as int\n'+''.join(' auto value'+str(i)+' = __xrt_string.'+e['Member']+'('+arguments(e)+')\n' for i,e in enumerate(table))+' return 0\nend\n',encoding='utf-8')
   generated=out/'legacy-control.c';ok('baseline.reachable',[oldcli,'cgen',baseline,'--emit',generated]);body=generated.read_text(encoding='utf-8')
   assert all(re.search(r'\b'+e['Name']+r'\([^;{}]*\)\s*\{',body) for e in table)
   report['OldAll26EmittersReachable']=True
  cli=release/products[0].name
  for i,e in enumerate(table):
   negative=isolated/('retired-'+str(i)+'.xl');negative.write_text('auto value = __xrt_string.'+e['Member']+'('+arguments(e)+')\n',encoding='utf-8');output=out/('retired-'+str(i)+'.c')
   code,text=run('retired.'+str(i),[cli,'cgen',negative,'--emit',output]);assert code!=0 and not output.exists() and '[stdlib]' in text,(e['Name'],text)
   # FFI remains explicit: it may be refused by the reserved native ABI or emit
   # a declaration, but must never recreate an internal compiler body.
   raw=isolated/('raw-'+str(i)+'.xl');params=', '.join(p['Type']+' '+p['Name'] for p in e['Parameters'])
   raw.write_text('extern "'+e['Name']+'" func old('+params+') as '+e['Result']+'\nauto value = old('+arguments(e)+')\n',encoding='utf-8');rawout=out/('raw-'+str(i)+'.c')
   rawcode,rawtext=run('raw.'+str(i),[cli,'cgen',raw,'--emit',rawout])
   if rawcode==0:
    assert rawout.exists() and not re.search(r'\b'+e['Name']+r'\([^;{}]*\)\s*\{',rawout.read_text(encoding='utf-8'))
   else:assert not rawout.exists() and ('[sem]' in rawtext or '[ir]' in rawtext or 'XLIR' in rawtext)
   controls.append(dict(Name=e['Name'],NamespaceRejected=True,RawExternRejected=rawcode!=0,NoGeneratedImplementation=True))
  flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(sdk)]+([] if win else ['-D_GNU_SOURCE'])
  compiler=release/products[2].name;links=[compiler] if win else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
  binary=isolated/('observer'+('.exe' if win else ''));ok('observer.build',[cc,*flags,'-O2',host,*links,'-o',binary]);local=isolated/source.name;shutil.copy2(source,local);image=isolated/('public-text'+('.exe' if win else ''))
  assert not any((isolated/n).exists() for n in ('src','lib','res','include'))
  for mode in ('run','module','aot'):
   work=tmp/mode;work.mkdir();assert 'internal-container '+mode+' complete' in ok('public.'+mode,[binary,compiler,local,mode,work,image])
  body=(work/'module.c').read_text(encoding='utf-8');assert not any(e['Name'] in body for e in table)
  compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
  for opt in ('O0','O2'):
   for f in ('module.c','xl6_xrt_impl.c'):ok('public.'+opt+'.'+f,[cc,*flags,*compat,'-'+opt,'-c',work/f,'-o',out/(opt+'.'+f+'.o')])
  target=moved/image.name;shutil.move(image,target);text=ok('public.imports',[inspect,'-p' if win else '-d',target]);deps=re.findall(r'DLL Name:\s*(\S+)',text) if win else re.findall(r'\(NEEDED\).*?\[(.*?)\]',text)
  assert not any(n.lower() in ('xl.dll','xlc.dll') or 'libxl' in n.lower() for n in deps);assert win or not re.search(r'\((?:RPATH|RUNPATH)\)',text);ok('public.relocated',[target])
  report['PublicResidentAcceptance']=True
  if not win:
   binary=isolated/'contract';ok('contract.build',[cc,*flags,'-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'lib/tcc'),'-O0',contract,*links,'-o',binary])
   text=ok('contract',[binary,source,*[e['Name'] for e in table]]);assert re.search(r'resident text targets=\d+ retired=26 exact native plan',text);report['TypedIrAndPlanAcceptance']=True
  report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged'] and len(controls)==26
 except Exception as e:failures.append(dict(Error=repr(e)))
 report['PhaseCount']=len(rows);report['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(out.rglob('*')) if f.is_file()]
 (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(dict(Complete=report['Complete'],Phases=len(rows),Failures=failures)),flush=True)
 return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
