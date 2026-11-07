exec(open('ide/build/Release/dialog-layout-20261107/verify.py',encoding='utf-8').read().split('results=[]')[0])
frozen=pathlib.Path('F:/xlang-validation/ide-dialog-layout-20261107/source/ide')
helpers=['task_test.exe','xide_task_program.exe','xide_toolchain_program.exe']
resource=run/'toolchain_program.res.o'
subprocess.run([str(pathlib.Path(meta['compiler']).parent/'windres.exe'),str(frozen/'tests/toolchain_program.rc'),'-O','coff','-o',str(resource)],env=env,check=True)
for src,dest in [('task_program.c',helpers[1]),('toolchain_program.c',helpers[2])]:
 args=[meta['compiler'],'-std=c11','-Wall','-Wextra','-Werror','-O2',str(frozen/'tests'/src)]
 if 'toolchain' in src:args.append(str(resource))
 subprocess.run([*args,'-lshell32','-o',str(run/dest)],env=env,check=True)
owned=[]
try:
 for name in helpers:
  p=runtime/name
  if p.exists():raise RuntimeError('Existing helper preserved: '+str(p))
  shutil.copy2(run/name,p);owned.append(p)
 subprocess.run([sys.executable,'-X','utf8',str(run/'verify.py'),*sys.argv[1:]],env=env,check=True)
finally:
 for p in owned:p.unlink(missing_ok=True)
