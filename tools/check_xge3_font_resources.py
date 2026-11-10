"""Matched declarative Face/Family/font-query JIT and static AOT acceptance.

Uses a generated, valid fixture. An enabled EGL package also draws queued text
after cache clear and Font close. No window, OS font, audio or network is needed.
This is not complete glyph-run/rasterizer or module-global delivery.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--graphics',action='store_true',help='Require actual queued EGL text rendering in the matched package')
 for name in ('root','compiler','compiler-report','dynamic-packages','static-packages','out','objdump'):
  p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();root,compiler,cohort,dynamic,static,out,objdump=[v.resolve() for v in
  (a.root,a.compiler,a.compiler_report,a.dynamic_packages,a.static_packages,a.out,a.objdump)]
 sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
 metadata=json.loads(cohort.read_text());assert metadata['passed'] and metadata['fullTccXrtAndVfs']
 case=root/'test/cases/foreign_xge3_font_resources.xl';example=root/'lib/bindings/xge/examples/font_faces.xl'
 font=example.parent/'data/font_fixture.ttf';sdk=compiler.parent/'include'
 files=[Path(__file__).resolve(),compiler,cohort,objdump,case,case.with_suffix('.c'),case.with_suffix('.h'),example,font,example.parent/'data/README.md']
 for folder in (sdk,dynamic,static):files.extend(v for v in folder.rglob('*') if v.is_file())
 for name in ('xl.dll','xlc.dll') if os.name=='nt' else ('libxl.so','libxlc.so'):files.append(compiler.parent/name)
 inputs={str(v):sha(v) for v in files};out.mkdir(parents=True,exist_ok=False)
 report=dict(passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,networkTests=False,graphicsDevice=a.graphics,hardwarePerformance=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def run(name,args,packages=None,working=None):
  args=list(map(str,args));env=dict(os.environ,XLANG_XGE_FONT_FIXTURE=str(font),XLANG_XGE_FONT_TEMP=str(out/'font-cache.xrf'))
  env.pop('XLANG_PACKAGE_PATH',None)
  if packages:env['XLANG_PACKAGE_PATH']=str(packages)
  r=subprocess.run(args,cwd=working or compiler.parent,env=env,capture_output=True,timeout=180)
  log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
  report['rows'].append(dict(name=name,command=args,workingDirectory=str(working or compiler.parent),exitCode=r.returncode,logSha256=sha(log)))
  save();print(name,r.returncode,flush=True);assert not r.returncode,log
  return r
 include=['--inc-path',root,'--inc-path',sdk]
 for file in (case,example):
  name=file.stem;working=example.parent if file==example else compiler.parent
  cfile,hfile=out/(name+'.c'),out/(name+'.h')
  run(name+'-emit',[compiler,'cgen',file,'--emit',cfile,'--emit-header',hfile],dynamic,working)
  r=run(name+'-jit',[compiler,'run',file,*include],dynamic,working)
  if file==case:
   assert b'font hot query allocations: 0' in r.stdout
   assert (b'font queued GPU rendering passed:' in r.stdout)==a.graphics
  binary=out/(name+('.exe' if os.name=='nt' else ''))
  run(name+'-static-build',[compiler,'build',file,'-o',binary,*include],static,working)
  r=run(name+'-static-run',[binary],working=working)
  if file==case:
   assert b'font hot query allocations: 0' in r.stdout
   assert (b'font queued GPU rendering passed:' in r.stdout)==a.graphics
  c=cfile.read_text(encoding='utf8')
  expected=('xgeFontFaceLoad','xgeFontFamilyResolveEx','xgeFontCreateOwned','xgeFontGetMetrics') if file==example else ('xgeFontFaceLoadMemory','xgeFontGlyphGetByIndex','xgeFontLoadXRFMemoryOwned','xgeFontLoadCachedOwned')
  for symbol in expected:assert re.search(r'\b'+symbol+r'\(',c),symbol
  if os.name=='nt':
   r=run(name+'-imports',[objdump,'-p',binary]);imports=re.findall(rb'DLL Name:\s*(\S+)',r.stdout)
   assert imports and not {b'xge3_host.dll',b'xge.dll',b'xl.dll',b'xlc.dll',b'libtcc.dll'}.intersection(v.lower() for v in imports)
   report.setdefault('binaries',[]).append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode('ascii') for v in imports]))
 report.update(passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
 assert report['inputsUnchanged'];save()

if __name__=='__main__':main()
