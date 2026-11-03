"""Private native storage checks using one consistently linked opaque Cell core.

Incremental mode verifies a prior canonical source build's object bytes,
replaces modified core units, and links all resident implementation calls locally.
It is an exploratory gate, never a replacement for final canonical compilation.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--base',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--sanitize',action='store_true')
    p.add_argument('--callbacks',action='store_true')
    p.add_argument('--userdata',action='store_true')
    p.add_argument('--values',action='store_true',help='test native view ownership in ordinary class/value storage')
    p.add_argument('--sequences',action='store_true',help='test owning typed native view slots, erasure and graph cycles')
    p.add_argument('--xui-build',type=Path,help='exercise actual hosted XUI event slots with the same resident core')
    p.add_argument('--xui-root',type=Path,default=Path('D:/GIT/xge'))
    p.add_argument('--sdk-root',type=Path,help='Pinned runtime SDK directory containing include/xrt_decl.h')
    a=p.parse_args();root,base,out=[v.resolve() for v in (a.root,a.base,a.out)]
    cc=shutil.which('gcc');assert cc
    build=json.loads((base/'build.json').read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged']
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    assert sum(map(bool,(a.callbacks,a.userdata,a.xui_build,a.values,a.sequences)))<=1
    fixture='foreign_view_sequence.c' if a.sequences else 'foreign_view_value.c' if a.values else 'foreign_native_xui_callback.c' if a.xui_build else 'foreign_native_userdata.c' if a.userdata else 'foreign_view_callbacks.c' if a.callbacks else 'foreign_view_storage.c'
    sources=[root/('test/support/'+fixture)]+[root/('src/runtime/'+n+'.c') for n in (
        'xl6_cell','xl6_native_future_payload','xl6_foreign_view','xl6_foreign_callback_scope','xl6_foreign_context',
        'xl6_closure','xl6_code_finalizer',
        'xl6_native_capture_graph','xl6_foreign_view_capture','xl6_capture_v3','xl6_closure_capture_v4',
        'xl6_cell_native_graph','xl6_native_graph_future_payload','xl6_ownership_domain_native_graph_future_payload')]
    if a.userdata or a.xui_build:sources.append(root/'src/runtime/xl6_native_callback.c')
    if a.values or a.sequences or a.xui_build:sources.append(root/'src/runtime/xl6_foreign_view_value.c')
    if a.sequences:sources.append(root/'src/runtime/xl6_foreign_view_sequence.c')
    base_root=next(Path(row['Source']).parents[2] for row in build['Objects'] if Path(row['Source']).name=='xl_api.c')
    runtime_directory=Path(build.get('RuntimeDirectory',root/('release/win_x64' if os.name=='nt' else 'release/linux_x64')))
    sdk=(a.sdk_root or runtime_directory).resolve()
    assert (sdk/'include/xrt_decl.h').is_file(), 'Specify --sdk-root for the matching cohort SDK'
    assert (base_root/'lib/xrt/xrt.h').read_bytes().replace(b'\r\n',b'\n')==(root/'lib/xrt/xrt.h').read_bytes().replace(b'\r\n',b'\n')
    replaced={str(path.relative_to(root)).lower() for path in sources}
    driver=('xlir_public_runtime_driver.c' if build['RuntimeOnly'] else 'xlir_public_source_driver.c') if os.name=='nt' else 'xlir_public_source_linux_driver.c'
    replaced.add(str(Path('test/cases')/driver).lower())
    reused=[row for row in build['Objects'] if str(Path(row['Source']).relative_to(base_root)).lower() not in replaced]
    assert all(sha(row['Path']).lower()==row['Sha256'].lower() for row in reused)
    out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    module_sources=[root/'test/support/foreign_native_userdata_module.c',root/'test/support/foreign_native_userdata_module.h'] if a.userdata else []
    inputs={str(path):sha(path) for path in [Path(__file__).resolve(),base/'build.json',*sources,*module_sources,
        root/'test/support/foreign_view_native_fixture.h',*sorted((root/'src/runtime').glob('*.h')),
        *sorted((root/'src/api').glob('*.h')),*sorted((root/'src/common').glob('*.h')),
        *sorted((sdk/'include').rglob('*.h'))]}
    engine=None
    if a.xui_build:
        a.xui_build=a.xui_build.resolve();a.xui_root=a.xui_root.resolve()
        engine=a.xui_build/('xge.dll' if os.name=='nt' else 'libxge.so')
        metadata=json.loads((a.xui_build/'build-report.json').read_text());assert metadata['xrt_mode']=='host'
        assert metadata['xrt_host_header_sha256']==hashlib.sha256((root/'lib/xrt/xrt.h').read_bytes().replace(b'\r\n',b'\n')).hexdigest()
        for path in (engine,a.xui_build/'build-report.json',a.xui_root/'include/xui.h',a.xui_build/'xge_build_config.h',
            *sorted((a.xui_build/'host-xrt').glob('*'))):
            if path.is_file():inputs[str(path)]=sha(path)
    report=dict(complete=False,passed=False,scope=__doc__,networkTests=False,
        canonicalFullRebuild=False,sourceSanitizers=a.sanitize,unchangedCoreInstrumented=False,
        baseOptimization=build['Optimization'],baseObjectCount=len(reused),
        sdkDirectory=str(sdk),xrtSourceMatchesCohort=True,
        reusedObjectBytesVerified=True,inputs=inputs,rows=[])
    env=dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'))
    env['PATH']=str(runtime_directory)+os.pathsep+env.get('PATH','')
    if engine:env['PATH']=str(engine.parent)+os.pathsep+env['PATH']
    if a.sanitize:
        assert os.name!='nt'
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    def save(): (out/'results.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    def run(name,command):
        started=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
            seconds=round(time.monotonic()-started,3),logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);return r.returncode==0
    baseflags=next(row['Flags'] for row in build['Objects'] if Path(row['Source']).name=='xl6_cell.c') if os.name=='nt' else [f.replace(str(base_root),str(root)) for f in build['CompileFlags']]
    # The fixture contains deliberately unused helper descriptors; no warning
    # about real type/signature mismatches is disabled.
    if os.name!='nt':baseflags+=['-Wno-unused-function','-Wno-unused-const-variable']
    baseflags+=['-I'+str(sdk),'-I'+str(sdk/'include')]
    if a.sanitize:baseflags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
    if engine:
        baseflags+=['-DXGE_DLL','-DXUI_DLL','-DXGE_XRT_HOST=1','-Wno-cast-function-type',
            '-I'+str(a.xui_root),'-I'+str(a.xui_root/'include'),'-I'+str(a.xui_build/'host-xrt'),
            '-include',str(a.xui_build/'xge_build_config.h')]
    cases=('typed','nullable','erased','stack','cycle','allocation','all') if a.sequences else ('values','class-field','cycle','cleanup','allocation','all') if a.values else ('all',) if engine else ('replacement','nested','binding','cycle','errors','cleanup','affinity','allocation','all') if a.userdata else ('finalizer','context-release','box-invoke','box-drop','all') if a.callbacks else ('cells','cell-cycle','futures','future-cycle','cleanup','allocation','all')
    for opt in ('O0','O2'):
        modules=[]
        if a.userdata:
            for role in ('body','adapter'):
                library=out/(opt+'-'+role+('.dll' if os.name=='nt' else '.so'))
                # Modules contain only typed entries; all ownership calls use
                # the exact resident operation table supplied by the host.
                if run(opt+'-module-'+role,[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,
                    '-I'+str(root),'-shared',*(['-fPIC'] if os.name!='nt' else []),
                    *(['-DUSERDATA_MODULE_BODY'] if role=='body' else []),module_sources[0],'-o',library]):modules.append(library)
        compiled=[]
        for source in sources:
            obj=out/(opt+'-'+source.stem+'.o')
            flags=[('-'+opt if f in ('-O0','-O2') else f) for f in baseflags]
            if not run(opt+'-'+source.stem,[cc,*flags,'-c',source,'-o',obj]):break
            compiled.append(obj)
        if len(compiled)!=len(sources):continue
        # Replace the original driver main by the storage test main, keeping
        # all other exact core objects.
        if os.name=='nt':
            link_response=Path(build.get('LinkResponse',base/'link.rsp'))
            if build.get('LinkResponseSha256'):assert sha(link_response).lower()==build['LinkResponseSha256'].lower()
            old_response=[line.strip('"') for line in link_response.read_text(encoding='utf-8-sig').splitlines()]
        else:old_response=next(row['Command'][1:] for row in build['Rows'] if row['Name']=='link')
        allowed={str(Path(row['Path']).resolve()).lower() for row in reused}
        response=[];skip_output=False
        for item in old_response:
            if skip_output:skip_output=False;continue
            if item=='-o':skip_output=True;continue
            if item.endswith('.o') and str(Path(item).resolve()).lower() not in allowed:continue
            response.append(item)
        exe=out/(opt+('.exe' if os.name=='nt' else '.out'));response += [str(obj) for obj in compiled]+['-o',str(exe)]
        if engine:response+=[str(engine),*(['-Wl,-rpath,'+str(engine.parent)] if os.name!='nt' else [])]
        if a.sanitize:response+=['-fsanitize=address,undefined','-no-pie']
        rsp=out/(opt+'.rsp');rsp.write_bytes(('\n'.join('"'+item.replace('\\','/')+'"' for item in response)+'\n').encode())
        if run(opt+'-link',[cc,'@'+str(rsp)]):
            for case in cases:
                run(opt+'-'+case,[exe,*([] if case=='all' else [case])])
            if a.userdata and len(modules)==2:run(opt+'-modules',[exe,'modules',*modules])
    report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
    report['complete']=True
    report['passed']=report['inputsUnchanged'] and len(report['rows'])==2*(len(sources)+1+len(cases)+(3 if a.userdata else 0)) and all(row['exitCode']==0 for row in report['rows'])
    save();assert report['passed'],out/'results.json'

if __name__=='__main__':main()
