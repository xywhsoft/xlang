"""Actual resident TLS ownership catalog over the ordinary pinned Dial fixture.

The fixture's resolver uses the real catalog. Its pinned-node driver also
honors the catalog's three-state completion descriptors after all ordinary
Finish calls, rather than treating BUSY as a bool failure. Native operations,
discovery, references, callbacks and OOM checks are unchanged. Explicit
the current native millisecond ABI is used directly.
No external endpoints, DNS, malformed traffic or security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time,sys

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True)
    p.add_argument('--native-root',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();root=a.root.resolve();native=a.native_root.resolve();out=a.out.resolve()
    assert not out.exists();out.mkdir(parents=True)
    compilers=[Path(shutil.which(s) or s).resolve() for s in a.compiler]
    gcc=next(c for c in compilers if c.stem!='tcc')
    sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
    fixtureNames=['tls_dial_future_ownership','tls_listener_ownership','tls_stream_future_ownership']
    inputs={Path(__file__).resolve(),*compilers,*[native/('tests/tls/test_'+name+'.c') for name in fixtureNames],native/'tools/build.py',native/'config/modules.json'}
    inputs.update(q for folder in ['src','release/include','lib/xrt','lib/xruntime'] for q in (root/folder).rglob('*') if q.is_file() and q.suffix in ['.h','.c','.inc'])
    inputs.update(q for q in (native/'tests/fixtures').rglob('*.h'))
    inputs.update([native/'tests/test.h',root/'test/support/tls_native_domains_plan.c'])
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(inputs)},Rows=[],Products=[],Failures=[],Plans={},
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,MalformedCryptoTests=False,
        OriginalTimeAbiPreserved=True,ActualResidentCatalog=True,PublicLanguageApiAccepted=False,FullTlsAccepted=False,ShippingAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def product(q):r['Products'].append(dict(Path=str(q),Sha256=sha(q)));save()
    def write(q,text):q.parent.mkdir(parents=True,exist_ok=True);q.write_text(text,encoding='utf8');product(q)
    def run(name,command,instrumented=False):
        command=list(map(str,command));args=command
        if Path(command[0]) in compilers:
            rsp=out/(name+'.rsp');write(rsp,'\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n');args=[command[0],'@'+str(rsp)]
        env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(args,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name,code,flush=True);assert not code,str(log)
        return log.read_text(encoding='utf8',errors='replace')
    def flags(cc,opt):return [cc,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*opt,*(['-std=c11'] if cc.stem!='tcc' else []),
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    try:
        ext='.exe' if os.name=='nt' else ''
        for opt in ['O0','O2']:
            plan=out/('plan.'+opt+ext)
            run('plan.'+opt+'.build',flags(gcc,['-'+opt,'-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables'])+
                [root/'test/support/tls_native_domains_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',plan])
            product(plan);text=run('plan.'+opt+'.run',[plan]);assert '8 independent exact closures' in text
        sources=set();domains=set();modules=set()
        for kind in range(8):
            row={}
            for field in ['sources','domains','modules']:
                text=run('plan.'+str(kind)+'.'+field,[plan,'--'+field,str(kind)]);row[field]=text.splitlines()
            r['Plans'][str(kind)]=row;sources.update(row['sources']);domains.update(row['domains']);modules.update(row['modules']);save()
        # Inspect the real XRT selector closure, not only resident IDs.
        features=['TLS_STREAM','TLS_STREAM_FUTURE','TLS_STREAM_LISTENER','TLS_STREAM_LISTENER_FUTURE',
            'TLS_STREAM_DIAL','TLS_STREAM_DIAL_FUTURE','TLS_STREAM_DIAL_PROXY','NET_TCP_DIAL_FUTURE','NET_PROXY_DIAL_FUTURE']
        for kind in range(8):
            expected={0}
            if kind==1:expected.add(1)
            if kind in [2,3]:expected.add(2)
            if kind==3:expected.add(3)
            if kind>=4:expected.add(4)
            if kind in [5,7]:expected.add(5)
            if kind in [6,7]:expected.add(6)
            text='\n'.join(r['Plans'][str(kind)]['modules'])+'\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n'
            for index,feature in enumerate(features):
                text+=('#if !defined(' if index in expected else '#if defined(')+'XRT_FEATURE_'+feature+')\n#error unexpected_'+feature+'\n#endif\n'
            text+='int main(void){return 0;}\n';source=out/('selector.'+str(kind)+'.c');write(source,text)
            for i,cc in enumerate(compilers):
                obj=out/('selector.'+str(kind)+'.'+str(i)+'.o')
                run('selector.'+str(kind)+'.'+str(i),flags(cc,[])+['-c',source,'-o',obj]);product(obj)
        assert all((root/q).is_file() for q in sources)
        # Native test-only context is the sole additional admission domain.
        # Production TLS/transitive nodes go through the actual sealed catalog.
        sys.path.insert(0,str(native/'tools'));from build import _load_modules,platform_name
        generated=[]
        for fixtureName in fixtureNames:
            fixture=native/('tests/tls/test_'+fixtureName+'.c');body=fixture.read_text(encoding='utf8')
            # Discard only the fixture's optional native-amalgamation prefix;
            # this TU already includes the explicitly pinned implementation.
            body=body[body.index('#include "../fixtures/tls_server.h"'):]
            body=re.sub(r'#ifn?def TLS_(?:LISTENER|STREAM)_OWNERSHIP_SINGLE\n#include "../../src/internal/xrt_tls_stream.h"\n#endif\n','',body)
            begin=body.index('static const xrtownershipadapterv1* resolve(');end=body.index('\nstatic bool admit(',begin)
            original=body[begin:end];assert original.count('static const xrtownershipadapterv1* resolve(')==1
            if fixtureName=='tls_dial_future_ownership':
                operations,adapter='contextOps','contextAdapter'
                loads=[('TlsStreamEvents','pullPolicy'),('TlsStreamEvents','pushPolicy'),('TlsListenerEvents','listenerPolicy'),('ListenerEvents','plainPolicy')]
                expected='TLS Dial Future ownership:'
            elif fixtureName=='tls_listener_ownership':
                operations,adapter='contextOps','contextAdapter'
                loads=[('TlsStreamEvents','clientPolicy'),('TlsListenerEvents','pullPolicy'),('TlsListenerEvents','pushPolicy')]
                expected='TLS Listener ownership:'
            else:
                operations,adapter='ownerOps','ownerAdapter'
                loads=[('TlsStreamEvents','streamPolicy')]
                expected='TLS Future ownership:'
            bridge='''
#include "src/runtime/xl6_ownership_domains.h"
static XL6_OwnershipCatalog* testCatalog;
static struct {xrtownershipref ref;const XL6_OwnershipFinishV1* completion;bool done;} testCompletions[256];
static size_t testCompletionCount;
static bool testCompletion(xrtownershipref ref){
    for(size_t i=0;i<testCompletionCount;i++)if(testCompletions[i].ref.Data==ref.Data&&testCompletions[i].ref.Ops==ref.Ops)return true;
    return false;
}
static void testFinishCompletions(const void* token){
    XL6_DeadlineV1 due=xl6DeadlineAfterMillisecondsV1(UINT64_C(10000));
    for(;;){bool all=true;
        for(size_t i=0;i<testCompletionCount;i++)if(!testCompletions[i].done){
            xrtownershipfinishresult state=testCompletions[i].completion->Step(testCompletions[i].ref.Data,token);
            CHECK(state==XRT_OWNERSHIP_FINISH_READY||state==XRT_OWNERSHIP_FINISH_BUSY);
            if(state==XRT_OWNERSHIP_FINISH_READY){testCompletions[i].done=true;
                CHECK(testCompletions[i].completion->Step(testCompletions[i].ref.Data,token)==XRT_OWNERSHIP_FINISH_READY);
            }else all=false;
        }
        if(all)return;
        CHECK(!xl6DeadlineExpiredV1(due));xrtThreadYield();
    }
}
static bool fixtureLoad(XL6_OwnershipCatalog* c){
    return '''+'&&\n        '.join('xl6OwnershipCatalogAdd'+group+'V1(c,&'+policy+')' for group,policy in loads)+''';
}
static bool fixtureResolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion){
    (void)policies;(void)preparation;(void)completion;
    if(ref.Ops!=&'''+operations+''')return false;
    *adapter=&'''+adapter+''';return true;
}
static XL6_OwnershipCatalog* testCreateCatalog(void){
    static const XL6_OwnershipDomainV1 fixtureDomain={sizeof(fixtureDomain),fixtureLoad,fixtureResolve,NULL};
    const XL6_OwnershipDomainV1* domains[]={&fixtureDomain,
'''+''.join(q+'\n' for q in sorted(domains))+'''};
    return xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,const xrtownershippreparationv1** preparation){
    const xrtownershipadapterv1* adapter=NULL;const XL6_OwnershipFinishV1* completion=NULL;
    bool admitted=xl6OwnershipCatalogResolveV1(ref,testCatalog,&adapter,preparation,&completion);
    return admitted?adapter:NULL;
}
static void testRegisterPinnedCompletion(xrtownershipref ref,const xrtownershipadapterv1* selected){
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* completion=NULL;
    CHECK(xl6OwnershipCatalogResolveV1(ref,testCatalog,&adapter,&preparation,&completion)&&adapter==selected);
    /* An admission probe confers no physical lifetime. Record Step only for
     * a genuine node pin, never for transient nodes seen in failed snapshots. */
    if(completion&&!testCompletion(ref)){
        CHECK(completion->Adapter==adapter&&testCompletionCount<256);
        testCompletions[testCompletionCount].ref=ref;testCompletions[testCompletionCount].completion=completion;
        testCompletions[testCompletionCount++].done=false;
    }
}
'''
            assert body.count('CHECK(xrtMemDebugEnable(true));')==1 and body.count('CHECK(xrtMemDebugReset());')==1
            body=body[:begin]+bridge+body[end:]
            body=body.replace('CHECK(xrtMemDebugEnable(true));','CHECK(xrtMemDebugEnable(true));testCatalog=testCreateCatalog();CHECK(testCatalog);',1)
            body=body.replace('CHECK(xrtMemDebugReset());','xl6OwnershipCatalogDestroyV1(testCatalog);testCatalog=NULL;CHECK(xrtMemDebugReset());',1)
            body=body.replace('#include "../fixtures/tls_server.h"','#include "'+(native/'tests/fixtures/tls_server.h').as_posix()+'"')
            # All real targets stay pinned through BOTH physical finish phases.
            if fixtureName=='tls_stream_future_ownership':
                marker='CHECK(s.adapters[i]->Hold(s.refs[i].Data)&&s.adapters[i]->Claim(s.refs[i].Data,&s));'
                assert body.count(marker)==1
                body=body.replace(marker,marker+'testRegisterPinnedCompletion(s.refs[i],s.adapters[i]);',1)
                marker='if(pinned)CHECK(s->adapters[at]->Hold(node.Reference.Data)&&s->adapters[at]->Claim(node.Reference.Data,s));'
                assert body.count(marker)==1
                body=body.replace(marker,'if(pinned){CHECK(s->adapters[at]->Hold(node.Reference.Data)&&s->adapters[at]->Claim(node.Reference.Data,s));testRegisterPinnedCompletion(node.Reference,s->adapters[at]);}',1)
                assert body.count('if(s.adapters[i]->Finish){')==1
                body=body.replace('if(s.adapters[i]->Finish){','if(s.adapters[i]->Finish&&!testCompletion(s.refs[i])){',1)
                marker='    CHECK(sawFailure==failure);';assert body.count(marker)==1
            else:
                marker='CHECK(n->adapter->Hold(n->ref.Data)&&n->adapter->Claim(n->ref.Data,s));'
                assert body.count(marker)==1
                body=body.replace(marker,marker+'testRegisterPinnedCompletion(n->ref,n->adapter);',1)
                assert body.count('if(!n->adapter->Finish)continue;')==1
                body=body.replace('if(!n->adapter->Finish)continue;','if(!n->adapter->Finish||testCompletion(n->ref))continue;',1)
                start=body.index('if(failure&&n->ref.')
                at=body.index('    CHECK(xrtAtomic32Load(&s.',start);marker=body[at:body.index('\n',at)]
            assert body.count(marker)==1
            body=body.replace(marker,'    testFinishCompletions(&s);\n'+marker,1)
            marker='static void balanced(xmemdebugsnapshot before)\n{';assert body.count(marker)==1
            body=body.replace(marker,marker+'testCompletionCount=0;',1)
            write(out/(fixtureName+'.fixture.c'),body)
            _,_,_,_,fixtureDefines,links,*_=_load_modules(fixtureName+'_tests',platform=platform_name())
            source=out/(fixtureName+'.implementation.c')
            write(source,'\n'.join(sorted(modules))+ '\n'+''.join('#define '+name+'\n' for name in fixtureDefines)+'''
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_NET_PORT_SELECT
'''+('#define XRT_MODULE_NET_PORT_IOCP\n' if os.name=='nt' else '#define XRT_MODULE_NET_PORT_EPOLL\n')+'''
#define XRUNTIME_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "release/include/xl_clock.h"
#define TLS_DIAL_OWNERSHIP_PINNED
#include "'''+fixtureName+'''.fixture.c"
''')
            generated.append((fixtureName,source,expected))
        write(out/'catalog-sources.json',json.dumps(sorted(sources),indent=2)+'\n')
        libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
        for i,cc in enumerate(compilers):
            modes=[('tcc',[])] if cc.stem=='tcc' else [('O0',['-O0']),('O2',['-O2'])]
            if a.sanitize and cc.stem!='tcc':
                assert os.name!='nt';modes.append(('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for mode,opt in modes:
                for fixtureName,source,expected in generated:
                    name=str(i)+'.'+mode+'.'+fixtureName;binary=out/(name+ext)
                    run(name+'.build',flags(cc,opt)+['-I'+str(native/'include'),source,*[root/q for q in sorted(sources)],*libraries,'-o',binary]);product(binary)
                    for backend in ['select','native']:
                        text=run(name+'.'+backend,[binary,*(['native'] if backend=='native' else [])],mode=='asan')
                        assert expected in text and 'balanced' in text,(name,text)
                        line=next(s for s in text.splitlines() if s.startswith(expected));n=list(map(int,re.findall(r'\d+',line)))
                        # Concurrent valid allocations may shift the first two
                        # successful constructor budgets. Assert the invariant,
                        # not one incidental observed allocation count.
                        if fixtureName=='tls_dial_future_ownership':
                            assert len(n)==6 and n[0]==n[1]==12+n[4] and n[2:4]==[10,5] and 0<n[5]<=n[4]-2<64
                        elif fixtureName=='tls_listener_ownership':
                            assert len(n)==10 and n[:6]==[8,8,10,2,32,4] and 0<n[7]<=n[6]-2 and 0<n[9]<=n[8]-2
                        else:
                            assert len(n)==8 and n[:5]==[6,12,12,24,3] and 0<n[6]<=n[5]-2 and n[7]==2
                        r['Rows'][-1]['ObservedCounters']=n;save()
        r['InputsUnchanged']=all(sha(q)==digest for q,digest in r['Inputs'].items())
        r['ProductsUnchanged']=all(sha(q['Path'])==q['Sha256'] for q in r['Products'])
        r['LogsUnchanged']=all(sha(q['Log'])==q['Sha256'] for q in r['Rows'])
        r['Complete']=all(r[k] for k in ['InputsUnchanged','ProductsUnchanged','LogsUnchanged']);save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise

if __name__=='__main__':main()
