"""Exercise find options against a frozen build on owned private desktops.

An explicit build report permits reproducing a bug without confusing new test
files with the older build inputs. Every frozen input/object/output is verified.
"""
import argparse, ctypes as c
from ctypes import wintypes as w
from datetime import datetime, timezone
import hashlib, json, os, shutil, subprocess, time, uuid
from pathlib import Path
from native_desktop import NativeDesktopProcess

IDE=Path(__file__).resolve().parents[1]
def read(p): return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):
    with Path(p).open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def write(p,v):
    temp=p.with_suffix('.tmp');temp.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');os.replace(temp,p)
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-report',type=Path)
    parser.add_argument('--sdk-build',type=Path)
    parser.add_argument('--view',action='store_true')
    args=parser.parse_args()
    report=args.build_report or Path(read(IDE/'build/current-build.json')['report'])
    build=read(report);base=Path(build['root'])
    assert build['status']=='current-source-build-passed' and build['exitCode']==0
    protected={report:sha(report)}
    for row in build['inputs']: protected[base/row['path']]=row['sha256']
    for group in ('objects','outputs'):
        for name,digest in build[group].items(): protected[base/name]=digest
    sdk=base/'third_party/xge_sdk';dll=base/'release/xge.dll'
    sdk_report=None
    if args.sdk_build:
        sdk_report=read(args.sdk_build)
        assert sdk_report['exitCode']==0 and sdk_report['wholeSdkWerror'] and sdk_report['sourceUnchanged']
        protected[args.sdk_build]=sha(args.sdk_build)
        manifest=Path(sdk_report['sourceManifest']);assert sha(manifest)==sdk_report['sourceManifestSha256'];protected[manifest]=sha(manifest)
        for row in read(manifest)['files']:protected[Path(sdk_report['source'])/row['path']]=row['sha256']
        for row in sdk_report['outputs']:protected[Path(row['path'])]=row['sha256']
        dll=Path(sdk_report['source'])/'build/ide/xge.dll'
    source=IDE/'tests'/('find_boundary_view_test.c' if args.view else 'find_boundaries_test.c')
    protected[source]=sha(source);protected[Path(__file__)]=sha(__file__)
    def verify():
        for p,digest in protected.items(): assert sha(p)==digest,str(p)
    verify()
    root=IDE/'build'/('find-boundaries-'+uuid.uuid4().hex[:12]);release=root/'release';release.mkdir(parents=True)
    for name in ('xge.dll','xlang-ls.exe'):shutil.copy2(dll if name=='xge.dll' else base/'release'/name,release/name)
    if args.view:shutil.copytree(base/'release/data',release/'data')
    shutil.copy2(source,root/source.name);shutil.copy2(__file__,root/Path(__file__).name)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-DXGE_DLL','-DXUI_DLL','-DXGE_XRT_PROFILE_IDE','-DXGE_DEBUGMODE=0','-O2','-finput-charset=UTF-8','-fexec-charset=UTF-8','-I'+str(base/'src'),'-I'+str(sdk),'-I'+str(sdk/'lib/xrt')]
    exe=release/'find-options.exe'
    cmd=[build['compiler'],*flags,str(root/source.name)]
    if args.view:cmd += [str(base/name) for name in sorted(build['objects'])]
    cmd += [str(sdk/'xge.lib'),'-lm','-lws2_32','-lwinhttp','-liphlpapi','-lgdi32','-luser32','-lshell32','-lopengl32','-lole32','-lwinmm','-lavrt','-lcomdlg32','-lversion','-lbcrypt','-ladvapi32','-o',str(exe)]
    record=dict(status='linking',utc=datetime.now(timezone.utc).isoformat(),root=str(root),buildReport=str(report),buildReportSha256=sha(report),sdkBuild=str(args.sdk_build) if args.sdk_build else None,sdkSha256=sha(dll),sourceSha256=sha(source),command=cmd,neverSwitchedDesktop=True,cases=[])
    evidence=root/'evidence.json';write(evidence,record);print(root,flush=True)
    with (root/'link.log').open('xb') as log:link=subprocess.run(cmd,stdout=log,stderr=log)
    record['linkExitCode']=link.returncode;record['linkLogSha256']=sha(root/'link.log');write(evidence,record)
    if link.returncode:
        print((root/'link.log').read_text(encoding='utf-8',errors='replace'));return link.returncode
    protected[exe]=sha(exe);protected[release/'xge.dll']=sha(release/'xge.dll')
    user=c.WinDLL('user32',use_last_error=True)
    user.CreateDesktopW.restype=w.HANDLE;user.CreateDesktopW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.c_void_p,w.DWORD,w.DWORD,c.c_void_p]
    user.CloseDesktop.restype=w.BOOL;user.CloseDesktop.argtypes=[w.HANDLE]
    cases=[[]]
    for index,arguments in enumerate(cases):
        case_root=root/str(index);case_root.mkdir();log=case_root/'native.log'
        desktop='XlangFindOptions-'+uuid.uuid4().hex;handle=user.CreateDesktopW(desktop,None,None,0,0x10000000,None);assert handle,c.get_last_error()
        started=time.monotonic();timed_out=False
        try:
            with log.open('xb') as stream:
                process=NativeDesktopProcess([str(exe),str(case_root)] if args.view else [str(exe),*arguments],stdout=stream,stderr=stream,startupinfo=desktop,cwd=str(release))
                try:code=process.wait(90)
                except subprocess.TimeoutExpired:timed_out=True;process.kill();code=process.wait(10)
                pid=process.pid;process.close()
        finally:user.CloseDesktop(handle)
        case=dict(arguments=arguments,pid=pid,exitCode=code,timedOut=timed_out,elapsedSeconds=round(time.monotonic()-started,3),log=str(log),logSha256=sha(log))
        record['cases'].append(case);write(evidence,record)
        print(json.dumps(case,ensure_ascii=False),flush=True)
    verify();record.update(status='native-find-options-passed' if all(row['exitCode']==0 and not row['timedOut'] for row in record['cases']) else 'native-find-options-failed',inputsUnchanged=True,executableSha256=sha(exe),artifacts=[dict(path=str(p.relative_to(root)),sha256=sha(p)) for p in sorted(root.rglob('*.png'))]);write(evidence,record)
    return 0 if record['status']=='native-find-options-passed' else 1
if __name__=='__main__':raise SystemExit(main())
