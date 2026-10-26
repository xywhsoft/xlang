"""Real standalone Windows TCC/SDK relocation and immutable-input checks."""
from pathlib import Path
import argparse, json, os, subprocess
from prepare_xlir_tcc_distribution import prepare, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    assert os.name == 'nt', 'Native Windows standalone SDK gate required'
    out = args.out.resolve()
    assert not out.exists(), 'Preserve previous distribution evidence'
    out.mkdir(parents=True)
    selected = args.compiler.resolve()
    distribution = out/'relocated compiler with spaces'
    staged = prepare(args.root, selected, distribution)
    assert staged['CompilerSha256'] == sha(selected)
    fixture = Path(__file__).resolve().parents[1]/'test/cases/tcc_distribution_sdk.c'
    inputs = {**staged['Inputs'], str(fixture): sha(fixture),
              str(Path(__file__).resolve()): sha(Path(__file__).resolve())}
    env = {k:v for k,v in os.environ.items() if k.upper() not in
           ('CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH', 'TCCDIR')}
    rows = []
    def run(name, command):
        log = out/(name+'.log')
        with log.open('wb') as stream:
            code = subprocess.run(list(map(str, command)), cwd=out, env=env,
                                  stdout=stream, stderr=subprocess.STDOUT).returncode
        assert code == 0, log
        rows.append(dict(Name=name, Exit=code, Log=str(log), Sha256=sha(log)))
    program = out/'sdk-probe.exe'
    run('build', [staged['Compiler'], '-std=c11', '-Wall', '-Werror', fixture, '-o', program])
    run('run', [program])
    assert 'C11 atomics, Windows SDK and CRT passed' in (out/'run.log').read_text()
    rejected = []
    for name, root, compiler, target in [
        ('existing-output', args.root, selected, distribution),
        ('missing-compiler', args.root, out/'missing.exe', out/'invalid-compiler'),
        ('missing-sdk', out, selected, out/'invalid-sdk'),
    ]:
        try:
            prepare(root, compiler, target)
        except ValueError:
            rejected.append(name)
        else:
            raise AssertionError('Invalid distribution accepted: '+name)
        if name != 'existing-output':
            assert not target.exists(), 'Invalid input created a partial distribution'
    assert all(sha(Path(p)) == h for p,h in inputs.items())
    assert all(sha(Path(p)) == h for p,h in staged['Files'].items())
    report = dict(Complete=True, InputsUnchanged=True, Inputs=inputs, Rows=rows,
                  Rejected=rejected, Files=staged['Files'], Compiler=str(selected),
                  ProductSha256=sha(program), WorkerResultsReused=False)
    (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('Standalone TCC:', len(staged['Files']), 'exact files; relocated build/run and 3 refusals passed')


if __name__ == '__main__':
    main()
