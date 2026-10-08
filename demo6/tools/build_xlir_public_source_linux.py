"""Build every public compiler source on Linux from the canonical source list.
Actual XRT/xruntime and all compiler/API/native providers are built, without
object overrides. Only the genuine released TCC core is reused dynamically.
This source-driver proof does not rebuild/certify shipping libraries or VFS.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','native-root','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root,native,out=[getattr(args,name).resolve() for name in ('root','native_root','out')]
    assert os.name!='nt' and not out.exists()
    project=Path(__file__).resolve().parents[1]; cc=shutil.which('gcc'); assert cc
    source_list=root/'tools/build_xlir_public_source_driver.ps1'
    body=source_list.read_text(encoding='utf-8-sig').split('$sources = @(',1)[1].split('$files =',1)[0]
    # This CRT UTF-8 I/O shim is Windows-only (io.h/_wopen). Linux uses its
    # native UTF-8 paths, as build_linux.sh does, and must not compile the shim.
    sources=[root/path for path in re.findall(r"'((?:src|tools|lib)/[^']+\.c)'",body)
        if path!='lib/tcc/tcc_utf8_io.c']
    sources.append(project/'test/cases/xlir_public_source_linux_driver.c')
    assert len({path.stem for path in sources})==len(sources)
    required={'xl_api','xl_api_symbols','xrt_single_header_impl','xl6_ir','xl6_ir_lower',
        'xl6_ir_cgen','xl6_compile_unit','xl6_tree','xl6_tree_admission','xl6_semaphore',
        'xl6_array_semaphore_admission','xl6_sync','xl6_mutex','xl6_cond','xl6_rwlock',
        'xl6_array_sync_admission','xl6_queue_spsc_admission','xl6_queue_mpsc_admission',
        'xl6_queue_mpmc_admission'}
    assert required.issubset({path.stem for path in sources})
    runtime=native/'release/libxl.so'; assert runtime.is_file()
    files={Path(__file__).resolve(),source_list,Path(cc),runtime,*sources}
    for folder in ('src','lib','res/tcc','release/include'):
        files.update(path for path in (root/folder).rglob('*') if path.is_file())
    files.update(path for path in (native/'build/linux/tcc-dist').rglob('*') if path.is_file())
    # Record the actual compiler/assembler/linker used, not just the gcc wrapper.
    for name in ('cc1','as','ld'):
        selected=subprocess.check_output([cc,'-print-prog-name='+name],text=True).strip()
        path=Path(selected) if Path(selected).is_absolute() else Path(shutil.which(selected))
        files.add(path.resolve())
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(); tmp=out/'tmp'; tmp.mkdir(); env=dict(os.environ,TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp))
    rows=[]; objects=[]; headers=[]
    report={'Complete':False,'InputsUnchanged':False,'Inputs':inputs,'Objects':objects,'Headers':headers,
        'Sources':list(map(str,sources)),'Rows':rows,'SourceXrt':True,'RuntimeOnly':False,'EnableExeCompiler':1,
        'SourceProductAcceptance':False,'ShippingLibraryAcceptance':False,'Scope':__doc__,'NativeRoot':str(native)}
    def save(): (out/'build.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert result.returncode==0,(name,data[-6000:].decode(errors='replace'))
    manifest=root/'tools/tcc_vfs_manifest_linux_xlc.txt'
    for line in manifest.read_text(encoding='utf-8-sig').splitlines():
        if not line.startswith('file|include/__xl_runtime/'): continue
        _,relative,source=line.split('|'); original=Path(source.replace('%XL6_ROOT%',str(root)))
        target=out/relative; target.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(original,target)
        assert sha(target)==sha(original)
        headers.append({'Path':str(target),'Source':str(original),'Sha256':sha(target)})
    assert len(headers)>=6
    flags=['-std=c11','-D_GNU_SOURCE','-Wall','-Wextra','-Werror','-O0','-DXL_STATIC_EMBED',
        '-DXL_ENABLE_EXE_COMPILER=1','-DTCC_DISABLE_BUILTIN_VFS',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc')]
    compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-Wno-pointer-sign','-Wno-format-truncation']
    strict={'xl6_sync','xl6_mutex','xl6_cond','xl6_rwlock','xl6_array_sync_admission','xlir_public_source_linux_driver'}
    try:
        for index,source in enumerate(sources,1):
            obj=out/(source.stem+'.o')
            extra=['-Wshadow'] if source.stem in strict else compat
            if source.is_relative_to(root/'lib/tcc'): extra+=['-Wno-error']
            run(source.stem,[cc,*flags,*extra,'-c',source,'-o',obj])
            objects.append({'Path':str(obj),'Sha256':sha(obj),'Source':str(source)})
            print(f'[{index}/{len(sources)}] compiled {source.name}',flush=True)
        driver=out/'public_source'
        run('link',[cc,*[row['Path'] for row in objects],'-L'+str(runtime.parent),'-lxl',
            '-Wl,-rpath,'+str(runtime.parent),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',driver])
        report.update(Driver=str(driver),DriverSha256=sha(driver))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Linux public source driver built from '+str(len(sources))+' translation units',flush=True)
if __name__=='__main__': main()
