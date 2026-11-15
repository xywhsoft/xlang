"""Qualify the committed FlowGraph invalidation fix in real hosted/static SDKs.

This does not publish a safe Graph API or certify arbitrary callback reentry,
command history, code/resource trees or long-lived widget aliases. The language
prototype uses a controlled synchronous adversarial C value and explicit close.
Complete O2 compiler, canonical XRT, real C O0/O2 clients and JIT/AOT are required.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort
from stage_runtime_headers import stage as stage_runtime_headers


DRIVER='''#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include <xui.h>
#include <assert.h>
#include <string.h>
#if defined(XGE_TEST_STATIC)
#include <xge_static_xrt_contract.h>
#else
#include <xge_host_xrt_contract.h>
static xge_host_xrt_proc resolve(void* user,const char* name)
{
 (void)user;
#define XGE_HOST_XRT_SLOT(symbol) if(!strcmp(name,#symbol))return (xge_host_xrt_proc)&symbol;
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
 return NULL;
}
#endif
int fixtureNativeMain(void);
int main(void)
{
#if defined(XGE_TEST_STATIC)
 const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
 assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
 const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
 assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
 assert(xrtMemDebugEnable(true));
 xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
 assert(fixtureNativeMain()==0 && !xrtGetError());
 xrtMemDebugSnapshot(&after);
 assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
 assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount);
 return 0;
}
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in('root','compiler','compiler-report','native-root','engine','static-build','dynamic-packages','static-packages','out'):
        parser.add_argument('--'+key,type=Path,required=True)
    a=parser.parse_args();root,compiler,build,native,engine,static_build,dynamic,static,out=[p.resolve()for p in(a.root,a.compiler,a.compiler_report,a.native_root,a.engine,a.static_build,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    h=json.loads((engine/'build-report.json').read_text());s=json.loads((static_build/'build-report.json').read_text())
    assert h['linked']and h['xrt_mode']=='host'and s['complete']and s['passed']and s['sourceUnchanged']and s['xrt_mode']=='direct-external'
    assert Path(h['source_root']).resolve()==native and Path(s['source_root']).resolve()==native
    runtime=compiler.parent/('xlc.dll'if win else'libxlc.so');library=engine/h['import_library']if win else engine/h['runtime_image'];archive=static_build/s['archive']
    case=root/'test/cases/foreign_xui_flow_config_probe.xl';native_cases=[native/'tests/xui'/p for p in('xui_flow_graph_config_reentry_test.c','xui_flow_graph_test.c')]
    tools={name:Path(shutil.which(name)).resolve()for name in('gcc','g++','objdump')}
    files=[Path(__file__).resolve(),compiler,build,runtime,*extra,*tools.values(),case,case.with_suffix('.h'),case.with_suffix('.c'),*native_cases,native/'src/xui/workflow/xui_flow_graph.c',library,archive,engine/h['runtime_image'],engine/'build-report.json',static_build/'build-report.json',engine/'xge_build_config.h',static_build/'xge_build_config.h',static_build/'xge_static_xrt_contract.h']
    files.extend(compiler.parent/p for p in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    files.extend(Path(__file__).with_name(p+'.py')for p in('foreign_compiler_cohort','full_compiler_qualification','stage_runtime_headers','gen_tcc_vfs_resources'))
    for folder in(sdk,dynamic,static,engine/'host-xrt',native/'include',native/'extlibs/xrt'):
        files.extend(p for p in folder.rglob('*')if p.is_file())
    inputs={str(p):sha(p)for p in files};out.mkdir(parents=True,exist_ok=False);scratch=out/'tmp';scratch.mkdir();snap=out/'input-snapshots';snap.mkdir()
    for p in(case,case.with_suffix('.h'),case.with_suffix('.c'),*native_cases,Path(__file__).resolve()):shutil.copy2(p,snap/p.name)
    frozen=Path(json.loads(build.read_text())['source']);fresh=out/'runtime-sdk'
    headers=stage_runtime_headers(frozen,fresh,frozen/'tools'/('tcc_vfs_manifest_win64_xlc.txt'if win else'tcc_vfs_manifest_linux_xlc.txt'))
    for p in fresh.rglob('*'):
        if p.is_file():inputs[str(p)]=sha(p)
    driver=out/'driver.c';driver.write_text(DRIVER,encoding='utf8')
    report=dict(complete=False,passed=False,shippingDelivery=False,scope=__doc__,fullCompilerBuild=True,fullNativeRebuild=False,networkTests=False,defaultGraphApiAdded=False,inputs=inputs,rows=[],programs=[],stagedPrivateHeaders=len(headers))
    def save():
        temp=out/'results.json.tmp';temp.write_text(json.dumps(report,indent=2)+'\n');os.replace(temp,out/'results.json')
    def run(name,command,packages=dynamic):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages),TEMP=str(scratch),TMP=str(scratch),TMPDIR=str(scratch))
        key='PATH'if win else'LD_LIBRARY_PATH';env[key]=os.pathsep.join((str(packages/'_xge_native/native'),str(engine),str(compiler.parent),env.get(key,'')))
        startup=None
        if win:startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        def no_core():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        command=list(map(str,command));start=time.monotonic();z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300,startupinfo=startup,preexec_fn=None if win else no_core)
        data=z.stdout+z.stderr;log=out/(name+'.log');log.write_bytes(data)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,passed=not z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log;return data
    ci=['-I'+str(p)for p in(fresh/'include',root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    includes=['--inc-path',root,'--inc-path',fresh/'include','--inc-path',sdk,'--inc-path',native]
    try:
        for source in native_cases:
            body=out/(source.stem+'-body.c');text=source.read_text();assert text.count('int main(void)')==1
            body.write_text('#define XRT_MODULE_ALL\n'+text.replace('int main(void)','int fixtureNativeMain(void)'),encoding='utf8')
            for kind,folder,lib in(('host',engine,library),('static',static_build,archive)):
                flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-Wno-cast-function-type','-DXGE_XRT_HOST=1','-include',folder/'xge_build_config.h',*ci,'-I'+str(folder),'-I'+str(engine/'host-xrt')]
                flags+=['-DXGE_TEST_STATIC=1','-DXL6_XGE_STATIC=1']if kind=='static'else['-DXGE_DLL=1','-DXUI_DLL=1']if win else[]
                systems=['-l'+n for n in s['system_libraries']]if kind=='static'else[]
                if not win:systems+=['-Wl,-rpath,'+str(engine),'-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
                for opt in('O0','O2'):
                    name=source.stem+'-'+kind+'-'+opt;exe=out/(name+('.exe'if win else''))
                    run(name+'-build',[tools['gcc'],*flags,'-'+opt,body,driver,lib,runtime,*systems,'-o',exe])
                    assert b'passed'in run(name,[exe])
        generated=out/'prototype.c';public=out/'prototype.h'
        run('prototype-emit',[compiler,'cgen',case,'--emit',generated,'--emit-header',public,*includes])
        code=generated.read_text(encoding='utf-8-sig')
        for symbol in('xuiFlowGraphCreate','xuiFlowGraphAddNode','xuiFlowGraphSetNodeConfig','xuiFlowGraphGetNodeConfig'):
            assert re.search(r'\b'+symbol+r'\(',code),symbol
        for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('prototype-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*ci,'-x',language,'-fsyntax-only',public])
        for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
            if kind=='jit':data=run('prototype-jit',[compiler,'run',case,*includes],packages)
            else:
                exe=out/('prototype-'+kind+('.exe'if win else''));run('prototype-'+kind+'-build',[compiler,'build',case,'-o',exe,*includes],packages);data=run('prototype-'+kind+'-execute',[exe],packages)
                table=run('prototype-'+kind+'-imports',[tools['objdump'],'-p',exe],packages);imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={v.lower()for v in imports}
                assert not{b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                assert any(b'xge3_host'in p for p in lower)==(kind=='dynamic')
                report['programs'].append(dict(path=str(exe),sha256=sha(exe),imports=[p.decode()for p in imports]))
            assert b'Flow config SDK: direct sized input'in data and b'checked clone invalidation passed'in data
        assert len(report['rows'])==27
        report.update(complete=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
