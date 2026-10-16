"""Run fourteen original native text suites against actual runtime/compiler DLL/SO.

O0/O2 consumer objects contain no runtime provider definitions or implementation
objects. Consumer-side reset observation preserves the actual call and checks
heap ledgers before legacy tests can reset counters. Original allocator-fault
prefixes are PHYSICAL/persistent failures; embedded regex/pattern prefixes use
the real caller-thread one-shot logical injector. Never conflate the two.
Not generated-language/JIT OOM, runtime sanitizers, worker-wide OOM, OS join,
image unload, all runtime branches or full53 acceptance.
"""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, os, re, shutil, subprocess, sys, threading, time


# name, entry ABI, original allocator-prefix modes. No reduced-suite option.
SUITES = (
    ('stdlib_text_values_host', True, ()),
    ('stdlib_text_queries_host', False, ('bytes', 'chars', 'distance')),
    ('stdlib_text_split_host', False, ('split', 'lines', 'fields', 'join', 'split-large', 'lines-large', 'fields-large', 'join-large')),
    ('stdlib_text_iterator_host', False, ('0', '1', '2')),
    ('stdlib_number_values_host', False, tuple(map(str, range(9)))),
    ('stdlib_number_format_host', False, tuple(map(str, range(6)))),
    ('stdlib_unicode_host', False, tuple(map(str, range(5)))),
    ('stdlib_encoding_host', False, tuple(map(str, range(6)))),
    ('stdlib_encoding_detect_host', True, ()),
    ('xlir_regex_host', True, ()), ('xlir_regex_extended_host', True, ()),
    ('xlir_regex_callback_host', True, ()), ('xlir_pattern_host', True, ()),
    ('xlir_pattern_builder_host', True, ()),
)
EMBEDDED = {
    'xlir_regex_host': ((rb'regex native OOM: (\d+) actual failures, 5 first no-hit completions, balanced', 5),),
    'xlir_regex_extended_host': ((rb'regex extended native OOM: (\d+) actual failures, 9 first no-hit completions, balanced', 9),),
    'xlir_regex_callback_host': ((rb'regex callback native OOM: (\d+) actual failures, 3 first no-hit completions, balanced', 3),),
    'xlir_pattern_host': ((rb'pattern native OOM: (\d+) actual failures, 5 first no-hit completions, balanced', 5),),
    'xlir_pattern_builder_host': (
        (rb'pattern state factory OOM: (\d+) actual failures, balanced', 1),
        (rb'pattern builder adapter OOM: (\d+) actual failures, 10 first no-hit completions', 10)),
}
LEDGER = re.compile(rb'stdlib-suite-ledger resets=(\d+) allocations=(\d+) frees=(\d+) '
                    rb'live=0 bytes=0 invalid=0 double=0 uaf=0 caller-error=0')
PHYSICAL = re.compile(rb'\bpoint=(\d+) calls=(\d+) hit=([01]) balanced')


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def providers(data): return re.findall(rb'\b[TDB]\s+((?:xrt|xl6)\w+)\r?$', data, re.M)
def require_failure_prefix(point):
    assert point>1,'Allocating fixture lost its physical fault injection'
def embedded_proofs(name,data):
    result=[]
    for expression,count in EMBEDDED.get(name,()):
        matches=re.findall(expression,data);assert len(matches)==1 and int(matches[0])>0,'Logical injection did not fail allocations'
        result.append(dict(Families=count,FailurePoints=int(matches[0]),Model='caller-thread-one-shot-logical',Confirmation='original first-no-hit only'))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'out'): parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args(); root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists() and 1 <= args.jobs <= 4
    task = Path(__file__).resolve().parents[1]; win = os.name == 'nt'
    cc, nm, inspector = [shutil.which(n) for n in ('gcc', 'nm', 'objdump' if win else 'readelf')]
    assert cc and nm and inspector
    build = json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit'] == 0
    products = [root/'release'/n for n in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so'))]
    recorded = {Path(r['Path']).resolve(): r['Sha256'] for r in build['Products']}
    assert all(recorded.get(p) == sha(p) for p in products)
    observer = task/'test/support/stdlib_native_suite_observer.c'
    hosts = {name: task/'test/cases'/(name+'.c') for name, _, _ in SUITES}
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), Path(nm), Path(inspector),
             args.build_report.resolve(), observer, *hosts.values()}
    for r in build['Inputs']+build['Products']:
        assert sha(r['Path']) == r['Sha256'], r['Path']; files.add(Path(r['Path']))
    for name, host in hosts.items():
        assert host.read_bytes().replace(b'\r\n',b'\n') == (root/'test/cases'/host.name).read_bytes().replace(b'\r\n',b'\n'), name
    inputs = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(files)]
    out.mkdir(); rows, proofs, controls, failures = [], [], [], []
    lock = threading.Lock()
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows, Proofs=proofs,
        Controls=controls, Failures=failures, Scope=__doc__, ExplicitSubset=False, SuiteCount=14,
        PhysicalPrefixModes=40, EmbeddedLogicalPrefixFamilies=33,
        CompilerSourceObjects=False, RuntimeImplementationObjects=False, ProviderOverrides=False,
        ConsumerResetObservation=True, PrivateInlineReferenceHelpers=['xl6NumberParseError','xl6PatternTagKeyFromId'],
        GeneratedLanguageOomAcceptance=False, RuntimeInstrumented=False, WorkerWideOomAcceptance=False,
        ImageUnloadAcceptance=False, Full53Acceptance=False, NativeTextSuiteProductAcceptance=False)
    env = {k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
           ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    if not win:
        import resource
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    flags = ['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
             '-I'+str(root/'release'),'-I'+str(root/'release/include')]
    if not win: flags += ['-D_GNU_SOURCE']
    def run(name, command, directory, expected=0):
        command = list(map(str,command)); start = time.monotonic()
        try:
            r = subprocess.run(command,cwd=directory,env=env,capture_output=True,timeout=180)
            code, data = r.returncode, r.stdout+r.stderr
        except subprocess.TimeoutExpired as e: code,data='timeout',(e.stdout or b'')+(e.stderr or b'')
        log=out/(name+'.log');log.write_bytes(data)
        with lock: rows.append(dict(Name=name,Command=command,Exit=code,ExpectedExit=expected,
                                    Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        assert code == expected, (name,code,data.decode(errors='replace')[-5000:])
        return data
    def observed(name, command, directory):
        data=run(name,command,directory); found=LEDGER.findall(data);assert len(found)==1,name
        resets,allocs,frees=map(int,found[0]);assert allocs==frees,name
        return data,dict(Resets=resets,Allocations=allocs,Frees=frees)
    def definitions(name,obj):
        data=run(name,[nm,'--defined-only',obj],out)
        assert not providers(data),(name,providers(data))
    try:
        directories={}
        for p in products:
            directory=out/p.stem;(directory/'release').mkdir(parents=True)
            shutil.copy2(p,directory/'release'/p.name)
            if win: shutil.copy2(p,directory/p.name)
            directories[p.name]=directory
        objects={}; observers={}
        for opt in ('O0','O2'):
            for void in (False,True):
                obj=out/('observer.'+opt+'.'+str(int(void))+'.o')
                run(obj.stem+'.compile',[cc,*flags,'-'+opt,*(['-DSTDLIB_SUITE_VOID_ENTRY'] if void else []),'-c',observer,'-o',obj],out)
                definitions(obj.stem+'.definitions',obj);observers[opt,void]=obj
            obj=out/('observer.'+opt+'.reentry.o')
            run(obj.stem+'.compile',[cc,*flags,'-'+opt,'-DSTDLIB_SUITE_UNTRACKED_REENTRY','-c',observer,'-o',obj],out)
            definitions(obj.stem+'.definitions',obj);observers[opt,'reentry']=obj
            for name,_,_ in SUITES:
                obj=out/(name+'.'+opt+'.o')
                run(obj.stem+'.compile',[cc,*flags,'-'+opt,'-Dmain=stdlibNativeSuiteEntry','-c',hosts[name],'-o',obj],out)
                definitions(obj.stem+'.definitions',obj);objects[name,opt]=obj
        def links(p,directory):
            return [directory/'release'/p.name] if win else ['-L'+str(directory/'release'),'-l'+p.stem[3:],'-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
        def exercise(spec):
            p,opt,suite=spec;name,void,modes=suite;tag=p.stem+'.'+opt+'.'+name;directory=directories[p.name]
            binary=directory/(opt+'.'+name+('.exe' if win else ''))
            try:
                run(tag+'.link',[cc,objects[name,opt],observers[opt,void],*links(p,directory),'-Wl,--wrap=xrtMemDebugReset','-o',binary],directory)
                imported=run(tag+'.imports',[inspector,'-p' if win else '-d',binary],directory)
                found=re.findall(rb'DLL Name:\s*([^\s]+)',imported) if win else re.findall(rb'Shared library:\s*\[([^\]]+)\]',imported)
                assert {n.decode().lower() for n in found}&{x.name.lower() for x in products}=={p.name.lower()}
                data,ledger=observed(tag+'.vectors',[binary],directory)
                assert ledger['Allocations']>0,tag
                embedded=embedded_proofs(name,data)
                physical=[]
                for mode in modes:
                    for point in range(1,513):
                        text,_=observed(tag+'.'+mode+'.'+str(point),[binary,mode,point],directory)
                        found=PHYSICAL.findall(text);assert len(found)==1 and int(found[0][0])==point
                        if found[0][2]==b'0':require_failure_prefix(point);break
                    else: raise AssertionError('Original physical allocator prefix incomplete: '+mode)
                    text,_=observed(tag+'.'+mode+'.confirmed',[binary,mode,point+1],directory)
                    found=PHYSICAL.findall(text);assert len(found)==1 and int(found[0][0])==point+1 and found[0][2]==b'0'
                    physical.append(dict(Mode=mode,FailurePoints=point-1,FirstNoHit=point,Confirmation=point+1,Model='persistent-physical-allocator'))
                if name=='stdlib_text_values_host':assert b'19 actual OOM paths balanced' in data
                # Original reentry deliberately disables debug quarantine.
                # It is a separate assertion probe, never a zero heap-ledger proof.
                reentry=None
                if name=='stdlib_text_iterator_host':
                    probe=directory/(opt+'.reentry'+('.exe' if win else ''))
                    run(tag+'.reentry.link',[cc,objects[name,opt],observers[opt,'reentry'],*links(p,directory),'-o',probe],directory)
                    text=run(tag+'.reentry',[probe,'reentry'],directory)
                    assert b'allocator reentry observes detached closed state, no double release' in text
                    assert b'stdlib-suite-reentry debug-quarantine=0 ledger-acceptance=0 caller-error=0' in text and not LEDGER.findall(text)
                    reentry=dict(DebugQuarantine=False,HeapLedgerAcceptance=False,OriginalAssertionsPassed=True,
                                 Image=dict(Path=str(probe),Sha256=sha(probe)))
                return dict(Name=tag,Suite=name,Product=p.name,Optimization=opt,PhysicalPrefixes=physical,
                            EmbeddedLogicalPrefixes=embedded,VectorLedger=ledger,
                            SeparateAllocatorReentry=reentry,
                            ForcedPhysicalFailures=19 if name=='stdlib_text_values_host' else 0,
                            Image=dict(Path=str(binary),Sha256=sha(binary)))
            except Exception as e:
                with lock:failures.append(dict(Name=tag,Error=repr(e)))
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for proof in pool.map(exercise,[(p,opt,s) for p in products for opt in ('O0','O2') for s in SUITES]):
                if proof:proofs.append(proof);print(proof['Name']+': passed',flush=True)
        for p in products:
            directory=directories[p.name];stem=p.stem+'.control'
            for macro,marker in (('LEAK_AT_END',b'!after.LiveCount'),('LEAK_BEFORE_RESET',b'xrtMemDebugReset'),('DISABLE_DEBUG',b'xrtMemDebugEnabled')):
                tag=stem+'.'+macro;obj=out/(tag+'.o');binary=directory/(tag+('.exe' if win else ''))
                run(tag+'.compile',[cc,*flags,'-O0','-DSTDLIB_SUITE_VOID_ENTRY','-DSTDLIB_SUITE_'+macro,'-c',observer,'-o',obj],out)
                run(tag+'.link',[cc,objects['stdlib_encoding_detect_host','O0'],obj,*links(p,directory),'-Wl,--wrap=xrtMemDebugReset','-o',binary],directory)
                data=run(tag+'.reject',[binary],directory,3 if win else -6)
                assert b'assert' in data.lower() and marker in data,tag
                controls.append(dict(Name=tag,Compiled=True,AssertionRejected=True))
            # Remove the original test allocator's two denial branches, not a
            # production implementation. The real product still completes.
            tag=stem+'.NO_PHYSICAL_INJECTION';source=out/(tag+'.c');obj=out/(tag+'.o')
            body=hosts['stdlib_text_queries_host'].read_text(encoding='utf-8-sig')
            assert body.count('++fault->calls >= fault->fail')==2
            source.write_text(body.replace('++fault->calls >= fault->fail','++fault->calls >= fault->fail && false'),encoding='utf-8')
            run(tag+'.compile',[cc,*flags,'-O0','-Dmain=stdlibNativeSuiteEntry','-c',source,'-o',obj],out)
            definitions(tag+'.definitions',obj);binary=directory/(tag+('.exe' if win else ''))
            run(tag+'.link',[cc,obj,observers['O0',False],*links(p,directory),'-Wl,--wrap=xrtMemDebugReset','-o',binary],directory)
            data,_=observed(tag+'.unarmed',[binary,'bytes',1],directory)
            found=PHYSICAL.findall(data);assert len(found)==1 and found[0][0]==b'1' and found[0][2]==b'0'
            try:require_failure_prefix(1)
            except AssertionError:controls.append(dict(Name=tag,Compiled=True,ExecutionSucceeded=True,MissingInjectionRejected=True))
            else:raise AssertionError('Missing physical injection accepted')
            tag=stem+'.NO_LOGICAL_INJECTION';obj=out/(tag+'.o');binary=directory/(tag+('.exe' if win else ''))
            run(tag+'.compile',[cc,*flags,'-O0','-DSTDLIB_SUITE_VOID_ENTRY','-DSTDLIB_SUITE_SKIP_LOGICAL_INJECTION','-c',observer,'-o',obj],out)
            run(tag+'.link',[cc,objects['xlir_regex_host','O0'],obj,*links(p,directory),'-Wl,--wrap=xrtMemDebugReset','-Wl,--wrap=xrtMemDebugFailAfter','-o',binary],directory)
            data,_=observed(tag+'.unarmed',[binary],directory)
            assert b'regex native OOM: 0 actual failures, 5 first no-hit completions' in data
            try:embedded_proofs('xlir_regex_host',data)
            except AssertionError:controls.append(dict(Name=tag,Compiled=True,ExecutionSucceeded=True,MissingInjectionRejected=True))
            else:raise AssertionError('Missing logical injection accepted')
        fake=out/'fake-provider.c';fake.write_text('void xl6ForgedTextProviderV1(void) {}\n',encoding='ascii')
        obj=out/'fake-provider.o';run('fake-provider.compile',[cc,*flags,'-c',fake,'-o',obj],out)
        data=run('fake-provider.definitions',[nm,'--defined-only',obj],out)
        assert providers(data)==[b'xl6ForgedTextProviderV1'] and providers(data.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))==providers(data)
        controls.append(dict(Name='fake-provider',Compiled=True,DefinitionRejected=True,CrLfChecked=True))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['NativeTextSuiteProductAcceptance']=len(proofs)==56 and len(controls)==11 and not failures
        report['Complete']=report['InputsUnchanged'] and report['NativeTextSuiteProductAcceptance']
    except Exception as e:failures.append(dict(Name='gate',Error=repr(e)))
    report['PhaseCount']=len(rows)
    report['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Proofs=len(proofs),Controls=len(controls),Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
