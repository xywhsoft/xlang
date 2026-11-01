"""Check a hosted XGE DLL against an actual xlang runtime (offline CPU only)."""
from pathlib import Path
import argparse, ctypes, hashlib, json, os, shutil, subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--engine-root',type=Path,required=True)
    p.add_argument('--engine-build',type=Path,required=True)
    p.add_argument('--runtime',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a = p.parse_args()
    assert os.name == 'nt', 'This delivery gate currently targets Windows DLLs'
    root, upstream, build, runtime, out = [x.resolve() for x in (a.root,a.engine_root,a.engine_build,a.runtime,a.out)]
    out.mkdir(parents=True,exist_ok=False); (out/'tmp').mkdir()
    def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
    header = root/'lib/xrt/xrt.h'
    canonical = hashlib.sha256(header.read_bytes().replace(b'\r\n',b'\n')).hexdigest()
    description = json.loads((build/'build-report.json').read_text())
    assert description['xrt_mode'] == 'host' and description['xrt_host_header_sha256'] == canonical
    native = build/'xge.dll'
    inputs = {str(path):sha(path) for path in [Path(__file__).resolve(),header,runtime,native,
        build/'build-report.json',upstream/'include/xge_host_xrt.h',build/'host-xrt/xge_host_xrt_contract.h',
        build/'host-xrt/xge_host_xrt_fingerprint.h']}
    source = out/'host_contract.c'
    source.write_bytes(b'''#define XRT_MODULE_ALL
#include "lib/xrt/xrt.h"
#include "xge_host_xrt_contract.h"
static const xge_host_xrt_contract_t contract = XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
static xmemdebugsnapshot baseline;
__declspec(dllexport) const xge_host_xrt_contract_t* hostContract(void) { return &contract; }
__declspec(dllexport) int hostBegin(void) {
 xrtMemDebugSnapshot(&baseline); return baseline.Enabled;
}
__declspec(dllexport) int hostObservedAllocation(void) {
 xmemdebugsnapshot now; xrtMemDebugSnapshot(&now);
 return now.LiveCount > baseline.LiveCount && now.LiveBytes > baseline.LiveBytes;
}
__declspec(dllexport) int hostEnd(void) {
 xmemdebugsnapshot now; xrtClearError(); xrtMemDebugSnapshot(&now);
 return now.LiveCount == baseline.LiveCount && now.LiveBytes == baseline.LiveBytes &&
 now.InvalidFreeCount == baseline.InvalidFreeCount && now.DoubleFreeCount == baseline.DoubleFreeCount;
}
''')
    helper = out/'host-contract.dll'
    command = [shutil.which('gcc'),'-std=c11','-Wall','-Wextra','-Werror','-O2','-shared',
        '-I'+str(root),'-I'+str(upstream/'include'),'-I'+str(build/'host-xrt'),str(source),str(runtime),'-o',str(helper)]
    env = dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'))
    process = subprocess.run(command,env=env,capture_output=True,timeout=120)
    (out/'build.log').write_bytes(process.stdout+process.stderr)
    assert process.returncode == 0,out/'build.log'
    # Keep runtime, engine, resolver and helper alive throughout native calls.
    search = os.add_dll_directory(str(runtime.parent))
    host = ctypes.CDLL(str(runtime)); engine = ctypes.CDLL(str(native)); provider = ctypes.CDLL(str(helper))
    class Contract(ctypes.Structure):
        _fields_ = [('size',ctypes.c_uint32),('revision',ctypes.c_uint32),('dataModel',ctypes.c_uint64),
            ('coreLayout',ctypes.c_uint64),('headerSha256',ctypes.c_char_p)]
    provider.hostContract.restype = ctypes.POINTER(Contract)
    contract = provider.hostContract()
    assert contract.contents.headerSha256.decode() == canonical
    missing_names = []
    Resolve = ctypes.CFUNCTYPE(ctypes.c_void_p,ctypes.c_void_p,ctypes.c_char_p)
    @Resolve
    def resolve(user,name):
        try: return ctypes.cast(getattr(host,name.decode()),ctypes.c_void_p).value
        except AttributeError:
            missing_names.append(name.decode()); return None
    engine.xgeHostXrtBind.argtypes = [ctypes.POINTER(Contract),Resolve,ctypes.c_void_p,ctypes.POINTER(ctypes.c_char_p)]
    engine.xgeHostXrtBind.restype = ctypes.c_int
    missing = ctypes.c_char_p()
    status = engine.xgeHostXrtBind(contract,resolve,None,ctypes.byref(missing))
    assert status == 0,(status,missing.value,missing_names)
    engine.xgeHostXrtRuntimeIdentity.restype = ctypes.c_void_p
    identity = engine.xgeHostXrtRuntimeIdentity()
    assert identity == ctypes.cast(host.xrtVersion,ctypes.c_void_p).value
    engine.xuiCreate.argtypes = [ctypes.POINTER(ctypes.c_void_p)]; engine.xuiCreate.restype = ctypes.c_int
    engine.xuiWidgetCreate.argtypes = [ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
    engine.xuiWidgetCreate.restype = ctypes.c_int
    engine.xuiDestroy.argtypes = [ctypes.c_void_p]; engine.xuiDestroy.restype = None
    assert provider.hostBegin(), 'Host memory-debug observation must be enabled'
    for iteration in range(3):
        context,widget = ctypes.c_void_p(),ctypes.c_void_p()
        assert engine.xuiCreate(ctypes.byref(context)) == 0 and context.value
        try:
            assert engine.xuiWidgetCreate(context,ctypes.byref(widget)) == 0 and widget.value
            assert provider.hostObservedAllocation(), 'Native allocations did not reach the actual host runtime'
        finally: engine.xuiDestroy(context)
    assert provider.hostEnd(), 'Native context shutdown left live/invalid/double allocations in host'
    assert engine.xgeHostXrtBind(contract,resolve,None,ctypes.byref(missing)) == 0
    signatures = {
        'tcc_new':([],ctypes.c_void_p),'tcc_delete':([ctypes.c_void_p],None),
        'tcc_set_lib_path':([ctypes.c_void_p,ctypes.c_char_p],None),
        'tcc_set_options':([ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),
        'tcc_set_output_type':([ctypes.c_void_p,ctypes.c_int],ctypes.c_int),
        'tcc_add_symbol':([ctypes.c_void_p,ctypes.c_char_p,ctypes.c_void_p],ctypes.c_int),
        'tcc_compile_string':([ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),
        'tcc_relocate':([ctypes.c_void_p],ctypes.c_int),
        'tcc_get_symbol':([ctypes.c_void_p,ctypes.c_char_p],ctypes.c_void_p)}
    for name,(arguments,result) in signatures.items():
        fn=getattr(host,name); fn.argtypes,fn.restype=arguments,result
    state=host.tcc_new(); assert state
    try:
        host.tcc_set_lib_path(state,str(root/'res/tcc/win64/x86_64').encode())
        assert host.tcc_set_output_type(state,1)==0 and host.tcc_set_options(state,b'-std=c11 -nostdlib')==0
        for name in ('xuiCreate','xuiWidgetCreate','xuiDestroy'):
            assert host.tcc_add_symbol(state,name.encode(),ctypes.cast(getattr(engine,name),ctypes.c_void_p))==0
        code=b'''extern int xuiCreate(void**); extern int xuiWidgetCreate(void*,void**);
extern void xuiDestroy(void*);
int exercise(void) {
 void* context=0; void* widget=0;
 int status=xuiCreate(&context); if(status || !context) return 1;
 status=xuiWidgetCreate(context,&widget); xuiDestroy(context);
 return status || !widget;
}'''
        (out/'tcc-exercise.c').write_bytes(code)
        assert host.tcc_compile_string(state,code)==0 and host.tcc_relocate(state)==0
        address=host.tcc_get_symbol(state,b'exercise'); assert address
        assert provider.hostBegin()
        assert ctypes.CFUNCTYPE(ctypes.c_int)(address)()==0
        assert provider.hostEnd()
    finally: host.tcc_delete(state)
    unchanged = all(sha(Path(path))==digest for path,digest in inputs.items())
    assert unchanged
    report = dict(complete=True,passed=True,networkTests=False,inputsUnchanged=unchanged,
        runtime=str(runtime),runtimeSha256=inputs[str(runtime)],nativeSha256=inputs[str(native)],
        xrtHeaderSha256=canonical,requiredSymbols=len(description['xrt_host_imports']),
        runtimeFunctionIdentityEqual=True,allocationObservation='actual runtime DLL',
        contextCycles=4,tccJitNativeCalls=True,cleanupBalanced=True,missingSymbols=missing_names,inputs=inputs,command=command)
    (out/'results.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    print(json.dumps({k:v for k,v in report.items() if k not in ('inputs','command')},indent=2))


if __name__ == '__main__': main()
