#!/usr/bin/env python3
"""Freeze and test PE record-level exception GC using the actual libtcc body."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--cc', default='gcc')
    args = ap.parse_args()
    root = Path(args.root).resolve()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = out / 'source'
    inputs = []
    names = [p.relative_to(root) for p in (root / 'lib/tcc').rglob('*') if p.is_file()]
    names += [Path('test/cases/tcc_pdata_gc.c'), Path('tools/check_tcc_pdata_gc.py')]
    for rel in sorted(names):
        p, q = root / rel, source / rel
        q.parent.mkdir(parents=True, exist_ok=True)
        before = sha(p)
        shutil.copyfile(p, q)
        if sha(p) != before or sha(q) != before:
            raise RuntimeError('Source changed during freeze: ' + str(p))
        inputs.append({'path': rel.as_posix(), 'sha256': before, 'bytes': q.stat().st_size})
    cc = shutil.which(args.cc) or args.cc
    report = {
        'scope': 'Host GCC includes exact libtcc. PE record graph/query/compaction and actual CPU JIT/OS unwind lookup; no emitted PE image execution, GUI, network, release edits, or production-provider qualification.',
        'sourceRoot': str(source), 'compiler': cc, 'sourceInputs': inputs,
        'steps': [], 'passed': False,
    }

    def run(label, command):
        start = time.monotonic()
        p = subprocess.run([str(x) for x in command], cwd=out, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, encoding='utf-8', errors='replace', timeout=240)
        log = out / (label + '.log')
        log.write_text(p.stdout, encoding='utf-8')
        row = {'label': label, 'command': [str(x) for x in command], 'exitCode': p.returncode,
               'seconds': time.monotonic() - start, 'log': str(log), 'logSha256': sha(log)}
        report['steps'].append(row)
        if p.returncode:
            raise RuntimeError(label + ' failed; see ' + str(log))
        return p.stdout, row

    try:
        run('compiler-version', [cc, '--version'])
        for opt in ('O0', 'O2'):
            dest = out / opt
            dest.mkdir()
            exe = dest / ('fixture.exe' if os.name == 'nt' else 'fixture')
            command = [cc, '-std=c11', '-' + opt, '-DTCC_DISABLE_BUILTIN_VFS',
                       '-I' + str(source / 'lib/tcc'), source / 'test/cases/tcc_pdata_gc.c']
            command += ([source / 'lib/tcc/tcc_utf8_io.c', '-lws2_32'] if os.name == 'nt'
                        else ['-ldl', '-lpthread', '-lm'])
            command += ['-o', exe]
            run(opt + '-build', command)
            text, row = run(opt + '-cpu', [exe, source / 'lib/tcc', dest])
            match = re.search(r'TCC_PDATA_CHECKS=(\d+) FAILURES=(\d+)', text)
            if not match or int(match[2]):
                raise RuntimeError('Missing or failed fixture assertions')
            row['checks'] = int(match[1])
            row['target'] = 'PE-x64' if os.name == 'nt' else 'non-PE guard'
            row['artifacts'] = [{'path': str(p), 'sha256': sha(p), 'bytes': p.stat().st_size}
                                for p in sorted(dest.glob('*.o'))]
        report['frozenInputsUnchanged'] = all(sha(source / x['path']) == x['sha256'] for x in inputs)
        report['workspaceInputsStillMatch'] = all(sha(root / x['path']) == x['sha256'] for x in inputs)
        report['passed'] = report['frozenInputsUnchanged']
    except Exception as exc:
        report['error'] = str(exc)
    report_path = out / 'results.json'
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'passed': report['passed'], 'report': str(report_path),
                      'checks': sum(x.get('checks', 0) for x in report['steps'])}))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
