"""Replay SDK29 pointer/callback grapheme equivalence in a new output directory."""
import argparse,ctypes,json,re,shutil,subprocess,time,uuid
from ctypes import wintypes
from pathlib import Path
from native_desktop import NativeDesktopProcess
from prepare_large_source_sdk import ancestors,name,plain,read,sha
IDE=Path(__file__).resolve().parents[1]
BUNDLE=IDE/'third_party/sdk_patches/large-source-experiment-v10'

def run(build_report,output,gcc):
    build_report=build_report.absolute();output=output.absolute();build=read(build_report)
    if build['status']!='sdk-build-finished' or build['exitCode']!=0:raise ValueError('Completed private build required')
    source=Path(build['source']).absolute()
    for root in (source,BUNDLE,output.parent):ancestors(root)
    if output.exists() or output.is_relative_to(source) or output.is_relative_to(BUNDLE):raise ValueError('Use a new output outside input trees')
    manifest=read(BUNDLE/'patch-manifest.json');descriptor=BUNDLE/'source-materials-before.json'
    if sha(descriptor)!=manifest['beforeDescriptorSha256']:raise ValueError('Source descriptor differs')
    inputs={r['path']:r['sha256'] for r in read(descriptor)['files']}
    inputs.update({r['path']:r['afterSha256'] for r in manifest['changes']})
    tests=manifest['pointerTestMaterials']
    if {r['path'] for r in tests}!={'tests/old_pointer_unicode.c','tests/pointer_next_test.c'}:raise ValueError('Unexpected test inventory')
    def verify():
        for relative,digest in inputs.items():
            p=source/name(relative);plain(p)
            if sha(p)!=digest:raise ValueError('Private source differs: '+relative)
        for row in tests:
            p=BUNDLE/name(row['path']);plain(p)
            if sha(p)!=row['sha256']:raise ValueError('Test material differs: '+row['path'])
        for row in build['outputs']:
            p=Path(row['path']);plain(p)
            if sha(p)!=row['sha256']:raise ValueError('Build output differs: '+str(p))
    verify();output.mkdir();engine=output/'engine';engine.mkdir()
    for relative,digest in inputs.items():
        target=engine/name(relative);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/relative,target)
        if sha(target)!=digest:raise ValueError('Engine copy differs')
    for row in tests:shutil.copy2(BUNDLE/row['path'],output/Path(row['path']).name)
    oldSource=output/'old_pointer_unicode.c';test=output/'pointer_next_test.c'
    symbols=re.findall(r'(?m)^(?:int|xui_internal_word_kind_t)\s+(xuiInternal\w+)\(',oldSource.read_text(encoding='utf8'))
    if len(symbols)!=len(set(symbols)) or 'xuiInternalTextGraphemeNextRead' not in symbols:raise ValueError('Unexpected reference API inventory')
    flags=['-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-Wno-cast-function-type',
           '-DXGE_DLL','-DXUI_DLL','-DXGE_DEBUGMODE=0','-DXGE_XRT_PROFILE_IDE','-I'+str(engine),'-I'+str(engine/'src')]
    executable=output/'pointer_next_test.exe'
    commands=[[str(gcc),*flags,*['-D'+s+'=old_'+s for s in symbols],'-c',str(oldSource),'-o',str(output/'old_unicode.o')],
              [str(gcc),*flags,'-c',str(engine/'src/xui_unicode.c'),'-o',str(output/'new_unicode.o')],
              [str(gcc),*flags,str(test),str(output/'old_unicode.o'),str(output/'new_unicode.o'),str(source/'build/ide/xge.lib'),
               '-lm','-luser32','-o',str(executable)]]
    report={'status':'running','root':str(output),'buildReport':str(build_report),'buildReportSha256':sha(build_report),
            'patchManifestSha256':sha(BUNDLE/'patch-manifest.json'),'commands':commands,'testMaterials':tests,
            'sourceFiles':len(inputs),'engineCopy':str(engine),'engineCopyUnmodified':True,
            'runtimeOwner':'SDK DLL; Unicode-only comparison objects import runtime',
            'steps':[],'sdkInstalled':False,'productionChanged':False}
    pointer=output/'result.json'
    def save():pointer.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    save();u=ctypes.WinDLL('user32',use_last_error=True);u.CreateDesktopW.restype=wintypes.HANDLE
    u.CreateDesktopW.argtypes=[wintypes.LPCWSTR,wintypes.LPCWSTR,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p]
    u.CloseDesktop.argtypes=[wintypes.HANDLE];desktop=None
    try:
        for i,arguments in enumerate(commands):
            log=output/f'strict-{i}.log'
            with log.open('xb') as f:code=subprocess.call(arguments,stdout=f,stderr=f)
            report['steps'].append({'exitCode':code,'log':str(log),'logSha256':sha(log)});save()
            if code:raise RuntimeError('Strict fixture compile failed')
        shutil.copy2(source/'build/ide/xge.dll',output/'xge.dll')
        desktopName='XlangPointerReplay-'+uuid.uuid4().hex;desktop=u.CreateDesktopW(desktopName,None,None,0,0x10000000,None)
        if not desktop:raise ctypes.WinError(ctypes.get_last_error())
        report.update(desktop=desktopName,neverSwitchedDesktop=True,executableSha256=sha(executable),sdkDllSha256=sha(output/'xge.dll'));save()
        log=output/'native.log';started=time.monotonic()
        with log.open('xb') as f:
            process=NativeDesktopProcess([str(executable)],f,f,desktopName,cwd=str(output))
            try:report['pid']=process.pid;save();code=process.wait(timeout=120)
            finally:process.close()
        report.update(exitCode=code,elapsedSeconds=round(time.monotonic()-started,3),log=str(log),logSha256=sha(log));save()
        if code:raise RuntimeError('Pointer grapheme comparison failed')
        text=log.read_text(encoding='utf8');m=re.search(r'pointer grapheme comparison passed: (\d+) assertions; (\d+) boundary cases; (\d+) scalar cases; (\d+) byte pairs; (\d+) callback failure cases; (\d+) guard-page cases;',text)
        if not m:raise ValueError('Native summary missing')
        values=tuple(map(int,m.groups()));fields=('assertions','boundaryCases','codepointValues','bytePairs','callbackFailureCases','guardPageCases')
        if values!=(36483444,3496738,1114112,65536,278528,8643):raise ValueError('Incomplete comparison')
        report.update(zip(fields,values));report['observations']=[json.loads(line) for line in text.splitlines() if line.startswith('{')]
        report['status']='pointer-grapheme-equivalence-passed';save()
    except BaseException as error:report.update(status='pointer-grapheme-equivalence-failed',error=str(error));save();raise
    finally:
        if desktop and not u.CloseDesktop(desktop):raise ctypes.WinError(ctypes.get_last_error())
        verify()
        for relative,digest in inputs.items():
            if sha(engine/relative)!=digest:raise ValueError('Engine copy changed')
        for row in tests:
            if sha(output/Path(row['path']).name)!=row['sha256']:raise ValueError('Test copy changed')
    print(json.dumps({k:report[k] for k in ('status',*fields,'observations')}),flush=True)

def main():
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--gcc',type=Path,default=Path(r'E:\software\w64devkit\bin\gcc.exe'))
    a=p.parse_args();run(a.build,a.output,a.gcc)
if __name__=='__main__':main()
