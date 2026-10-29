"""Actual pinned core/runtime + both declaration headers; ordinary native owners.

Reuse the committed XRT ownership fixture unchanged apart from include routing
and explicit declaration routing. No network/security/malformed crypto.
This does not accept public language callbacks or a complete TLS consumer graph.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--native-root',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--compiler',action='append',required=True)
    args=parser.parse_args();root=args.root.resolve();native=args.native_root.resolve();out=args.out.resolve()
    assert not out.exists();out.mkdir(parents=True)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    compilers=[Path(shutil.which(p) or p).resolve() for p in args.compiler]
    fixture=native/'tests/tls/test_tls_verify_ownership.c';vectors=native/'tests/fixtures/x509_path_legacy.h'
    inputs=[Path(__file__).resolve(),*compilers,fixture,vectors,root/'lib/xrt/backports/tls_verifier_ownership.json',
            *[root/p for p in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h']]]
    report=dict(Complete=False,Inputs={str(p):sha(p) for p in inputs},Rows=[],Failures=[],
                SourceFixtureAdaptations=['declaration/include routing','assert native milliseconds'],
                NetworkTraffic=False,SecurityTests=False,PublicLanguageCallbacksAccepted=False,FullTlsAccepted=False,FullGoalComplete=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    text=fixture.read_text(encoding='utf8')
    prefix='''#if defined(XL6_TEST_XRUNTIME_DECL)
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#else
#include "include/xrt_decl.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void testRequire(bool ok, const char* message)
{ if (!ok) { fprintf(stderr,"[FAIL] %s\\n",message); exit(1); } }
'''
    assert text.count('#include "../test.h"')==1 and text.count('#include "../fixtures/x509_path_legacy.h"')==1
    text=text.replace('#include "../test.h"',prefix).replace('#include "../fixtures/x509_path_legacy.h"','#include "owned_path_vectors.h"')
    entry='int main(void)\n{';assert text.count(entry)==1
    text=text.replace(entry,entry+'\n    xtime at; testRequire(xrtDateTime(2030,1,1,0,0,0,0,&at) && at==INT64_C(64029052800000), "Gregorian millisecond ABI");')
    case=out/'owned.c';case.write_text(text,encoding='utf8')
    data=vectors.read_text(encoding='utf8');assert data.count('#include <xrt/core.h>')==1
    (out/'owned_path_vectors.h').write_text(data.replace('#include <xrt/core.h>','/* Core declarations supplied by the actual pinned declaration header. */'),encoding='utf8')
    def run(name,command,instrumented=False):
        command=list(map(str,command));invocation=command
        if Path(command[0]) in compilers:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+p.replace('\\','\\\\').replace('"','\\"')+'"' for p in command[1:]),encoding='utf8')
            invocation=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        start=time.monotonic()
        with log.open('wb') as stream:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=300).returncode
        report['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name,code,flush=True);assert code==0,(name,str(log))
        return log.read_text(encoding='utf8',errors='replace')
    modules=''.join('#define XRT_MODULE_'+p+'\n' for p in ['TLS_VERIFY','X509_VERIFY_RSA','THREAD','MEMORY_DEBUG'])
    for runtime in [False,True]:
        product='runtime' if runtime else 'core';impl=out/(product+'.c')
        impl.write_text(modules+('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if runtime else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'),encoding='utf8')
        for cc in compilers:
            tcc=cc.stem.lower()=='tcc'
            modes=[('tcc',[])] if tcc else [('gcc-O0',['-O0']),('gcc-O2',['-O2'])]
            if not tcc and os.name!='nt':modes.append(('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for name,options in modes:
                prefix=product+'.'+name
                flags=['-std=c11','-Wall','-Werror',*options,'-I'+str(root/'release'),'-I'+str(root),'-I'+str(out)]
                if not tcc:flags+=['-Wextra']
                if os.name!='nt':flags+=['-D_GNU_SOURCE']
                obj=out/(prefix+'.o')
                try:
                    run(prefix+'.implementation',[cc,*flags,'-c',impl,'-o',obj])
                    for xrdecl in [False,True]:
                        label=prefix+('.xrdecl' if xrdecl else '.xdecl');binary=out/(label+('.exe' if os.name=='nt' else ''))
                        libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
                        run(label+'.build',[cc,*flags,*(['-DXL6_TEST_XRUNTIME_DECL'] if xrdecl else []),case,obj,*libs,'-o',binary])
                        assert 'TLS verifier ownership: 5 allocation failures' in run(label+'.run',[binary],name=='asan')
                except (AssertionError,OSError,subprocess.TimeoutExpired) as error:
                    report['Failures'].append(dict(Name=prefix,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(p)==digest for p,digest in report['Inputs'].items())
    report['Products']={str(p):sha(p) for p in out.iterdir() if p.is_file() and p.name!='results.json'}
    report['Complete']=not report['Failures'] and report['InputsUnchanged'];save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
