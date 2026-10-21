"""Compare live clip reuse with independent prior rendering and exact state queries."""
import argparse,ctypes,json,re,shutil,stat,subprocess,time,uuid
from ctypes import wintypes
from pathlib import Path
from native_desktop import NativeDesktopProcess
from prepare_large_source_sdk import ancestors,name,plain,read,sha
IDE=Path(__file__).resolve().parents[1]
BUNDLE=IDE/'third_party/sdk_patches/large-source-experiment-v11'
def run(build_report,output,gcc,latin,cjk):
    build_report=build_report.absolute();output=output.absolute();build=read(build_report)
    if build['status']!='sdk-build-finished' or build['exitCode']!=0:raise ValueError('Completed private build required')
    source=Path(build['source']).absolute()
    for root in (source,BUNDLE,output.parent):ancestors(root)
    if output.exists() or output.is_relative_to(source) or output.is_relative_to(BUNDLE):raise ValueError('Use a new output outside input trees')
    manifest=read(BUNDLE/'patch-manifest.json');descriptor=BUNDLE/'source-materials-before.json'
    if sha(descriptor)!=manifest['beforeDescriptorSha256']:raise ValueError('Source descriptor differs')
    inputs={r['path']:r['sha256'] for r in read(descriptor)['files']}
    inputs.update({r['path']:r['afterSha256'] for r in manifest['changes']})
    tests=manifest['clipTestMaterials']
    if {r['path'] for r in tests}!={'tests/paint_cache_proxy.inc','tests/old_paint_renderer.c','tests/paint_cache_test.c','tests/old_clip_renderer.c','tests/clip_query_test.c'}:raise ValueError('Unexpected test inventory')
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
    verify()
    for p in (latin,cjk):
        ancestors(p.parent);info=p.lstat()
        if info.st_file_attributes&0x400 or not stat.S_ISREG(info.st_mode):raise ValueError('Regular read-only font required')
    output.mkdir();engine=output/'engine';engine.mkdir()
    for relative,digest in inputs.items():
        target=engine/name(relative);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/relative,target)
        if sha(target)!=digest:raise ValueError('Engine copy differs')
    p=engine/'src/xge_text_run.c';s=p.read_text(encoding='utf8')
    for marker,replacement in [('pGlyphs = (xge_glyph_position_t*)xrtRealloc(pCache->tRun.pGlyphs,','pGlyphs = (xge_glyph_position_t*)diagPaintRealloc(pCache->tRun.pGlyphs,'),
        ('pPens = (xge_vec2_t*)xrtRealloc(pCache->pPens,','pPens = (xge_vec2_t*)diagPaintRealloc(pCache->pPens,'),
        ('pCache = (xge_glyph_paint_cache_t*)xrtCalloc(1, sizeof(*pCache));','pCache = (xge_glyph_paint_cache_t*)diagPaintCalloc(1, sizeof(*pCache));')]:
        if s.count(marker)!=1:raise ValueError('Unexpected allocation-hook site')
        s=s.replace(marker,replacement)
    for marker,replacement in [('pClip->tPixels = xgeClipGetPixels();','pClip->tPixels = diagClipPixels();'),
        ('xgeDrawEx(&tDraw);','diagClipDrawEx(&tDraw);'),('__xgeEmojiDraw(','diagClipEmojiDraw(')]:
        if s.count(marker)!=1:raise ValueError('Unexpected clip-hook site')
        s=s.replace(marker,replacement)
    p.write_text(s,encoding='utf8',newline='\n');diagnosticHash=sha(p)
    fonts=[]
    for p,n in ((latin,'latin.ttf'),(cjk,'cjk.otf')):
        target=output/n;shutil.copy2(p,target);fonts.append({'original':str(p),'copy':str(target),'sha256':sha(target)})
    for row in tests:shutil.copy2(BUNDLE/row['path'],output/Path(row['path']).name)
    fixture=output/'paint_cache_all.c'
    clip=(output/'clip_query_test.c').read_text(encoding='utf8')
    split=clip.index('static void clipRenderTests(')
    paint=(output/'paint_cache_test.c').read_text(encoding='utf8')
    marker='    xui_font f=fontLoad(args[2],args[3],20), other=fontLoad(args[2],args[3],24);'
    if paint.count(marker)!=1:raise ValueError('Unexpected native clip test insertion')
    paint=paint.replace(marker,marker+'\n    clipRenderTests(f);')
    paint=paint.replace('    if(argc!=4)return 2;','    if(argc!=4)return 2;\n    clipStateTests();')
    fixture.write_text('#include "'+(engine/'xge.h').as_posix()+'"\n#include <stddef.h>\n'
        'static void* diagPaintRealloc(void*,size_t);\nstatic void* diagPaintCalloc(size_t,size_t);\n'
        'static xge_rect_i_t diagClipPixels(void);\nstatic void diagClipDrawEx(const xge_draw_t*);\n'
        'static int diagClipEmojiDraw(xge_emoji_pack,uint32_t,xge_rect_t,int,int*);\n'+
        '#include "'+(engine/'xge.c').as_posix()+'"\n#include "'+(engine/'src/xui_internal.h').as_posix()+'"\n'+
        (output/'old_clip_renderer.c').read_text(encoding='utf8')+'\n'+clip[:split]+'\n'+
        (output/'old_paint_renderer.c').read_text(encoding='utf8')+'\n'+(output/'paint_cache_proxy.inc').read_text(encoding='utf8')+'\n'+
        'static void clipRenderTests(xui_font);\n'+paint+'\n'+clip[split:],encoding='utf8')
    executable=output/'paint_cache_test.exe'
    args=[str(gcc),'-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-Wno-cast-function-type',
        '-DXGE_DEBUGMODE=0','-DXGE_XRT_PROFILE_IDE','-I'+str(engine),str(fixture),'-lm','-lws2_32','-liphlpapi','-lgdi32',
        '-luser32','-lshell32','-lole32','-loleaut32','-luuid','-limm32','-lwinmm','-lavrt','-o',str(executable)]
    report={'status':'running','root':str(output),'buildReport':str(build_report),'buildReportSha256':sha(build_report),
        'patchManifestSha256':sha(BUNDLE/'patch-manifest.json'),'arguments':args,'testMaterials':tests,'sourceFiles':len(inputs),
        'engineCopy':str(engine),'diagnosticEngineSourceSha256':diagnosticHash,'fonts':fonts,'fixtureSha256':sha(fixture),
        'runtimeOwner':'one executable XRT owner; no SDK DLL linked','allocationHooksInDiagnosticCopy':3,'clipHooksInDiagnosticCopy':3,'sdkInstalled':False,'productionChanged':False}
    pointer=output/'result.json'
    def save():pointer.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    save();u=ctypes.WinDLL('user32',use_last_error=True);u.CreateDesktopW.restype=wintypes.HANDLE
    u.CreateDesktopW.argtypes=[wintypes.LPCWSTR,wintypes.LPCWSTR,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p]
    u.CloseDesktop.argtypes=[wintypes.HANDLE];desktop=None
    try:
        log=output/'strict-link.log'
        with log.open('xb') as f:code=subprocess.call(args,stdout=f,stderr=f)
        report.update(linkExitCode=code,linkLog=str(log),linkLogSha256=sha(log));save()
        if code!=0:raise RuntimeError('Strict fixture compile failed')
        desktopName='XlangPaintReplay-'+uuid.uuid4().hex;desktop=u.CreateDesktopW(desktopName,None,None,0,0x10000000,None)
        if not desktop:raise ctypes.WinError(ctypes.get_last_error())
        report.update(desktop=desktopName,neverSwitchedDesktop=True,executableSha256=sha(executable));save()
        log=output/'native.log';started=time.monotonic()
        with log.open('xb') as f:
            process=NativeDesktopProcess([str(executable),str(output),fonts[0]['copy'],fonts[1]['copy']],f,f,desktopName,cwd=str(output))
            try:report['pid']=process.pid;save();code=process.wait(timeout=120)
            finally:process.close()
        report.update(exitCode=code,elapsedSeconds=round(time.monotonic()-started,3),log=str(log),logSha256=sha(log));save()
        if code!=0:raise RuntimeError('Visible paint comparison failed')
        m=re.search(r'viewport paint comparison passed: (\d+) assertions; (\d+) successful pixel cases; (\d+) matching error cases; (\d+) unchanged long run reuse cases; (\d+) cache-path shape calls; (\d+) maximum cached records;',log.read_text(encoding='utf8'))
        if not m:raise ValueError('Native summary missing')
        fields=('assertions','successfulPixelCases','matchingErrorCases','unchangedLongRunReuseCases','cachePathShapeCalls','maximumCachedRecords')
        values=tuple(map(int,m.groups()))
        if values!=(36434,605,96,216,314,96):raise ValueError('Incomplete comparison')
        report.update(zip(fields,values));report['status']='clip-query-and-paint-equivalence-passed'
        m=re.search(r'clip queries passed: (\d+) assertions; (\d+) exact state queries; (\d+) conversions;',log.read_text(encoding='utf8'))
        if not m:raise ValueError('Clip state summary missing')
        report.update(zip(('clipAssertions','clipStateQueries','clipConversions'),map(int,m.groups())))
        m=re.search(r'clip drawing passed: (\d+) matching pixel cases;',log.read_text(encoding='utf8'))
        if not m or int(m[1])!=24:raise ValueError('Live clip rendering summary missing')
        report['clipMutationPixelCases']=int(m[1]);save()
    except BaseException as error:report.update(status='clip-query-and-paint-equivalence-failed',error=str(error));save();raise
    finally:
        if desktop and not u.CloseDesktop(desktop):raise ctypes.WinError(ctypes.get_last_error())
        verify()
        for relative,digest in inputs.items():
            expected=diagnosticHash if relative=='src/xge_text_run.c' else digest
            if sha(engine/relative)!=expected:raise ValueError('Diagnostic engine changed')
        for row in fonts:
            if sha(Path(row['original']))!=row['sha256'] or sha(Path(row['copy']))!=row['sha256']:raise ValueError('Font material changed')
    print(json.dumps({k:report[k] for k in ('status',*fields,'clipAssertions','clipStateQueries','clipConversions','clipMutationPixelCases')}),flush=True)
def main():
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--gcc',type=Path,default=Path(r'E:\software\w64devkit\bin\gcc.exe'))
    p.add_argument('--latin-font',type=Path,default=Path(r'C:\Windows\Fonts\consola.ttf'));p.add_argument('--cjk-font',type=Path,default=IDE/'data/fonts/NotoSansMonoCJKsc-Regular.otf')
    a=p.parse_args();run(a.build,a.output,a.gcc,a.latin_font,a.cjk_font)
if __name__=='__main__':main()
