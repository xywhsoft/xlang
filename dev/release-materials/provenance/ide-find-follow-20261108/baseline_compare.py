import json,pathlib,subprocess,shutil,uuid
root=pathlib.Path('D:/GIT/x-lang');run=root/'ide/build/Release/find-follow-20261108';m=json.loads((root/'ide/build/Release/help-follow-20261108/objects/gui-candidate-build-inputs.json').read_text());runtime=root/'release/win_x64';rows=[]
for name in ['find_options_view_test','find_boundary_view_test','find_escape_test','find_boundaries_test']:
 src=pathlib.Path('F:/xlang-validation/ide-help-follow-20261108/source/ide/tests')/(name+'.c');exe=run/('baseline-'+name+'.exe')
 subprocess.run([m['compiler'],*m['flags'],str(src),*[p for p in m['objects']],*m['libraries'],'-o',str(exe)],check=True,capture_output=True)
 tmp=runtime/('xide-verify-'+uuid.uuid4().hex+'.exe');out=run/('baseline-'+name);out.mkdir(exist_ok=True)
 try:
  shutil.copy2(exe,tmp);p=subprocess.run([str(tmp),str(out)],cwd=runtime,capture_output=True,timeout=90);log=run/('baseline-'+name+'.log');log.write_bytes(p.stdout+p.stderr);rows.append({'test':name,'exit':p.returncode,'log':str(log)});print(name,p.returncode,(p.stdout+p.stderr).decode('utf-8',errors='replace')[-300:],flush=True)
 finally:tmp.unlink(missing_ok=True)
(run/'baseline-comparisons.json').write_text(json.dumps(rows,indent=2))
