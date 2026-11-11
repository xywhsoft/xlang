"""Qualify AudioGroup/Sound/Music/AudioStream with real no-device native audio.

Uses the complete O2 compiler, an explicitly private engine test variant and
generated valid PCM WAV. Covers JIT/dynamic/static AOT and allocation prefixes.
The shipping engine image is separately qualified, never replaced by this SDK.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, struct, subprocess, wave
from full_compiler_qualification import verify_full_compiler


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'test-variant', 'out', 'objdump'):
        p.add_argument('--'+name, type=Path, required=True)
    a = p.parse_args()
    root, compiler, cohort, variant, out, objdump = [v.resolve() for v in
        (a.root, a.compiler, a.compiler_report, a.test_variant, a.out, a.objdump)]
    sha = lambda v: hashlib.sha256(Path(v).read_bytes()).hexdigest()
    metadata = verify_full_compiler(cohort, compiler)
    private = json.loads((variant/'test-variant.json').read_text())
    assert private['passed'] and private['inputsUnchanged'] and not private['shippingDelivery']
    assert not private['audioDevice']
    dynamic, static = variant/'packages-host', variant/'packages-static'
    cases = [root/'test/cases'/(name+'.xl') for name in
             ('foreign_xge3_audio_resources','foreign_xge3_audio_resources_oom','foreign_xge3_audio_threads')]
    sdk = compiler.parent/'include'
    files = [Path(__file__).resolve(), Path(__file__).with_name('full_compiler_qualification.py'),
             Path(metadata['sourceFreeze']), compiler, cohort, objdump, *cases,
             cases[0].with_suffix('.h'), cases[2].with_suffix('.h'), root/'test/support/xge3_audio_headless.inc', variant/'test-variant.json']
    for folder in (sdk, dynamic, static):
        files.extend(v for v in folder.rglob('*') if v.is_file())
    for name in ('xl.dll', 'xlc.dll') if os.name=='nt' else ('libxl.so', 'libxlc.so'):
        files.append(compiler.parent/name)
    inputs = {str(v): sha(v) for v in files}
    out.mkdir(parents=True, exist_ok=False)
    pcm = out/'audio.wav'
    with wave.open(str(pcm), 'wb') as f:
        f.setparams((2, 2, 8000, 0, 'NONE', 'not compressed'))
        f.writeframes(struct.pack('<hh', 8192, 8192)*8000)
    inputs[str(pcm)] = sha(pcm)
    manifest = json.loads((dynamic/'_xge_native/xlang.package.json').read_text(encoding='utf8'))
    image = dynamic/'_xge_native'/manifest['links'][0]
    assert sha(image)==manifest['native']['sha256']
    report = dict(passed=False, scope=__doc__, fullCompilerBuild=True, shippingDelivery=False,
                  audioDevice=False, realMiniaudio=True, privateSetupExports=True,
                  window=False, networkTests=False, inputs=inputs, rows=[])
    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    def run(name, args, packages=None):
        args = list(map(str, args))
        env = dict(os.environ, XLANG_XGE_AUDIO_FIXTURE=str(pcm))
        env.pop('XLANG_PACKAGE_PATH', None)
        if packages:
            env['XLANG_PACKAGE_PATH'] = str(packages)
        if os.name=='nt':
            env['PATH'] = str(image.parent)+';'+str(compiler.parent)+';'+env.get('PATH','')
        else:
            env['LD_LIBRARY_PATH'] = str(image.parent)+':'+str(compiler.parent)+':'+env.get('LD_LIBRARY_PATH','')
        result = subprocess.run(args, cwd=compiler.parent, env=env, capture_output=True, timeout=240)
        log = out/(name+'.log')
        log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name, command=args, exitCode=result.returncode, logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert not result.returncode, log
        if name.endswith('-jit') or name.endswith('-run'):
            if '_oom-' in name:
                budgets = re.findall(rb'Audio language budgets\s+(\d+)\s*:\s*(\d+)\s+failures /\s*(\d+)\s+successes', result.stdout)
                assert len(budgets)==4 and {int(k) for k,n,s in budgets}==set(range(4))
                assert all(int(n)>0 and int(s)>0 and int(n)+int(s)==64 for k,n,s in budgets)
                report.setdefault('allocationPrefixes', {})[name] = {
                    k.decode(): dict(failures=int(n), successes=int(s)) for k,n,s in budgets}
            elif '_threads-' in name:
                assert b'Audio creator-thread calls and four deferred final releases passed' in result.stdout
            else:
                rows = re.findall(rb'(Native|Language) AudioGroup:\s*(\d+)\s+allocations /\s*(\d+)\s+bytes', result.stdout)
                costs = {k.decode(): dict(allocations=int(n), bytes=int(b)) for k,n,b in rows}
                assert set(costs)=={'Native','Language'} and costs['Language']['allocations']>=costs['Native']['allocations']
                assert b'Audio direct factories, methods, real PCM, parent ownership and close passed' in result.stdout
                report.setdefault('retainedCosts', {})[name] = costs
            save()
        return result
    include = ['--inc-path', root, '--inc-path', sdk]
    for case in cases:
        name = case.stem
        cfile, hfile = out/(name+'.c'), out/(name+'.h')
        run(name+'-emit', [compiler, 'cgen', case, '--emit', cfile, '--emit-header', hfile], dynamic)
        run(name+'-jit', [compiler, 'run', case, *include], dynamic)
        for mode, packages in [('dynamic', dynamic), ('static', static)]:
            binary = out/(name+'-'+mode+('.exe' if os.name=='nt' else ''))
            run(name+'-'+mode+'-build', [compiler, 'build', case, '-o', binary, *include], packages)
            run(name+'-'+mode+'-run', [binary])
            result = run(name+'-'+mode+'-imports', [objdump, '-p', binary])
            dependencies = re.findall(rb'DLL Name:\s*(\S+)', result.stdout) if os.name=='nt' else re.findall(rb'NEEDED\s+(\S+)', result.stdout)
            language = {b'xl.dll',b'xlc.dll',b'libtcc.dll',b'libxl.so',b'libxlc.so'}
            forbidden = language | ({b'xge3_host.dll',b'xge.dll',b'libxge3_host.so'} if mode=='static' else set())
            assert not forbidden.intersection(v.lower() for v in dependencies)
            report.setdefault('binaries', []).append(dict(path=str(binary), variant=mode, sha256=sha(binary),
                bytes=binary.stat().st_size, imports=[v.decode('ascii') for v in dependencies]))
        generated = cfile.read_text(encoding='utf8')
        methods = ['xgeAudioGroupInit','xgeSoundCreateOwned','xgeMusicCreateOwned','xgeStreamCreateOwned']
        if case==cases[0]:
            methods += ['xgeAudioUnitChecked','xgeUnitChecked','xgeAudioGroupSetVolume','xgeAudioGroupGetVolume',
                'xgeAudioGroupFade']
            methods += ['xge'+kind+method for kind in ('Sound','Music','Stream') for method in
                        ('Play','Stop','Pause','Resume','SetLoop','SetVolume','Fade','IsPlaying')]
            methods += ['xgeSoundSetPosition','xgeStreamSetPosition']
        for symbol in methods:
            assert re.search(r'\b'+symbol+r'\(', generated), symbol
            assert not re.search(r'\b(?:int|void)\s+'+symbol+r'\s*\([^;]*\)\s*\{', generated), symbol
    report.update(passed=True, inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
    assert report['inputsUnchanged']
    save()


if __name__=='__main__':
    main()
