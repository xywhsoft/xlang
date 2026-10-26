"""Stage the selected Windows TCC with this source cohort's complete SDK.

The bootstrap executable deliberately has no embedded SDK. Scenario workers
need a real standalone layout, not an installed TCC or ambient include paths.
Only the compiler/SDK is reused; no compiled test or test result is reused.
"""
from pathlib import Path
import argparse, hashlib, json, shutil


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(root, compiler, out):
    root, compiler, out = root.resolve(), compiler.resolve(), out.resolve()
    if out.exists():
        raise ValueError('Preserve prior standalone TCC evidence; choose a fresh directory')
    sdk = root / 'res/tcc/win64/x86_64'
    core = root / 'lib/tcc/include'
    for path in (compiler, sdk/'include/winapi/windows.h', sdk/'include/winapi/winbase.h',
                 sdk/'lib/libtcc1.a', core/'tccdefs.h', core/'stdatomic.h'):
        if not path.is_file():
            raise ValueError('Incomplete selected TCC/SDK input: ' + str(path))
    # Match the ordinary source build: project TCC compiler headers precede
    # the pinned Windows SDK. No silent fallback to another installation.
    selected = {p.relative_to(sdk).as_posix(): p for p in sdk.rglob('*') if p.is_file()}
    selected.update({'include/'+p.relative_to(core).as_posix(): p
                     for p in core.rglob('*') if p.is_file()})
    selected['tcc.exe'] = compiler
    for path in compiler.parent.glob('*.dll'):
        selected[path.name] = path
    manifest = Path(str(compiler)+'.manifest')
    if manifest.is_file():
        selected['tcc.exe.manifest'] = manifest
    inputs = {str(path): sha(path) for path in selected.values()}
    inputs[str(Path(__file__).resolve())] = sha(Path(__file__).resolve())
    out.mkdir(parents=True)
    files = {}
    for relative, source in sorted(selected.items()):
        target = out/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        actual = sha(target)
        if actual != inputs[str(source)]:
            raise ValueError('TCC/SDK changed while copying: ' + str(source))
        files[str(target)] = actual
    if any(sha(Path(path)) != digest for path, digest in inputs.items()):
        raise ValueError('TCC/SDK inputs changed during preparation')
    report = dict(Complete=True, InputsUnchanged=True, Root=str(root),
                  SelectedCompiler=str(compiler), Compiler=str(out/'tcc.exe'),
                  CompilerSha256=sha(compiler), Inputs=inputs, Files=files,
                  WorkerResultsReused=False, Scope=__doc__)
    (out/'distribution.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    report = prepare(args.root, args.compiler, args.out)
    print('Standalone selected TCC:', len(report['Files']), 'verified compiler/SDK files')


if __name__ == '__main__':
    main()
