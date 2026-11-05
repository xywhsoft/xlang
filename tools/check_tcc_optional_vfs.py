"""Optional dynamic/static TinyCC VFS slots, exact ABI and no-VFS compatibility.

This CPU-only gate uses one hash-verified compiler cohort (full or explicit
API overlay), its real provider, and a separate vanilla provider built from
the same frozen TinyCC sources with VFS disabled. It does not update release.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
from foreign_validation_build import add_build_arguments,load_build

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','out'):p.add_argument('--'+n,type=Path,required=True)
    add_build_arguments(p);a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();build=load_build(a.build,a.overlay)
    cohort,basePath,base,objects,inputs,driver,runtime=[build[k] for k in ('cohort','basePath','base','objects','inputs','driver','runtime')]
    overlay=json.loads(Path(build['overlay']).read_text()) if build['overlay'] else {}
    provider=Path(overlay.get('TccCore',base.get('TccCore')))
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest();out.mkdir(parents=True,exist_ok=False)
    fixtures=[root/'test/cases'/n for n in ('tcc_optional_vfs_loader.c','tcc_optional_vfs_signature.c')]
    inputs.update((str(v),sha(v)) for v in [Path(__file__).resolve(),provider,*fixtures,*[v for v in (cohort/'lib/tcc').rglob('*') if v.is_file()]])
    report=dict(complete=False,passed=False,scope=__doc__,fullSourceRebuild=build['fullSourceRebuild'],buildManifest=build['buildManifest'],overlay=build['overlay'],inputs=inputs,rows=[])
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort));sys.path.insert(0,str(root/'tools'));from compiler_response import command_with_response
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+str(provider.parent)+':'+env.get('LD_LIBRARY_PATH','')
    def run(name,command,accepted=True):
        command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
        started=time.monotonic();r=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=240);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,accepted=accepted,seconds=round(time.monotonic()-started,3),logSha256=sha(log)))
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(name,r.returncode,flush=True);assert (r.returncode==0)==accepted,log
    sdk=Path(base.get('SdkDirectory') or base['RuntimeDirectory']);includes=['-I'+str(v) for v in (cohort,basePath.parent/'include',sdk,sdk/'include',cohort/'lib/tcc',cohort/'lib/lzma')]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    run('signature-C11',['gcc',*flags,'-fsyntax-only',fixtures[1]])
    # Type-check assignments in the separately compiled static loader branch.
    staticFlags=next(v['Flags'] for v in base['Objects'] if Path(v['Source']).name=='xl_api.c') if os.name=='nt' else None
    if staticFlags is None:
        row=next(v for v in base['Rows'] if '-c' in v['Command'] and any(str(x).endswith('/src/api/xl_api.c') for x in v['Command']));staticFlags=row['Command'][1:row['Command'].index('-c')]
    parentRoot=Path(base['Objects'][0]['Source']).parents[2]
    staticFlags=[v.replace(str(parentRoot),str(cohort)).replace(parentRoot.as_posix(),cohort.as_posix()) for v in staticFlags]
    run('static-loader-signatures',['gcc',*staticFlags,'-U','TCC_DISABLE_BUILTIN_VFS','-DXL_STATIC_TCC','-fsyntax-only',cohort/'src/api/xl_api.c'])
    header=cohort/'lib/tcc/tcc_builtin_vfs.h';original=header.read_text()
    changes={
        'open':('int tcc_vfs_open(', 'long tcc_vfs_open('),'close':('int tcc_vfs_close(', 'long tcc_vfs_close('),
        'read':('int tcc_vfs_read(int fd, void *buf, size_t count);','int tcc_vfs_read(int fd, void *buf, int count);'),
        'lseek':('long tcc_vfs_lseek(', 'long long tcc_vfs_lseek('),'fopen':('FILE *tcc_vfs_fopen(', 'void *tcc_vfs_fopen('),
        'fclose':('int tcc_vfs_fclose(FILE *file);','int tcc_vfs_fclose(void *file);'),
        'mount':('const void *data, size_t size);','void *data, size_t size);'),
        'clear':('void tcc_vfs_clear_dynamic(void);','int tcc_vfs_clear_dynamic(void);')}
    for name,(before,after) in changes.items():
        assert before in original,(name,before);folder=out/('bad-'+name);folder.mkdir();(folder/header.name).write_text(original.replace(before,after,1))
        run('reject-signature-'+name,['gcc','-I'+str(folder),*flags,'-fsyntax-only',fixtures[1]],False)
    source=cohort/'lib/tcc';vanilla=out/('vanilla.dll' if os.name=='nt' else 'libvanilla.so')
    nativeFlags=['gcc','-std=c11','-O2','-DTCC_DISABLE_BUILTIN_VFS','-DLIBTCC_AS_DLL','-I'+str(source)]
    if os.name!='nt':
        # Frozen host/source cohorts retain the Windows config; use the same
        # independently recorded Linux producer configuration as the real VFS.
        nativeSource=out/'vanilla-source';shutil.copytree(source,nativeSource)
        stage=Path(overlay.get('NativeRoot',provider.parent.parent))
        provider_report=stage/'provider.json'
        inputs[str(provider_report)]=sha(provider_report)
        producer=Path(json.loads(provider_report.read_text())['producer'])/'lib/tcc'
        for name in ('config.h','tccdefs_.h'):
            inputs[str(producer/name)]=sha(producer/name);shutil.copy2(producer/name,nativeSource/name)
        source=nativeSource;nativeFlags=['gcc','-std=c11','-O2','-DTCC_DISABLE_BUILTIN_VFS','-DLIBTCC_AS_DLL','-I'+str(source),'-fPIC','-Wl,-Bsymbolic']
    units=[source/'libtcc.c',source/'tcctools.c']+([source/'tcc_utf8_io.c'] if os.name=='nt' else [])
    run('vanilla-provider',['gcc',*nativeFlags[1:],'-shared',*units,*(['-lws2_32'] if os.name=='nt' else ['-ldl','-lm']),'-o',vanilla])
    report['vanillaProvider']=dict(path=str(vanilla),sha256=sha(vanilla))
    if os.name=='nt':distribution=cohort/'res/tcc/win64/x86_64'
    else:distribution=Path(overlay.get('NativeRoot',provider.parent.parent))/'build/linux/tcc-dist'
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for opt in ('O0','O2'):
        exe=out/('loader-'+opt+('.exe' if os.name=='nt' else '.out'));run(opt+'-build',['gcc',*flags,'-'+opt,fixtures[0],*objects,runtime,*systems,'-o',exe])
        for name,module,count in (('vfs',provider,8),('vanilla',vanilla,0)):
            run(opt+'-'+name,[exe,module,distribution,cohort,basePath.parent/'include',sdk/'include',str(count)])
    report.update(complete=True,passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));assert report['inputsUnchanged'];(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
