"""Exercise package nativeLink through the real compiler and TinyCC API.

The Native dead section contains an unresolved reference. Its removal, exported
language functions, checked module entry and retirement are actual JIT checks,
not Cgen-only assertions. No window/device/network operations run.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time
from foreign_validation_build import add_build_arguments, load_build
from compiler_response import command_with_response


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'out', 'provider', 'old-provider', 'libraries'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--sdk', type=Path)
    add_build_arguments(p)
    a = p.parse_args()
    root, out, provider, old, libraries = [x.resolve() for x in
        (a.root, a.out, a.provider, a.old_provider, a.libraries)]
    b = load_build(a.build, a.overlay)
    cohort, runtime = b['cohort'], b['runtime']
    selected = json.loads(Path(b['overlay'] or b['buildManifest']).read_text(encoding='utf-8-sig'))
    public_sdk = Path(selected.get('SdkDirectory') or b['base'].get('SdkDirectory')
        or (a.sdk.resolve().parent if a.sdk else b['base']['RuntimeDirectory']))
    sdk = a.sdk.resolve() if a.sdk else public_sdk / 'include'
    assert provider.is_file() and old.is_file() and libraries.is_dir() and sdk.is_dir()
    out.mkdir(parents=True, exist_ok=False)
    sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
    inputs = dict(b['inputs'])
    host = root / 'test/cases/native_link_module.c'
    for f in (Path(__file__), host, provider, old): inputs[str(f)] = sha(f)
    for directory in (libraries, sdk):
        inputs.update((str(f), sha(f)) for f in directory.rglob('*') if f.is_file())
    # Public API and private compiler/runtime aliases are separate products.
    # Build one explicit test SDK; later layers are exact runtime overrides.
    combined = out / 'include'; combined.mkdir()
    header_layers = [public_sdk / 'include', sdk, b['driver'].parent / 'include']
    header_rows = []
    for directory in header_layers:
        assert directory.is_dir(), directory
        for f in sorted(directory.rglob('*')):
            if not f.is_file() or f.suffix not in ('.h', '.inc', '.def'): continue
            inputs[str(f)] = sha(f)
            target = combined / f.relative_to(directory)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(f, target)
            header_rows.append(dict(source=str(f), target=str(target), sha256=sha(f)))
    sdk = combined
    package = out / 'native'
    package.mkdir()
    (package / 'native.h').write_text('int native_link_live(void);\nvoid native_link_record_drop(int);\n')
    (package / 'provider.c').write_text('''#include "native.h"
#if !defined(XL6_NATIVE_LINK_SCHEMA) || XL6_NATIVE_LINK_SCHEMA != 1
#error package nativeLink did not reach the actual Native compiler
#endif
extern int native_link_missing_dead(void);
extern int native_link_missing_data;
int native_link_used_data = 42;
int* native_link_dead_data = &native_link_missing_data;
int native_link_dead(void) { return native_link_missing_dead(); }
int native_link_live(void) { return native_link_used_data; }
''')
    (package / 'api.xl').write_text('''export extern "native_link_live" @header("native.h") @no_raise @nonblocking func value() as c_int
export extern "native_link_record_drop" @header("native.h") @no_raise func drop(c_int id)
''')
    manifest = dict(format='xlang-package-1', name='native', version='1.0.0',
        artifact=dict(kind='source', xlSources=['api.xl'], cSources=['provider.c'],
            includeDirs=['.'], nativeLink=dict(functionSections=True,
                dataSections=True, gcSections=True, printGcSections=True)))
    (package / 'xlang.package.json').write_text(json.dumps(manifest, indent=2) + '\n')
    source = out / 'module.xl'
    source.write_text('''import "./native"
class ModuleMarker
 destroy()
  native.drop(c_int(71))
 end
end
ModuleMarker marker=ModuleMarker()
export "native_link_unreferenced_export" func unreferenced(int value) as int
 return value+int(native.value())
end
export "native_link_checked_error" func fail(int value) as int
 if value==0
  throw RuntimeError "nativeLink checked export"
 end
 return value
end
func main(...) as int
 assert native.value()==c_int(42)
 return 0
end
''')
    generated = [source, *[f for f in package.rglob('*') if f.is_file()]]
    inputs.update((str(f), sha(f)) for f in generated)
    windows = os.name == 'nt'
    env = dict(os.environ)
    for name in list(env):
        if name.startswith('XL6_') or name in ('XLANG_PACKAGE_PATH', 'TCCDIR'):
            env.pop(name)
    if windows: shutil.copy2(runtime, out / runtime.name)
    else: env['LD_LIBRARY_PATH'] = str(runtime.parent)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
        '-I' + str(cohort), '-I' + str(sdk), '-I' + str(sdk.parent),
        '-I' + str(root), '-I' + str(public_sdk), '-I' + str(public_sdk / 'include')]
    system = (['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
        if windows else ['-Wl,--export-dynamic', '-pthread', '-ldl', '-lm',
            '-Wl,-rpath,' + str(runtime.parent)])
    report = dict(scope=__doc__, complete=False, passed=False,
        compiler=b['buildManifest'] or b['overlay'], fullSourceRebuild=b['fullSourceRebuild'],
        shippingDelivery=False, osUiNetworkGpuExecuted=False, inputs=inputs,
        headerLayers=header_rows, rows=[])
    def save(): (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    def run(name, command):
        command = list(map(str, command))
        actual = command_with_response(command, 'gcc', out / (name + '.rsp'))
        start = time.monotonic()
        result = subprocess.run(actual, cwd=out, env=env, capture_output=True, timeout=600)
        log = out / (name + '.log'); log.write_bytes(result.stdout + result.stderr)
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
            seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save(); print(name, result.returncode, flush=True)
        assert result.returncode == 0, log
    for opt in ('O0', 'O2'):
        executable = out / ('host-' + opt + ('.exe' if windows else ''))
        run(opt + '-build', ['gcc', *flags, '-' + opt, host, *b['objects'], runtime,
            *system, '-o', executable])
        arguments = [source, out, sdk, provider, libraries, out / ('jit-' + opt)]
        run(opt + '-module', [executable, *arguments, 'module', public_sdk / 'include'])
        arguments[3] = old
        run(opt + '-old-provider-rejected', [executable, *arguments, 'reject', public_sdk / 'include'])
    report.update(complete=True, passed=True,
        inputsUnchanged=all(Path(f).is_file() and sha(f) == h for f, h in inputs.items()))
    assert report['inputsUnchanged']; save()


if __name__ == '__main__': main()
