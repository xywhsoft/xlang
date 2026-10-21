"""Compare the actual prior and current document models, including physical gaps."""
import argparse,ctypes,json,re,shutil,subprocess,time
from ctypes import wintypes
from pathlib import Path
from native_desktop import NativeDesktopProcess
from prepare_large_source_sdk import ancestors,name,plain,read,sha
IDE=Path(__file__).resolve().parents[1]
BUNDLE=IDE/'third_party/sdk_patches/large-source-experiment-v12'
def run(build_report,output,gcc):
    build_report=build_report.absolute();output=output.absolute();build=read(build_report)
    if build['status']!='sdk-build-finished' or build['exitCode']!=0:raise ValueError('Completed private SDK build required')
    source=Path(build['source']).absolute()
    for root in (source,BUNDLE,output.parent):ancestors(root)
    if output.exists() or output.is_relative_to(source) or output.is_relative_to(BUNDLE):raise ValueError('Use a fresh output outside inputs')
    manifest=read(BUNDLE/'patch-manifest.json');descriptor=BUNDLE/'source-materials-before.json'
    if sha(descriptor)!=manifest['beforeDescriptorSha256']:raise ValueError('Source descriptor differs')
    inputs={r['path']:r['sha256'] for r in read(descriptor)['files']};inputs.update({r['path']:r['afterSha256'] for r in manifest['changes']})
    tests=manifest['gapTestMaterials']
    if {r['path'] for r in tests}!={'tests/old_gap_document.c','tests/gap_grapheme_test.c','tests/models.h','tests/position_test.c'}:raise ValueError('Unexpected comparison inventory')
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
    oldSource=output/'old_gap_document.c';symbols=re.findall(r'(?m)^XUI_API\s+[^;\n]+?\b(xuiCodeDocument\w+)\(',oldSource.read_text(encoding='utf8'))
    if len(symbols)!=30 or len(set(symbols))!=30:raise ValueError('Unexpected document API inventory')
    for prefix,inputPath in (('old',oldSource),('new',engine/'src/xui_code_document.c')):
        wrappers=f'''
void {prefix}_setVersion(xui_code_document d,uint32_t v) {{ d->iVersion=v; }}
int {prefix}_raw(xui_code_document d,int a,int b,const char* s) {{return __xuiCodeDocumentReplaceRaw(d,a,b,s,(int)strlen(s));}}
int {prefix}_owned(xui_code_document d,const char* s) {{size_t n=strlen(s);char* p=xrtMalloc(n+1);if(!p)return XUI_ERROR_OUT_OF_MEMORY;memcpy(p,s,n+1);return __xuiCodeDocumentSetOwnedText(d,p,(int)n);}}
int {prefix}_gap(xui_code_document d,int at) {{return __xuiCodeDocumentMoveGap(d,at);}}
int {prefix}_probe_buffer(const char* buffer,int length,int gapStart,int gapEnd,int offset) {{struct xui_code_document_t d={{0}};d.sBuffer=(char*)buffer;d.iLength=length;d.iBufferCapacity=length+gapEnd-gapStart;d.iGapStart=gapStart;d.iGapEnd=gapEnd;return __xuiCodeDocumentUtf8Next(&d,offset);}}
'''
        (output/(prefix+'_model.c')).write_text('#include "'+inputPath.as_posix()+'"\n'+wrappers,encoding='utf8')
    regression=(output/'position_test.c').read_text(encoding='utf8')
    if regression.count('int main(void)')!=1:raise ValueError('Unexpected regression main')
    regression=regression.replace('int main(void)','static int positionRegression(void)')
    fixture=output/'gap_all.c';fixture.write_text(regression+'\n'+(output/'gap_grapheme_test.c').read_text(encoding='utf8'),encoding='utf8')
    flags=['-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-Wno-cast-function-type','-DXGE_DLL','-DXGE_DEBUGMODE=0','-DXGE_XRT_PROFILE_IDE','-I'+str(engine),'-I'+str(engine/'src')]
    exe=output/'gap_test.exe'
    commands=[]
    for prefix in ('old','new'):
        hook=['-DxuiInternalTextGraphemeNextRead=countedNextRead','-DxuiInternalTextGraphemeNext=countedPointerNext'] if prefix=='new' else []
        commands.append([str(gcc),*flags,*['-D'+symbol+'='+prefix+'_'+symbol for symbol in symbols],*hook,'-c',str(output/(prefix+'_model.c')),'-o',str(output/(prefix+'_model.o'))])
    commands.append([str(gcc),*flags,'-c',str(engine/'src/xui_unicode.c'),'-o',str(output/'unicode.o')])
    commands.append([str(gcc),*flags,str(fixture),str(output/'old_model.o'),str(output/'new_model.o'),str(output/'unicode.o'),str(source/'build/ide/xge.lib'),'-lm','-luser32','-o',str(exe)])
    report=dict(status='running',root=str(output),buildReport=str(build_report),buildReportSha256=sha(build_report),patchManifestSha256=sha(BUNDLE/'patch-manifest.json'),
        testMaterials=tests,sourceFiles=len(inputs),engineCopy=str(engine),engineCopyUnmodified=True,commands=commands,steps=[],
        referenceDocumentSourceSha256=sha(oldSource),actualDocumentApiCount=30,fixtureSha256=sha(fixture),
        runtimeOwner='one SDK DLL XRT owner; two document-only API-renamed objects and unchanged Unicode-only object',diagnosticHooks='new document next traversal call counters only',
        sdkInstalled=False,productionChanged=False)
    pointer=output/'result.json'
    def save():pointer.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    save();u=ctypes.WinDLL('user32',use_last_error=True);u.CreateDesktopW.restype=wintypes.HANDLE
    u.CreateDesktopW.argtypes=[wintypes.LPCWSTR,wintypes.LPCWSTR,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p];u.CloseDesktop.argtypes=[wintypes.HANDLE];desktop=None
    try:
        for i,command in enumerate(commands):
            log=output/f'strict-{i}.log'
            with log.open('xb') as f:code=subprocess.call(command,stdout=f,stderr=f)
            report['steps'].append(dict(exitCode=code,log=str(log),logSha256=sha(log)));save()
            if code:raise RuntimeError('Strict document fixture compile failed')
        shutil.copy2(source/'build/ide/xge.dll',output/'xge.dll')
        import uuid
        desktopName='XlangGapTraversal-'+uuid.uuid4().hex;desktop=u.CreateDesktopW(desktopName,None,None,0,0x10000000,None)
        if not desktop:raise ctypes.WinError(ctypes.get_last_error())
        report.update(desktop=desktopName,neverSwitchedDesktop=True,executableSha256=sha(exe),sdkDllSha256=sha(output/'xge.dll'));save()
        log=output/'native.log';started=time.monotonic()
        with log.open('xb') as f:
            p=NativeDesktopProcess([str(exe)],f,f,desktopName,cwd=str(output))
            try:report['pid']=p.pid;save();code=p.wait(timeout=120)
            finally:p.close()
        report.update(exitCode=code,elapsedSeconds=round(time.monotonic()-started,3),log=str(log),logSha256=sha(log));save()
        if code:raise RuntimeError('Gap traversal equivalence failed')
        text=log.read_text(encoding='utf8');m=re.search(r'gap traversal passed: (\d+) assertions; (\d+) next cases; (\d+) byte pairs; (\d+) guard-page cases;',text)
        if not m:raise ValueError('Gap summary missing')
        report.update(zip(('gapAssertions','gapCases','bytePairs','guardPageCases'),map(int,m.groups())))
        m=re.search(r'position cache equivalence passed: (\d+) assertions;',text)
        if not m:raise ValueError('Position regression summary missing')
        report['positionAssertions']=int(m[1]);report['observations']=[json.loads(line) for line in text.splitlines() if line.startswith('{')]
        report['status']='gap-reader-and-document-equivalence-passed';save()
    except BaseException as e:report.update(status='gap-reader-and-document-equivalence-failed',error=str(e));save();raise
    finally:
        if desktop and not u.CloseDesktop(desktop):raise ctypes.WinError(ctypes.get_last_error())
        verify()
        for relative,digest in inputs.items():
            if sha(engine/relative)!=digest:raise ValueError('Engine copy changed')
        for row in tests:
            if sha(output/Path(row['path']).name)!=row['sha256']:raise ValueError('Test copy changed')
    print(json.dumps({k:report[k] for k in ('status','positionAssertions','gapAssertions','gapCases','bytePairs','guardPageCases','observations')}),flush=True)
def main():
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--gcc',type=Path,default=Path(r'E:\software\w64devkit\bin\gcc.exe'))
    a=p.parse_args();run(a.build,a.output,a.gcc)
if __name__=='__main__':main()
