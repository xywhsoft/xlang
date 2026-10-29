"""Run the three original Buffer suites against actual runtime/compiler DLL/SO.

O0/O2 consumers contain no runtime/compiler implementation objects or provider
definitions. Real reset observation checks ledgers before counters disappear.
Core/codec prefixes use caller-thread one-shot LOGICAL allocation denial; value
prefixes use the original persistent PHYSICAL allocator. Self-assignment alone
is explicitly allocation-free. Allocator reentry disables debug quarantine and
is a separate assertion probe, NOT a heap-ledger acceptance.
Not generated-language OOM, runtime sanitizers, worker-wide OOM, image unload,
all runtime branches or full53 completion.
"""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, os, re, shutil, subprocess, sys, threading, time

SUITES = {
    'stdlib_buffer_core_host': ('logical', ('create', 'create-reserve', 'reserve', 'append-byte', 'append-raw', 'append-alias', 'insert-alias', 'copy-gap')),
    'stdlib_buffer_codec_host': ('logical', ('hex', 'base64', 'from-hex', 'from-base64')),
    'stdlib_buffer_value_host': ('physical', ('from-text', 'from-bytes', 'copy', 'assign', 'append', 'insert', 'self-assign', 'self-append', 'self-insert', 'resize', 'trim', 'bytes', 'text', 'iterator', 'take')),
}
LEDGER = re.compile(rb'stdlib-suite-ledger resets=(\d+) allocations=(\d+) frees=(\d+) live=0 bytes=0 invalid=0 double=0 uaf=0 caller-error=0')
PHYSICAL = re.compile(rb'buffer ([\w-]+) point=(\d+) calls=(\d+) hit=([01]) balanced')
LOGICAL = re.compile(rb'buffer (core|codec) ([\w-]+) point=(\d+) hit=([01]) balanced')


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def providers(data):
    result = []
    for line in data.splitlines():
        fields = line.split()
        if len(fields) < 3: continue
        name = fields[-1]
        if name.startswith(b'.weak.'): name = name[6:].split(b'.', 1)[0]
        if name.startswith((b'xrt', b'xl6')): result.append(name)
    return result


def prefix_event(data, model, mode, point):
    found = (PHYSICAL if model == 'physical' else LOGICAL).findall(data)
    assert len(found) == 1, (model, mode, point, found)
    row = found[0]; actual_mode, actual_point = row[0:2] if model == 'physical' else row[1:3]
    assert actual_mode.decode() == mode and int(actual_point) == point
    calls = int(row[2]) if model == 'physical' else None
    hit = row[3] == b'1'
    if model == 'physical':
        assert (calls >= point) == hit, (mode, point, calls, hit)
    return hit, calls


def require_prefix(model, mode, first_no_hit, calls):
    start = 1 if model == 'physical' else 0
    if model == 'physical' and mode == 'self-assign':
        assert first_no_hit == 1 and calls == 0, 'Self-assignment unexpectedly allocates'
    else:
        assert first_no_hit > start, 'Allocating fixture lost its failure injection'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'out'): parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args(); root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists() and 1 <= args.jobs <= 4
    task = Path(__file__).resolve().parents[1]; win = os.name == 'nt'
    cc, nm, inspector = [shutil.which(n) for n in ('gcc', 'nm', 'objdump' if win else 'readelf')]
    assert cc and nm and inspector
    build = json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit'] == 0
    products = [root/'release'/n for n in (('xl.dll', 'xlc.dll') if win else ('libxl.so', 'libxlc.so'))]
    recorded = {Path(r['Path']).resolve(): r['Sha256'] for r in build['Products']}
    assert all(recorded.get(p) == sha(p) for p in products)
    observer = task/'test/support/stdlib_native_suite_observer.c'
    hosts = {name: task/'test/cases'/(name+'.c') for name in SUITES}
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), Path(nm), Path(inspector), args.build_report.resolve(), observer, *hosts.values()}
    for row in build['Inputs'] + build['Products']:
        assert sha(row['Path']) == row['Sha256'], row['Path']; files.add(Path(row['Path']))
    for name, host in hosts.items():
        assert host.read_bytes().replace(b'\r\n', b'\n') == (root/'test/cases'/host.name).read_bytes().replace(b'\r\n', b'\n'), name
    inputs = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(files)]
    out.mkdir(); rows, proofs, controls, failures = [], [], [], []; lock = threading.Lock()
    report = dict(Complete=False, InputsUnchanged=False, Platform='windows' if win else 'linux', Inputs=inputs,
        Rows=rows, Proofs=proofs, Controls=controls, Failures=failures, Scope=__doc__, ExplicitSubset=False,
        SuiteCount=3, LogicalPrefixModes=12, PhysicalPrefixModes=15, CompilerSourceObjects=False,
        RuntimeImplementationObjects=False, ProviderOverrides=False, ConsumerResetObservation=True,
        GeneratedLanguageOomAcceptance=False, RuntimeInstrumented=False, WorkerWideOomAcceptance=False,
        ImageUnloadAcceptance=False, Full53Acceptance=False, NativeBufferSuiteProductAcceptance=False)
    env = {k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
           ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    if not win:
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I'+str(root),
             '-I'+str(root/'release'), '-I'+str(root/'release/include')]
    if not win: flags += ['-D_GNU_SOURCE']
    def run(name, command, directory, expected=0):
        command = list(map(str, command)); start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=directory, env=env, capture_output=True, timeout=180)
            code, data = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error: code, data = 'timeout', (error.stdout or b'') + (error.stderr or b'')
        log = out/(name+'.log'); log.write_bytes(data)
        with lock: rows.append(dict(Name=name, Command=command, Exit=code, ExpectedExit=expected,
                                    Log=str(log), LogSha256=sha(log), Seconds=time.monotonic()-start))
        assert code == expected, (name, code, data.decode(errors='replace')[-5000:])
        return data
    def observed(name, command, directory):
        data = run(name, command, directory); found = LEDGER.findall(data); assert len(found) == 1, name
        resets, allocs, frees = map(int, found[0]); assert allocs == frees, name
        return data, dict(Resets=resets, Allocations=allocs, Frees=frees)
    def definitions(name, obj):
        data = run(name, [nm, '--defined-only', '--extern-only', obj], out)
        assert not providers(data), (name, providers(data))
    try:
        directories = {}
        for product in products:
            directory = out/product.stem; (directory/'release').mkdir(parents=True)
            shutil.copy2(product, directory/'release'/product.name)
            if win: shutil.copy2(product, directory/product.name)
            directories[product.name] = directory
        objects, observers = {}, {}
        for opt in ('O0', 'O2'):
            for reentry in (False, True):
                obj = out/('observer.'+opt+'.'+str(int(reentry))+'.o')
                run(obj.stem+'.compile', [cc, *flags, '-'+opt, *(['-DSTDLIB_SUITE_UNTRACKED_REENTRY'] if reentry else []), '-c', observer, '-o', obj], out)
                definitions(obj.stem+'.definitions', obj); observers[opt, reentry] = obj
            for name, host in hosts.items():
                obj = out/(name+'.'+opt+'.o')
                run(obj.stem+'.compile', [cc, *flags, '-'+opt, '-Dmain=stdlibNativeSuiteEntry', '-c', host, '-o', obj], out)
                definitions(obj.stem+'.definitions', obj); objects[name, opt] = obj
        def links(product, directory):
            return [directory/'release'/product.name] if win else ['-L'+str(directory/'release'), '-l'+product.stem[3:], '-Wl,-rpath,$ORIGIN/release', '-pthread', '-ldl', '-lm']
        def exercise(spec):
            product, opt, name = spec; model, modes = SUITES[name]; tag = product.stem+'.'+opt+'.'+name
            directory = directories[product.name]; binary = directory/(opt+'.'+name+('.exe' if win else ''))
            try:
                run(tag+'.link', [cc, objects[name,opt], observers[opt,False], *links(product,directory), '-Wl,--wrap=xrtMemDebugReset', '-o', binary], directory)
                imported = run(tag+'.imports', [inspector, '-p' if win else '-d', binary], directory)
                found = re.findall(rb'DLL Name:\s*([^\s]+)', imported) if win else re.findall(rb'Shared library:\s*\[([^\]]+)\]', imported)
                assert {n.decode().lower() for n in found} & {p.name.lower() for p in products} == {product.name.lower()}
                _, ledger = observed(tag+'.vectors', [binary], directory); assert ledger['Allocations'] > 0
                prefixes = []
                for mode in modes:
                    for point in range(1 if model == 'physical' else 0, 513):
                        data, _ = observed(tag+'.'+mode+'.'+str(point), [binary, mode, point], directory)
                        hit, calls = prefix_event(data, model, mode, point)
                        if not hit: require_prefix(model, mode, point, calls); break
                    else: raise AssertionError('Original allocation prefix incomplete: '+mode)
                    data, _ = observed(tag+'.'+mode+'.confirmed', [binary, mode, point+1], directory)
                    hit, confirmation_calls = prefix_event(data, model, mode, point+1); assert not hit
                    if model == 'physical': assert confirmation_calls == calls
                    prefixes.append(dict(Mode=mode, Model='persistent-physical-allocator' if model == 'physical' else 'caller-thread-one-shot-logical',
                        FailurePoints=point-(1 if model == 'physical' else 0), FirstNoHit=point, Confirmation=point+1,
                        CallsAtSuccess=calls, AllocationFree=mode == 'self-assign'))
                reentry = None
                if name == 'stdlib_buffer_value_host':
                    probe = directory/(opt+'.reentry'+('.exe' if win else ''))
                    run(tag+'.reentry.link', [cc, objects[name,opt], observers[opt,True], *links(product,directory), '-o', probe], directory)
                    data = run(tag+'.reentry', [probe, 'reentry'], directory)
                    assert b'buffer close reentry detached state, balanced' in data
                    assert b'stdlib-suite-reentry debug-quarantine=0 ledger-acceptance=0 caller-error=0' in data and not LEDGER.findall(data)
                    reentry = dict(DebugQuarantine=False, HeapLedgerAcceptance=False, OriginalAssertionsPassed=True,
                                   Image=dict(Path=str(probe), Sha256=sha(probe)))
                return dict(Name=tag, Suite=name, Product=product.name, Optimization=opt, Prefixes=prefixes,
                    VectorLedger=ledger, SeparateAllocatorReentry=reentry, Image=dict(Path=str(binary), Sha256=sha(binary)))
            except Exception as error:
                with lock: failures.append(dict(Name=tag, Error=repr(error)))
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for proof in pool.map(exercise, [(p,opt,s) for p in products for opt in ('O0','O2') for s in SUITES]):
                if proof: proofs.append(proof); print(proof['Name']+': passed', flush=True)
        for product in products:
            directory = directories[product.name]; stem = product.stem+'.control'
            for macro, marker in (('LEAK_AT_END', b'!after.LiveCount'), ('LEAK_BEFORE_RESET', b'xrtMemDebugReset'), ('DISABLE_DEBUG', b'xrtMemDebugEnabled')):
                tag = stem+'.'+macro; obj = out/(tag+'.o'); binary = directory/(tag+('.exe' if win else ''))
                run(tag+'.compile', [cc,*flags,'-O0','-DSTDLIB_SUITE_'+macro,'-c',observer,'-o',obj], out)
                definitions(tag+'.definitions',obj)
                run(tag+'.link', [cc,objects['stdlib_buffer_core_host','O0'],obj,*links(product,directory),'-Wl,--wrap=xrtMemDebugReset','-o',binary], directory)
                data = run(tag+'.reject', [binary], directory, 3 if win else -6)
                assert b'assert' in data.lower() and marker in data, tag
                controls.append(dict(Name=tag, Compiled=True, AssertionRejected=True))
            # Mechanically disarm the TEST allocator only, never a product API.
            tag = stem+'.NO_PHYSICAL_INJECTION'; source = out/(tag+'.c'); obj = out/(tag+'.o')
            body = hosts['stdlib_buffer_value_host'].read_text(encoding='utf-8-sig')
            assert body.count('++fault->calls >= fault->fail') == 2
            source.write_text(body.replace('++fault->calls >= fault->fail', '++fault->calls >= fault->fail && false'), encoding='utf-8')
            run(tag+'.compile', [cc,*flags,'-O0','-Dmain=stdlibNativeSuiteEntry','-c',source,'-o',obj], out)
            definitions(tag+'.definitions',obj); binary = directory/(tag+('.exe' if win else ''))
            run(tag+'.link', [cc,obj,observers['O0',False],*links(product,directory),'-Wl,--wrap=xrtMemDebugReset','-o',binary], directory)
            data,_ = observed(tag+'.unarmed', [binary,'from-text',1], directory)
            # Without denial, allocator calls still occur: validate the oracle,
            # not the unchanged suite's success status alone.
            match = PHYSICAL.findall(data); assert len(match)==1 and match[0][3]==b'0' and int(match[0][2])>0
            try: prefix_event(data,'physical','from-text',1)
            except AssertionError: controls.append(dict(Name=tag,Compiled=True,ExecutionSucceeded=True,MissingInjectionRejected=True))
            else: raise AssertionError('Missing physical injection accepted')
            tag = stem+'.NO_LOGICAL_INJECTION'; obj = out/(tag+'.o'); binary = directory/(tag+('.exe' if win else ''))
            run(tag+'.compile', [cc,*flags,'-O0','-DSTDLIB_SUITE_SKIP_LOGICAL_INJECTION','-c',observer,'-o',obj], out)
            definitions(tag+'.definitions',obj)
            run(tag+'.link', [cc,objects['stdlib_buffer_core_host','O0'],obj,*links(product,directory),'-Wl,--wrap=xrtMemDebugReset','-Wl,--wrap=xrtMemDebugFailAfter','-o',binary], directory)
            data,_ = observed(tag+'.unarmed', [binary,'create',0], directory)
            hit,calls = prefix_event(data,'logical','create',0); assert not hit
            try: require_prefix('logical','create',0,calls)
            except AssertionError: controls.append(dict(Name=tag,Compiled=True,ExecutionSucceeded=True,MissingInjectionRejected=True))
            else: raise AssertionError('Missing logical injection accepted')
        fake = out/'fake-provider.c'
        fake.write_text('void xl6ForgedBufferProviderV1(void) {}\n__attribute__((weak)) void xrtForgedBufferWeak(void) {}\nint xrtForgedBufferData;\n',encoding='ascii')
        obj = out/'fake-provider.o'; run('fake-provider.compile',[cc,*flags,'-c',fake,'-o',obj],out)
        data = run('fake-provider.definitions',[nm,'--defined-only','--extern-only',obj],out)
        assert set(providers(data)) == {b'xl6ForgedBufferProviderV1',b'xrtForgedBufferWeak',b'xrtForgedBufferData'}
        assert providers(data.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')) == providers(data)
        controls.append(dict(Name='fake-provider',Compiled=True,StrongWeakDataRejected=True,CrLfChecked=True))
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        report['NativeBufferSuiteProductAcceptance'] = len(proofs) == 12 and len(controls) == 11 and not failures
        report['Complete'] = report['InputsUnchanged'] and report['NativeBufferSuiteProductAcceptance']
    except Exception as error: failures.append(dict(Name='gate',Error=repr(error)))
    report['PhaseCount'] = len(rows)
    report['Products'] = [dict(Path=str(p),Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Proofs=len(proofs),Controls=len(controls),Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__': raise SystemExit(main())
