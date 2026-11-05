"""Native/ForeignView semantic finalization CPU kernel over an explicit compiler overlay.
No full compiler/VFS release qualification; reused objects stay individually pinned.
"""
from pathlib import Path
import argparse,json,hashlib,os,shutil,subprocess,time
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','overlay','out'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();o=json.loads(a.overlay.read_text());assert o['complete'] and o['passed'] and not o['fullRebuild']
 parent=Path(o['parentBuild']);assert sha(parent)==o['parentBuildSha256'];base=json.loads(parent.read_text(encoding='utf-8-sig'));assert base['Complete'] and base['InputsUnchanged']
 source=Path(o['sourceRoot']);sdk=source/'release/win_x64';out=a.out.resolve();out.mkdir(exist_ok=False)
 replacements={row['ParentObject']:row for row in o['Objects']};objects=[];pins={a.overlay,parent,Path(__file__),a.root/'test/cases/xlir_ownership_finalization_view_runtime.c',a.root/'test/cases/stdlib_static_module_native_host.c'}
 for row in base['Objects']:
  if Path(row['Source']).name in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c'):continue
  row=replacements.get(row['Path'],row);assert sha(row['Path'])==row['Sha256'].lower();objects.append(row['Path']);pins.update([Path(row['Source']),Path(row['Path'])])
 cc=shutil.which('gcc');assert cc;pins.add(Path(cc));runtime=Path(o['RuntimeLibrary']);pins.add(runtime)
 if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
 pins.update((source/'src/runtime').glob('xl6_ownership*.h'));pins.update([source/'src/runtime/xl6_foreign_view.h',sdk/'include/xrt_decl.h'])
 inputs={str(p):sha(p) for p in pins};report=dict(complete=False,passed=False,fullCompilerRebuild=False,scope=__doc__,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(source),'-I'+str(sdk)]
 systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
 def run(name,command):
  command=list(map(str,command));start=time.monotonic();r=subprocess.run(command,cwd=source,capture_output=True,timeout=120);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
  report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save();print(name,r.returncode,flush=True);assert not r.returncode,log
 try:
  for opt in ('O0','O2'):
   target=out/('view-'+opt+('.exe' if os.name=='nt' else ''));args=[cc,*flags,'-'+opt,a.root/'test/cases/xlir_ownership_finalization_view_runtime.c',*objects,runtime,*systems,'-o',target]
   rsp=out/(opt+'.rsp');rsp.write_text('\n'.join('"'+str(v).replace('\\','/')+'"' for v in args[1:]))
   run(opt+'-build',[cc,'@'+str(rsp)]);run(opt+'-run',[target])
   legacy=out/('static-'+opt+('.exe' if os.name=='nt' else ''))
   args=[cc,*flags,'-'+opt,a.root/'test/cases/stdlib_static_module_native_host.c',*objects,runtime,*systems,'-o',legacy]
   rsp=out/(opt+'-static.rsp');rsp.write_text('\n'.join('"'+str(v).replace('\\','/')+'"' for v in args[1:]))
   run(opt+'-static-build',[cc,'@'+str(rsp)])
   for mode in ('2','3','4','7','8','14','15','16','20','21'):run(opt+'-static-'+mode,[legacy,mode])
  report['inputsUnchanged']=all(sha(p)==h for p,h in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True)
 finally:save()
if __name__=='__main__':main()
