"""NetFile borrowed completion boundary and ordinary EOF/retirement acceptance.
Four actual products: pinned XRT/xruntime and independent single/modular XRT.
Only self-created regular files; no application sockets, network data, security
tests, helper-thread fallback or full language NetFile/shipping claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
 p.add_argument('--xrt-root',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
 p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true')
 a=p.parse_args();root=a.root.resolve();native=a.xrt_root.resolve();out=a.out.resolve();assert not out.exists()
 compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler];gcc=next(c for c in compilers if c.stem.lower()!='tcc')
 sys.path.insert(0,str(native/'tools'));import build,amalgamate
 closure=build._load_modules('memory_debug,net_file',[],platform='windows' if os.name=='nt' else 'linux')
 sources,defines=closure[0],closure[4];links=['-l'+n for n in closure[5]]
 links+=['-ladvapi32','-liphlpapi'] if os.name=='nt' else ['-pthread','-ldl','-lm']
 files={Path(__file__).resolve(),*compilers,root/'test/support/net_file_boundary_native.c',root/'test/support/net_file_win32_eof_contract.c',native/'tests/network/net_file_boundary_cases.h'}
 for folder in (root/'lib/xrt',root/'lib/xruntime',native/'src',native/'include',native/'config',native/'tools'):
  files.update(f for f in folder.rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc','.py','.json'))
 sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
 inputs={str(f):sha(f) for f in sorted(files)};out.mkdir(parents=True);rows=[];products=[];failures=[];coverage=[]
 report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,ProductsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
  Coverage=coverage,NativeSources=sources,Scope=__doc__,ClockContractsPreserved=True,ApplicationNetworkTraffic=False,NetworkSecurityTests=False,
  LanguageNetFileComplete=False,ShippingLibraryAcceptance=False)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
 def run(name,args,proof=None,reject=False,instrumented=False):
  log=out/(name+'.log');env=dict(os.environ);cwd=out/(name+'.cwd');cwd.mkdir();started=time.monotonic()
  if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
  with log.open('wb') as f:r=subprocess.run(list(map(str,args)),cwd=cwd,stdout=f,stderr=subprocess.STDOUT,timeout=180,env=env)
  rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
  print(name+': '+str(r.returncode),flush=True);assert (r.returncode!=0 if reject else r.returncode==0),(name,str(log))
  text=log.read_text(encoding='utf8',errors='replace')
  if proof:assert proof in text,(name,'missing proof')
  if 'NetFile boundary:' in text:
   coverage.append(dict(Name=name,NativeFileIo='actual-I/O=4' in text,UnsupportedNativeBackend='backend unavailable:' in text));save()
  return text
 def flags(cc):return [cc,'-std=c11','-Wall','-Werror',*(['-Wextra'] if cc.stem.lower()!='tcc' else []),*(['-D_GNU_SOURCE'] if os.name!='nt' else []),'-I'+str(root),'-I'+str(native/'tests/network'),'-I'+str(out)]
 fixture=root/'test/support/net_file_boundary_native.c';save()
 try:
  frozen=out/'net-file-native.h';frozen.write_text(amalgamate._content([])[1],encoding='utf8');product(frozen)
  if os.name=='nt':
   for i,cc in enumerate(compilers):
    binary=out/(str(i)+'.win32.exe');run(str(i)+'.win32.build',flags(cc)+['-O0',root/'test/support/net_file_win32_eof_contract.c','-o',binary]);product(binary)
    run(str(i)+'.win32.run',[binary],'Win32 EOF:')
  for lane in ('pinned-core','pinned-runtime','native-single','native-modular'):
   folder=out/lane;folder.mkdir();impl=folder/'impl.c'
   selected=lane.startswith('pinned');include='lib/xruntime/xruntime.h' if lane.endswith('runtime') else 'lib/xrt/xrt.h'
   macro='XRUNTIME_IMPLEMENTATION' if lane.endswith('runtime') else 'XRT_IMPLEMENTATION'
   if not selected:include='net-file-native.h'
   impl.write_text('#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_NET_FILE\n#define '+macro+'\n#include "'+include+'"\n',encoding='utf8');product(impl)
   production=[native/s for s in sources] if lane=='native-modular' else [impl]
   extra=(['-DXL6_NET_FILE_NATIVE_HEADER','-I'+str(native/'include')] if not selected else [])
   if lane=='native-modular':extra+=['-D'+d for d in defines]
   for i,cc in enumerate(compilers):
    for opt in ('O0','O2'):
     if cc.stem.lower()=='tcc' and opt=='O2':continue
     name=f'{lane}.{i}.{opt}';binary=folder/(str(i)+'.'+opt+('.exe' if os.name=='nt' else ''))
     run(name+'.build',flags(cc)+['-'+opt,*extra,fixture,*production,*links,'-o',binary]);product(binary)
     run(name+'.run',[binary],'NetFile boundary:')
   if a.sanitize:
    assert os.name!='nt';binary=folder/'asan';name=lane+'.asan'
    run(name+'.build',flags(gcc)+['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie',*extra,fixture,*production,*links,'-o',binary]);product(binary)
    run(name+'.run',[binary],'NetFile boundary:',instrumented=True)
  original=(root/'lib/xrt/xrt.h').read_text(encoding='utf-8-sig')
  publish='if ( bSubmitted ) __xrtNetFileExposeCompletion(pWorker);'
  assert original.count(publish)==2
  # Keep an unused-function warning from substituting for runtime refusal.
  lost=original.replace(publish,'/* lost caller-owned completion boundary */').replace(
   'static void __xrtNetFileExposeCompletion(', 'void __xrtNetFileExposeCompletion(')
  mutants=[('missing-opaque',lost)]
  if os.name=='nt':
   begin=original.index('\t/* Regular-file EOF is a successful zero-byte read')
   end=original.index('\tmemset(pEvent, 0, sizeof(*pEvent));',begin)
   mutants.append(('missing-eof',original[:begin]+original[end:]))
   mutants.append(('reject-marks-opaque',original.replace(publish,'__xrtNetFileExposeCompletion(pWorker);')))
  elif not any(c['NativeFileIo'] for c in coverage):mutants=[]
  for name,text in mutants:
   assert text!=original;folder=out/name;(folder/'lib/xrt').mkdir(parents=True)
   header=folder/'lib/xrt/xrt.h';header.write_text(text,encoding='utf8');product(header)
   impl=folder/'impl.c';impl.write_text('#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_NET_FILE\n#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n',encoding='utf8');product(impl)
   binary=folder/('mutant.exe' if os.name=='nt' else 'mutant')
   run(name+'.build',[*flags(gcc)[:1],'-I'+str(folder),*flags(gcc)[1:],'-O0',fixture,impl,*links,'-o',binary]);product(binary)
   run(name+'.refused',[binary],reject=True)
  if any(c['NativeFileIo'] for c in coverage):
   # Actual accepted OS I/O, instrumented only between backend return and the
   # production marker. Independent freeze must succeed while Engine is BUSY.
   folder=out/'gap-probe';(folder/'lib/xrt').mkdir(parents=True)
   instrumented=original.replace(publish,'if ( bSubmitted ) { nfBoundaryGapProbe(); __xrtNetFileExposeCompletion(pWorker); }')
   anchor='static void __xrtNetFileExposeCompletion(xnetworker* pWorker)'
   instrumented=instrumented.replace(anchor,'extern void nfBoundaryGapProbe(void);\n'+anchor,1)
   header=folder/'lib/xrt/xrt.h';header.write_text(instrumented,encoding='utf8');product(header)
   impl=folder/'impl.c';impl.write_text('#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_NET_FILE\n#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n',encoding='utf8');product(impl)
   binary=folder/('probe.exe' if os.name=='nt' else 'probe')
   run('gap-probe.build',[*flags(gcc)[:1],'-I'+str(folder),*flags(gcc)[1:],'-DNET_FILE_GAP_PROBE','-O0',fixture,impl,*links,'-o',binary]);product(binary)
   run('gap-probe.run',[binary],'NetFile gap: 4 independent freezes admitted')
  report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items());report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
  report['ProductsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in products)
  assert report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged'];report['Complete']=True;save()
 except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
