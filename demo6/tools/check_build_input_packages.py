"""Actual recursive package/C inputs and environment guard for build receipts."""
from pathlib import Path
import argparse,ctypes,hashlib,json,os,subprocess,uuid,shutil
p=argparse.ArgumentParser();p.add_argument('--compiler-root',required=True);p.add_argument('--work-root',required=True);a=p.parse_args()
c=Path(a.compiler_root).resolve();w=Path(a.work_root).resolve()/('build-input-packages-'+str(uuid.uuid4()));w.mkdir(parents=True);shutil.copy2(__file__,w/'check_build_input_packages.py')
src=w/'project';src.mkdir();packages=w/'packages';packages.mkdir();checks=[]
def check(label,value):
 checks.append({'label':label,'passed':bool(value)})
 if not value: raise AssertionError(label)
for name in ['pkg','pkg2']:
 root=packages/name;(root/'src').mkdir(parents=True)
 manifest={'format':'xlang-package-1','name':name,'version':'1.0.0','abiVersion':'1','kind':'source','artifact':{'kind':'source','xlSources':['src/api.xl']},'dependencies':[],'links':[],'loadlibs':[]}
 if name=='pkg':
  manifest['dependencies']=[{'name':'pkg2','version':'1.0.0','abiVersion':'1'}];manifest['artifact']['cSources']=['src/native.c']
  (root/'src/native.c').write_text('#include "native.h"\nint package_native(void) {return NATIVE_VALUE;}\n',encoding='utf8');(root/'src/native.h').write_text('#define NATIVE_VALUE 3\n',encoding='utf8')
  body='import pkg2\nexport func value() as int\n return pkg2.value()\nend\n'
 else: body='export func value() as int\n return 11\nend\n'
 (root/'src/api.xl').write_text(body,encoding='utf8');(root/'xlang.package.json').write_text(json.dumps(manifest),encoding='utf8')
(src/'main.xl').write_text('import pkg\nfunc main(...) as int\n return pkg.value()\nend\n',encoding='utf8')
env=os.environ.copy();env['XLANG_PACKAGE_PATH']=str(packages)
try:
 result=subprocess.run([str(c/'release/xl.exe'),'build','main.xl','-o','package.exe','--input-report','package.json'],cwd=src,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
 (w/'build.log').write_bytes(result.stdout);check('recursive package build',result.returncode==0)
 receipt=json.loads((src/'package.json').read_text());paths={Path(f['path']).resolve() for f in receipt['files']}
 check('both manifests observed',all(packages/n/'xlang.package.json' in paths for n in ['pkg','pkg2']))
 check('both package sources observed',all(packages/n/'src/api.xl' in paths for n in ['pkg','pkg2']))
 check('package C implementation and header observed',{packages/'pkg/src/native.c',packages/'pkg/src/native.h'}<=paths)
 check('package environment observed',receipt['packagePathObserved'] and receipt['XLANG_PACKAGE_PATH']==str(packages))
 check('actual package output',subprocess.run([str(src/'package.exe')],cwd=src,timeout=15).returncode==11)
 for row in [*receipt['files'],receipt['artifact']]: check('input digest '+Path(row['path']).name,hashlib.sha256(Path(row['path']).read_bytes()).hexdigest()==row['sha256'])
 dll=ctypes.CDLL(str(c/'release/xlc.dll'))
 for name,types,result in [('xlcCreateContext',[ctypes.c_void_p],ctypes.c_void_p),('xlcBeginInputReport',[ctypes.c_void_p],ctypes.c_int),('xlcAddSourceFile',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),('xlcEmitExe',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),('xlcWriteInputReport',[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_char_p],ctypes.c_int),('xlcDestroyContext',[ctypes.c_void_p],None)]:
  f=getattr(dll,name);f.argtypes=types;f.restype=result
 old=os.environ.get('XLANG_PACKAGE_PATH');os.environ['XLANG_PACKAGE_PATH']=str(packages);ctx=None
 try:
  ctx=dll.xlcCreateContext(None);check('API context',bool(ctx));check('API report begin',dll.xlcBeginInputReport(ctx)==1)
  check('API source',dll.xlcAddSourceFile(ctx,str(src/'main.xl').encode())==1)
  artifact=src/'environment.exe';record=src/'environment.json'
  check('API actual package emit',dll.xlcEmitExe(ctx,str(artifact).encode())==1)
  os.environ['XLANG_PACKAGE_PATH']=str(w/'different-packages')
  check('environment change rejects receipt',dll.xlcWriteInputReport(ctx,str(artifact).encode(),str(record).encode())==0)
  check('no success record',not record.exists())
 finally:
  if ctx: dll.xlcDestroyContext(ctx)
  if old is None: os.environ.pop('XLANG_PACKAGE_PATH',None)
  else: os.environ['XLANG_PACKAGE_PATH']=old
except Exception as e: error=str(e)
else: error=None
(w/'evidence.json').write_text(json.dumps({'status':'passed' if error is None else 'failed','error':error,'checks':checks,'compilerDllSha256':hashlib.sha256((c/'release/xlc.dll').read_bytes()).hexdigest()},ensure_ascii=False,indent=2),encoding='utf8');print(w,'passed' if error is None else 'failed',error or '');raise SystemExit(0 if error is None else 1)
