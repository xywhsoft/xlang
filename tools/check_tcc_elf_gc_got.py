"""Actual GCC PIC archive -> TinyCC JIT section-GC/GOT hash regression.

The before-provider must reproduce the stale global hash tombstone failure;
the fixed provider must run the same archive and still reject a genuine user
global absolute GOT definition. No symbol renaming or duplicate suppression.
"""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, time


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'out', 'provider', 'before-provider', 'libraries'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    root, out, provider, before, libraries = [x.resolve() for x in
        (a.root, a.out, a.provider, a.before_provider, a.libraries)]
    assert os.name != 'nt'
    sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
    source = root / 'test/cases/tcc_elf_gc_pic.c'
    host = root / 'test/cases/tcc_elf_gc_pic_host.c'
    header = root / 'lib/tcc/libtcc.h'
    inputs = {str(f): sha(f) for f in (source, host, header, provider, before, Path(__file__))}
    out.mkdir(parents=True, exist_ok=False)
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs, rows=[])
    def save(): (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    def run(name, command):
        command = list(map(str, command)); start = time.monotonic()
        r = subprocess.run(command, cwd=out, capture_output=True, timeout=300)
        log = out / (name + '.log'); log.write_bytes(r.stdout + r.stderr)
        report['rows'].append(dict(name=name, command=command, exitCode=r.returncode,
            seconds=round(time.monotonic()-start, 3), logSha256=sha(log)))
        save(); print(name, r.returncode, flush=True); assert r.returncode == 0, log
    for kind in ('pic', 'conflict'):
        obj = out / (kind + '.o'); archive = out / (kind + '.a')
        flags = ['-DREAL_GOT_CONFLICT'] if kind == 'conflict' else []
        run(kind + '-native', ['gcc', '-O2', '-fPIC', '-ffunction-sections',
            '-fdata-sections', *flags, '-c', source, '-o', obj])
        run(kind + '-archive', ['ar', 'rcs', archive, obj])
        run(kind + '-symbol-oracle', ['readelf', '-Ws', obj])
        text = (out / (kind + '-symbol-oracle.log')).read_text()
        got = [line for line in text.splitlines() if line.endswith('_GLOBAL_OFFSET_TABLE_')]
        assert len(got) == 1 and (' UND ' if kind == 'pic' else ' ABS ') in got[0]
        assert 'GLOBAL' in got[0]
    for opt in ('O0', 'O2'):
        for label, library in (('before', before), ('fixed', provider)):
            exe = out / ('host-' + opt + '-' + label)
            run(opt + '-' + label + '-build', ['gcc', '-std=c11', '-Wall', '-Wextra',
                '-Werror', '-' + opt, '-I' + str(header.parent), host, library,
                '-Wl,-rpath,' + str(library.parent), '-ldl', '-lpthread', '-lm', '-o', exe])
            run(opt + '-' + label + '-pic', [exe, libraries, out / 'pic.a',
                'success' if label == 'fixed' else 'failure'])
            if label == 'fixed':
                run(opt + '-genuine-global-abs-zero-rejected',
                    [exe, libraries, out / 'conflict.a', 'failure'])
    report.update(complete=True, passed=True,
        inputsUnchanged=all(sha(f) == h for f, h in inputs.items()))
    assert report['inputsUnchanged']; save()


if __name__ == '__main__': main()
