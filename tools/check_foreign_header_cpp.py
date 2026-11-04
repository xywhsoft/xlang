"""Keep authoritative native ABI proofs active in C11 and C++11/17 headers.

Compile-only, offline tests. This is not execution or shipping SDK acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess
from xlir_public_build_inputs import verify_public_build
from runtime_layout import runtime_directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('root', 'public-directory', 'out'):
        parser.add_argument('--'+key, type=Path, required=True)
    args = parser.parse_args()
    root, public, out = [p.resolve() for p in (args.root, args.public_directory, args.out)]
    build, pinned, _ = verify_public_build(root, public)
    cc, cpp = shutil.which('gcc'), shutil.which('g++')
    assert cc and cpp
    sources = sorted(p for p in (root/'test/cases').glob('foreign_*.xl')
                     if not p.stem.startswith(('foreign_xge_', 'foreign_xui_', 'foreign_xge3_')))
    pinned.update([Path(__file__).resolve(), Path(cc), Path(cpp), *sources,
                   *(root/'test/cases').glob('foreign_*.h')])
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs = {str(p): sha(p) for p in pinned}
    out.mkdir(parents=True, exist_ok=False)
    sdk = runtime_directory(root) if os.name == 'nt' else Path(build['SdkDirectory'])
    env = dict(os.environ, XL6_SOURCE_NATIVE_ROOT=str(root))
    if os.name != 'nt':
        env['LD_LIBRARY_PATH'] = str(Path(build['RuntimeLibrary']).parent)+':'+env.get('LD_LIBRARY_PATH', '')
    includes = ['-I'+str(p) for p in (root, public/'include', sdk, sdk/'include')]
    report = dict(complete=False, passed=False, scope=__doc__, networkTests=False,
                  sourceBuild=str(public/'build.json'), inputs=inputs, rows=[])

    def run(name, command, refused=False):
        result = subprocess.run(list(map(str, command)), cwd=root, env=env,
                                capture_output=True, timeout=90)
        log = out/(name+'.log')
        log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name, exitCode=result.returncode,
                                  refused=refused, logSha256=sha(log)))
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
        print(name, result.returncode, flush=True)
        assert result.returncode == (1 if refused else 0), log
        if refused:
            assert b'declared C function ABI' in log.read_bytes(), log

    for source in sources:
        prefix = out/source.stem
        run(source.stem+'-emit', [build['Driver'], source, 'emit', prefix])
        for compiler, standard in ((cc, 'c11'), (cpp, 'c++11'), (cpp, 'c++17')):
            run(source.stem+'-'+standard, [compiler, '-std='+standard, '-Wall', '-Wextra', '-Werror',
                '-x', 'c++' if compiler == cpp else 'c', '-fsyntax-only', *includes, prefix.with_suffix('.h')])
    # In C an enum is compatible with its underlying integer type. Keep that
    # rule in function signatures; pointers retain exact enum identity and qualifiers.
    template = '''extern "NativeEnumPointer" class EnumPointer @header("test/cases/foreign_header_cpp.h") @lifetime("caller")
 native point handle
end
extern "EnumHandle" class Raw @header("test/cases/foreign_header_cpp.h") @lifetime("caller")
 native point handle
 extern "enum_proof" @no_raise func read(EnumPointer? value) as RESULT
end
export "enum_probe" func probe(Raw raw) as RESULT
 return raw.read(null)
end
func main(...) as int
 return 0
end
'''
    for name, content in (
        ('enum-compatible', template.replace('RESULT', 'c_int')),
        ('enum-wrong-width', template.replace('RESULT', 'c_double')),
        ('enum-wrong-qualifier', template.replace('RESULT', 'c_int').replace('NativeEnumPointer', 'native_enum*')),
    ):
        case = out/(name+'.xl')
        case.write_text(content, encoding='utf8')
        prefix = out/name
        run(name+'-emit', [build['Driver'], case, 'emit', prefix])
        for compiler, standard in ((cc, 'c11'), (cpp, 'c++11'), (cpp, 'c++17')):
            run(name+'-'+standard, [compiler, '-std='+standard, '-Wall', '-Wextra', '-Werror',
                '-x', 'c++' if compiler == cpp else 'c', '-fsyntax-only', *includes,
                prefix.with_suffix('.h')], name != 'enum-compatible')
    report['inputsUnchanged'] = all(sha(p) == value for p, value in inputs.items())
    assert report['inputsUnchanged']
    report.update(complete=True, passed=True, fixtureHeaders=len(sources))
    (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')


if __name__ == '__main__':
    main()
