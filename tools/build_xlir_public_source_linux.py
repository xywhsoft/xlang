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
    parser.add_argument('--optimization',choices=['O0','O2'],default='O0')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--runtime-resource',type=Path)
    args=parser.parse_args(); root,native,out=[getattr(args,name).resolve() for name in ('root','native_root','out')]
    assert os.name!='nt' and not out.exists()
    project=root; cc=shutil.which('gcc'); assert cc
    source_list=root/'tools/build_xlir_public_source_driver.ps1'
    body=source_list.read_text(encoding='utf-8-sig').split('$sources = @(',1)[1].split('$files =',1)[0]
    # This CRT UTF-8 I/O shim is Windows-only (io.h/_wopen). Linux uses its
    # native UTF-8 paths, as build_linux.sh does, and must not compile the shim.
    sources=[root/path for path in re.findall(r"'((?:src|tools|lib)/[^']+\.c)'",body)
        if path!='lib/tcc/tcc_utf8_io.c']
    runtime_manifests=[root/'tools/compiler_dll.rsp.in',root/'build_dll.bat']
    for manifest in runtime_manifests:
        for name in re.findall(r'\bsrc/runtime/[A-Za-z0-9_]+\.c\b',manifest.read_text(encoding='utf-8-sig').replace('\\','/')):
            path=root/name
            if path not in sources:sources.append(path)
    sources.append(project/'test/cases/xlir_public_source_linux_driver.c')
    assert len({path.stem for path in sources})==len(sources)
    required={'xl_api','xl_api_symbols','xrt_single_header_impl','xl6_ir','xl6_ir_lower',
        'xl6_ir_cgen','xl6_compile_unit','xl6_tree','xl6_tree_admission','xl6_semaphore',
        'xl6_array_semaphore_admission','xl6_sync','xl6_mutex','xl6_cond','xl6_rwlock',
        'xl6_array_sync_admission','xl6_queue_spsc_admission','xl6_queue_mpsc_admission',
        'xl6_queue_mpmc_admission','xl6_thread','xl6_array_thread_admission'}
    assert required.issubset({path.stem for path in sources})
    tcc_core=native/'release/libxlc.so'; assert tcc_core.is_file()
    # ELF interposes exported TCC helpers. Linking libxl while loading libxlc
    # would mix two private TCC states; the host and selected core must agree.
    runtime=tcc_core
    configuration=native/'build/linux/tcc-source/config.h'
    assert configuration.is_file(), 'Use a native build with its actual TCC configuration'
    configured=re.search(r'^#define\s+CONFIG_TRIPLET\s+"([A-Za-z0-9_.+\-]+)"\s*$',
        configuration.read_text(),re.M)
    triplet=configured.group(1) if configured else None
    files={Path(__file__).resolve(),source_list,Path(cc),runtime,tcc_core,*sources,*runtime_manifests}
    files.add(configuration)
    runtime_resource=args.runtime_resource.resolve() if args.runtime_resource else None
    if runtime_resource:
        assert runtime_resource.is_file()
        files.add(runtime_resource)
    for folder in ('src','lib','res/tcc','release/include','release/extlibs'):
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
        'SourceProductAcceptance':False,'ShippingLibraryAcceptance':False,'Scope':__doc__,'NativeRoot':str(native),'Optimization':args.optimization,
        'Sanitize':args.sanitize,'AllSourceProvidersInstrumented':args.sanitize,'ReleasedTccCoreInstrumented':False,
        'ConfiguredTriplet':triplet,'RuntimeResource':str(runtime_resource) if runtime_resource else None,
        'TccCore':str(tcc_core),'TccCoreSha256':sha(tcc_core),'RuntimeLibrary':str(runtime)}
    def save(): (out/'build.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode}); save()
        assert result.returncode==0,(name,data[-6000:].decode(errors='replace'))
    manifest=root/'tools/tcc_vfs_manifest_linux_xlc.txt'
    for line in manifest.read_text(encoding='utf-8-sig').splitlines():
        if line.startswith('dir|include/__xl_runtime|'):
            parts=line.split('|'); assert len(parts)==4 and parts[3]=='*.h'
            directory=Path(parts[2].replace('%XL6_ROOT%',str(root)))
            for original in sorted(directory.glob('*.h')):
                assert original.is_file()
                target=out/'include/__xl_runtime'/original.name
                target.parent.mkdir(parents=True,exist_ok=True); assert not target.exists()
                shutil.copy2(original,target); assert sha(target)==sha(original)
                headers.append({'Path':str(target),'Source':str(original),'Sha256':sha(target)})
            continue
        if not line.startswith('file|include/__xl_runtime/'): continue
        _,relative,source=line.split('|'); original=Path(source.replace('%XL6_ROOT%',str(root)))
        target=out/relative; target.parent.mkdir(parents=True,exist_ok=True); assert not target.exists()
        shutil.copy2(original,target)
        assert sha(target)==sha(original)
        headers.append({'Path':str(target),'Source':str(original),'Sha256':sha(target)})
    assert len(headers)>=6
    if runtime_resource:
        target=out/'include/__xl_compiler/build/linux/tcc_builtin_resources_runtime.c'
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(runtime_resource,target)
        assert sha(target)==sha(runtime_resource)
        headers.append({'Path':str(target),'Source':str(runtime_resource),'Sha256':sha(target)})
    # Mirror manifest-owned native sources for standalone component builds;
    # generated nested compiler resources are deliberately outside this gate.
    for line in manifest.read_text(encoding='utf-8-sig').splitlines():
        parts=line.split('|')
        if len(parts)<3 or not parts[1].startswith('__xl_compiler/') or not parts[2].startswith('%XL6_ROOT%'): continue
        relative=Path(parts[1]); assert '..' not in relative.parts and not relative.is_absolute()
        source=Path(parts[2].replace('%XL6_ROOT%',str(root))).resolve()
        assert source.is_relative_to(root)
        originals=sorted(f for f in source.rglob('*') if f.is_file() and
            any(f.match(pattern) for pattern in parts[3].split(';'))) if parts[0]=='dir' else [source]
        for original in originals:
            target=out/'include'/relative
            if parts[0]=='dir': target/=original.relative_to(source)
            target.parent.mkdir(parents=True,exist_ok=True); assert not target.exists()
            shutil.copy2(original,target); assert sha(target)==sha(original)
            headers.append({'Path':str(target),'Source':str(original),'Sha256':sha(target)})
    flags=['-std=c11','-D_GNU_SOURCE','-Wall','-Wextra','-Werror','-'+args.optimization,'-DXL_STATIC_EMBED',
        '-DXL_ENABLE_EXE_COMPILER=1','-DTCC_DISABLE_BUILTIN_VFS',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc')]
    if triplet:flags.append('-DXL6_TCC_CONFIGURED_TRIPLET="'+triplet+'"')
    instrumentation=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []
    flags+=instrumentation+(['-fno-pie'] if args.sanitize else [])
    report['CompileFlags']=flags
    compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-Wno-pointer-sign','-Wno-format-truncation']
    strict={'xl6_sync','xl6_mutex','xl6_cond','xl6_rwlock','xl6_array_sync_admission',
        'xl6_thread','xl6_array_thread_admission','xlir_public_source_linux_driver'}
    try:
        for index,source in enumerate(sources,1):
            obj=out/(source.stem+'.o')
            extra=['-Wshadow'] if source.stem in strict else compat
            if source.is_relative_to(root/'lib/tcc'): extra+=['-Wno-error']
            run(source.stem,[cc,*flags,*extra,'-c',source,'-o',obj])
            objects.append({'Path':str(obj),'Sha256':sha(obj),'Source':str(source)})
            print(f'[{index}/{len(sources)}] compiled {source.name}',flush=True)
        driver=out/'public_source'
        run('link',[cc,*instrumentation,*(['-no-pie'] if args.sanitize else []),*[row['Path'] for row in objects],'-L'+str(runtime.parent),'-lxlc',
            '-Wl,-rpath,'+str(runtime.parent),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',driver])
        report.update(Driver=str(driver),DriverSha256=sha(driver))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print('Linux public source driver built from '+str(len(sources))+' translation units',flush=True)
if __name__=='__main__': main()
