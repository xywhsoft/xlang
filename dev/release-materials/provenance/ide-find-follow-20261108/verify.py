import json, os, pathlib, shutil, subprocess, sys, uuid
root=pathlib.Path('D:/GIT/x-lang')
run=root/'ide/build/Release/find-follow-20261108'
baseline=os.environ.get('DIALOG_BASELINE')=='1'
meta=json.loads((run/('baseline-meta.json' if baseline else 'objects/gui-candidate-build-inputs.json')).read_text(encoding='utf-8-sig'))
runtime=root/'release/win_x64'
env=os.environ.copy();env['PATH']=str(runtime)+';E:/software/w64devkit/bin;'+env['PATH']
results=[]
for name in sys.argv[1:]:
    source=pathlib.Path('F:/xlang-validation/ide-find-follow-20261108/source/ide/tests')/f'{name}.c'
    exe=run/f'{name}.exe'
    excluded=['main.c.o']
    if name=='settings_menu_frame_test':excluded+=['src_ui_language_view.c.o']
    if name in ['actions_test','help_viewport_test']:excluded+=['src_app_xide_app.c.o']
    if name=='migration_checkpoint_test':excluded+=['src_ui_rename_view.c.o']
    if name=='resource_rebind_test':excluded+=['src_services_resource_rebind.c.o']
    args=[meta['compiler'],*meta['flags'],str(source),*[p for p in meta['objects'] if not any(p.endswith(q) for q in excluded)],*meta['libraries'],'-o',str(exe)]
    compiled=subprocess.run(args,capture_output=True,env=env)
    (run/f'{name}-build.log').write_bytes(compiled.stdout+compiled.stderr)
    if compiled.returncode: print(name,'BUILD FAILED',compiled.stderr.decode(errors='replace'),flush=True);raise RuntimeError(name+' compile failed')
    temp=runtime/f'xide-verify-{uuid.uuid4().hex}.exe'
    output=run/(('baseline-' if baseline else '')+name+'-'+uuid.uuid4().hex[:8]);output.mkdir(exist_ok=True)
    try:
        shutil.copy2(exe,temp)
        extra=['0.6.0'] if name=='about_view_test' else ['manifest','apply'] if name=='migration_checkpoint_test' else [str(runtime/'xl.exe')] if name=='settings_menu_frame_test' else [str(runtime/'data/commands.json')] if name in ['catalog_test','help_document_test'] else []
        result=subprocess.run([str(temp),str(output),*extra],cwd=runtime,env=env,capture_output=True,timeout=110)
        (run/f'{"baseline-" if baseline else ""}{name}.log').write_bytes(result.stdout+result.stderr)
        print(name,result.returncode,(result.stdout+result.stderr).decode('utf-8',errors='replace')[-3500:],flush=True)
        results.append({'test':name,'exit':result.returncode,'output':str(output),'log':str(run/f'{name}.log')})
    finally:temp.unlink(missing_ok=True)
(run/'last-results.json').write_text(json.dumps(results,indent=2))
