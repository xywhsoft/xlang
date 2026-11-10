"""Qualify declarative owned FontLoad with a complete compiler and matched SDKs.

Valid generated font; CPU/local IO only. JIT, dynamic/static AOT, OOM prefixes,
direct generated calls and native/language retained costs are checked together.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from full_compiler_qualification import verify_full_compiler

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','compiler','compiler-report','dynamic-packages','static-packages','out','objdump'):
  p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();root,compiler,cohort,dynamic,static,out,objdump=[v.resolve() for v in
  (a.root,a.compiler,a.compiler_report,a.dynamic_packages,a.static_packages,a.out,a.objdump)]
 sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
 metadata=verify_full_compiler(cohort,compiler);sdk=compiler.parent/'include'
 cases=[root/'test/cases/foreign_xge3_font_load.xl',root/'test/cases/foreign_xge3_font_load_oom.xl',root/'lib/bindings/xge/examples/font_loading.xl']
 font=root/'lib/bindings/xge/examples/data/font_fixture.ttf'
 files=[Path(__file__).resolve(),Path(__file__).with_name('full_compiler_qualification.py'),Path(metadata['sourceFreeze']),compiler,cohort,objdump,*cases,font,root/'test/cases/foreign_xge3_font_load.h']
 for folder in (sdk,dynamic,static):files.extend(v for v in folder.rglob('*') if v.is_file())
 for name in ('xl.dll','xlc.dll') if os.name=='nt' else ('libxl.so','libxlc.so'):files.append(compiler.parent/name)
 inputs={str(v):sha(v) for v in files};out.mkdir(parents=True,exist_ok=False)
 private=dynamic/'_xge_native';manifest=json.loads((private/'xlang.package.json').read_text(encoding='utf8'))
 image=private/manifest['links'][0];assert sha(image)==manifest['native']['sha256']
 report=dict(passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,graphicsDevice=False,networkTests=False,inputs=inputs,rows=[])
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,args,packages=None,working=None):
  args=list(map(str,args));env=dict(os.environ,XLANG_XGE_FONT_FIXTURE=str(font));env.pop('XLANG_PACKAGE_PATH',None)
  if packages:env['XLANG_PACKAGE_PATH']=str(packages)
  if os.name=='nt':env['PATH']=str(image.parent)+';'+str(compiler.parent)+';'+env.get('PATH','')
  else:env['LD_LIBRARY_PATH']=str(image.parent)+':'+str(compiler.parent)+':'+env.get('LD_LIBRARY_PATH','')
  working=working or compiler.parent
  r=subprocess.run(args,cwd=working,env=env,capture_output=True,timeout=180)
  log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
  report['rows'].append(dict(name=name,command=args,workingDirectory=str(working),exitCode=r.returncode,logSha256=sha(log)));save();print(name,r.returncode,flush=True)
  assert not r.returncode,log
  if name.startswith('foreign_xge3_font_load-') and (name.endswith('-jit') or name.endswith('-run')):
   matches=re.findall(rb'(Native|Language) empty FontLoad:\s*(\d+)\s+allocations\s*/\s*(\d+)\s+bytes',r.stdout)
   costs={kind.decode():dict(allocations=int(n),bytes=int(b)) for kind,n,b in matches}
   assert costs.get('Native')==dict(allocations=1,bytes=248),costs
   assert set(costs)=={'Native','Language'} and costs['Language']['allocations']>=costs['Native']['allocations'],costs
   report.setdefault('retainedCosts',{})[name]=costs;save()
   take=re.search(rb'Language FontLoad take:\s*(\d+)\s+allocations\s*/\s*(\d+)\s+bytes',r.stdout)
   assert take
   report.setdefault('transferCosts',{})[name]=dict(allocations=int(take[1]),bytes=int(take[2]));save()
  return r
 include=['--inc-path',root,'--inc-path',sdk]
 for case in cases:
  name=case.stem;working=case.parent if case==cases[-1] else compiler.parent
  cfile,hfile=out/(name+'.c'),out/(name+'.h')
  run(name+'-emit',[compiler,'cgen',case,'--emit',cfile,'--emit-header',hfile],dynamic,working)
  run(name+'-jit',[compiler,'run',case,*include],dynamic,working)
  for variant,packages in [('dynamic',dynamic),('static',static)]:
   binary=out/(name+'-'+variant+('.exe' if os.name=='nt' else ''))
   run(name+'-'+variant+'-build',[compiler,'build',case,'-o',binary,*include],packages,working)
   run(name+'-'+variant+'-run',[binary],working=working)
   r=run(name+'-'+variant+'-imports',[objdump,'-p',binary])
   dependencies=re.findall(rb'DLL Name:\s*(\S+)',r.stdout) if os.name=='nt' else re.findall(rb'NEEDED\s+(\S+)',r.stdout)
   if variant=='static':
    assert not {b'xge3_host.dll',b'xge.dll',b'xl.dll',b'xlc.dll',b'libtcc.dll',b'libxge3_host.so',b'libxl.so',b'libxlc.so'}.intersection(v.lower() for v in dependencies)
   report.setdefault('binaries',[]).append(dict(path=str(binary),variant=variant,sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode('ascii') for v in dependencies]))
  source=cfile.read_text(encoding='utf8')
  for symbol in ('xl6Xge3AsyncFontCreateOwned','xgeAsyncFontStartOwned','xgeAsyncFontTakeOwned'):
   assert re.search(r'\b'+symbol+r'\(',source),symbol
  assert not re.search(r'(?:int|void)\s+xgeAsyncFont(?:Start|Take)Owned\s*\([^;]*\)\s*\{',source)
 report.update(passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));assert report['inputsUnchanged'];save()

if __name__=='__main__':main()
