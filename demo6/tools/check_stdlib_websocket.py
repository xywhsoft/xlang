"""Ordinary WebSocket vectors, owned local loopback, products and trimming.

Uses real CLI/DLL/SO products and their embedded VFS. --driver is explicitly
development-only. No external traffic, probing or security-specialist tests.
"""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,os,re,shutil,subprocess,sys,threading

CASES=['stdlib_websocket_values_v2','stdlib_websocket_extensions_v2',
 'stdlib_websocket_codecs_v2','stdlib_websocket_upgrade_v2',
 'stdlib_websocket_stream_v2','stdlib_websocket_handoff_v2','stdlib_websocket_tls_v2','stdlib_websocket_send_v2',
 'stdlib_websocket_cycle_v2','stdlib_websocket_callback_error_v2','stdlib_websocket_wait_oom_v2','stdlib_websocket_language_oom',
 'stdlib_websocket_core','stdlib_websocket_upgrade_loopback','stdlib_websocket_upgrade_tls_loopback',
 *['stdlib_websocket_'+n+'_trim' for n in ['frame','close','handshake','keygen','upgrade','stream_config','stream','stream_tls']]]
BAD=['bad_stdlib_websocket_native_handle','bad_stdlib_websocket_raw_callback',
 'bad_stdlib_websocket_no_raise','bad_stdlib_websocket_enum_result','bad_stdlib_websocket_legacy']

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['root','out']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--driver',type=Path);p.add_argument('--case',action='append',choices=CASES);p.add_argument('--jobs',type=int,default=2)
 a=p.parse_args();assert 1<=a.jobs<=2;root=a.root.resolve();out=a.out.resolve();selected=a.case or CASES
 assert not out.exists();out.mkdir(parents=True);rows=[];lock=threading.Lock();windows=os.name=='nt';driver=a.driver.resolve() if a.driver else None
 report=dict(Complete=False,SourceDriverOnly=bool(driver),ShippingAcceptance=False,ExternalTraffic=False,NetworkSecurityTests=False,Rows=rows)
 inputs={Path(__file__).resolve(),root/'tools/gen_stdlib_websocket.py',*[root/'test/cases'/(n+'.xl') for n in selected+BAD]}
 inputs.update(q for folder in ['src/runtime','src/stdlib','src/ir','src/compile'] for q in (root/folder).glob('*ws*') if q.is_file())
 inputs.update(root/'test/cases'/n for n in ['stdlib_websocket_native_host.c','stdlib_codec_template_language_host.c','stdlib_codec_template_language_oom_host.c'])
 inputs.update(root/'test/support'/n for n in ['module_product_memory.c','module_product_memory.h'])
 for n in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xruntime_decl.h','src/cgen/xl6_ir_cgen.c','src/cgen/xl6_ir_cgen_module_emit.c']:inputs.add(root/n)
 env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
 def run(name,args,cwd=root,negative=False):
  log=out/(name+'.log');args=list(map(str,args))
  try:r=subprocess.run(args,cwd=cwd,env=env,capture_output=True,timeout=240);code=r.returncode;data=r.stdout+r.stderr
  except subprocess.TimeoutExpired as e:code='timeout';data=(e.stdout or b'')+(e.stderr or b'')
  log.write_bytes(data)
  with lock:rows.append(dict(Name=name,Command=args,Exit=code,Negative=negative,Log=str(log),Sha256=sha(log)))
  assert isinstance(code,int) and (code!=0 if negative else code==0),(name,data[-4000:].decode(errors='replace'))
  return data
 try:
  run('metadata',[sys.executable,root/'tools/gen_stdlib_websocket.py','--check'])
  run('module-roles',[sys.executable,root/'tools/websocket_module_coverage.py','--root',root,'--check'])
  inputs.update([root/'tools/websocket_module_coverage.py',root/'docs/websocket-module-coverage.json'])
  expected={q.name for q in (root/'src/runtime').glob('xl6_ws_*.c')}
  for n in ['compiler_dll.rsp.in','runtime_dll.rsp.in']:
   template=root/'tools'/n;inputs.add(template)
   sources=re.findall(r'src[/\\]runtime[/\\](xl6_ws_\w+\.c)\b',template.read_text(encoding='utf-8-sig'))
   assert len(sources)==len(set(sources)) and set(sources)==expected,(n,'missing or duplicate WebSocket source')
  retired=['xl6_cgen_websocket.c','xl6_cgen_websocket.h','xl6_cgen_websocket_protocol.c','xl6_cgen_websocket_stream.c','xl6_cgen_http1.c','xl6_cgen_http1.h']
  assert not any((root/'src/cgen'/n).exists() for n in retired)
  isolated=out/'isolated';isolated.mkdir();release=isolated/'release';release.mkdir();relocated=out/'relocated';relocated.mkdir()
  if driver:inputs.add(driver)
  else:
   names=['xl.exe','xl.dll','xlc.dll'] if windows else ['xl','libxl.so','libxlc.so']
   for n in names:
    q=root/'release'/n;inputs.add(q);shutil.copy2(q,release/n)
    if windows and q.suffix=='.dll':shutil.copy2(q,isolated/n)
   cli=release/names[0];library=release/names[2];cc=shutil.which('gcc');assert cc
   host=isolated/('host.exe' if windows else 'host');observer=isolated/('observer.exe' if windows else 'observer')
   links=[library] if windows else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
   for binary,n in [(host,'stdlib_product_fixture_host.c'),(observer,'stdlib_pruning_product_host.c')]:
    source=root/'test/cases'/n;inputs.add(source)
    run(binary.stem+'.build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2','-DXL_STATIC_EMBED','-I'+str(root/'release/include'),source,*links,'-o',binary])
   native=root/'test/cases/stdlib_websocket_native_host.c'
   flags=['-std=c11','-Wall','-Wextra','-Werror','-O2','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
   for n in names[1:]:
    binary=isolated/(n+'.ws-native'+('.exe' if windows else ''))
    nativeLinks=[release/n] if windows else ['-L'+str(release),'-l:'+n,'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
    run(n+'.native.build',[cc,*flags,native,*nativeLinks,'-o',binary]);run(n+'.native.run',[binary])
  report['Inputs']={str(q):sha(q) for q in sorted(inputs)}
  def positive(n):
   q=root/'test/cases'/(n+'.xl');saved=out/(n+'.saved');saved.mkdir()
   if driver:
    run(n+'.run',[driver,q,'run',saved/'jit'])
    run(n+'.cgen',[driver,q,'cgen-default',saved/'generated.c'])
   else:
    run(n+'.run',[cli,'run',q],isolated)
    run(n+'.module',[host,library,q,'module',saved/'module-tmp'],isolated)
    run(n+'.cgen',[cli,'cgen',q,'--emit',saved/'generated.c'],isolated)
    image=saved/('program.exe' if windows else 'program')
    run(n+'.aot-build',[observer,library,q,saved,image],isolated)
    independent=relocated/(n+('.exe' if windows else ''));shutil.copy2(image,independent)
    inspect=shutil.which('objdump' if windows else 'readelf');assert inspect
    data=run(n+'.imports',[inspect,'-p' if windows else '-d',independent],isolated)
    deps=re.findall(rb'DLL Name:\s*([^\s]+)',data) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]',data)
    assert deps and not {d.decode().lower() for d in deps}&{name.lower() for name in names}
    if not windows:assert b'(RPATH)' not in data and b'(RUNPATH)' not in data
    run(n+'.relocated',[independent],relocated)
   text=(saved/'generated.c').read_text(encoding='utf-8-sig')
   assert 'xl6_xrt_ws_' not in text and 'xl6_xrt_http1_' not in text
   print(n,'passed',flush=True)
  failures=[]
  with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
   for result in concurrent.futures.as_completed([pool.submit(positive,n) for n in selected]):
    try:result.result()
    except Exception as error:failures.append(repr(error))
  for n in BAD:
   try:
    q=root/'test/cases'/(n+'.xl');text=run(n,[driver,q,'cgen-default',out/(n+'.c')] if driver else [cli,'cgen',q,'--emit',out/(n+'.c')],negative=True)
    assert b'exact declared ABI' in text if n.endswith(('no_raise','enum_result')) else b'[sem]' in text,(n,text[-1000:])
   except Exception as error:failures.append(repr(error))
  if not driver:
   sys.path.insert(0,str(root/'tools'))
   from gen_tcc_vfs_resources import load_resources
   sdk=out/'sdk';sdk.mkdir();bd=root/('build/tmp' if windows else 'build/linux')
   dist=root/'res/tcc/win64/x86_64' if windows else bd/'tcc-dist'
   manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
   headerInputs=[]
   for resource in load_resources(manifest,root,dist,bd/'xrt.o',bd):
    if resource.virtual.startswith('include/__xl_runtime/'):
     dest=sdk/Path(resource.virtual).relative_to('include');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(resource.source,dest)
     headerInputs.append(dict(Path=str(resource.source),Sha256=sha(resource.source)))
   report['HeaderInputs']=headerInputs
   strict=[*flags,'-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label','-Wno-unused-const-variable',
    '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I'+str(sdk)]
   for n in [n for n in selected if n.endswith(('_v2','language_oom'))]:
    for opt in ['O0','O2']:
     try:
      module=out/(n+'.saved')/'module.c';assert module.is_file(),str(module)
      source=root/'test/cases'/('stdlib_codec_template_language_oom_host.c' if n.endswith('language_oom') else 'stdlib_codec_template_language_host.c')
      binary=isolated/(n+'.'+opt+('.exe' if windows else ''))
      run(n+'.'+opt+'.build',[cc,*strict,'-'+opt,'-DXLIR_GENERATED_MODULE="'+module.as_posix()+'"',source,root/'test/support/module_product_memory.c',*links,'-o',binary])
      run(n+'.'+opt+'.lifetime',[binary])
     except Exception as error:failures.append(repr(error))
   report['NativeBenchmarkCompiler']='GCC -O2, matching Linux resident providers'
   if windows:
    # The ordinary Windows DLL build uses GCC -Os. TCC remains the script
    # compiler/bootstrap; comparing a TCC caller against a GCC provider would
    # not establish the cost of equivalent handwritten C.
    build=(root/'build_dll.bat').read_text(encoding='utf-8-sig')
    assert 'XL6_RELEASE_CC=gcc' in build and '-Os -ffunction-sections -fdata-sections' in build
    benchmark=isolated/'ws-gcc-os-benchmark.exe'
    run('same-compiler.benchmark.build',[cc,*[f for f in flags if f!='-O2'],'-Os',native,*links,'-o',benchmark])
    run('same-compiler.benchmark',[benchmark,'--benchmark-only'])
    report['NativeBenchmarkCompiler']='GCC -Os, matching ordinary Windows resident providers'
    benchmark=isolated/'ws-tcc-interop.exe';tcc=bd/'tcc_bootstrap.exe';assert tcc.is_file()
    tccFlags=['-std=c11','-Wall','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),
     '-I'+str(root/'lib/tcc/include'),'-I'+str(dist/'include'),'-I'+str(dist/'include/winapi'),'-L'+str(dist/'lib')]
    run('tcc-interoperability.build',[tcc,*tccFlags,native,root/'release/xlc.def','-o',benchmark])
    run('tcc-interoperability.run',[benchmark,'--benchmark-only'])
   else:run('same-compiler.benchmark',[isolated/(names[2]+'.ws-native'),'--benchmark-only'])
   minimal={'frame':('WEBSOCKET_FRAME',{'WEBSOCKET_CLOSE','WEBSOCKET_HANDSHAKE','WEBSOCKET_KEYGEN'}),
    'close':('WEBSOCKET_CLOSE',{'WEBSOCKET_FRAME','WEBSOCKET_HANDSHAKE','WEBSOCKET_KEYGEN'}),
    'handshake':('WEBSOCKET_HANDSHAKE',{'WEBSOCKET_FRAME','WEBSOCKET_CLOSE','WEBSOCKET_KEYGEN','RANDOM_SECURE'}),
    'keygen':('WEBSOCKET_KEYGEN',{'WEBSOCKET_FRAME','WEBSOCKET_CLOSE','WEBSOCKET_HANDSHAKE'})}
   for kind,(required,forbidden) in minimal.items():
    n='stdlib_websocket_'+kind+'_trim'
    if n not in selected:continue
    macros=run(n+'.features',[cc,'-E','-dM','-I'+str(root/'lib/xruntime'),out/(n+'.saved')/'xl6_xrt_impl.c'])
    features={m.decode() for m in re.findall(rb'^#define XRT_FEATURE_([A-Z0-9_]+)\b',macros,re.M)}
    assert required in features and not features&(forbidden|{'NET_ENGINE','TLS','INFLATE','DEFLATE'}),(kind,features)
   assert all(sha(r['Path'])==r['Sha256'] for r in headerInputs)
  assert not failures,failures
  report['InputsUnchanged']=all(sha(q)==h for q,h in report['Inputs'].items());report['Complete']=report['InputsUnchanged']
  report['ShippingAcceptance']=report['Complete'] and not driver
 except Exception as error:report['Error']=repr(error)
 (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:report[k] for k in ['Complete','SourceDriverOnly','ShippingAcceptance']},ensure_ascii=False),flush=True)
 if 'Error' in report:print(report['Error'],flush=True)
 return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
