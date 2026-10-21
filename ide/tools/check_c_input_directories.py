"""Reproduce directory observations against a frozen application or the updated C guard."""
import argparse
import ctypes as c
from ctypes import wintypes as w
from datetime import datetime, timezone
import hashlib, json, os, shutil, subprocess, time, uuid
from pathlib import Path
from native_desktop import NativeDesktopProcess

IDE = Path(__file__).resolve().parents[1]
read = lambda p: json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(path):
    with Path(path).open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()
def publish(path, value):
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    for attempt in range(80):
        try: os.replace(temp, path); return
        except PermissionError:
            if attempt == 79: raise
            time.sleep(.025)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-report', type=Path, required=True)
    parser.add_argument('--updated', action='store_true')
    args=parser.parse_args()
    report=args.build_report.absolute(); build=read(report); base=Path(build['root']); sdk=base/'third_party/xge_sdk'
    assert build['exitCode']==0 and build['testsBuilt']
    protected={report:sha(report), Path(__file__):sha(__file__)}
    for row in build['inputs']: protected[base/row['path']]=row['sha256']
    for group in ('objects','outputs'):
        for name, digest in build[group].items(): protected[base/name]=digest
    source=IDE/'tests/c_input_directory_test.c'; protected[source]=sha(source)
    root=IDE/'build'/('c-input-directories-'+uuid.uuid4().hex[:12]); release=root/'release'; release.mkdir(parents=True)
    shutil.copy2(base/'release/xge.dll', release/'xge.dll')
    shutil.copy2(source,root/source.name); shutil.copy2(__file__,root/Path(__file__).name)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-DXGE_DLL','-DXUI_DLL','-DXGE_XRT_PROFILE_IDE','-DXGE_DEBUGMODE=0','-O2','-finput-charset=UTF-8','-fexec-charset=UTF-8','-I'+str(base/'src'),'-I'+str(sdk),'-I'+str(sdk/'lib/xrt')]
    libraries=[str(sdk/'xge.lib'),'-lm','-lws2_32','-lwinhttp','-liphlpapi','-lgdi32','-luser32','-lshell32','-lopengl32','-lole32','-lwinmm','-lavrt','-lcomdlg32','-lversion','-lbcrypt','-ladvapi32']
    objects=[str(base/name) for name in sorted(build['objects'])]
    compile_commands=[]
    if args.updated:
        guard=IDE/'src/services/c_input_trace.c'; protected[guard]=sha(guard)
        copied=root/'src/services/c_input_trace.c'; copied.parent.mkdir(parents=True); shutil.copy2(guard,copied)
        header=root/'tools/clangd_input_trace/NativeInput.h'; header.parent.mkdir(parents=True); shutil.copy2(base/'tools/clangd_input_trace/NativeInput.h',header)
        obj=root/'c_input_trace.o'; compile_commands.append([build['compiler'],*flags,'-c',str(copied),'-o',str(obj)])
        objects=[name for name in objects if not name.endswith('src_services_c_input_trace.c.o')]+[str(obj)]
    exe=release/'c_input_directory_test.exe'
    compile_commands.append([build['compiler'],*flags,str(root/source.name),*objects,*libraries,'-o',str(exe)])
    record=dict(status='building', utc=datetime.now(timezone.utc).isoformat(), root=str(root), updatedGuard=args.updated,
                buildReport=str(report), buildReportSha256=sha(report), sourceSha256=sha(source),
                compileCommands=compile_commands, actualExitCodes=[], neverSwitchedDesktop=True)
    receipt=root/'evidence.json'; publish(receipt,record); print(root,flush=True)
    def verify():
        for path, digest in protected.items(): assert sha(path)==digest,str(path)
    verify()
    with (root/'compile.log').open('xb') as stream:
        for command in compile_commands:
            code=subprocess.run(command, stdout=stream, stderr=stream).returncode
            record['actualExitCodes'].append(code); publish(receipt,record)
            if code:
                record['status']='build-failed'; publish(receipt,record)
                print((root/'compile.log').read_text(encoding='utf-8',errors='replace')); return code
    protected[exe]=sha(exe); protected[release/'xge.dll']=sha(release/'xge.dll')
    user=c.WinDLL('user32',use_last_error=True); user.CreateDesktopW.restype=w.HANDLE
    user.CreateDesktopW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.c_void_p,w.DWORD,w.DWORD,c.c_void_p]
    user.CloseDesktop.restype=w.BOOL; user.CloseDesktop.argtypes=[w.HANDLE]
    name='XlangCDirectories-'+uuid.uuid4().hex; desktop=user.CreateDesktopW(name,None,None,0,0x10000000,None); assert desktop
    started=time.monotonic(); process=None; code=1
    try:
        with (root/'native.log').open('xb') as stream:
            process=NativeDesktopProcess([str(exe),str(root/'cases')],stream,stream,name,cwd=str(release))
            record.update(status='running',pid=process.pid,desktop=name); publish(receipt,record)
            code=process.wait(90)
    finally:
        if process: process.close()
        assert user.CloseDesktop(desktop)
    verify()
    record.update(status='directory-observations-passed' if code==0 else 'directory-observations-failed',
                  exitCode=code, elapsedSeconds=round(time.monotonic()-started,3), inputsUnchanged=True,
                  executableSha256=sha(exe), nativeLogSha256=sha(root/'native.log'))
    publish(receipt,record); print((root/'native.log').read_text(encoding='utf-8',errors='replace'))
    return code
if __name__=='__main__': raise SystemExit(main())
