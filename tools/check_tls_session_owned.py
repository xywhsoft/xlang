"""Offline managed-session storage, physical ownership and allocation rollback.
Normal authenticated in-memory handshakes are checked separately by the source
compiler. Private synthetic queues here are not a TLS/security validation.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True);p.add_argument('--controls',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();controls=a.controls.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    support=controls/'test/support/tls_session_owned.c';plan=controls/'test/support/tls_session_plan.c'
    paths=[Path(__file__),gcc,tcc,support,plan,root/'src/compile/xl6_compile_unit.c',
        *[q for q in (root/'src').rglob('*') if q.is_file() and q.suffix in ['.h','.inc']],
        root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h']
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in paths},Rows=[],Failures=[],NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False,SyntheticStorageFixture=True)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0] in [str(gcc),str(tcc)]:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True);assert not code,str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(out)]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    planner=out/('planner.exe' if os.name=='nt' else 'planner')
    run('planner.build',[gcc,*flags,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-O0','-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',plan,root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',planner])
    sources=[root/s for s in run('planner.sources',[planner,'--sources']).splitlines()];modules=run('planner.modules',[planner,'--modules'])
    assert not any(s in modules for s in ['_MODULE_ALL','MODULE_TLS_STREAM','MODULE_NET_SOCKET','MODULE_NET_TCP','MODULE_NET_ENGINE'])
    for name in ['MODULE_TLS_SESSION','MODULE_TLS_CLIENT_VERIFY','MODULE_TLS_SERVER','MODULE_TLS_RECORD_AES','MODULE_TLS_RECORD_CHACHA']:assert name in modules
    r['Closure']=dict(Sources=list(map(str,sources)),Modules=modules)
    for source in sources:r['Inputs'][str(source)]=sha(source)
    helper='''
#include <assert.h>
static unsigned fixtureDeaths;
static void fixtureClean(xtlssession* session,ptr role){assert(session&&role);fixtureDeaths++;}
unsigned fixtureSessionDestroyed(void){return fixtureDeaths;}
xtlssession* fixtureSessionCreate(void){
 xtlscontext* context=xrtTlsContextCreate(NULL);assert(context);
 xtlssession* native=__xrtTlsSessionCreateSized(context,NULL,XTLS_CLIENT,1,fixtureClean);
 xrtTlsContextRelease(context);assert(native);return native;
}
void fixtureSessionPlain(xtlssession* session,const void* data,size_t size){
 assert(__xrtTlsSessionSetState(session,XTLS_STATE_HANDSHAKE));
 assert(__xrtTlsSessionSetState(session,XTLS_STATE_READY));
 assert(xrtNetBufAppend(&session->Plain,data,size));
}
void fixtureSessionCipher(xtlssession* session,xnetbytes* data,unsigned count){
 for(unsigned i=0;i<count;i++)assert(xrtNetBufAppendBytes(&session->Send,data,0,3));
}
void fixtureSessionPlainBytes(xtlssession* session,xnetbytes* data,unsigned count){
 assert(__xrtTlsSessionSetState(session,XTLS_STATE_HANDSHAKE));
 assert(__xrtTlsSessionSetState(session,XTLS_STATE_READY));
 for(unsigned i=0;i<count;i++)assert(xrtNetBufAppendBytes(&session->Plain,data,0,3));
}
void fixtureSessionFeedBytes(xtlssession* session,xnetbytes* data){assert(xrtNetBufAppendBytes(&session->Feed,data,0,3));}
void fixtureSessionOpaque(xtlssession* session,void* data,xnetreleaseproc release){assert(xrtNetBufAppendRef(&session->Feed,"abc",3,release,data));}
'''
    impl=out/'native.c';impl.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n')+helper)
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]+([] if os.name=='nt' else [('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'])])
    for name,cc,options in modes:
        try:
            binary=out/(name+('.exe' if os.name=='nt' else ''));run(name+'.build',[cc,*flags,*options,*(['-Wall','-Wextra','-Werror','-fmax-errors=5'] if cc==gcc else []),support,impl,*sources,*libs,'-o',binary])
            assert 'TLS managed session owners:' in run(name+'.run',[binary])
        except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    probe=out/'header.c';probe.write_text('#include "src/runtime/xl6_tls_session_abi.h"\n')
    try:run('header.compile',[gcc,*flags,'-Wall','-Wextra','-Werror','-c',probe,'-o',out/'header.o'])
    except Exception as e:r['Failures'].append(dict(Name='header',Error=repr(e)));save()
    r['Stages']=len(r['Rows']);r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items());r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.iterdir() if q.is_file() and q.name!='results.json'];r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
