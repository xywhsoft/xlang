"""Replay full-engine layout and DPI-provider comparisons on a private SDK35 build."""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import time
import uuid

from native_desktop import NativeDesktopProcess
from prepare_large_source_sdk import ancestors, name, plain, read, sha

IDE = Path(__file__).resolve().parents[1]
BUNDLE = IDE / 'third_party/sdk_patches/large-source-experiment-v13'

def run(build_report, output, gcc, latin_font, cjk_font):
    build_report = build_report.absolute()
    output = output.absolute()
    build = read(build_report)
    if build['status'] != 'sdk-build-finished' or build['exitCode'] != 0 or not build['wholeSdkWerror']:
        raise ValueError('A completed strict private SDK build is required')
    source = Path(build['source']).absolute()
    ancestors(source)
    ancestors(BUNDLE)
    ancestors(output.parent)
    if output.exists() or output.is_relative_to(source) or output.is_relative_to(BUNDLE):
        raise ValueError('Use a new output directory outside both input trees')
    manifest = read(BUNDLE/'patch-manifest.json')
    descriptor = BUNDLE/'source-materials-before.json'
    if not manifest['privateOnly'] or sha(descriptor) != manifest['beforeDescriptorSha256']:
        raise ValueError('Unexpected private patch descriptor')
    materials = {row['path']: row['sha256'] for row in read(descriptor)['files']}
    materials.update({row['path']: row['afterSha256'] for row in manifest['changes']})
    if len(materials) != 238:
        raise ValueError('Unexpected engine material count')
    tests = manifest['layoutStreamTestMaterials']
    wanted = {'layout_old_shape.c','layout_old_layout.c','layout_shape_test.c',
              'layout_stream_checks.c','layout_widget_checks.c','layout_engine_comparison.c'}
    if len(tests) != len(wanted) or {Path(row['path']).name for row in tests} != wanted:
        raise ValueError('Unexpected comparison fixture inventory')
    app = [r for r in manifest['appChanges'] if r['path'] in ('src/platform/dpi_proxy.c','src/platform/dpi_proxy.h')]
    if len(app) != 2:
        raise ValueError('Unexpected DPI adapter inventory')

    def verify():
        for relative, digest in materials.items():
            p = source/name(relative)
            ancestors(p.parent)
            plain(p)
            if sha(p) != digest:
                raise ValueError('Private source differs: '+relative)
        for row in tests:
            p = BUNDLE/name(row['path']); plain(p)
            if sha(p) != row['sha256']:
                raise ValueError('Test material differs: '+row['path'])
        for row in app:
            p = BUNDLE/name(row['material']); plain(p)
            if sha(p) != row['afterSha256']:
                raise ValueError('DPI material differs: '+row['path'])
        for row in build['outputs']:
            p = Path(row['path']); plain(p)
            if sha(p) != row['sha256']:
                raise ValueError('Private build output differs: '+str(p))

    if latin_font is None:
        latin_font = Path(os.environ['WINDIR'])/'Fonts/consola.ttf'
    font_inputs = [(Path(latin_font).absolute(),'latin.ttf'),(Path(cjk_font).absolute(),'cjk.otf')]
    font_hashes = {}
    for p, _ in font_inputs:
        ancestors(p.parent)
        info = p.lstat()
        # Installed Windows fonts may have read-only WinSxS hard links.
        if not stat.S_ISREG(info.st_mode) or info.st_file_attributes & 0x400:
            raise ValueError('Unexpected runtime font path: '+str(p))
        font_hashes[p] = sha(p)
    verify()
    output.mkdir()
    engine = output/'engine'; engine.mkdir()
    for relative, digest in materials.items():
        target = engine/relative; target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source/relative,target)
        if sha(target) != digest:
            raise ValueError('Source changed while copying')
    for row in tests:
        shutil.copy2(BUNDLE/row['path'],output/Path(row['path']).name)
    for row in app:
        target = output/'app'/row['path']; target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(BUNDLE/row['material'],target)
    fonts = []
    for p, filename in font_inputs:
        target = output/filename; shutil.copy2(p,target)
        if sha(target) != font_hashes[p]:
            raise ValueError('Font changed while copying')
        fonts.append(dict(original=str(p),copy=str(target),sha256=font_hashes[p]))
    inventory_command = [os.environ['COMSPEC'],'/d','/c','call xui_sources.bat >nul && set XUI_SRC']
    inventory = subprocess.run(inventory_command,cwd=engine,capture_output=True,check=True)
    lines = [s for s in inventory.stdout.decode().splitlines() if s.startswith('XUI_SRC=')]
    if len(lines) != 1:
        raise ValueError('Unexpected XUI build inventory')
    modules = [engine/Path(p) for p in lines[0].split('=',1)[1].split()]
    if len(modules) != len(set(modules)) or not all(p.is_file() and p.absolute().is_relative_to(engine) for p in modules):
        raise ValueError('Invalid XUI source inventory')
    modules = [p for p in modules if p.name not in ('xui_proxy_xge.c','xui_code_edit.c')]
    executable = output/'layout_stream_test.exe'
    arguments = [str(gcc),'-O2','--param=ggc-min-expand=5','--param=ggc-min-heapsize=8192',
                 '-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-Wno-cast-function-type',
                 '-DXGE_DEBUGMODE=0','-DXGE_XRT_PROFILE_IDE','-I'+str(engine),'-I'+str(engine/'lib/xrt'),
                 '-I'+str(output/'app/src'),'-I'+str(IDE/'src'),str(output/'layout_engine_comparison.c'),
                 *map(str,modules),str(output/'app/src/platform/dpi_proxy.c'),'-lm','-lws2_32','-liphlpapi','-lgdi32',
                 '-luser32','-lshell32','-lole32','-loleaut32','-luuid','-limm32','-lwinmm','-lavrt','-o',str(executable)]
    report = dict(status='compiling',root=str(output),buildReport=str(build_report),buildReportSha256=sha(build_report),
                  patchManifestSha256=sha(BUNDLE/'patch-manifest.json'),arguments=arguments,engineCopy=str(engine),
                  sourceFiles=len(materials),separateXuiSources=len(modules),testMaterials=tests,fonts=fonts,
                  fixtureSha256=sha(output/'layout_engine_comparison.c'),oneXrtOwner='one executable; full XGE/XUI; no SDK DLL',
                  sdkInstalled=False,appInstalled=False,productionChanged=False)
    pointer = output/'result.json'
    def save():
        pointer.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    save()
    user32 = ctypes.WinDLL('user32',use_last_error=True)
    user32.CreateDesktopW.restype = wintypes.HANDLE
    user32.CreateDesktopW.argtypes = [wintypes.LPCWSTR,wintypes.LPCWSTR,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p]
    user32.CloseDesktop.argtypes = [wintypes.HANDLE]
    desktop = None
    try:
        link_log = output/'actual-strict.log'; started=time.monotonic()
        with link_log.open('xb') as f:
            process = subprocess.Popen(arguments,stdout=f,stderr=f)
            report['compilerPid']=process.pid;save()
            report['linkExitCode']=process.wait(timeout=360)
        report.update(linkLog=str(link_log),linkLogSha256=sha(link_log),compileElapsedSeconds=round(time.monotonic()-started,3));save()
        if report['linkExitCode'] != 0:
            raise RuntimeError('Strict whole-engine comparison compile failed')
        desktop_name = 'XlangLayoutStream-'+uuid.uuid4().hex
        desktop = user32.CreateDesktopW(desktop_name,None,None,0,0x10000000,None)
        if not desktop:
            raise ctypes.WinError(ctypes.get_last_error())
        report.update(status='running-native',desktop=desktop_name,neverSwitchedDesktop=True,executableSha256=sha(executable));save()
        log = output/'actual-native.log'; started=time.monotonic()
        with log.open('xb') as f:
            process = NativeDesktopProcess([str(executable),str(output),fonts[0]['copy'],fonts[1]['copy']],f,f,desktop_name,cwd=str(output))
            report['pid']=process.pid;save()
            try:
                report['exitCode']=process.wait(timeout=120)
            finally:
                process.close()
        report.update(log=str(log),logSha256=sha(log),elapsedSeconds=round(time.monotonic()-started,3));save()
        if report['exitCode'] != 0:
            raise RuntimeError('Native layout comparison failed')
        text=log.read_text(encoding='utf8')
        patterns=[
            (r'stream layout comparison passed: (\d+) position assertions; (\d+) position cases; (\d+) positions; (\d+) callback failures; (\d+) layout assertions; (\d+) layout cases; (\d+) proxy cases',
             ('streamAssertions','streamPositionCases','streamPositions','streamErrorCases','layoutAssertions','layoutCases','proxyCases')),
            (r'shape lookup comparison passed: (\d+) assertions; (\d+) complete shape cases; (\d+) scalar lookup checks; (\d+) old/new pixel cases;',
             ('shapeAssertions','shapeCases','lookupCases','pixelCases')),
            (r'stream widget lifecycle passed: (\d+) assertions; (\d+) cases',('widgetAssertions','widgetCases')),
        ]
        for pattern,fields in patterns:
            match=re.search(pattern,text)
            if not match:
                raise RuntimeError('Incomplete native fixture counters')
            report.update(zip(fields,map(int,match.groups())))
        for field,value in manifest['expectedLayoutCounters'].items():
            if report[field] != value:
                raise RuntimeError('Unexpected native fixture counter: '+field)
        if report['widgetAssertions'] < 250:
            raise RuntimeError('Incomplete widget lifecycle checks')
        report['status']='layout-stream-geometry-and-provider-lifecycle-passed'
        print(text,end='')
        return report
    except Exception as error:
        report.update(status='layout-stream-comparison-failed',error=str(error))
        raise
    finally:
        if desktop and not user32.CloseDesktop(desktop):
            raise ctypes.WinError(ctypes.get_last_error())
        verify()
        for relative,digest in materials.items():
            if sha(engine/relative) != digest:
                raise ValueError('Comparison source was modified')
        for p,_ in font_inputs:
            if sha(p) != font_hashes[p]:
                raise ValueError('Font input changed')
        report['inputsUnchanged']=True;save()

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-report',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--gcc',type=Path,default=Path(r'E:\software\w64devkit\bin\gcc.exe'))
    parser.add_argument('--latin-font',type=Path)
    parser.add_argument('--cjk-font',type=Path,default=IDE/'data/fonts/NotoSansMonoCJKsc-Regular.otf')
    args=parser.parse_args()
    run(args.build_report,args.output,args.gcc,args.latin_font,args.cjk_font)
