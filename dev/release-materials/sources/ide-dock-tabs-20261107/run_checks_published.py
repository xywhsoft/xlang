from pathlib import Path
import concurrent.futures,hashlib,json,os,subprocess,sys
r=Path.cwd();b=r/'ide/build/Release/dock-tabs-20261107';runtime=r/'release/win_x64';record=json.loads((b/'build.json').read_text());cc=record['compiler'];flags=record['flags'];objects=[v['object']for v in record['objects'].values()];libraries=record['libraries'];snapshot=b/'source/ide';out=b/(sys.argv[1] if len(sys.argv)>1 else 'checks-r1');out.mkdir(exist_ok=False)
names=sys.argv[2:] or ['settings_test','settings_view_test','input_test','font_test','ui_font_test','actions_test']
rows=[];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={name:sha(runtime/name)for name in ['xlang_ide.exe','xge.dll','xlang-ls.exe','xl.exe','xl.dll','xlc.dll','data/commands.json','data/catalog-sources/index.json','option/settings.json']}
def build(name):
 source=snapshot/'tests'/(name+'.c');raw=(r/'ide/tests'/(name+'.c')).read_bytes();raw=raw.replace(b'"task_test.exe"',b'".settings-task-helper.exe"') if name=='actions_test' else raw;source.write_bytes(raw);exe=out/(name+'.exe');selected=[o for o in objects if (name!='actions_test' or not o.endswith('src_app_xide_app.c.o')) and (name!='settings_menu_frame_test' or not o.endswith('src_ui_language_view.c.o'))];command=[cc,*flags,str(source),*selected,*libraries,'-o',str(exe)];v=subprocess.run(command,capture_output=True,timeout=90);(out/(name+'-build.log')).write_bytes(v.stdout+v.stderr);return dict(name=name,sourceSha256=sha(source),command=command,buildExit=v.returncode,executable=str(exe))
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:rows=list(pool.map(build,names))
if 'actions_test' in names:
 helper=out/'task-helper.exe';helperSource=out/'task-helper.c';helperSource.write_bytes((r/'ide/tests/task_test.c').read_bytes().replace(b'"xide_task_program.exe"',b'".settings-task-program.exe"'));subprocess.run([cc,*flags,str(helperSource),*objects,*libraries,'-o',str(helper)],check=True);subprocess.run([cc,'-std=c11','-O2',str(r/'ide/tests/task_program.c'),'-lshell32','-o',str(out/'task-program.exe')],check=True)
environment=dict(os.environ,PATH=str(runtime)+os.pathsep+os.environ.get('PATH',''))
for row in rows:
 if row['buildExit']:print('BUILD FAIL',row['name'],(out/(row['name']+'-build.log')).read_text(encoding='utf-8',errors='replace')[-2600:],flush=True);continue
 executable=Path(row['executable']);installed=runtime/('.settings-'+row['name']+'.exe');installed.write_bytes(executable.read_bytes());work=out/row['name'];work.mkdir();command=[str(installed),str(work)]+([str(runtime/'xl.exe')] if row['name']=='settings_menu_frame_test' else [])
 try:
  if row['name']=='actions_test':
   helperInstalled=runtime/'.settings-task-helper.exe';assert not helperInstalled.exists();helperInstalled.write_bytes((out/'task-helper.exe').read_bytes());programInstalled=runtime/'.settings-task-program.exe';assert not programInstalled.exists();programInstalled.write_bytes((out/'task-program.exe').read_bytes())
  v=subprocess.run(command,cwd=runtime,env=environment,capture_output=True,timeout=120);(out/(row['name']+'.log')).write_bytes(v.stdout+v.stderr);row.update(runCommand=command,runExit=v.returncode);print(row['name'],v.returncode,(v.stdout+v.stderr).decode('utf-8',errors='replace')[-3000:],flush=True)
 except subprocess.TimeoutExpired as e:
  row.update(runCommand=command,runExit=124);(out/(row['name']+'.log')).write_bytes((e.stdout or b'')+(e.stderr or b'')+b'\nTIMEOUT');print('TIMEOUT',row['name'],flush=True)
 finally:
  installed.unlink()
  if row['name']=='actions_test':
   for name in ['.settings-task-helper.exe','.settings-task-program.exe']:
    if (runtime/name).exists():(runtime/name).unlink()
 for name,digest in protected.items():assert sha(runtime/name)==digest,name
 report=dict(passed=all(row.get('runExit',1)==0 and row['buildExit']==0 for row in rows),rows=rows,protectedRuntime=protected)
 (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('Overall',report['passed'])
